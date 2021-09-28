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
#include "drmtest.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

IGT_TEST_DESCRIPTION("Test atomic mode setting with a plane by switching between high and low resolutions");

#define MAX_CRCS          1
#define SIZE            256
#define LOOP_FOREVER     -1

typedef struct {
	int drm_fd;
	igt_display_t display;
	struct igt_fb fb_primary;
	struct igt_fb fb_plane;
} data_t;

static drmModeModeInfo
get_lowres_mode(int drmfd, igt_output_t *output, drmModeModeInfo *mode_default)
{
	drmModeModeInfo mode;
	bool found = false;
	int limit = mode_default->vdisplay - SIZE;
	int j;

	for (j = 0; j < output->config.connector->count_modes; j++) {
		mode = output->config.connector->modes[j];
		if (mode.vdisplay < limit) {
			found = true;
			break;
		}
	}

	if (!found)
		return *igt_std_1024_mode_get();

	return mode;
}

static void
check_mode(drmModeModeInfo *mode1, drmModeModeInfo *mode2)
{
	igt_assert_eq(mode1->hdisplay, mode2->hdisplay);
	igt_assert_eq(mode1->vdisplay, mode2->vdisplay);
	igt_assert_eq(mode1->vrefresh, mode2->vrefresh);
}

/*
 * Return false if test on this plane should be skipped
 */
static bool
setup_plane(data_t *data, igt_plane_t *plane, drmModeModeInfo *mode,
	    uint64_t modifier)
{
	uint64_t plane_modifier;
	uint32_t plane_format;
	int size, x, y;

	if (plane->type == DRM_PLANE_TYPE_PRIMARY)
		return false;

	if (plane->type == DRM_PLANE_TYPE_CURSOR)
		size = 64;
	else
		size = SIZE;

	x = 0;
	y = mode->vdisplay - size;

	if (plane->type == DRM_PLANE_TYPE_CURSOR) {
		plane_format = DRM_FORMAT_ARGB8888;
		plane_modifier = LOCAL_DRM_FORMAT_MOD_NONE;
	} else {
		plane_format = DRM_FORMAT_XRGB8888;
		plane_modifier = modifier;
	}

	if (!igt_plane_has_format_mod(plane, plane_format, plane_modifier))
		return false;

	igt_create_color_fb(data->drm_fd, size, size, plane_format,
			    plane_modifier, 1.0, 1.0, 0.0, &data->fb_plane);
	igt_plane_set_position(plane, x, y);
	igt_plane_set_fb(plane, &data->fb_plane);

	return true;
}

static igt_plane_t *
primary_plane_get(igt_display_t *display, enum pipe pipe)
{
	unsigned plane_primary_id = display->pipes[pipe].plane_primary;
	return &display->pipes[pipe].planes[plane_primary_id];
}

static unsigned
test_planes_on_pipe_with_output(data_t *data, enum pipe pipe,
				igt_output_t *output, uint64_t modifier)
{
	drmModeModeInfo *mode, mode_lowres;
	igt_pipe_crc_t *pipe_crc;
	unsigned tested = 0;
	igt_plane_t *plane;

	igt_info("Testing connector %s using pipe %s\n",
		 igt_output_name(output), kmstest_pipe_name(pipe));

	igt_output_set_pipe(output, pipe);
	mode = igt_output_get_mode(output);
	mode_lowres = get_lowres_mode(data->drm_fd, output, mode);

	igt_create_color_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
			    DRM_FORMAT_XRGB8888, modifier, 0.0, 0.0, 1.0,
			    &data->fb_primary);
	igt_plane_set_fb(primary_plane_get(&data->display, pipe),
			 &data->fb_primary);

	pipe_crc = igt_pipe_crc_new(data->drm_fd, pipe,
				    INTEL_PIPE_CRC_SOURCE_AUTO);

	/* yellow sprite plane in lower left corner */
	for_each_plane_on_pipe(&data->display, pipe, plane) {
		igt_crc_t crc_hires1, crc_hires2;
		int r;

		if (!setup_plane(data, plane, mode, modifier))
			continue;

		r = igt_display_try_commit2(&data->display, COMMIT_ATOMIC);
		if (r) {
			igt_debug("Commit failed %i", r);
			continue;
		}

		igt_pipe_crc_start(pipe_crc);
		igt_pipe_crc_get_single(pipe_crc, &crc_hires1);

		igt_assert_plane_visible(data->drm_fd, pipe, plane->index,
					 true);

		/* switch to lower resolution */
		igt_output_override_mode(output, &mode_lowres);
		igt_output_set_pipe(output, pipe);
		check_mode(&mode_lowres, igt_output_get_mode(output));

		igt_display_commit2(&data->display, COMMIT_ATOMIC);

		igt_assert_plane_visible(data->drm_fd, pipe, plane->index,
					 false);

		/* switch back to higher resolution */
		igt_output_override_mode(output, NULL);
		igt_output_set_pipe(output, pipe);
		check_mode(mode, igt_output_get_mode(output));

		igt_display_commit2(&data->display, COMMIT_ATOMIC);

		igt_pipe_crc_get_current(data->drm_fd, pipe_crc, &crc_hires2);

		igt_assert_plane_visible(data->drm_fd, pipe, plane->index,
					 true);

		igt_assert_crc_equal(&crc_hires1, &crc_hires2);

		igt_pipe_crc_stop(pipe_crc);

		igt_plane_set_fb(plane, NULL);
		igt_remove_fb(data->drm_fd, &data->fb_plane);
		tested++;
	}

	igt_pipe_crc_free(pipe_crc);

	igt_plane_set_fb(primary_plane_get(&data->display, pipe), NULL);
	igt_remove_fb(data->drm_fd, &data->fb_primary);
	igt_output_set_pipe(output, PIPE_NONE);

	return tested;
}

static void
test_planes_on_pipe(data_t *data, enum pipe pipe, uint64_t modifier)
{
	igt_output_t *output;
	unsigned tested = 0;

	igt_skip_on(pipe >= data->display.n_pipes);
	igt_display_require_output_on_pipe(&data->display, pipe);
	igt_skip_on(!igt_display_has_format_mod(&data->display,
						DRM_FORMAT_XRGB8888, modifier));

	for_each_valid_output_on_pipe(&data->display, pipe, output)
		tested += test_planes_on_pipe_with_output(data, pipe, output,
							  modifier);

	igt_assert(tested > 0);
}

igt_main
{
	data_t data = {};
	enum pipe pipe;

	igt_skip_on_simulation();

	igt_fixture {
		data.drm_fd = drm_open_driver_master(DRIVER_ANY);

		kmstest_set_vt_graphics_mode();

		igt_require_pipe_crc(data.drm_fd);
		igt_display_require(&data.display, data.drm_fd);
		igt_require(data.display.is_atomic);
	}

	for_each_pipe_static(pipe) {
		igt_subtest_f("pipe-%s-tiling-none", kmstest_pipe_name(pipe))
			test_planes_on_pipe(&data, pipe,
					    LOCAL_DRM_FORMAT_MOD_NONE);

		igt_subtest_f("pipe-%s-tiling-x", kmstest_pipe_name(pipe))
			test_planes_on_pipe(&data, pipe,
					    LOCAL_I915_FORMAT_MOD_X_TILED);

		igt_subtest_f("pipe-%s-tiling-y", kmstest_pipe_name(pipe))
			test_planes_on_pipe(&data, pipe,
					    LOCAL_I915_FORMAT_MOD_Y_TILED);

		igt_subtest_f("pipe-%s-tiling-yf", kmstest_pipe_name(pipe))
			test_planes_on_pipe(&data, pipe,
					    LOCAL_I915_FORMAT_MOD_Yf_TILED);
	}

	igt_fixture {
		igt_display_fini(&data.display);
	}
}
