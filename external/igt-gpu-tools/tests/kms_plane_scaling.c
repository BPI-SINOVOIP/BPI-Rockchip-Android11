/*
 * Copyright © 2013,2014 Intel Corporation
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
#include <math.h>


IGT_TEST_DESCRIPTION("Test display plane scaling");

typedef struct {
	uint32_t devid;
	int drm_fd;
	igt_display_t display;
	igt_crc_t ref_crc;

	int image_w;
	int image_h;

	struct igt_fb fb[4];

	igt_plane_t *plane1;
	igt_plane_t *plane2;
	igt_plane_t *plane3;
	igt_plane_t *plane4;
} data_t;

static int get_num_scalers(data_t* d, enum pipe pipe)
{
	if (!is_i915_device(d->drm_fd))
		return 1;

	igt_require(intel_gen(d->devid) >= 9);

	if (intel_gen(d->devid) >= 10)
		return 2;
	else if (pipe != PIPE_C)
		return 2;
	else
		return 1;
}

static bool is_planar_yuv_format(uint32_t pixelformat)
{
	switch (pixelformat) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_P010:
	case DRM_FORMAT_P012:
	case DRM_FORMAT_P016:
		return true;
	default:
		return false;
	}
}

static void cleanup_fbs(data_t *data)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(data->fb); i++)
		igt_remove_fb(data->drm_fd, &data->fb[i]);
}

static void cleanup_crtc(data_t *data)
{
	igt_display_reset(&data->display);

	cleanup_fbs(data);
}

static void prepare_crtc(data_t *data, igt_output_t *output, enum pipe pipe,
			igt_plane_t *plane, drmModeModeInfo *mode)
{
	igt_display_t *display = &data->display;
	uint64_t tiling = is_i915_device(data->drm_fd) ?
		LOCAL_I915_FORMAT_MOD_X_TILED : LOCAL_DRM_FORMAT_MOD_NONE;

	cleanup_crtc(data);

	igt_output_set_pipe(output, pipe);

	igt_skip_on(!igt_display_has_format_mod(display, DRM_FORMAT_XRGB8888,
						tiling));

	/* allocate fb for plane 1 */
	igt_create_pattern_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
			      DRM_FORMAT_XRGB8888,
			      tiling,
			      &data->fb[0]);

	igt_plane_set_fb(plane, &data->fb[0]);

	if (plane->type != DRM_PLANE_TYPE_PRIMARY) {
		igt_plane_t *primary;
		int ret;

		/* Do we succeed without enabling the primary plane? */
		ret = igt_display_try_commit2(display, COMMIT_ATOMIC);
		if (!ret)
			return;

		/*
		 * Fallback: set the primary plane to actually enable the pipe.
		 * Some drivers always require the primary plane to be enabled.
		 */
		primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);
		igt_plane_set_fb(primary, &data->fb[0]);
	}
	igt_display_commit2(display, COMMIT_ATOMIC);
}

static void check_scaling_pipe_plane_rot(data_t *d, igt_plane_t *plane,
					 uint32_t pixel_format,
					 uint64_t tiling, enum pipe pipe,
					 igt_output_t *output,
					 igt_rotation_t rot)
{
	igt_display_t *display = &d->display;
	int width, height;
	drmModeModeInfo *mode;

	cleanup_crtc(d);

	igt_output_set_pipe(output, pipe);
	mode = igt_output_get_mode(output);

	/* create buffer in the range of  min and max source side limit.*/
	width = height = 8;
	if (is_i915_device(d->drm_fd) && is_planar_yuv_format(pixel_format))
		width = height = 16;
	igt_create_color_fb(display->drm_fd, width, height,
		       pixel_format, tiling, 0.0, 1.0, 0.0, &d->fb[0]);
	igt_plane_set_fb(plane, &d->fb[0]);

	/* Check min to full resolution upscaling */
	igt_fb_set_position(&d->fb[0], plane, 0, 0);
	igt_fb_set_size(&d->fb[0], plane, width, height);
	igt_plane_set_position(plane, 0, 0);
	igt_plane_set_size(plane, mode->hdisplay, mode->vdisplay);
	igt_plane_set_rotation(plane, rot);
	igt_display_commit2(display, COMMIT_ATOMIC);

	igt_plane_set_fb(plane, NULL);
	igt_plane_set_position(plane, 0, 0);
}

static const igt_rotation_t rotations[] = {
	IGT_ROTATION_0,
	IGT_ROTATION_90,
	IGT_ROTATION_180,
	IGT_ROTATION_270,
};

static bool can_rotate(data_t *d, unsigned format, uint64_t tiling,
		       igt_rotation_t rot)
{

	if (!is_i915_device(d->drm_fd))
		return true;

	switch (format) {
	case DRM_FORMAT_RGB565:
		if (intel_gen(d->devid) >= 11)
			break;
		/* fall through */
	case DRM_FORMAT_C8:
	case DRM_FORMAT_XRGB16161616F:
	case DRM_FORMAT_XBGR16161616F:
	case DRM_FORMAT_ARGB16161616F:
	case DRM_FORMAT_ABGR16161616F:
	case DRM_FORMAT_Y210:
	case DRM_FORMAT_Y212:
	case DRM_FORMAT_Y216:
	case DRM_FORMAT_XVYU12_16161616:
	case DRM_FORMAT_XVYU16161616:
		return false;
	default:
		break;
	}

	return true;
}

static bool can_scale(data_t *d, unsigned format)
{
	if (!is_i915_device(d->drm_fd))
		return true;

	switch (format) {
	case DRM_FORMAT_XRGB16161616F:
	case DRM_FORMAT_XBGR16161616F:
	case DRM_FORMAT_ARGB16161616F:
	case DRM_FORMAT_ABGR16161616F:
		if (intel_gen(d->devid) >= 11)
			return true;
		/* fall through */
	case DRM_FORMAT_C8:
		return false;
	default:
		return true;
	}
}

static void test_scaler_with_rotation_pipe(data_t *d, enum pipe pipe,
					   igt_output_t *output)
{
	igt_display_t *display = &d->display;
	igt_plane_t *plane;
	uint64_t tiling = is_i915_device(d->drm_fd) ?
		LOCAL_I915_FORMAT_MOD_Y_TILED : LOCAL_DRM_FORMAT_MOD_NONE;

	igt_output_set_pipe(output, pipe);
	for_each_plane_on_pipe(display, pipe, plane) {
		if (plane->type == DRM_PLANE_TYPE_CURSOR)
			continue;

		for (int i = 0; i < ARRAY_SIZE(rotations); i++) {
			igt_rotation_t rot = rotations[i];
			for (int j = 0; j < plane->drm_plane->count_formats; j++) {
				unsigned format = plane->drm_plane->formats[j];

				if (igt_fb_supported_format(format) &&
				    igt_plane_has_format_mod(plane, format, tiling) &&
				    can_rotate(d, format, tiling, rot) &&
				    can_scale(d, format))
					check_scaling_pipe_plane_rot(d, plane, format,
								     tiling, pipe,
								     output, rot);
			}
		}
	}
}

static const uint64_t tilings[] = {
	LOCAL_DRM_FORMAT_MOD_NONE,
	LOCAL_I915_FORMAT_MOD_X_TILED,
	LOCAL_I915_FORMAT_MOD_Y_TILED,
	LOCAL_I915_FORMAT_MOD_Yf_TILED
};

static void test_scaler_with_pixel_format_pipe(data_t *d, enum pipe pipe, igt_output_t *output)
{
	igt_display_t *display = &d->display;
	igt_plane_t *plane;

	igt_output_set_pipe(output, pipe);

	for_each_plane_on_pipe(display, pipe, plane) {
		if (plane->type == DRM_PLANE_TYPE_CURSOR)
			continue;

		for (int i = 0; i < ARRAY_SIZE(tilings); i++) {
			uint64_t tiling = tilings[i];

			for (int j = 0; j < plane->drm_plane->count_formats; j++) {
				uint32_t format = plane->drm_plane->formats[j];

				if (igt_fb_supported_format(format) &&
				    igt_plane_has_format_mod(plane, format, tiling) &&
				    can_scale(d, format))
					check_scaling_pipe_plane_rot(d, plane,
								     format, tiling,
								     pipe, output, IGT_ROTATION_0);
			}
		}
	}
}

/* does iterative scaling on plane2 */
static void iterate_plane_scaling(data_t *d, drmModeModeInfo *mode)
{
	igt_display_t *display = &d->display;

	if (mode->hdisplay >= d->fb[1].width) {
		int w, h;
		/* fixed fb */
		igt_fb_set_position(&d->fb[1], d->plane2, 0, 0);
		igt_fb_set_size(&d->fb[1], d->plane2, d->fb[1].width, d->fb[1].height);
		igt_plane_set_position(d->plane2, 0, 0);

		/* adjust plane size */
		for (w = d->fb[1].width; w <= mode->hdisplay; w+=10) {
			h = w * d->fb[1].height / d->fb[1].width;
			igt_plane_set_size(d->plane2, w, h);
			igt_display_commit2(display, COMMIT_ATOMIC);
		}
	} else {
		int w, h;
		/* fixed plane */
		igt_plane_set_position(d->plane2, 0, 0);
		igt_plane_set_size(d->plane2, mode->hdisplay, mode->vdisplay);
		igt_fb_set_position(&d->fb[1], d->plane2, 0, 0);

		/* adjust fb size */
		for (w = mode->hdisplay; w <= d->fb[1].width; w+=10) {
			/* Source coordinates must not be clipped. */
			h = min(w * mode->hdisplay / mode->vdisplay, d->fb[1].height);
			igt_fb_set_size(&d->fb[1], d->plane2, w, h);
			igt_display_commit2(display, COMMIT_ATOMIC);
		}
	}
}

static void
test_plane_scaling_on_pipe(data_t *d, enum pipe pipe, igt_output_t *output)
{
	igt_display_t *display = &d->display;
	igt_pipe_t *pipe_obj = &display->pipes[pipe];
	drmModeModeInfo *mode;
	int primary_plane_scaling = 0; /* For now */
	uint64_t tiling = is_i915_device(display->drm_fd) ?
		LOCAL_I915_FORMAT_MOD_X_TILED : LOCAL_DRM_FORMAT_MOD_NONE;

	igt_skip_on(!igt_display_has_format_mod(display, DRM_FORMAT_XRGB8888,
						tiling));

	mode = igt_output_get_mode(output);

	/* Set up display with plane 1 */
	d->plane1 = igt_pipe_get_plane_type(pipe_obj, DRM_PLANE_TYPE_PRIMARY);
	prepare_crtc(d, output, pipe, d->plane1, mode);

	igt_create_color_pattern_fb(display->drm_fd, 600, 600,
				    DRM_FORMAT_XRGB8888,
				    tiling,
				    .5, .5, .5, &d->fb[1]);

	igt_create_pattern_fb(d->drm_fd,
			      mode->hdisplay, mode->vdisplay,
			      DRM_FORMAT_XRGB8888,
			      tiling,
			      &d->fb[2]);

	if (primary_plane_scaling) {
		/* Primary plane upscaling */
		igt_fb_set_position(&d->fb[0], d->plane1, 100, 100);
		igt_fb_set_size(&d->fb[0], d->plane1, 500, 500);
		igt_plane_set_position(d->plane1, 0, 0);
		igt_plane_set_size(d->plane1, mode->hdisplay, mode->vdisplay);
		igt_display_commit2(display, COMMIT_ATOMIC);

		/* Primary plane 1:1 no scaling */
		igt_fb_set_position(&d->fb[0], d->plane1, 0, 0);
		igt_fb_set_size(&d->fb[0], d->plane1, d->fb[0].width, d->fb[0].height);
		igt_plane_set_position(d->plane1, 0, 0);
		igt_plane_set_size(d->plane1, mode->hdisplay, mode->vdisplay);
		igt_display_commit2(display, COMMIT_ATOMIC);
	}

	/* Set up fb[1]->plane2 mapping. */
	d->plane2 = igt_pipe_get_plane_type_index(pipe_obj,
						  DRM_PLANE_TYPE_OVERLAY, 0);

	if (!d->plane2) {
		igt_debug("Plane-2 doesnt exist on pipe %s\n", kmstest_pipe_name(pipe));
		return;
	}

	igt_plane_set_fb(d->plane2, &d->fb[1]);

	/* 2nd plane windowed */
	igt_fb_set_position(&d->fb[1], d->plane2, 100, 100);
	igt_fb_set_size(&d->fb[1], d->plane2, d->fb[1].width-200, d->fb[1].height-200);
	igt_plane_set_position(d->plane2, 100, 100);
	igt_plane_set_size(d->plane2, mode->hdisplay-200, mode->vdisplay-200);
	igt_display_commit2(display, COMMIT_ATOMIC);

	iterate_plane_scaling(d, mode);

	/* 2nd plane up scaling */
	igt_fb_set_position(&d->fb[1], d->plane2, 100, 100);
	igt_fb_set_size(&d->fb[1], d->plane2, 500, 500);
	igt_plane_set_position(d->plane2, 10, 10);
	igt_plane_set_size(d->plane2, mode->hdisplay-20, mode->vdisplay-20);
	igt_display_commit2(display, COMMIT_ATOMIC);

	/* 2nd plane downscaling */
	igt_fb_set_position(&d->fb[1], d->plane2, 0, 0);
	igt_fb_set_size(&d->fb[1], d->plane2, d->fb[1].width, d->fb[1].height);
	igt_plane_set_position(d->plane2, 10, 10);

	/* Downscale (10/9)x of original image */
	igt_plane_set_size(d->plane2, (d->fb[1].width * 10)/9, (d->fb[1].height * 10)/9);
	igt_display_commit2(display, COMMIT_ATOMIC);

	if (primary_plane_scaling) {
		/* Primary plane up scaling */
		igt_fb_set_position(&d->fb[0], d->plane1, 100, 100);
		igt_fb_set_size(&d->fb[0], d->plane1, 500, 500);
		igt_plane_set_position(d->plane1, 0, 0);
		igt_plane_set_size(d->plane1, mode->hdisplay, mode->vdisplay);
		igt_display_commit2(display, COMMIT_ATOMIC);
	}

	/* Set up fb[2]->plane3 mapping. */
	d->plane3 = igt_pipe_get_plane_type_index(pipe_obj,
						  DRM_PLANE_TYPE_OVERLAY, 1);
	if(!d->plane3) {
		igt_debug("Plane-3 doesnt exist on pipe %s\n", kmstest_pipe_name(pipe));
		return;
	}

	igt_plane_set_fb(d->plane3, &d->fb[2]);

	/* 3rd plane windowed - no scaling */
	igt_fb_set_position(&d->fb[2], d->plane3, 100, 100);
	igt_fb_set_size(&d->fb[2], d->plane3, d->fb[2].width-300, d->fb[2].height-300);
	igt_plane_set_position(d->plane3, 100, 100);
	igt_plane_set_size(d->plane3, mode->hdisplay-300, mode->vdisplay-300);
	igt_display_commit2(display, COMMIT_ATOMIC);

	/* Switch scaler from plane 2 to plane 3 */
	igt_fb_set_position(&d->fb[1], d->plane2, 100, 100);
	igt_fb_set_size(&d->fb[1], d->plane2, d->fb[1].width-200, d->fb[1].height-200);
	igt_plane_set_position(d->plane2, 100, 100);
	igt_plane_set_size(d->plane2, d->fb[1].width-200, d->fb[1].height-200);

	igt_fb_set_position(&d->fb[2], d->plane3, 100, 100);
	igt_fb_set_size(&d->fb[2], d->plane3, d->fb[2].width-400, d->fb[2].height-400);
	igt_plane_set_position(d->plane3, 10, 10);
	igt_plane_set_size(d->plane3, mode->hdisplay-300, mode->vdisplay-300);
	igt_display_commit2(display, COMMIT_ATOMIC);

	if (primary_plane_scaling) {
		/* Switch scaler from plane 1 to plane 2 */
		igt_fb_set_position(&d->fb[0], d->plane1, 0, 0);
		igt_fb_set_size(&d->fb[0], d->plane1, d->fb[0].width, d->fb[0].height);
		igt_plane_set_position(d->plane1, 0, 0);
		igt_plane_set_size(d->plane1, mode->hdisplay, mode->vdisplay);

		igt_fb_set_position(&d->fb[1], d->plane2, 100, 100);
		igt_fb_set_size(&d->fb[1], d->plane2, d->fb[1].width-500,d->fb[1].height-500);
		igt_plane_set_position(d->plane2, 100, 100);
		igt_plane_set_size(d->plane2, mode->hdisplay-200, mode->vdisplay-200);
		igt_display_commit2(display, COMMIT_ATOMIC);
	}
}

static void
__test_scaler_with_clipping_clamping_scenario(data_t *d, drmModeModeInfo *mode,
					      uint32_t f1, uint32_t f2)
{
	cleanup_fbs(d);

	igt_create_pattern_fb(d->drm_fd,
			      mode->hdisplay, mode->vdisplay, f1,
			      LOCAL_I915_FORMAT_MOD_X_TILED, &d->fb[1]);

	igt_create_pattern_fb(d->drm_fd,
			      mode->hdisplay, mode->vdisplay, f2,
			      LOCAL_I915_FORMAT_MOD_Y_TILED, &d->fb[2]);

	igt_plane_set_fb(d->plane1, &d->fb[1]);
	igt_plane_set_fb(d->plane2, &d->fb[2]);

	igt_fb_set_position(&d->fb[1], d->plane1, 0, 0);
	igt_fb_set_size(&d->fb[1], d->plane1, 300, 300);
	igt_plane_set_position(d->plane1, 100, 400);
	igt_fb_set_position(&d->fb[2], d->plane2, 0, 0);
	igt_fb_set_size(&d->fb[2], d->plane2, 400, 400);
	igt_plane_set_position(d->plane2, 100, 100);

	/* scaled window size is outside the modeset area.*/
	igt_plane_set_size(d->plane1, mode->hdisplay + 200,
					    mode->vdisplay + 200);
	igt_plane_set_size(d->plane2, mode->hdisplay + 100,
					    mode->vdisplay + 100);

	/*
	 * Can't guarantee that the clipped coordinates are
	 * suitably aligned for yuv. So allow the commit to fail.
	 */
	if (igt_format_is_yuv(d->fb[1].drm_format) ||
	    igt_format_is_yuv(d->fb[2].drm_format))
		igt_display_try_commit2(&d->display, COMMIT_ATOMIC);
	else
		igt_display_commit2(&d->display, COMMIT_ATOMIC);
}

static void
test_scaler_with_clipping_clamping_scenario(data_t *d, enum pipe pipe, igt_output_t *output)
{
	igt_pipe_t *pipe_obj = &d->display.pipes[pipe];
	drmModeModeInfo *mode;

	igt_require(get_num_scalers(d, pipe) >= 2);

	mode = igt_output_get_mode(output);
	d->plane1 = igt_pipe_get_plane_type(pipe_obj, DRM_PLANE_TYPE_PRIMARY);
	d->plane2 = igt_pipe_get_plane_type(pipe_obj, DRM_PLANE_TYPE_OVERLAY);
	prepare_crtc(d, output, pipe, d->plane1, mode);

	for (int i = 0; i < d->plane1->drm_plane->count_formats; i++) {
		unsigned f1 = d->plane1->drm_plane->formats[i];
		if (!igt_fb_supported_format(f1) ||
		    !can_scale(d, f1))
			continue;

		for (int j = 0; j < d->plane2->drm_plane->count_formats; j++) {
			unsigned f2 = d->plane2->drm_plane->formats[j];

			if (!igt_fb_supported_format(f2) ||
			    !can_scale(d, f2))
				continue;

			__test_scaler_with_clipping_clamping_scenario(d, mode, f1, f2);
		}
	}
}

static void find_connected_pipe(igt_display_t *display, bool second, enum pipe *pipe, igt_output_t **output)
{
	enum pipe first = PIPE_NONE;
	igt_output_t *first_output = NULL;
	bool found = false;

	for_each_pipe_with_valid_output(display, *pipe, *output) {
		if (first == *pipe || *output == first_output)
			continue;

		if (second) {
			first = *pipe;
			first_output = *output;
			second = false;
			continue;
		}

		return;
	}

	if (first_output)
		igt_require_f(found, "No second valid output found\n");
	else
		igt_require_f(found, "No valid outputs found\n");
}

static void test_scaler_with_multi_pipe_plane(data_t *d)
{
	igt_display_t *display = &d->display;
	igt_output_t *output1, *output2;
	drmModeModeInfo *mode1, *mode2;
	enum pipe pipe1, pipe2;
	uint64_t tiling = is_i915_device(display->drm_fd) ?
		LOCAL_I915_FORMAT_MOD_Y_TILED : LOCAL_DRM_FORMAT_MOD_NONE;

	cleanup_crtc(d);

	find_connected_pipe(display, false, &pipe1, &output1);
	find_connected_pipe(display, true, &pipe2, &output2);

	igt_skip_on(!output1 || !output2);

	igt_output_set_pipe(output1, pipe1);
	igt_output_set_pipe(output2, pipe2);

	d->plane1 = igt_output_get_plane(output1, 0);
	d->plane2 = get_num_scalers(d, pipe1) >= 2 ? igt_output_get_plane(output1, 1) : NULL;
	d->plane3 = igt_output_get_plane(output2, 0);
	d->plane4 = get_num_scalers(d, pipe2) >= 2 ? igt_output_get_plane(output2, 1) : NULL;

	mode1 = igt_output_get_mode(output1);
	mode2 = igt_output_get_mode(output2);

	igt_skip_on(!igt_display_has_format_mod(display, DRM_FORMAT_XRGB8888,
						tiling));

	igt_create_pattern_fb(d->drm_fd, 600, 600,
			      DRM_FORMAT_XRGB8888,
			      tiling, &d->fb[0]);

	igt_create_pattern_fb(d->drm_fd, 500, 500,
			      DRM_FORMAT_XRGB8888,
			      tiling, &d->fb[1]);

	igt_create_pattern_fb(d->drm_fd, 700, 700,
			      DRM_FORMAT_XRGB8888,
			      tiling, &d->fb[2]);

	igt_create_pattern_fb(d->drm_fd, 400, 400,
			      DRM_FORMAT_XRGB8888,
			      tiling, &d->fb[3]);

	igt_plane_set_fb(d->plane1, &d->fb[0]);
	if (d->plane2)
		igt_plane_set_fb(d->plane2, &d->fb[1]);
	igt_plane_set_fb(d->plane3, &d->fb[2]);
	if (d->plane4)
		igt_plane_set_fb(d->plane4, &d->fb[3]);
	igt_display_commit2(display, COMMIT_ATOMIC);

	/* Upscaling Primary */
	igt_plane_set_size(d->plane1, mode1->hdisplay, mode1->vdisplay);
	igt_plane_set_size(d->plane3, mode2->hdisplay, mode2->vdisplay);
	igt_display_commit2(display, COMMIT_ATOMIC);

	/* Upscaling Sprites */
	igt_plane_set_size(d->plane2 ?: d->plane1, mode1->hdisplay, mode1->vdisplay);
	igt_plane_set_size(d->plane4 ?: d->plane3, mode2->hdisplay, mode2->vdisplay);
	igt_display_commit2(display, COMMIT_ATOMIC);
}

igt_main
{
	data_t data = {};
	enum pipe pipe;

	igt_skip_on_simulation();

	igt_fixture {
		data.drm_fd = drm_open_driver_master(DRIVER_INTEL | DRIVER_AMDGPU);
		igt_display_require(&data.display, data.drm_fd);
		data.devid = is_i915_device(data.drm_fd) ?
			intel_get_drm_devid(data.drm_fd) : 0;
		igt_require(data.display.is_atomic);
	}

	for_each_pipe_static(pipe) igt_subtest_group {
		igt_output_t *output;

		igt_fixture {
			igt_display_require_output_on_pipe(&data.display, pipe);

			igt_require(get_num_scalers(&data, pipe) > 0);
		}

		igt_subtest_f("pipe-%s-plane-scaling", kmstest_pipe_name(pipe))
			for_each_valid_output_on_pipe(&data.display, pipe, output)
				test_plane_scaling_on_pipe(&data, pipe, output);

		igt_subtest_f("pipe-%s-scaler-with-pixel-format", kmstest_pipe_name(pipe))
			for_each_valid_output_on_pipe(&data.display, pipe, output)
				test_scaler_with_pixel_format_pipe(&data, pipe, output);

		igt_subtest_f("pipe-%s-scaler-with-rotation", kmstest_pipe_name(pipe))
			for_each_valid_output_on_pipe(&data.display, pipe, output)
				test_scaler_with_rotation_pipe(&data, pipe, output);

		igt_subtest_f("pipe-%s-scaler-with-clipping-clamping", kmstest_pipe_name(pipe))
			for_each_valid_output_on_pipe(&data.display, pipe, output)
				test_scaler_with_clipping_clamping_scenario(&data, pipe, output);
	}

	igt_subtest_f("2x-scaler-multi-pipe")
		test_scaler_with_multi_pipe_plane(&data);

	igt_fixture
		igt_display_fini(&data.display);
}
