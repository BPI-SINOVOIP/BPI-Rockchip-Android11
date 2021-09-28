/*
 * Copyright © 2016 Intel Corporation
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

#include "igt.h"

IGT_TEST_DESCRIPTION("Test render compression (RC), in which the main surface "
		     "is complemented by a color control surface (CCS) that "
		     "the display uses to interpret the compressed data.");

enum test_flags {
	TEST_CRC			= 1 << 1,
	TEST_ROTATE_180			= 1 << 2,
	TEST_BAD_PIXEL_FORMAT		= 1 << 3,
	TEST_BAD_ROTATION_90		= 1 << 4,
	TEST_NO_AUX_BUFFER		= 1 << 5,
	TEST_BAD_CCS_HANDLE		= 1 << 6,
	TEST_BAD_AUX_STRIDE		= 1 << 7,
};

#define TEST_FAIL_ON_ADDFB2 \
	(TEST_BAD_PIXEL_FORMAT | TEST_NO_AUX_BUFFER | TEST_BAD_CCS_HANDLE | \
	 TEST_BAD_AUX_STRIDE)

enum test_fb_flags {
	FB_COMPRESSED			= 1 << 0,
	FB_HAS_PLANE			= 1 << 1,
	FB_MISALIGN_AUX_STRIDE		= 1 << 2,
	FB_SMALL_AUX_STRIDE		= 1 << 3,
	FB_ZERO_AUX_STRIDE		= 1 << 4,
};

typedef struct {
	int drm_fd;
	igt_display_t display;
	igt_output_t *output;
	enum pipe pipe;
	enum test_flags flags;
	igt_plane_t *plane;
	igt_pipe_crc_t *pipe_crc;
	uint64_t ccs_modifier;
} data_t;

static const struct {
	double r;
	double g;
	double b;
} colors[2] = {
	{1.0, 0.0, 0.0},
	{0.0, 1.0, 0.0}
};

static const uint64_t ccs_modifiers[] = {
	LOCAL_I915_FORMAT_MOD_Y_TILED_CCS,
	LOCAL_I915_FORMAT_MOD_Yf_TILED_CCS,
};

/*
 * Limit maximum used sprite plane width so this test will not mistakenly
 * fail on hardware limitations which are not interesting to this test.
 * On this test too wide sprite plane may fail during creation with dmesg
 * comment saying:
 * "Requested display configuration exceeds system watermark limitations"
 */
#define MAX_SPRITE_PLANE_WIDTH 2000

static void addfb_init(struct igt_fb *fb, struct drm_mode_fb_cmd2 *f)
{
	int i;

	f->width = fb->width;
	f->height = fb->height;
	f->pixel_format = fb->drm_format;
	f->flags = LOCAL_DRM_MODE_FB_MODIFIERS;

	for (i = 0; i < fb->num_planes; i++) {
		f->handles[i] = fb->gem_handle;
		f->modifier[i] = fb->modifier;
		f->pitches[i] = fb->strides[i];
		f->offsets[i] = fb->offsets[i];
	}
}

static void generate_fb(data_t *data, struct igt_fb *fb,
			int width, int height,
			enum test_fb_flags fb_flags)
{
	struct drm_mode_fb_cmd2 f = {0};
	uint32_t format;
	uint64_t modifier;
	cairo_t *cr;
	int ret;

	/* Use either compressed or Y-tiled to test. However, given the lack of
	 * available bandwidth, we use linear for the primary plane when
	 * testing sprites, since we cannot fit two CCS planes into the
	 * available FIFO configurations.
	 */
	if (fb_flags & FB_COMPRESSED)
		modifier = data->ccs_modifier;
	else if (!(fb_flags & FB_HAS_PLANE))
		modifier = LOCAL_I915_FORMAT_MOD_Y_TILED;
	else
		modifier = 0;

	if (data->flags & TEST_BAD_PIXEL_FORMAT)
		format = DRM_FORMAT_RGB565;
	else
		format = DRM_FORMAT_XRGB8888;

	igt_create_bo_for_fb(data->drm_fd, width, height, format, modifier, fb);
	igt_assert(fb->gem_handle > 0);

	addfb_init(fb, &f);

	if (fb_flags & FB_COMPRESSED) {
		if (fb_flags & FB_MISALIGN_AUX_STRIDE) {
			igt_skip_on_f(width <= 1024,
				      "FB already has the smallest possible stride\n");
			f.pitches[1] -= 64;
		}

		if (fb_flags & FB_SMALL_AUX_STRIDE) {
			igt_skip_on_f(width <= 1024,
				      "FB already has the smallest possible stride\n");
			f.pitches[1] = ALIGN(f.pitches[1]/2, 128);
		}

		if (fb_flags & FB_ZERO_AUX_STRIDE)
			f.pitches[1] = 0;

		/* Put the CCS buffer on a different BO. */
		if (data->flags & TEST_BAD_CCS_HANDLE)
			f.handles[1] = gem_create(data->drm_fd, fb->size);

		if (data->flags & TEST_NO_AUX_BUFFER) {
			f.handles[1] = 0;
			f.modifier[1] = 0;
			f.pitches[1] = 0;
			f.offsets[1] = 0;
		}
	}

	if (!(data->flags & TEST_BAD_PIXEL_FORMAT)) {
		int c = !!data->plane;

		cr = igt_get_cairo_ctx(data->drm_fd, fb);
		igt_paint_color(cr, 0, 0, width, height,
				colors[c].r, colors[c].g, colors[c].b);
		igt_put_cairo_ctx(data->drm_fd, fb, cr);
	}

	ret = drmIoctl(data->drm_fd, LOCAL_DRM_IOCTL_MODE_ADDFB2, &f);
	if (data->flags & TEST_FAIL_ON_ADDFB2) {
		igt_assert_eq(ret, -1);
		igt_assert_eq(errno, EINVAL);
		return;
	} else
		igt_assert_eq(ret, 0);

	fb->fb_id = f.fb_id;
}

static bool try_config(data_t *data, enum test_fb_flags fb_flags,
		       igt_crc_t *crc)
{
	igt_display_t *display = &data->display;
	igt_plane_t *primary;
	drmModeModeInfo *drm_mode = igt_output_get_mode(data->output);
	enum igt_commit_style commit;
	struct igt_fb fb, fb_sprite;
	int ret;

	if (data->display.is_atomic)
		commit = COMMIT_ATOMIC;
	else
		commit = COMMIT_UNIVERSAL;

	primary = igt_output_get_plane_type(data->output,
					    DRM_PLANE_TYPE_PRIMARY);
	if (!igt_plane_has_format_mod(primary, DRM_FORMAT_XRGB8888,
				      data->ccs_modifier))
		return false;

	if (data->plane && fb_flags & FB_COMPRESSED) {
		if (!igt_plane_has_format_mod(data->plane, DRM_FORMAT_XRGB8888,
					      data->ccs_modifier))
			return false;

		generate_fb(data, &fb, min(MAX_SPRITE_PLANE_WIDTH, drm_mode->hdisplay),
			    drm_mode->vdisplay,
			    (fb_flags & ~FB_COMPRESSED) | FB_HAS_PLANE);
		generate_fb(data, &fb_sprite, 256, 256, fb_flags);
	} else {
		generate_fb(data, &fb, min(MAX_SPRITE_PLANE_WIDTH, drm_mode->hdisplay),
			    drm_mode->vdisplay, fb_flags);
	}

	if (data->flags & TEST_FAIL_ON_ADDFB2)
		return true;

	igt_plane_set_position(primary, 0, 0);
	igt_plane_set_size(primary, drm_mode->hdisplay, drm_mode->vdisplay);
	igt_plane_set_fb(primary, &fb);

	if (data->plane && fb_flags & FB_COMPRESSED) {
		igt_plane_set_position(data->plane, 0, 0);
		igt_plane_set_size(data->plane, 256, 256);
		igt_plane_set_fb(data->plane, &fb_sprite);
	}

	if (data->flags & TEST_ROTATE_180)
		igt_plane_set_rotation(primary, IGT_ROTATION_180);
	if (data->flags & TEST_BAD_ROTATION_90)
		igt_plane_set_rotation(primary, IGT_ROTATION_90);

	ret = igt_display_try_commit2(display, commit);
	if (data->flags & TEST_BAD_ROTATION_90) {
		igt_assert_eq(ret, -EINVAL);
	} else {
		igt_assert_eq(ret, 0);

		if (crc)
			igt_pipe_crc_collect_crc(data->pipe_crc, crc);
	}

	igt_debug_wait_for_keypress("ccs");

	if (data->plane && fb_flags & FB_COMPRESSED) {
		igt_plane_set_position(data->plane, 0, 0);
		igt_plane_set_size(data->plane, 0, 0);
		igt_plane_set_fb(data->plane, NULL);
		igt_remove_fb(display->drm_fd, &fb_sprite);
	}

	igt_plane_set_fb(primary, NULL);
	igt_plane_set_rotation(primary, IGT_ROTATION_0);
	igt_display_commit2(display, commit);

	if (data->flags & TEST_CRC)
		igt_remove_fb(data->drm_fd, &fb);

	return true;
}

static int test_ccs(data_t *data)
{	int valid_tests = 0;
	igt_crc_t crc, ref_crc;
	enum test_fb_flags fb_flags = 0;

	if (data->flags & TEST_CRC) {
		data->pipe_crc = igt_pipe_crc_new(data->drm_fd, data->pipe, INTEL_PIPE_CRC_SOURCE_AUTO);

		if (try_config(data, fb_flags | FB_COMPRESSED, &ref_crc) &&
		    try_config(data, fb_flags, &crc)) {
			igt_assert_crc_equal(&crc, &ref_crc);
			valid_tests++;
		}

		igt_pipe_crc_free(data->pipe_crc);
		data->pipe_crc = NULL;
	}

	if (data->flags & TEST_BAD_PIXEL_FORMAT ||
	    data->flags & TEST_BAD_ROTATION_90 ||
	    data->flags & TEST_NO_AUX_BUFFER ||
	    data->flags & TEST_BAD_CCS_HANDLE) {
		valid_tests += try_config(data, fb_flags | FB_COMPRESSED, NULL);
	}

	if (data->flags & TEST_BAD_AUX_STRIDE) {
		valid_tests += try_config(data, fb_flags | FB_COMPRESSED | FB_MISALIGN_AUX_STRIDE , NULL);
		valid_tests += try_config(data, fb_flags | FB_COMPRESSED | FB_SMALL_AUX_STRIDE , NULL);
		valid_tests += try_config(data, fb_flags | FB_COMPRESSED | FB_ZERO_AUX_STRIDE , NULL);
	}

	return valid_tests;
}

static int __test_output(data_t *data)
{
	igt_display_t *display = &data->display;
	int i, valid_tests = 0;

	data->output = igt_get_single_output_for_pipe(display, data->pipe);
	igt_require(data->output);

	igt_output_set_pipe(data->output, data->pipe);

	for (i = 0; i < ARRAY_SIZE(ccs_modifiers); i++) {
		data->ccs_modifier = ccs_modifiers[i];
		valid_tests += test_ccs(data);
	}

	igt_output_set_pipe(data->output, PIPE_NONE);
	igt_display_commit2(display, display->is_atomic ? COMMIT_ATOMIC : COMMIT_LEGACY);

	return valid_tests;
}

static void test_output(data_t *data)
{
	int valid_tests = __test_output(data);
	igt_require_f(valid_tests > 0, "CCS not supported, skipping\n");
}

static data_t data;

igt_main
{
	enum pipe pipe;

	igt_fixture {
		data.drm_fd = drm_open_driver_master(DRIVER_INTEL);

		igt_require(intel_gen(intel_get_drm_devid(data.drm_fd)) >= 9);
		kmstest_set_vt_graphics_mode();
		igt_require_pipe_crc(data.drm_fd);

		igt_display_require(&data.display, data.drm_fd);
	}

	for_each_pipe_static(pipe) {
		const char *pipe_name = kmstest_pipe_name(pipe);

		data.pipe = pipe;

		data.flags = TEST_BAD_PIXEL_FORMAT;
		igt_subtest_f("pipe-%s-bad-pixel-format", pipe_name)
			test_output(&data);

		data.flags = TEST_BAD_ROTATION_90;
		igt_subtest_f("pipe-%s-bad-rotation-90", pipe_name)
			test_output(&data);

		data.flags = TEST_CRC;
		igt_subtest_f("pipe-%s-crc-primary-basic", pipe_name)
			test_output(&data);

		data.flags = TEST_CRC | TEST_ROTATE_180;
		igt_subtest_f("pipe-%s-crc-primary-rotation-180", pipe_name)
			test_output(&data);

		data.flags = TEST_CRC;
		igt_subtest_f("pipe-%s-crc-sprite-planes-basic", pipe_name) {
			int valid_tests = 0;

			igt_display_require_output_on_pipe(&data.display, data.pipe);

			for_each_plane_on_pipe(&data.display, data.pipe, data.plane) {
				if (data.plane->type == DRM_PLANE_TYPE_PRIMARY)
					continue;
				valid_tests += __test_output(&data);
			}

			igt_require_f(valid_tests > 0,
				      "CCS not supported, skipping\n");
		}

		data.plane = NULL;

		data.flags = TEST_NO_AUX_BUFFER;
		igt_subtest_f("pipe-%s-missing-ccs-buffer", pipe_name)
			test_output(&data);

		data.flags = TEST_BAD_CCS_HANDLE;
		igt_subtest_f("pipe-%s-ccs-on-another-bo", pipe_name)
			test_output(&data);

		data.flags = TEST_BAD_AUX_STRIDE;
		igt_subtest_f("pipe-%s-bad-aux-stride", pipe_name)
			test_output(&data);
	}

	igt_fixture
		igt_display_fini(&data.display);
}
