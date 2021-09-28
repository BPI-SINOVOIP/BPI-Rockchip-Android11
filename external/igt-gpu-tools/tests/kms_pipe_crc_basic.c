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
 */

#include "igt.h"
#include "igt_sysfs.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>


typedef struct {
	int drm_fd;
	int debugfs;
	igt_display_t display;
	struct igt_fb fb;
} data_t;

static struct {
	double r, g, b;
	igt_crc_t crc;
} colors[2] = {
	{ .r = 0.0, .g = 1.0, .b = 0.0 },
	{ .r = 0.0, .g = 1.0, .b = 1.0 },
};

static void test_bad_source(data_t *data)
{
	errno = 0;
	if (igt_sysfs_set(data->debugfs, "crtc-0/crc/control", "foo")) {
		igt_assert(openat(data->debugfs, "crtc-0/crc/data", O_WRONLY) == -1);
		igt_skip_on(errno == EIO);
	}

	igt_assert_eq(errno, EINVAL);
}

#define N_CRCS	3

#define TEST_SEQUENCE (1<<0)
#define TEST_NONBLOCK (1<<1)

static void test_read_crc(data_t *data, enum pipe pipe, unsigned flags)
{
	igt_display_t *display = &data->display;
	igt_output_t *output = igt_get_single_output_for_pipe(display, pipe);
	igt_plane_t *primary;
	drmModeModeInfo *mode;
	igt_crc_t *crcs = NULL;
	int c, j;

	igt_skip_on(pipe >= data->display.n_pipes);
	igt_require_f(output, "No connector found for pipe %s\n",
		      kmstest_pipe_name(pipe));

	igt_display_reset(display);
	igt_output_set_pipe(output, pipe);

	for (c = 0; c < ARRAY_SIZE(colors); c++) {
		char *crc_str;
		int n_crcs;

		igt_debug("Clearing the fb with color (%.02lf,%.02lf,%.02lf)\n",
			  colors[c].r, colors[c].g, colors[c].b);

		mode = igt_output_get_mode(output);
		igt_create_color_fb(data->drm_fd,
					mode->hdisplay, mode->vdisplay,
					DRM_FORMAT_XRGB8888,
					LOCAL_DRM_FORMAT_MOD_NONE,
					colors[c].r,
					colors[c].g,
					colors[c].b,
					&data->fb);

		primary = igt_output_get_plane(output, 0);
		igt_plane_set_fb(primary, &data->fb);

		igt_display_commit(display);

		/* wait for N_CRCS vblanks and the corresponding N_CRCS CRCs */
		if (flags & TEST_NONBLOCK) {
			igt_pipe_crc_t *pipe_crc;

			pipe_crc = igt_pipe_crc_new_nonblock(data->drm_fd, pipe, INTEL_PIPE_CRC_SOURCE_AUTO);
			igt_wait_for_vblank(data->drm_fd, pipe);
			igt_pipe_crc_start(pipe_crc);

			igt_wait_for_vblank_count(data->drm_fd, pipe, N_CRCS);
			n_crcs = igt_pipe_crc_get_crcs(pipe_crc, N_CRCS+1, &crcs);
			igt_pipe_crc_stop(pipe_crc);
			igt_pipe_crc_free(pipe_crc);

			/* allow a one frame difference */
			igt_assert_lte(N_CRCS, n_crcs);
		} else {
			igt_pipe_crc_t *pipe_crc;

			pipe_crc = igt_pipe_crc_new(data->drm_fd, pipe, INTEL_PIPE_CRC_SOURCE_AUTO);
			igt_pipe_crc_start(pipe_crc);

			n_crcs = igt_pipe_crc_get_crcs(pipe_crc, N_CRCS, &crcs);

			igt_pipe_crc_stop(pipe_crc);
			igt_pipe_crc_free(pipe_crc);

			igt_assert_eq(n_crcs, N_CRCS);
		}


		/*
		 * save the CRC in colors so it can be compared to the CRC of
		 * other fbs
		 */
		colors[c].crc = crcs[0];

		crc_str = igt_crc_to_string(&crcs[0]);
		igt_debug("CRC for this fb: %s\n", crc_str);
		free(crc_str);

		/* and ensure that they'are all equal, we haven't changed the fb */
		for (j = 0; j < (n_crcs - 1); j++)
			igt_assert_crc_equal(&crcs[j], &crcs[j + 1]);

		if (flags & TEST_SEQUENCE)
			for (j = 0; j < (n_crcs - 1); j++)
				igt_assert_eq(crcs[j].frame + 1, crcs[j + 1].frame);

		free(crcs);
		igt_remove_fb(data->drm_fd, &data->fb);
	}
}

data_t data = {0, };

igt_main
{
	enum pipe pipe;

	igt_fixture {
		data.drm_fd = drm_open_driver_master(DRIVER_ANY);

		kmstest_set_vt_graphics_mode();

		igt_require_pipe_crc(data.drm_fd);

		igt_display_require(&data.display, data.drm_fd);
		data.debugfs = igt_debugfs_dir(data.drm_fd);
	}

	igt_subtest("bad-source")
		test_bad_source(&data);

	igt_skip_on_simulation();

	for_each_pipe_static(pipe) {
		igt_subtest_f("read-crc-pipe-%s", kmstest_pipe_name(pipe))
			test_read_crc(&data, pipe, 0);

		igt_subtest_f("read-crc-pipe-%s-frame-sequence", kmstest_pipe_name(pipe))
			test_read_crc(&data, pipe, TEST_SEQUENCE);

		igt_subtest_f("nonblocking-crc-pipe-%s", kmstest_pipe_name(pipe))
			test_read_crc(&data, pipe, TEST_NONBLOCK);

		igt_subtest_f("nonblocking-crc-pipe-%s-frame-sequence", kmstest_pipe_name(pipe))
			test_read_crc(&data, pipe, TEST_SEQUENCE | TEST_NONBLOCK);

		igt_subtest_f("suspend-read-crc-pipe-%s", kmstest_pipe_name(pipe)) {
			igt_skip_on(pipe >= data.display.n_pipes);

			test_read_crc(&data, pipe, 0);

			igt_system_suspend_autoresume(SUSPEND_STATE_MEM,
						      SUSPEND_TEST_NONE);

			test_read_crc(&data, pipe, 0);
		}

		igt_subtest_f("hang-read-crc-pipe-%s", kmstest_pipe_name(pipe)) {
			igt_hang_t hang = igt_allow_hang(data.drm_fd, 0, 0);

			test_read_crc(&data, pipe, 0);

			igt_force_gpu_reset(data.drm_fd);

			test_read_crc(&data, pipe, 0);

			igt_disallow_hang(data.drm_fd, hang);
		}
	}

	igt_fixture {
		igt_display_fini(&data.display);
	}
}
