/*
 * Copyright © 2019 Intel Corporation
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
 */

#include "igt.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

IGT_TEST_DESCRIPTION("Test big framebuffers");

typedef struct {
	int drm_fd;
	uint32_t devid;
	igt_display_t display;
	enum pipe pipe;
	igt_output_t *output;
	igt_plane_t *plane;
	igt_pipe_crc_t *pipe_crc;
	struct igt_fb small_fb, big_fb;
	uint32_t format;
	uint64_t modifier;
	int width, height;
	igt_rotation_t rotation;
	int max_fb_width, max_fb_height;
	int big_fb_width, big_fb_height;
	uint64_t ram_size, aper_size, mappable_size;
	igt_render_copyfunc_t render_copy;
	drm_intel_bufmgr *bufmgr;
	struct intel_batchbuffer *batch;
} data_t;

static void init_buf(data_t *data,
		     struct igt_buf *buf,
		     const struct igt_fb *fb,
		     const char *name)
{
	igt_assert_eq(fb->offsets[0], 0);

	buf->bo = gem_handle_to_libdrm_bo(data->bufmgr, data->drm_fd,
					  name, fb->gem_handle);
	buf->tiling = igt_fb_mod_to_tiling(fb->modifier);
	buf->stride = fb->strides[0];
	buf->bpp = fb->plane_bpp[0];
	buf->size = fb->size;
}

static void fini_buf(struct igt_buf *buf)
{
	drm_intel_bo_unreference(buf->bo);
}

static void copy_pattern(data_t *data,
			 struct igt_fb *dst_fb, int dx, int dy,
			 struct igt_fb *src_fb, int sx, int sy,
			 int w, int h)
{
	struct igt_buf src = {}, dst = {};

	init_buf(data, &src, src_fb, "big fb src");
	init_buf(data, &dst, dst_fb, "big fb dst");

	gem_set_domain(data->drm_fd, dst_fb->gem_handle,
		       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	gem_set_domain(data->drm_fd, src_fb->gem_handle,
		       I915_GEM_DOMAIN_GTT, 0);

	/*
	 * We expect the kernel to limit the max fb
	 * size/stride to something that can still
	 * rendered with the blitter/render engine.
	 */
	if (data->render_copy) {
		data->render_copy(data->batch, NULL, &src, sx, sy, w, h, &dst, dx, dy);
	} else {
		w = min(w, src_fb->width - sx);
		w = min(w, dst_fb->width - dx);

		h = min(h, src_fb->height - sy);
		h = min(h, dst_fb->height - dy);

		intel_blt_copy(data->batch, src.bo, sx, sy, src.stride,
			       dst.bo, dx, dy, dst.stride, w, h, dst.bpp);
	}

	fini_buf(&dst);
	fini_buf(&src);
}

static void generate_pattern(data_t *data,
			     struct igt_fb *fb,
			     int w, int h)
{
	struct igt_fb pat_fb;

	igt_create_pattern_fb(data->drm_fd, w, h,
			      data->format, data->modifier,
			      &pat_fb);

	for (int y = 0; y < fb->height; y += h) {
		for (int x = 0; x < fb->width; x += w) {
			copy_pattern(data, fb, x, y,
				     &pat_fb, 0, 0,
				     pat_fb.width, pat_fb.height);
			w++;
			h++;
		}
	}

	igt_remove_fb(data->drm_fd, &pat_fb);
}

static bool size_ok(data_t *data, uint64_t size)
{
	/*
	 * The kernel limits scanout to the
	 * mappable portion of ggtt on gmch platforms.
	 */
	if ((intel_gen(data->devid) < 5 ||
	     IS_VALLEYVIEW(data->devid) ||
	     IS_CHERRYVIEW(data->devid)) &&
	    size > data->mappable_size / 2)
		return false;

	/*
	 * Limit the big fb size to at most half the RAM or half
	 * the aperture size. Could go a bit higher I suppose since
	 * we shouldn't need more than one big fb at a time.
	 */
	if (size > data->ram_size / 2 || size > data->aper_size / 2)
		return false;

	return true;
}


static void max_fb_size(data_t *data, int *width, int *height,
			uint32_t format, uint64_t modifier)
{
	unsigned int stride;
	uint64_t size;
	int i = 0;

	*width = data->max_fb_width;
	*height = data->max_fb_height;

	/* max fence stride is only 8k bytes on gen3 */
	if (intel_gen(data->devid) < 4 &&
	    format == DRM_FORMAT_XRGB8888)
		*width = min(*width, 8192 / 4);

	igt_calc_fb_size(data->drm_fd, *width, *height,
			 format, modifier, &size, &stride);

	while (!size_ok(data, size)) {
		if (i++ & 1)
			*width >>= 1;
		else
			*height >>= 1;

		igt_calc_fb_size(data->drm_fd, *width, *height,
				 format, modifier, &size, &stride);
	}

	igt_info("Max usable framebuffer size for format "IGT_FORMAT_FMT" / modifier 0x%"PRIx64": %dx%d\n",
		 IGT_FORMAT_ARGS(format), modifier,
		 *width, *height);
}

static void prep_fb(data_t *data)
{
	if (data->big_fb.fb_id)
		return;

	igt_create_fb(data->drm_fd,
		      data->big_fb_width, data->big_fb_height,
		      data->format, data->modifier,
		      &data->big_fb);

	generate_pattern(data, &data->big_fb, 640, 480);
}

static void cleanup_fb(data_t *data)
{
	igt_remove_fb(data->drm_fd, &data->big_fb);
	data->big_fb.fb_id = 0;
}

static void set_c8_lut(data_t *data)
{
	igt_pipe_t *pipe = &data->display.pipes[data->pipe];
	struct drm_color_lut *lut;
	int i, lut_size = 256;

	lut = calloc(lut_size, sizeof(lut[0]));

	/* igt_fb uses RGB332 for C8 */
	for (i = 0; i < lut_size; i++) {
		lut[i].red = ((i & 0xe0) >> 5) * 0xffff / 0x7;
		lut[i].green = ((i & 0x1c) >> 2) * 0xffff / 0x7;
		lut[i].blue = ((i & 0x03) >> 0) * 0xffff / 0x3;
	}

	igt_pipe_obj_replace_prop_blob(pipe, IGT_CRTC_GAMMA_LUT, lut,
				       lut_size * sizeof(lut[0]));

	free(lut);
}

static void unset_lut(data_t *data)
{
	igt_pipe_t *pipe = &data->display.pipes[data->pipe];

	igt_pipe_obj_replace_prop_blob(pipe, IGT_CRTC_GAMMA_LUT, NULL, 0);
}

static bool test_plane(data_t *data)
{
	igt_plane_t *plane = data->plane;
	struct igt_fb *small_fb = &data->small_fb;
	struct igt_fb *big_fb = &data->big_fb;
	int w = data->big_fb_width - small_fb->width;
	int h = data->big_fb_height - small_fb->height;
	struct {
		int x, y;
	} coords[] = {
		/* bunch of coordinates pulled out of thin air */
		{ 0, 0, },
		{ w * 4 / 7, h / 5, },
		{ w * 3 / 7, h / 3, },
		{ w / 2, h / 2, },
		{ w / 3, h * 3 / 4, },
		{ w, h, },
	};

	if (!igt_plane_has_format_mod(plane, data->format, data->modifier))
		return false;

	if (data->rotation != IGT_ROTATION_0 &&
	    !igt_plane_has_prop(plane, IGT_PLANE_ROTATION))
		return false;

	/* FIXME need atomic on i965/g4x */
	if (data->rotation != IGT_ROTATION_0 &&
	    data->rotation != IGT_ROTATION_180 &&
	    !data->display.is_atomic)
		return false;

	if (igt_plane_has_prop(plane, IGT_PLANE_ROTATION))
		igt_plane_set_rotation(plane, data->rotation);
	igt_plane_set_position(plane, 0, 0);

	for (int i = 0; i < ARRAY_SIZE(coords); i++) {
		igt_crc_t small_crc, big_crc;
		int x = coords[i].x;
		int y = coords[i].y;

		/* Hardware limitation */
		if (data->format == DRM_FORMAT_RGB565 &&
		    (data->rotation == IGT_ROTATION_90 ||
		     data->rotation == IGT_ROTATION_270)) {
			x &= ~1;
			y &= ~1;
		}

		igt_plane_set_fb(plane, small_fb);
		igt_plane_set_size(plane, data->width, data->height);

		/*
		 * Try to check that the rotation+format+modifier
		 * combo is supported.
		 */
		if (i == 0 && data->display.is_atomic &&
		    igt_display_try_commit_atomic(&data->display,
						  DRM_MODE_ATOMIC_TEST_ONLY,
						  NULL) != 0) {
			if (igt_plane_has_prop(plane, IGT_PLANE_ROTATION))
				igt_plane_set_rotation(plane, IGT_ROTATION_0);
			igt_plane_set_fb(plane, NULL);
			return false;
		}

		/*
		 * To speed up skips we delay the big fb creation until
		 * the above rotation related check has been performed.
		 */
		prep_fb(data);

		/*
		 * Make a 1:1 copy of the desired part of the big fb
		 * rather than try to render the same pattern (translated
		 * accordinly) again via cairo. Something in cairo's
		 * rendering pipeline introduces slight differences into
		 * the result if we try that, and so the crc will not match.
		 */
		copy_pattern(data, small_fb, 0, 0, big_fb, x, y,
			     small_fb->width, small_fb->height);

		igt_display_commit2(&data->display, data->display.is_atomic ?
				    COMMIT_ATOMIC : COMMIT_UNIVERSAL);


		igt_pipe_crc_collect_crc(data->pipe_crc, &small_crc);

		igt_plane_set_fb(plane, big_fb);
		igt_fb_set_position(big_fb, plane, x, y);
		igt_fb_set_size(big_fb, plane, small_fb->width, small_fb->height);
		igt_plane_set_size(plane, data->width, data->height);
		igt_display_commit2(&data->display, data->display.is_atomic ?
				    COMMIT_ATOMIC : COMMIT_UNIVERSAL);

		igt_pipe_crc_collect_crc(data->pipe_crc, &big_crc);

		igt_plane_set_fb(plane, NULL);

		igt_assert_crc_equal(&big_crc, &small_crc);
	}

	return true;
}

static bool test_pipe(data_t *data)
{
	drmModeModeInfo *mode;
	igt_plane_t *primary;
	int width, height;
	bool ret = false;

	if (data->format == DRM_FORMAT_C8 &&
	    !igt_pipe_obj_has_prop(&data->display.pipes[data->pipe],
				   IGT_CRTC_GAMMA_LUT))
		return false;

	mode = igt_output_get_mode(data->output);

	data->width = mode->hdisplay;
	data->height = mode->vdisplay;

	width = mode->hdisplay;
	height = mode->vdisplay;
	if (data->rotation == IGT_ROTATION_90 ||
	    data->rotation == IGT_ROTATION_270)
		igt_swap(width, height);

	igt_create_color_fb(data->drm_fd, width, height,
			    data->format, data->modifier,
			    0, 1, 0, &data->small_fb);

	igt_output_set_pipe(data->output, data->pipe);

	primary = igt_output_get_plane_type(data->output, DRM_PLANE_TYPE_PRIMARY);
	igt_plane_set_fb(primary, NULL);

	if (!data->display.is_atomic) {
		struct igt_fb fb;

		igt_create_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
			      DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_LINEAR,
			      &fb);

		/* legacy setcrtc needs an fb */
		igt_plane_set_fb(primary, &fb);
		igt_display_commit2(&data->display, COMMIT_LEGACY);

		igt_plane_set_fb(primary, NULL);
		igt_display_commit2(&data->display, COMMIT_UNIVERSAL);

		igt_remove_fb(data->drm_fd, &fb);
	}

	if (data->format == DRM_FORMAT_C8)
		set_c8_lut(data);

	igt_display_commit2(&data->display, data->display.is_atomic ?
			    COMMIT_ATOMIC : COMMIT_UNIVERSAL);

	data->pipe_crc = igt_pipe_crc_new(data->drm_fd, data->pipe,
					  INTEL_PIPE_CRC_SOURCE_AUTO);

	for_each_plane_on_pipe(&data->display, data->pipe, data->plane) {
		ret = test_plane(data);
		if (ret)
			break;
	}

	if (data->format == DRM_FORMAT_C8)
		unset_lut(data);

	igt_pipe_crc_free(data->pipe_crc);

	igt_output_set_pipe(data->output, PIPE_ANY);

	igt_remove_fb(data->drm_fd, &data->small_fb);

	return ret;
}

static void test_scanout(data_t *data)
{
	max_fb_size(data, &data->big_fb_width, &data->big_fb_height,
		    data->format, data->modifier);

	for_each_pipe_with_valid_output(&data->display, data->pipe, data->output) {
		if (test_pipe(data))
			return;
		break;
	}

	igt_skip("unsupported configuration\n");
}

static void
test_size_overflow(data_t *data)
{
	uint32_t fb_id;
	uint32_t bo;
	uint32_t offsets[4] = {};
	uint32_t strides[4] = { 256*1024, };
	int ret;

	igt_require(igt_display_has_format_mod(&data->display,
					       DRM_FORMAT_XRGB8888,
					       data->modifier));

	/*
	 * Try to hit a specific integer overflow in i915 fb size
	 * calculations. 256k * 16k == 1<<32 which is checked
	 * against the bo size. The check should fail on account
	 * of the bo being smaller, but due to the overflow the
	 * computed fb size is 0 and thus the check never trips.
	 */
	igt_require(data->max_fb_width >= 16383 &&
		    data->max_fb_height >= 16383);

	bo = gem_create(data->drm_fd, (1ULL << 32) - 4096);
	igt_require(bo);

	ret = __kms_addfb(data->drm_fd, bo,
			  16383, 16383,
			  DRM_FORMAT_XRGB8888,
			  data->modifier,
			  strides, offsets, 1,
			  DRM_MODE_FB_MODIFIERS, &fb_id);

	igt_assert_neq(ret, 0);

	gem_close(data->drm_fd, bo);
}

static void
test_size_offset_overflow(data_t *data)
{
	uint32_t fb_id;
	uint32_t bo;
	uint32_t offsets[4] = {};
	uint32_t strides[4] = { 8192, };
	int ret;

	igt_require(igt_display_has_format_mod(&data->display,
					       DRM_FORMAT_NV12,
					       data->modifier));

	/*
	 * Try to hit a specific integer overflow in i915 fb size
	 * calculations. This time it's offsets[1] + the tile
	 * aligned chroma plane size that overflows and
	 * incorrectly passes the bo size check.
	 */
	igt_require(igt_display_has_format_mod(&data->display,
					       DRM_FORMAT_NV12,
					       data->modifier));

	bo = gem_create(data->drm_fd, (1ULL << 32) - 4096);
	igt_require(bo);

	offsets[0] = 0;
	offsets[1] = (1ULL << 32) - 8192 * 4096;

	ret = __kms_addfb(data->drm_fd, bo,
			  8192, 8188,
			  DRM_FORMAT_NV12,
			  data->modifier,
			  strides, offsets, 1,
			  DRM_MODE_FB_MODIFIERS, &fb_id);
	igt_assert_neq(ret, 0);

	gem_close(data->drm_fd, bo);
}

static int rmfb(int fd, uint32_t id)
{
	int err;

	err = 0;
	if (igt_ioctl(fd, DRM_IOCTL_MODE_RMFB, &id))
		err = -errno;

	errno = 0;
	return err;
}

static void
test_addfb(data_t *data)
{
	uint64_t size;
	uint32_t fb_id;
	uint32_t bo;
	uint32_t offsets[4] = {};
	uint32_t strides[4] = {};
	uint32_t format;
	int ret;

	/*
	 * gen3 max tiled stride is 8k bytes, but
	 * max fb size of 4k pixels, hence we can't test
	 * with 32bpp and must use 16bpp instead.
	 */
	if (intel_gen(data->devid) == 3)
		format = DRM_FORMAT_RGB565;
	else
		format = DRM_FORMAT_XRGB8888;

	igt_require(igt_display_has_format_mod(&data->display,
					       format, data->modifier));

	igt_calc_fb_size(data->drm_fd,
			 data->max_fb_width,
			 data->max_fb_height,
			 format, data->modifier,
			 &size, &strides[0]);

	bo = gem_create(data->drm_fd, size);
	igt_require(bo);

	if (intel_gen(data->devid) < 4)
		gem_set_tiling(data->drm_fd, bo,
			       igt_fb_mod_to_tiling(data->modifier), strides[0]);

	ret = __kms_addfb(data->drm_fd, bo,
			  data->max_fb_width,
			  data->max_fb_height,
			  format, data->modifier,
			  strides, offsets, 1,
			  DRM_MODE_FB_MODIFIERS, &fb_id);
	igt_assert_eq(ret, 0);

	rmfb(data->drm_fd, fb_id);
	gem_close(data->drm_fd, bo);
}

static data_t data;

static const struct {
	uint64_t modifier;
	const char *name;
} modifiers[] = {
	{ DRM_FORMAT_MOD_LINEAR, "linear", },
	{ I915_FORMAT_MOD_X_TILED, "x-tiled", },
	{ I915_FORMAT_MOD_Y_TILED, "y-tiled", },
	{ I915_FORMAT_MOD_Yf_TILED, "yf-tiled", },
};

static const struct {
	uint32_t format;
	uint8_t bpp;
} formats[] = {
	{ DRM_FORMAT_C8, 8, },
	{ DRM_FORMAT_RGB565, 16, },
	{ DRM_FORMAT_XRGB8888, 32, },
	{ DRM_FORMAT_XBGR16161616F, 64, },
};

static const struct {
	igt_rotation_t rotation;
	uint16_t angle;
} rotations[] = {
	{ IGT_ROTATION_0, 0, },
	{ IGT_ROTATION_90, 90, },
	{ IGT_ROTATION_180, 180, },
	{ IGT_ROTATION_270, 270, },
};

igt_main
{
	igt_fixture {
		drmModeResPtr res;

		igt_skip_on_simulation();

		data.drm_fd = drm_open_driver_master(DRIVER_INTEL);

		igt_require(is_i915_device(data.drm_fd));

		data.devid = intel_get_drm_devid(data.drm_fd);

		kmstest_set_vt_graphics_mode();

		igt_require_pipe_crc(data.drm_fd);
		igt_display_require(&data.display, data.drm_fd);

		res = drmModeGetResources(data.drm_fd);
		igt_assert(res);

		data.max_fb_width = res->max_width;
		data.max_fb_height = res->max_height;

		drmModeFreeResources(res);

		igt_info("Max driver framebuffer size %dx%d\n",
			 data.max_fb_width, data.max_fb_height);

		data.ram_size = intel_get_total_ram_mb() << 20;
		data.aper_size = gem_aperture_size(data.drm_fd);
		data.mappable_size = gem_mappable_aperture_size();

		igt_info("RAM: %"PRIu64" MiB, GPU address space: %"PRId64" MiB, GGTT mappable size: %"PRId64" MiB\n",
			 data.ram_size >> 20, data.aper_size >> 20,
			 data.mappable_size >> 20);

		/*
		 * Gen3 render engine is limited to 2kx2k, whereas
		 * the display engine can do 4kx4k. Use the blitter
		 * on gen3 to avoid exceeding the render engine limits.
		 * On gen2 we could use either, but let's go for the
		 * blitter there as well.
		 */
		if (intel_gen(data.devid) >= 4)
			data.render_copy = igt_get_render_copyfunc(data.devid);

		data.bufmgr = drm_intel_bufmgr_gem_init(data.drm_fd, 4096);
		data.batch = intel_batchbuffer_alloc(data.bufmgr, data.devid);
	}

	/*
	 * Skip linear as it doesn't hit the overflow we want
	 * on account of the tile height being effectively one,
	 * and thus the kenrnel rounding up to the next tile
	 * height won't do anything.
	 */
	for (int i = 1; i < ARRAY_SIZE(modifiers); i++) {
		igt_subtest_f("%s-addfb-size-overflow",
			      modifiers[i].name) {
			data.modifier = modifiers[i].modifier;
			test_size_overflow(&data);
		}
	}

	for (int i = 1; i < ARRAY_SIZE(modifiers); i++) {
		igt_subtest_f("%s-addfb-size-offset-overflow",
			      modifiers[i].name) {
			data.modifier = modifiers[i].modifier;
			test_size_offset_overflow(&data);
		}
	}

	for (int i = 0; i < ARRAY_SIZE(modifiers); i++) {
		igt_subtest_f("%s-addfb", modifiers[i].name) {
			data.modifier = modifiers[i].modifier;

			test_addfb(&data);
		}
	}

	for (int i = 0; i < ARRAY_SIZE(modifiers); i++) {
		data.modifier = modifiers[i].modifier;

		for (int j = 0; j < ARRAY_SIZE(formats); j++) {
			data.format = formats[j].format;

			for (int k = 0; k < ARRAY_SIZE(rotations); k++) {
				data.rotation = rotations[k].rotation;

				igt_subtest_f("%s-%dbpp-rotate-%d", modifiers[i].name,
					      formats[j].bpp, rotations[k].angle) {
					igt_require(data.format == DRM_FORMAT_C8 ||
						    igt_fb_supported_format(data.format));
					igt_require(igt_display_has_format_mod(&data.display, data.format, data.modifier));
					test_scanout(&data);
				}
			}

			igt_fixture
				cleanup_fb(&data);
		}
	}

	igt_fixture {
		igt_display_fini(&data.display);

		intel_batchbuffer_free(data.batch);
		drm_intel_bufmgr_destroy(data.bufmgr);
	}
}
