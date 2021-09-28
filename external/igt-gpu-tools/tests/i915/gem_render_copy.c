/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Damien Lespiau <damien.lespiau@intel.com>
 */

/*
 * This file is a basic test for the render_copy() function, a very simple
 * workload for the 3D engine.
 */

#include "igt.h"
#include "igt_x86.h"
#include <stdbool.h>
#include <unistd.h>
#include <cairo.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <drm.h>

#include "intel_bufmgr.h"

IGT_TEST_DESCRIPTION("Basic test for the render_copy() function.");

#define WIDTH 512
#define HEIGHT 512

typedef struct {
	int drm_fd;
	uint32_t devid;
	drm_intel_bufmgr *bufmgr;
	struct intel_batchbuffer *batch;
	igt_render_copyfunc_t render_copy;
} data_t;
static int opt_dump_png = false;
static int check_all_pixels = false;

static const char *make_filename(const char *filename)
{
	static char buf[64];

	snprintf(buf, sizeof(buf), "%s_%s", igt_subtest_name(), filename);

	return buf;
}

static void *yf_ptr(void *ptr,
		    unsigned int x, unsigned int y,
		    unsigned int stride, unsigned int cpp)
{
	const int tile_size = 4 * 1024;
	const int tile_width = 128;
	int row_size = (stride / tile_width) * tile_size;

	x *= cpp; /* convert to Byte offset */


	/*
	 * Within a 4k Yf tile, the byte swizzling pattern is
	 * msb......lsb
	 * xyxyxyyyxxxx
	 * The tiles themselves are laid out in row major order.
	 */
	return ptr +
		((x & 0xf) * 1) + /* 4x1 pixels(32bpp) = 16B */
		((y & 0x3) * 16) + /* 4x4 pixels = 64B */
		(((y & 0x4) >> 2) * 64) + /* 1x2 64B blocks */
		(((x & 0x10) >> 4) * 128) + /* 2x2 64B blocks = 256B block */
		(((y & 0x8) >> 3) * 256) + /* 2x1 256B blocks */
		(((x & 0x20) >> 5) * 512) + /* 2x2 256B blocks */
		(((y & 0x10) >> 4) * 1024) + /* 4x2 256 blocks */
		(((x & 0x40) >> 6) * 2048) + /* 4x4 256B blocks = 4k tile */
		(((x & ~0x7f) >> 7) * tile_size) + /* row of tiles */
		(((y & ~0x1f) >> 5) * row_size);
}

static void copy_linear_to_yf(data_t *data, struct igt_buf *buf,
			      const uint32_t *linear)
{
	int height = igt_buf_height(buf);
	int width = igt_buf_width(buf);
	void *map;

	gem_set_domain(data->drm_fd, buf->bo->handle,
		       I915_GEM_DOMAIN_CPU, I915_GEM_DOMAIN_CPU);
	map = gem_mmap__cpu(data->drm_fd, buf->bo->handle, 0,
			    buf->bo->size, PROT_READ | PROT_WRITE);

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			uint32_t *ptr = yf_ptr(map, x, y,
					       buf->stride, buf->bpp / 8);

			*ptr = linear[y * width + x];
		}
	}

	munmap(map, buf->bo->size);
}

static void copy_yf_to_linear(data_t *data, struct igt_buf *buf,
			      uint32_t *linear)
{
	int height = igt_buf_height(buf);
	int width = igt_buf_width(buf);
	void *map;

	gem_set_domain(data->drm_fd, buf->bo->handle,
		       I915_GEM_DOMAIN_CPU, 0);
	map = gem_mmap__cpu(data->drm_fd, buf->bo->handle, 0,
			    buf->bo->size, PROT_READ);

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			uint32_t *ptr = yf_ptr(map, x, y,
					       buf->stride, buf->bpp / 8);

			linear[y * width + x] = *ptr;
		}
	}

	munmap(map, buf->bo->size);
}

static void copy_linear_to_gtt(data_t *data, struct igt_buf *buf,
			       const uint32_t *linear)
{
	void *map;

	gem_set_domain(data->drm_fd, buf->bo->handle,
		       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);

	map = gem_mmap__gtt(data->drm_fd, buf->bo->handle,
			    buf->bo->size, PROT_READ | PROT_WRITE);

	memcpy(map, linear, buf->bo->size);

	munmap(map, buf->bo->size);
}

static void copy_gtt_to_linear(data_t *data, struct igt_buf *buf,
			       uint32_t *linear)
{
	void *map;

	gem_set_domain(data->drm_fd, buf->bo->handle,
		       I915_GEM_DOMAIN_GTT, 0);

	map = gem_mmap__gtt(data->drm_fd, buf->bo->handle,
			    buf->bo->size, PROT_READ);

	igt_memcpy_from_wc(linear, map, buf->bo->size);

	munmap(map, buf->bo->size);
}

static void *linear_copy(data_t *data, struct igt_buf *buf)
{
	void *linear;

	/* 16B alignment allows to potentially make use of SSE4 for copying */
	igt_assert_eq(posix_memalign(&linear, 16, buf->bo->size), 0);

	if (buf->tiling == I915_TILING_Yf)
		copy_yf_to_linear(data, buf, linear);
	else
		copy_gtt_to_linear(data, buf, linear);

	return linear;
}

static void scratch_buf_write_to_png(data_t *data, struct igt_buf *buf,
				     const char *filename)
{
	cairo_surface_t *surface;
	cairo_status_t ret;
	void *linear;

	linear = linear_copy(data, buf);

	surface = cairo_image_surface_create_for_data(linear,
						      CAIRO_FORMAT_RGB24,
						      igt_buf_width(buf),
						      igt_buf_height(buf),
						      buf->stride);
	ret = cairo_surface_write_to_png(surface, make_filename(filename));
	igt_assert(ret == CAIRO_STATUS_SUCCESS);
	cairo_surface_destroy(surface);

	free(linear);
}

static int scratch_buf_aux_width(const struct igt_buf *buf)
{
	return DIV_ROUND_UP(igt_buf_width(buf), 1024) * 128;
}

static int scratch_buf_aux_height(const struct igt_buf *buf)
{
	return DIV_ROUND_UP(igt_buf_height(buf), 512) * 32;
}

static void *linear_copy_aux(data_t *data, struct igt_buf *buf)
{
	void *map, *linear;
	int aux_size = scratch_buf_aux_width(buf) *
		scratch_buf_aux_height(buf);

	igt_assert_eq(posix_memalign(&linear, 16, aux_size), 0);

	gem_set_domain(data->drm_fd, buf->bo->handle,
		       I915_GEM_DOMAIN_GTT, 0);

	map = gem_mmap__gtt(data->drm_fd, buf->bo->handle,
			    buf->bo->size, PROT_READ);

	igt_memcpy_from_wc(linear, map + buf->aux.offset, aux_size);

	munmap(map, buf->bo->size);

	return linear;
}

static void scratch_buf_aux_write_to_png(data_t *data,
					 struct igt_buf *buf,
					 const char *filename)
{
	cairo_surface_t *surface;
	cairo_status_t ret;
	void *linear;

	linear = linear_copy_aux(data, buf);

	surface = cairo_image_surface_create_for_data(linear,
						      CAIRO_FORMAT_A8,
						      scratch_buf_aux_width(buf),
						      scratch_buf_aux_height(buf),
						      buf->aux.stride);
	ret = cairo_surface_write_to_png(surface, make_filename(filename));
	igt_assert(ret == CAIRO_STATUS_SUCCESS);
	cairo_surface_destroy(surface);

	free(linear);
}

static void scratch_buf_draw_pattern(data_t *data, struct igt_buf *buf,
				     int x, int y, int w, int h,
				     int cx, int cy, int cw, int ch,
				     bool use_alternate_colors)
{
	cairo_surface_t *surface;
	cairo_pattern_t *pat;
	cairo_t *cr;
	void *linear;

	linear = linear_copy(data, buf);

	surface = cairo_image_surface_create_for_data(linear,
						      CAIRO_FORMAT_RGB24,
						      igt_buf_width(buf),
						      igt_buf_height(buf),
						      buf->stride);

	cr = cairo_create(surface);

	cairo_rectangle(cr, cx, cy, cw, ch);
	cairo_clip(cr);

	pat = cairo_pattern_create_mesh();
	cairo_mesh_pattern_begin_patch(pat);
	cairo_mesh_pattern_move_to(pat, x,   y);
	cairo_mesh_pattern_line_to(pat, x+w, y);
	cairo_mesh_pattern_line_to(pat, x+w, y+h);
	cairo_mesh_pattern_line_to(pat, x,   y+h);
	if (use_alternate_colors) {
		cairo_mesh_pattern_set_corner_color_rgb(pat, 0, 0.0, 1.0, 1.0);
		cairo_mesh_pattern_set_corner_color_rgb(pat, 1, 1.0, 0.0, 1.0);
		cairo_mesh_pattern_set_corner_color_rgb(pat, 2, 1.0, 1.0, 0.0);
		cairo_mesh_pattern_set_corner_color_rgb(pat, 3, 0.0, 0.0, 0.0);
	} else {
		cairo_mesh_pattern_set_corner_color_rgb(pat, 0, 1.0, 0.0, 0.0);
		cairo_mesh_pattern_set_corner_color_rgb(pat, 1, 0.0, 1.0, 0.0);
		cairo_mesh_pattern_set_corner_color_rgb(pat, 2, 0.0, 0.0, 1.0);
		cairo_mesh_pattern_set_corner_color_rgb(pat, 3, 1.0, 1.0, 1.0);
	}
	cairo_mesh_pattern_end_patch(pat);

	cairo_rectangle(cr, x, y, w, h);
	cairo_set_source(cr, pat);
	cairo_fill(cr);
	cairo_pattern_destroy(pat);

	cairo_destroy(cr);

	cairo_surface_destroy(surface);

	if (buf->tiling == I915_TILING_Yf)
		copy_linear_to_yf(data, buf, linear);
	else
		copy_linear_to_gtt(data, buf, linear);

	free(linear);
}

static void
scratch_buf_copy(data_t *data,
		 struct igt_buf *src, int sx, int sy, int w, int h,
		 struct igt_buf *dst, int dx, int dy)
{
	int width = igt_buf_width(dst);
	int height  = igt_buf_height(dst);
	uint32_t *linear_dst;

	igt_assert_eq(igt_buf_width(dst), igt_buf_width(src));
	igt_assert_eq(igt_buf_height(dst), igt_buf_height(src));
	igt_assert_eq(dst->bo->size, src->bo->size);
	igt_assert_eq(dst->bpp, src->bpp);

	w = min(w, width - sx);
	w = min(w, width - dx);

	h = min(h, height - sy);
	h = min(h, height - dy);

	gem_set_domain(data->drm_fd, dst->bo->handle,
		       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	linear_dst = gem_mmap__gtt(data->drm_fd, dst->bo->handle,
				   dst->bo->size, PROT_WRITE);

	if (src->tiling == I915_TILING_Yf) {
		void *map;

		gem_set_domain(data->drm_fd, src->bo->handle,
			       I915_GEM_DOMAIN_CPU, 0);
		map = gem_mmap__cpu(data->drm_fd, src->bo->handle, 0,
				    src->bo->size, PROT_READ);

		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				const uint32_t *ptr = yf_ptr(map, sx+x, sy+y,
							     src->stride,
							     src->bpp / 8);

				linear_dst[(dy+y) * width + dx+x] = *ptr;
			}
		}

		munmap(map, src->bo->size);
	} else {
		uint32_t *linear_src;

		gem_set_domain(data->drm_fd, src->bo->handle,
			       I915_GEM_DOMAIN_GTT, 0);

		linear_src = gem_mmap__gtt(data->drm_fd, src->bo->handle,
					   src->bo->size, PROT_READ);

		for (int y = 0; y < h; y++) {
			igt_memcpy_from_wc(&linear_dst[(dy+y) * width + dx],
					   &linear_src[(sy+y) * width + sx],
					   w * (src->bpp / 8));
		}

		munmap(linear_src, src->bo->size);
	}

	munmap(linear_dst, dst->bo->size);
}

static void scratch_buf_init(data_t *data, struct igt_buf *buf,
			     int width, int height,
			     uint32_t req_tiling, bool ccs)
{
	uint32_t tiling = req_tiling;
	unsigned long pitch;
	int bpp = 32;

	memset(buf, 0, sizeof(*buf));

	if (ccs) {
		int aux_width, aux_height;
		int size;

		igt_require(intel_gen(data->devid) >= 9);
		igt_assert(tiling == I915_TILING_Y ||
			   tiling == I915_TILING_Yf);

		buf->stride = ALIGN(width * (bpp / 8), 128);
		buf->size = buf->stride * height;
		buf->tiling = tiling;
		buf->bpp = bpp;

		aux_width = scratch_buf_aux_width(buf);
		aux_height = scratch_buf_aux_height(buf);

		buf->aux.offset = buf->stride * ALIGN(height, 32);
		buf->aux.stride = aux_width;

		size = buf->aux.offset + aux_width * aux_height;

		buf->bo = drm_intel_bo_alloc(data->bufmgr, "", size, 4096);

		if (tiling == I915_TILING_Y) {
			drm_intel_bo_set_tiling(buf->bo, &tiling, buf->stride);
			igt_assert_eq(tiling, req_tiling);
		}
	} else if (req_tiling == I915_TILING_Yf) {
		int size;

		buf->stride = ALIGN(width * (bpp / 8), 128);
		buf->size = buf->stride * height;
		buf->tiling = tiling;
		buf->bpp = bpp;

		size = buf->stride * ALIGN(height, 32);

		buf->bo = drm_intel_bo_alloc(data->bufmgr, "", size, 4096);
	} else {
		buf->bo = drm_intel_bo_alloc_tiled(data->bufmgr, "",
						   width, height, bpp / 8,
						   &tiling, &pitch, 0);
		igt_assert_eq(tiling, req_tiling);

		buf->stride = pitch;
		buf->tiling = tiling;
		buf->size = pitch * height;
		buf->bpp = bpp;
	}

	igt_assert(igt_buf_width(buf) == width);
	igt_assert(igt_buf_height(buf) == height);
}

static void scratch_buf_fini(struct igt_buf *buf)
{
	drm_intel_bo_unreference(buf->bo);
}

static void
scratch_buf_check(data_t *data,
		  struct igt_buf *buf,
		  struct igt_buf *ref,
		  int x, int y)
{
	int width = igt_buf_width(buf);
	uint32_t buf_val, ref_val;
	uint32_t *linear;

	igt_assert_eq(igt_buf_width(buf), igt_buf_width(ref));
	igt_assert_eq(igt_buf_height(buf), igt_buf_height(ref));
	igt_assert_eq(buf->bo->size, ref->bo->size);

	linear = linear_copy(data, buf);
	buf_val = linear[y * width + x];
	free(linear);

	linear = linear_copy(data, ref);
	ref_val = linear[y * width + x];
	free(linear);

	igt_assert_f(buf_val == ref_val,
		     "Expected 0x%08x, found 0x%08x at (%d,%d)\n",
		     ref_val, buf_val, x, y);
}

static void
scratch_buf_check_all(data_t *data,
		      struct igt_buf *buf,
		      struct igt_buf *ref)
{
	int width = igt_buf_width(buf);
	int height  = igt_buf_height(buf);
	uint32_t *linear_buf, *linear_ref;

	igt_assert_eq(igt_buf_width(buf), igt_buf_width(ref));
	igt_assert_eq(igt_buf_height(buf), igt_buf_height(ref));
	igt_assert_eq(buf->bo->size, ref->bo->size);

	linear_buf = linear_copy(data, buf);
	linear_ref = linear_copy(data, ref);

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			uint32_t buf_val = linear_buf[y * width + x];
			uint32_t ref_val = linear_ref[y * width + x];

			igt_assert_f(buf_val == ref_val,
				     "Expected 0x%08x, found 0x%08x at (%d,%d)\n",
				     ref_val, buf_val, x, y);
		}
	}

	free(linear_ref);
	free(linear_buf);
}

static void scratch_buf_aux_check(data_t *data,
				  struct igt_buf *buf)
{
	int aux_size = scratch_buf_aux_width(buf) *
		scratch_buf_aux_height(buf);
	uint8_t *linear;
	int i;

	linear = linear_copy_aux(data, buf);

	for (i = 0; i < aux_size; i++) {
		if (linear[i])
			break;
	}

	free(linear);

	igt_assert_f(i < aux_size,
		     "Aux surface indicates that nothing was compressed\n");
}

static void test(data_t *data, uint32_t tiling, uint64_t ccs_modifier)
{
	struct igt_buf dst, ccs, ref;
	struct {
		struct igt_buf buf;
		const char *filename;
		uint32_t tiling;
		int x, y;
	} src[] = {
		{
			.filename = "source-linear.png",
			.tiling = I915_TILING_NONE,
			.x = 1, .y = HEIGHT/2+1,
		},
		{
			.filename = "source-x-tiled.png",
			.tiling = I915_TILING_X,
			.x = WIDTH/2+1, .y = HEIGHT/2+1,
		},
		{
			.filename = "source-y-tiled.png",
			.tiling = I915_TILING_Y,
			.x = WIDTH/2+1, .y = 1,
		},
		{
			.filename = "source-yf-tiled.png",
			.tiling = I915_TILING_Yf,
			.x = 1, .y = 1,
		},
	};

	int opt_dump_aub = igt_aub_dump_enabled();
	int num_src = ARRAY_SIZE(src);

	/* no Yf before gen9 */
	if (intel_gen(data->devid) < 9)
		num_src--;

	if (tiling == I915_TILING_Yf || ccs_modifier)
		igt_require(intel_gen(data->devid) >= 9);

	for (int i = 0; i < num_src; i++)
		scratch_buf_init(data, &src[i].buf, WIDTH, HEIGHT, src[i].tiling, false);
	scratch_buf_init(data, &dst, WIDTH, HEIGHT, tiling, false);
	if (ccs_modifier)
		scratch_buf_init(data, &ccs, WIDTH, HEIGHT, ccs_modifier, true);
	scratch_buf_init(data, &ref, WIDTH, HEIGHT, I915_TILING_NONE, false);

	for (int i = 0; i < num_src; i++)
		scratch_buf_draw_pattern(data, &src[i].buf,
					 0, 0, WIDTH, HEIGHT,
					 0, 0, WIDTH, HEIGHT, true);
	scratch_buf_draw_pattern(data, &dst,
				 0, 0, WIDTH, HEIGHT,
				 0, 0, WIDTH, HEIGHT, false);

	scratch_buf_copy(data,
			 &dst, 0, 0, WIDTH, HEIGHT,
			 &ref, 0, 0);
	for (int i = 0; i < num_src; i++)
		scratch_buf_copy(data,
				 &src[i].buf, WIDTH/4, HEIGHT/4, WIDTH/2-2, HEIGHT/2-2,
				 &ref, src[i].x, src[i].y);

	if (opt_dump_png) {
		for (int i = 0; i < num_src; i++)
			scratch_buf_write_to_png(data, &src[i].buf, src[i].filename);
		scratch_buf_write_to_png(data, &dst, "destination.png");
		scratch_buf_write_to_png(data, &ref, "reference.png");
	}

	if (opt_dump_aub) {
		drm_intel_bufmgr_gem_set_aub_filename(data->bufmgr,
						      "rendercopy.aub");
		drm_intel_bufmgr_gem_set_aub_dump(data->bufmgr, true);
	}

	/* This will copy the src to the mid point of the dst buffer. Presumably
	 * the out of bounds accesses will get clipped.
	 * Resulting buffer should look like:
	 *	  _______
	 *	 |dst|dst|
	 *	 |dst|src|
	 *	  -------
	 */
	if (ccs_modifier)
		data->render_copy(data->batch, NULL,
				  &dst, 0, 0, WIDTH, HEIGHT,
				  &ccs, 0, 0);

	for (int i = 0; i < num_src; i++)
		data->render_copy(data->batch, NULL,
				  &src[i].buf, WIDTH/4, HEIGHT/4, WIDTH/2-2, HEIGHT/2-2,
				  ccs_modifier ? &ccs : &dst, src[i].x, src[i].y);

	if (ccs_modifier)
		data->render_copy(data->batch, NULL,
				  &ccs, 0, 0, WIDTH, HEIGHT,
				  &dst, 0, 0);

	if (opt_dump_png){
		scratch_buf_write_to_png(data, &dst, "result.png");
		if (ccs_modifier) {
			scratch_buf_write_to_png(data, &ccs, "compressed.png");
			scratch_buf_aux_write_to_png(data, &ccs, "compressed-aux.png");
		}
	}

	if (opt_dump_aub) {
		drm_intel_gem_bo_aub_dump_bmp(dst.bo,
					      0, 0, igt_buf_width(&dst),
					      igt_buf_height(&dst),
					      AUB_DUMP_BMP_FORMAT_ARGB_8888,
					      dst.stride, 0);
		drm_intel_bufmgr_gem_set_aub_dump(data->bufmgr, false);
	} else if (check_all_pixels) {
		scratch_buf_check_all(data, &dst, &ref);
	} else {
		scratch_buf_check(data, &dst, &ref, 10, 10);
		scratch_buf_check(data, &dst, &ref, WIDTH - 10, HEIGHT - 10);
	}

	if (ccs_modifier)
		scratch_buf_aux_check(data, &ccs);

	scratch_buf_fini(&ref);
	if (ccs_modifier)
		scratch_buf_fini(&ccs);
	scratch_buf_fini(&dst);
	for (int i = 0; i < num_src; i++)
		scratch_buf_fini(&src[i].buf);
}

static int opt_handler(int opt, int opt_index, void *data)
{
	switch (opt) {
	case 'd':
		opt_dump_png = true;
		break;
	case 'a':
		check_all_pixels = true;
		break;
	default:
		return IGT_OPT_HANDLER_ERROR;
	}

	return IGT_OPT_HANDLER_SUCCESS;
}

const char *help_str =
	"  -d\tDump PNG\n"
	"  -a\tCheck all pixels\n"
	;

igt_main_args("da", NULL, help_str, opt_handler, NULL)
{
	data_t data = {0, };

	igt_fixture {
		data.drm_fd = drm_open_driver_render(DRIVER_INTEL);
		data.devid = intel_get_drm_devid(data.drm_fd);
		igt_require_gem(data.drm_fd);

		data.bufmgr = drm_intel_bufmgr_gem_init(data.drm_fd, 4096);
		igt_assert(data.bufmgr);

		data.render_copy = igt_get_render_copyfunc(data.devid);
		igt_require_f(data.render_copy,
			      "no render-copy function\n");

		data.batch = intel_batchbuffer_alloc(data.bufmgr, data.devid);
		igt_assert(data.batch);
	}

	igt_subtest("linear")
		test(&data, I915_TILING_NONE, 0);
	igt_subtest("x-tiled")
		test(&data, I915_TILING_X, 0);
	igt_subtest("y-tiled")
		test(&data, I915_TILING_Y, 0);
	igt_subtest("yf-tiled")
		test(&data, I915_TILING_Yf, 0);

	igt_subtest("y-tiled-ccs-to-linear")
		test(&data, I915_TILING_NONE, I915_TILING_Y);
	igt_subtest("y-tiled-ccs-to-x-tiled")
		test(&data, I915_TILING_X, I915_TILING_Y);
	igt_subtest("y-tiled-ccs-to-y-tiled")
		test(&data, I915_TILING_Y, I915_TILING_Y);
	igt_subtest("y-tiled-ccs-to-yf-tiled")
		test(&data, I915_TILING_Yf, I915_TILING_Y);

	igt_subtest("yf-tiled-ccs-to-linear")
		test(&data, I915_TILING_NONE, I915_TILING_Yf);
	igt_subtest("yf-tiled-ccs-to-x-tiled")
		test(&data, I915_TILING_X, I915_TILING_Yf);
	igt_subtest("yf-tiled-ccs-to-y-tiled")
		test(&data, I915_TILING_Y, I915_TILING_Yf);
	igt_subtest("yf-tiled-ccs-to-yf-tiled")
		test(&data, I915_TILING_Yf, I915_TILING_Yf);

	igt_fixture {
		intel_batchbuffer_free(data.batch);
		drm_intel_bufmgr_destroy(data.bufmgr);
	}
}
