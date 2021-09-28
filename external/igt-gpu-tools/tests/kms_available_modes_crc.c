/*
 * Copyright © 2018 Intel Corporation
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
 */

#include "drm_mode.h"
#include "drm_fourcc.h"
#include "igt.h"

IGT_TEST_DESCRIPTION("CRC test all different plane modes which kernel advertises.");

typedef struct {
	int gfx_fd;
	igt_display_t display;
	enum igt_commit_style commit;

	struct igt_fb fb;
	struct igt_fb primary_fb;

	union {
		char name[5];
		uint32_t dword;
	} format;
	bool separateprimaryplane;

	uint32_t gem_handle;
	uint32_t gem_handle_yuv;
	unsigned int size;
	unsigned char* buf;

	/*
	 * comparison crcs
	 */
	igt_pipe_crc_t *pipe_crc;

	igt_crc_t cursor_crc;
	igt_crc_t fullscreen_crc;
} data_t;


static void do_write(int fd, int handle, void *buf, int size)
{	void *screenbuf;

	gem_set_domain(fd, handle, I915_GEM_DOMAIN_CPU, I915_GEM_DOMAIN_CPU);
	screenbuf = gem_mmap__gtt(fd, handle, size, PROT_WRITE);
	memcpy(screenbuf, buf, size);
	gem_munmap(screenbuf, size);
	gem_set_domain(fd, handle, I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	gem_sync(fd, handle);
}


static void generate_comparison_crc_list(data_t *data, igt_output_t *output)
{
	drmModeModeInfo *mode;
	uint64_t w, h;
	int fbid;
	cairo_t *cr;
	igt_plane_t *primary;

	mode = igt_output_get_mode(output);
	fbid = igt_create_color_fb(data->gfx_fd,
				   mode->hdisplay,
				   mode->vdisplay,
				   DRM_FORMAT_XRGB8888,
				   LOCAL_DRM_FORMAT_MOD_NONE,
				   0, 0, 0,
				   &data->primary_fb);

	igt_assert(fbid);

	drmGetCap(data->gfx_fd, DRM_CAP_CURSOR_WIDTH, &w);
	drmGetCap(data->gfx_fd, DRM_CAP_CURSOR_HEIGHT, &h);

	cr = igt_get_cairo_ctx(data->gfx_fd, &data->primary_fb);
	igt_paint_color(cr, 0, 0, mode->hdisplay, mode->vdisplay,
			    0.0, 0.0, 0.0);
	igt_paint_color(cr, 0, 0, w, h, 1.0, 1.0, 1.0);
	igt_assert(cairo_status(cr) == 0);
	igt_put_cairo_ctx(data->gfx_fd, &data->primary_fb, cr);

	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);
	igt_plane_set_fb(primary, &data->primary_fb);
	igt_display_commit2(&data->display, data->commit);

	igt_pipe_crc_get_current(data->gfx_fd, data->pipe_crc, &data->cursor_crc);
	igt_plane_set_fb(primary, NULL);
	igt_display_commit2(&data->display, data->commit);

	cr = igt_get_cairo_ctx(data->gfx_fd, &data->primary_fb);
	igt_paint_color(cr, 0, 0, mode->hdisplay, mode->vdisplay, 1.0, 1.0, 1.0);
	igt_put_cairo_ctx(data->gfx_fd, &data->primary_fb, cr);

	igt_plane_set_fb(primary, &data->primary_fb);
	igt_display_commit2(&data->display, data->commit);

	igt_pipe_crc_get_current(data->gfx_fd, data->pipe_crc, &data->fullscreen_crc);

	igt_remove_fb(data->gfx_fd, &data->primary_fb);
}

static const struct {
	uint32_t	fourcc;
	char		zeropadding;
	enum		{ BYTES_PP_1 = 1,
				BYTES_PP_4 = 4} bpp;
	uint32_t	value;
} fillers[] = {
	{ DRM_FORMAT_XBGR2101010, 0, BYTES_PP_4, 0xffffffff},
	{ 0, 0, 0, 0 }
};

/*
 * fill_in_fb tell in return value if selected mode should be
 * proceed to crc check
 */
static bool fill_in_fb(data_t *data, igt_output_t *output, igt_plane_t *plane,
		       uint32_t format)
{
	signed i, c, writesize;
	unsigned int* ptemp_32_buf;

	for (i = 0; i < ARRAY_SIZE(fillers)-1; i++) {
		if (fillers[i].fourcc == format)
			break;
	}

	switch (fillers[i].bpp) {
	case BYTES_PP_4:
		ptemp_32_buf = (unsigned int*)data->buf;
		for (c = 0; c < data->size/4; c++)
			ptemp_32_buf[c] = fillers[i].value;
		writesize = data->size;
		break;
	case BYTES_PP_1:
		memset((void *)data->buf, fillers[i].value, data->size);
		writesize = data->size;
		break;
	default:
		igt_assert_f(0, "unknown bpp");
	}

	do_write(data->gfx_fd, data->gem_handle, (void*)data->buf, writesize);
	return true;
}


static bool setup_fb(data_t *data, igt_output_t *output, igt_plane_t *plane,
		     uint32_t format)
{
	drmModeModeInfo *mode;
	uint64_t w, h;
	signed ret, gemsize = 0;
	unsigned tile_width, tile_height;
	int num_planes = 1;
	uint64_t tiling;
	int bpp = 0;
	int i;

	mode = igt_output_get_mode(output);
	if (plane->type != DRM_PLANE_TYPE_CURSOR) {
		w = mode->hdisplay;
		h = mode->vdisplay;
		tiling = LOCAL_I915_FORMAT_MOD_X_TILED;
	} else {
		drmGetCap(data->gfx_fd, DRM_CAP_CURSOR_WIDTH, &w);
		drmGetCap(data->gfx_fd, DRM_CAP_CURSOR_HEIGHT, &h);
		tiling = LOCAL_DRM_FORMAT_MOD_NONE;
	}

	for (i = 0; i < ARRAY_SIZE(fillers)-1; i++) {
		if (fillers[i].fourcc == format)
			break;
	}

	switch (fillers[i].bpp) {
	case BYTES_PP_1:
		bpp = 8;
		break;
	case BYTES_PP_4:
		bpp = 32;
		break;
	default:
		igt_assert_f(0, "unknown bpp");
	}

	igt_get_fb_tile_size(data->gfx_fd, tiling, bpp,
			     &tile_width, &tile_height);
	data->fb.offsets[0] = 0;
	data->fb.strides[0] = ALIGN(w * bpp / 8, tile_width);
	gemsize = data->size = data->fb.strides[0] * ALIGN(h, tile_height);
	data->buf = (unsigned char *)calloc(data->size*2, 1);

	data->gem_handle = gem_create(data->gfx_fd, gemsize);
	ret = __gem_set_tiling(data->gfx_fd, data->gem_handle,
			       igt_fb_mod_to_tiling(tiling),
			       data->fb.strides[0]);

	data->fb.gem_handle = data->gem_handle;
	data->fb.width = w;
	data->fb.height = h;
	fill_in_fb(data, output, plane, format);

	igt_assert_eq(ret, 0);

	ret = __kms_addfb(data->gfx_fd, data->gem_handle, w, h,
			  format, tiling, data->fb.strides, data->fb.offsets,
			  num_planes, LOCAL_DRM_MODE_FB_MODIFIERS,
			  &data->fb.fb_id);

	if(ret < 0) {
		igt_info("Creating fb for format %s failed, return code %d\n",
			 (char*)&data->format.name, ret);

		return false;
	}

	return true;
}


static void remove_fb(data_t* data, igt_output_t* output, igt_plane_t* plane)
{
	if (data->separateprimaryplane) {
		igt_plane_t* primary = igt_output_get_plane_type(output,
								 DRM_PLANE_TYPE_PRIMARY);
		igt_plane_set_fb(primary, NULL);
		igt_remove_fb(data->gfx_fd, &data->primary_fb);
		data->separateprimaryplane = false;
	}

	igt_remove_fb(data->gfx_fd, &data->fb);
	free(data->buf);
	data->buf = NULL;
}


static bool prepare_crtc(data_t *data, igt_output_t *output,
			 igt_plane_t *plane, uint32_t format)
{
	drmModeModeInfo *mode;
	igt_plane_t *primary;

	if (plane->type != DRM_PLANE_TYPE_PRIMARY) {
		mode = igt_output_get_mode(output);
		igt_create_color_fb(data->gfx_fd,
				    mode->hdisplay, mode->vdisplay,
				    DRM_FORMAT_XRGB8888,
				    LOCAL_DRM_FORMAT_MOD_NONE,
				    0, 0, 0,
				    &data->primary_fb);

		primary = igt_output_get_plane_type(output,
						    DRM_PLANE_TYPE_PRIMARY);

		igt_plane_set_fb(primary, &data->primary_fb);
		igt_display_commit2(&data->display, data->commit);
		data->separateprimaryplane = true;
	}

	if (!setup_fb(data, output, plane, format))
		return false;

	return true;
}


static int
test_one_mode(data_t* data, igt_output_t *output, igt_plane_t* plane,
	      int mode, enum pipe pipe)
{
	igt_crc_t current_crc;
	signed rVal = 0;
	int i;

	/*
	 * Limit tests only to those fb formats listed in fillers table
	 */
	for (i = 0; i < ARRAY_SIZE(fillers)-1; i++) {
		if (fillers[i].fourcc == mode)
			break;
	}

	if (fillers[i].bpp == 0)
		return false;

	if (prepare_crtc(data, output, plane, mode)) {
		igt_plane_set_fb(plane, &data->fb);
		igt_fb_set_size(&data->fb, plane, data->fb.width, data->fb.height);
		igt_plane_set_size(plane, data->fb.width, data->fb.height);
		igt_fb_set_position(&data->fb, plane, 0, 0);
		igt_display_commit2(&data->display, data->commit);

		igt_wait_for_vblank(data->gfx_fd, pipe);
		igt_pipe_crc_get_current(data->gfx_fd, data->pipe_crc, &current_crc);

		if (plane->type != DRM_PLANE_TYPE_CURSOR) {
			if (!igt_check_crc_equal(&current_crc,
				&data->fullscreen_crc)) {
				igt_warn("crc mismatch. connector %s using pipe %s" \
					" plane index %d mode %.4s\n",
					igt_output_name(output),
					kmstest_pipe_name(pipe),
					plane->index,
					(char *)&mode);
				rVal++;
			}
		} else {
			if (!igt_check_crc_equal(&current_crc,
				&data->cursor_crc)) {
				igt_warn("crc mismatch. connector %s using pipe %s" \
					" plane index %d mode %.4s\n",
					igt_output_name(output),
					kmstest_pipe_name(pipe),
					plane->index,
					(char *)&mode);
				rVal++;
			}
		}
	}
	remove_fb(data, output, plane);
	return rVal;
}


static void
test_available_modes(data_t* data)
{
	igt_output_t *output;
	igt_plane_t *plane;
	int modeindex;
	enum pipe pipe;
	int invalids = 0, i, lut_size;
	drmModePlane *modePlane;

	struct {
		uint16_t red;
		uint16_t green;
		uint16_t blue;
		uint16_t reserved;
	} *lut = NULL;

	for_each_pipe_with_valid_output(&data->display, pipe, output) {
		igt_output_set_pipe(output, pipe);
		igt_display_commit2(&data->display, data->commit);

		if (igt_pipe_obj_has_prop(&data->display.pipes[pipe], IGT_CRTC_GAMMA_LUT_SIZE)) {
			lut_size = igt_pipe_get_prop(&data->display, pipe,
						     IGT_CRTC_GAMMA_LUT_SIZE);

			lut = calloc(sizeof(*lut), lut_size);

			for (i = 0; i < lut_size; i++) {
				lut[i].red = (i * 0xffff / (lut_size - 1)) & 0xfc00;
				lut[i].green = (i * 0xffff / (lut_size - 1)) & 0xfc00;
				lut[i].blue = (i * 0xffff / (lut_size - 1)) & 0xfc00;
			}

			igt_pipe_replace_prop_blob(&data->display, pipe,
						   IGT_CRTC_GAMMA_LUT,
						   lut, sizeof(*lut) * lut_size);
			igt_display_commit2(&data->display, data->commit);

			for (i = 0; i < lut_size; i++) {
				lut[i].red = i * 0xffff / (lut_size - 1);
				lut[i].green = i * 0xffff / (lut_size - 1);
				lut[i].blue = i * 0xffff / (lut_size - 1);
			}
		}

		data->pipe_crc = igt_pipe_crc_new(data->gfx_fd, pipe,
						  INTEL_PIPE_CRC_SOURCE_AUTO);

		igt_pipe_crc_start(data->pipe_crc);

		/*
		 * regenerate comparison crcs for each pipe just in case.
		 */
		generate_comparison_crc_list(data, output);

		for_each_plane_on_pipe(&data->display, pipe, plane) {
			modePlane = drmModeGetPlane(data->gfx_fd,
						    plane->drm_plane->plane_id);

			if (plane->type == DRM_PLANE_TYPE_CURSOR)
				continue;

			for (modeindex = 0;
			     modeindex < modePlane->count_formats;
			     modeindex++) {
				data->format.dword = modePlane->formats[modeindex];

				invalids += test_one_mode(data, output,
							  plane,
							  modePlane->formats[modeindex],
							  pipe);
			}
			drmModeFreePlane(modePlane);
		}
		igt_pipe_crc_stop(data->pipe_crc);
		igt_pipe_crc_free(data->pipe_crc);

		if (lut != NULL) {
			igt_pipe_replace_prop_blob(&data->display, pipe,
						   IGT_CRTC_GAMMA_LUT,
						   lut, sizeof(*lut) * lut_size);
			free(lut);
			lut = NULL;
		}

		igt_output_set_pipe(output, PIPE_NONE);
		igt_display_commit2(&data->display, data->commit);
	}
	igt_assert(invalids == 0);
}


igt_main
{
	data_t data = {};

	igt_skip_on_simulation();

	igt_fixture {
		data.gfx_fd = drm_open_driver_master(DRIVER_INTEL);
		kmstest_set_vt_graphics_mode();
		igt_display_require(&data.display, data.gfx_fd);
		igt_require_pipe_crc(data.gfx_fd);
	}

	data.commit = data.display.is_atomic ? COMMIT_ATOMIC : COMMIT_LEGACY;

	igt_subtest("available_mode_test_crc") {
		test_available_modes(&data);
	}

	igt_fixture {
		kmstest_restore_vt_mode();
		igt_display_fini(&data.display);
	}
}
