/*
 * Copyright © 2014 Intel Corporation
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
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "igt_device.h"

typedef struct {
	int drm_fd;
	igt_display_t display;
} data_t;

IGT_TEST_DESCRIPTION(
    "This test tries to provoke the kernel into leaking a pending page flip "
    "event when the fd is closed before the flip has completed. The test "
    "itself won't fail even if the kernel leaks the event, but the resulting "
    "dmesg WARN will indicate a failure.");

static void test(data_t *data, enum pipe pipe, igt_output_t *output)
{
	igt_plane_t *primary;
	drmModeModeInfo *mode;
	struct igt_fb fb[2];
	int fd, ret;

	/* select the pipe we want to use */
	igt_output_set_pipe(output, pipe);

	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);
	mode = igt_output_get_mode(output);

	igt_create_color_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
			    DRM_FORMAT_XRGB8888,
			    LOCAL_DRM_FORMAT_MOD_NONE,
			    0.0, 0.0, 0.0, &fb[0]);

	igt_plane_set_fb(primary, &fb[0]);
	igt_display_commit2(&data->display, COMMIT_LEGACY);

	fd = drm_open_driver(DRIVER_ANY);

	igt_device_drop_master(data->drm_fd);

	igt_device_set_master(fd);

	igt_create_color_fb(fd, mode->hdisplay, mode->vdisplay,
			    DRM_FORMAT_XRGB8888,
			    LOCAL_DRM_FORMAT_MOD_NONE,
			    0.0, 0.0, 0.0, &fb[1]);
	ret = drmModePageFlip(fd, output->config.crtc->crtc_id,
			      fb[1].fb_id, DRM_MODE_PAGE_FLIP_EVENT,
			      data);
	igt_assert_eq(ret, 0);

	ret = close(fd);
	igt_assert_eq(ret, 0);

	igt_device_set_master(data->drm_fd);

	igt_plane_set_fb(primary, NULL);
	igt_output_set_pipe(output, PIPE_ANY);
	igt_display_commit(&data->display);

	igt_remove_fb(data->drm_fd, &fb[0]);
}

igt_simple_main
{
	data_t data = {};
	igt_output_t *output;
	int valid_tests = 0;
	enum pipe pipe;

	igt_skip_on_simulation();

	data.drm_fd = drm_open_driver_master(DRIVER_ANY);
	kmstest_set_vt_graphics_mode();

	igt_display_require(&data.display, data.drm_fd);

	for_each_pipe_with_valid_output(&data.display, pipe, output) {
		test(&data, pipe, output);
		valid_tests++;
	}

	igt_require_f(valid_tests, "no valid crtc/connector combinations found\n");

	igt_display_fini(&data.display);
}
