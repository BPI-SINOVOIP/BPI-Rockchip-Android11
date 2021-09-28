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
 *
 */

#include "igt.h"
#include "igt_sysfs.h"
#include "igt_psr.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/timerfd.h>
#include "intel_bufmgr.h"

IGT_TEST_DESCRIPTION("Test PSR2 selective update");

#define SQUARE_SIZE 100
/* each selective update block is 4 lines tall */
#define EXPECTED_NUM_SU_BLOCKS ((SQUARE_SIZE / 4) + (SQUARE_SIZE % 4 ? 1 : 0))

/*
 * Minimum is 15 as the number of frames to active PSR2 could be configured
 * to 15 frames plus a few more in case we miss a selective update between
 * debugfs reads.
 */
#define MAX_SCREEN_CHANGES 20

enum operations {
	PAGE_FLIP,
	FRONTBUFFER,
	LAST
};

static const char *op_str(enum operations op)
{
	static const char * const name[] = {
		[PAGE_FLIP] = "page_flip",
		[FRONTBUFFER] = "frontbuffer"
	};

	return name[op];
}

typedef struct {
	int drm_fd;
	int debugfs_fd;
	igt_display_t display;
	drm_intel_bufmgr *bufmgr;
	drmModeModeInfo *mode;
	igt_output_t *output;
	struct igt_fb fb[2];
	enum operations op;
	cairo_t *cr;
	int change_screen_timerfd;
	uint32_t screen_changes;
} data_t;

static void setup_output(data_t *data)
{
	igt_display_t *display = &data->display;
	igt_output_t *output;
	enum pipe pipe;

	for_each_pipe_with_valid_output(display, pipe, output) {
		drmModeConnectorPtr c = output->config.connector;

		if (c->connector_type != DRM_MODE_CONNECTOR_eDP)
			continue;

		igt_output_set_pipe(output, pipe);
		data->output = output;
		data->mode = igt_output_get_mode(output);

		return;
	}
}

static void display_init(data_t *data)
{
	igt_display_require(&data->display, data->drm_fd);
	setup_output(data);
}

static void display_fini(data_t *data)
{
	igt_display_fini(&data->display);
}

static void prepare(data_t *data)
{
	igt_plane_t *primary;

	/* all green frame */
	igt_create_color_fb(data->drm_fd,
			    data->mode->hdisplay, data->mode->vdisplay,
			    DRM_FORMAT_XRGB8888,
			    LOCAL_DRM_FORMAT_MOD_NONE,
			    0.0, 1.0, 0.0,
			    &data->fb[0]);

	if (data->op == PAGE_FLIP) {
		cairo_t *cr;

		igt_create_color_fb(data->drm_fd,
				    data->mode->hdisplay, data->mode->vdisplay,
				    DRM_FORMAT_XRGB8888,
				    LOCAL_DRM_FORMAT_MOD_NONE,
				    0.0, 1.0, 0.0,
				    &data->fb[1]);

		cr = igt_get_cairo_ctx(data->drm_fd, &data->fb[1]);
		/* paint a white square */
		igt_paint_color_alpha(cr, 0, 0, SQUARE_SIZE, SQUARE_SIZE,
				      1.0, 1.0, 1.0, 1.0);
		igt_put_cairo_ctx(data->drm_fd,  &data->fb[1], cr);
	} else if (data->op == FRONTBUFFER) {
		data->cr = igt_get_cairo_ctx(data->drm_fd, &data->fb[0]);
	}

	primary = igt_output_get_plane_type(data->output,
					    DRM_PLANE_TYPE_PRIMARY);

	igt_plane_set_fb(primary, &data->fb[0]);
	igt_display_commit2(&data->display, COMMIT_ATOMIC);
}

static bool update_screen_and_test(data_t *data)
{
	uint16_t su_blocks;
	bool ret = false;

	switch (data->op) {
	case PAGE_FLIP: {
		igt_plane_t *primary;

		primary = igt_output_get_plane_type(data->output,
						    DRM_PLANE_TYPE_PRIMARY);

		igt_plane_set_fb(primary, &data->fb[data->screen_changes & 1]);
		igt_display_commit2(&data->display, COMMIT_ATOMIC);
		break;
	}
	case FRONTBUFFER: {
		drmModeClip clip;

		clip.x1 = clip.y1 = 0;
		clip.x2 = clip.y2 = SQUARE_SIZE;

		if (data->screen_changes & 1) {
			/* go back to all green frame with a square */
			igt_paint_color_alpha(data->cr, 0, 0, SQUARE_SIZE,
					      SQUARE_SIZE, 1.0, 1.0, 1.0, 1.0);
		} else {
			/* go back to all green frame */
			igt_paint_color_alpha(data->cr, 0, 0, SQUARE_SIZE,
					      SQUARE_SIZE, 0, 1.0, 0, 1.0);
		}

		drmModeDirtyFB(data->drm_fd, data->fb[0].fb_id, &clip, 1);
		break;
	}
	default:
		igt_assert_f(data->op, "Operation not handled\n");
	}

	if (psr2_wait_su(data->debugfs_fd, &su_blocks))
		ret = su_blocks == EXPECTED_NUM_SU_BLOCKS;

	return ret;
}

static void run(data_t *data)
{
	bool result = false;

	igt_assert(psr_wait_entry(data->debugfs_fd, PSR_MODE_2));

	for (data->screen_changes = 0;
	     data->screen_changes < MAX_SCREEN_CHANGES && !result;
	     data->screen_changes++) {
		uint64_t exp;
		int r;

		r = read(data->change_screen_timerfd, &exp, sizeof(exp));
		if (r == sizeof(uint64_t) && exp)
			result = update_screen_and_test(data);
	}

	igt_debug("Screen changes: %u\n", data->screen_changes);
	igt_assert_f(result,
		     "No matching selective update blocks read from debugfs\n");
}

static void cleanup(data_t *data)
{
	igt_plane_t *primary;

	primary = igt_output_get_plane_type(data->output,
					    DRM_PLANE_TYPE_PRIMARY);
	igt_plane_set_fb(primary, NULL);
	igt_display_commit2(&data->display, COMMIT_ATOMIC);

	if (data->op == PAGE_FLIP)
		igt_remove_fb(data->drm_fd, &data->fb[1]);
	else if (data->op == FRONTBUFFER)
		igt_put_cairo_ctx(data->drm_fd, &data->fb[0], data->cr);

	igt_remove_fb(data->drm_fd, &data->fb[0]);
}

igt_main
{
	data_t data = {};

	igt_skip_on_simulation();

	igt_fixture {
		struct itimerspec interval;
		int r;

		data.drm_fd = drm_open_driver_master(DRIVER_INTEL);
		data.debugfs_fd = igt_debugfs_dir(data.drm_fd);
		kmstest_set_vt_graphics_mode();

		igt_require_f(psr_sink_support(data.debugfs_fd, PSR_MODE_2),
			      "Sink does not support PSR2\n");

		data.bufmgr = drm_intel_bufmgr_gem_init(data.drm_fd, 4096);
		igt_assert(data.bufmgr);
		drm_intel_bufmgr_gem_enable_reuse(data.bufmgr);

		display_init(&data);

		/* Test if PSR2 can be enabled */
		igt_require_f(psr_enable(data.debugfs_fd, PSR_MODE_2),
			      "Error enabling PSR2\n");
		data.op = FRONTBUFFER;
		prepare(&data);
		r = psr_wait_entry(data.debugfs_fd, PSR_MODE_2);
		cleanup(&data);
		igt_require_f(r, "PSR2 can not be enabled\n");

		/* blocking timerfd */
		data.change_screen_timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
		igt_require(data.change_screen_timerfd != -1);
		/* Changing screen at 30hz to support 30hz panels */
		interval.it_value.tv_nsec = NSEC_PER_SEC / 30;
		interval.it_value.tv_sec = 0;
		interval.it_interval.tv_nsec = interval.it_value.tv_nsec;
		interval.it_interval.tv_sec = interval.it_value.tv_sec;
		r = timerfd_settime(data.change_screen_timerfd, 0, &interval, NULL);
		igt_require_f(r != -1, "Error setting timerfd\n");
	}

	for (data.op = PAGE_FLIP; data.op < LAST; data.op++) {
		igt_subtest_f("%s", op_str(data.op)) {
			prepare(&data);
			run(&data);
			cleanup(&data);
		}
	}

	igt_fixture {
		close(data.debugfs_fd);
		drm_intel_bufmgr_destroy(data.bufmgr);
		display_fini(&data);
	}
}
