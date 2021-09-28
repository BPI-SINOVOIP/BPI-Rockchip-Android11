/*
 * Copyright © 2015 Intel Corporation
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
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

IGT_TEST_DESCRIPTION("Exercise CHV pipe C cursor fail");

#ifndef DRM_CAP_CURSOR_WIDTH
#define DRM_CAP_CURSOR_WIDTH 0x8
#endif
#ifndef DRM_CAP_CURSOR_HEIGHT
#define DRM_CAP_CURSOR_HEIGHT 0x9
#endif

typedef struct {
	int drm_fd;
	igt_display_t display;
	struct igt_fb primary_fb;
	struct igt_fb fb;
	igt_output_t *output;
	enum pipe pipe;
	igt_crc_t ref_crc;
	int curw, curh; /* cursor size */
	igt_pipe_crc_t *pipe_crc;
	uint32_t devid;
	bool colored, jump, disable;
	int jump_x, jump_y;
} data_t;

enum {
	EDGE_LEFT = 0x1,
	EDGE_RIGHT = 0x2,
	EDGE_TOP = 0x4,
	EDGE_BOTTOM = 0x8,
};

static void create_cursor_fb(data_t *data, int cur_w, int cur_h)
{
	cairo_t *cr;
	uint32_t fb_id;

	fb_id = igt_create_fb(data->drm_fd, cur_w, cur_h,
			      DRM_FORMAT_ARGB8888,
			      LOCAL_DRM_FORMAT_MOD_NONE,
			      &data->fb);
	igt_assert(fb_id);

	cr = igt_get_cairo_ctx(data->drm_fd, &data->fb);
	if (data->colored)
		igt_paint_color_alpha(cr, 0, 0, data->fb.width, data->fb.height,
				      1.0, 0.0, 0.0, 1.0);
	else
		igt_paint_color_alpha(cr, 0, 0, data->fb.width, data->fb.height,
				      0.0, 0.0, 0.0, 0.0);
	igt_put_cairo_ctx(data->drm_fd, &data->fb, cr);
}

static void cursor_move(data_t *data, int x, int y, int i)
{
	int crtc_id = data->output->config.crtc->crtc_id;

	igt_debug("[%d] x=%d, y=%d\n", i, x, y);

	/*
	 * The "fixed" kernel will refuse the ioctl when pipe C cursor
	 * would straddle the left screen edge (which is when the hw
	 * fails). So let's accept a failure from the ioctl in that case.
	 */
	igt_assert(drmModeMoveCursor(data->drm_fd, crtc_id, x, y) == 0 ||
		   (IS_CHERRYVIEW(data->devid) && data->pipe == PIPE_C &&
		    x < 0 && x > -data->curw));
	igt_wait_for_vblank(data->drm_fd, data->pipe);
}

#define XSTEP 8
#define YSTEP 8
#define NCRC 128

static void test_edge_pos(data_t *data, int sx, int ex, int y, bool swap_axis)
{
	igt_crc_t *crc = NULL;
	int i, n, x, xdir, dx;

	if (sx > ex)
		xdir = -1;
	else
		xdir = 1;

	dx = (ex - sx)/XSTEP;

	i = 0;

	for (x = sx; xdir * (x - ex) <= 0; x += dx) {
		int xx, yy;

		if (swap_axis) {
			xx = y;
			yy = x;
		} else {
			xx = x;
			yy = y;
		}

		if (data->jump) {
			cursor_move(data, data->jump_x, data->jump_y, i++);
		}
		if (data->disable) {
			cursor_move(data, -data->curw, -data->curh, i++);
		}
		cursor_move(data, xx, yy, i++);
		if (data->jump) {
			cursor_move(data, data->jump_x, data->jump_y, i++);
		}
		if (data->disable) {
			cursor_move(data, -data->curw, -data->curh, i++);
		}
	}

	n = igt_pipe_crc_get_crcs(data->pipe_crc, NCRC, &crc);

	if (!data->colored) {
		igt_debug("Checking CRCs: ");
		for (i = 0; i < n; i++) {
			igt_debug("[%d] ", i);
			igt_assert_crc_equal(&data->ref_crc, &crc[i]);
		}
		igt_debug("\n");
	}
}

static void test_edge(data_t *data, int sy, int ey, int sx, int ex, bool swap_axis)
{
	int crtc_id = data->output->config.crtc->crtc_id;
	int y, ydir, dy;

	if (sy > ey)
		ydir = -1;
	else
		ydir = 1;

	dy = (ey - sy) / YSTEP;

	igt_assert_eq(drmModeMoveCursor(data->drm_fd, crtc_id, -data->curw, -data->curh), 0);
	igt_assert_eq(drmModeSetCursor(data->drm_fd, crtc_id, data->fb.gem_handle, data->curw, data->curh), 0);

	for (y = sy; ydir * (y - ey) <= 0; ) {
		test_edge_pos(data, sx, ex, y, swap_axis);
		y += dy;
		test_edge_pos(data, ex, sx, y, swap_axis);
		y += dy;
	}

	igt_assert_eq(drmModeMoveCursor(data->drm_fd, crtc_id, -data->curw, -data->curh), 0);
	igt_assert_eq(drmModeSetCursor(data->drm_fd, crtc_id, 0, data->curw, data->curh), 0);
}

static void test_edges(data_t *data, unsigned int edges)
{
	drmModeModeInfo *mode = igt_output_get_mode(data->output);

	if (edges & EDGE_LEFT) {
		test_edge(data, mode->vdisplay, -data->curh,
			  -data->curw, 0, false);
		test_edge(data, -data->curh, mode->vdisplay,
			  -data->curw, 0, false);
	}

	if (edges & EDGE_RIGHT) {
		test_edge(data, mode->vdisplay, -data->curh,
			  mode->hdisplay - data->curw, mode->hdisplay, false);
		test_edge(data, -data->curh, mode->vdisplay,
			  mode->hdisplay - data->curw, mode->hdisplay, false);
	}

	if (edges & EDGE_TOP) {
		test_edge(data, mode->hdisplay, -data->curw,
			  -data->curh, 0, true);
		test_edge(data, -data->curw, mode->hdisplay,
			  -data->curh, 0, true);
	}

	if (edges & EDGE_BOTTOM) {
		test_edge(data, mode->hdisplay, -data->curw,
			  mode->vdisplay - data->curh, mode->vdisplay, true);
		test_edge(data, -data->curw, mode->hdisplay,
			  mode->vdisplay - data->curh, mode->vdisplay, true);
	}
}

static void cleanup_crtc(data_t *data)
{
	igt_display_t *display = &data->display;

	igt_display_reset(display);
	igt_pipe_crc_free(data->pipe_crc);
	data->pipe_crc = NULL;

	igt_remove_fb(data->drm_fd, &data->primary_fb);
	igt_remove_fb(data->drm_fd, &data->fb);
}

static void prepare_crtc(data_t *data)
{
	drmModeModeInfo *mode;
	igt_display_t *display = &data->display;
	igt_plane_t *primary;

	cleanup_crtc(data);

	/* select the pipe we want to use */
	igt_output_set_pipe(data->output, data->pipe);

	mode = igt_output_get_mode(data->output);
	igt_create_pattern_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
			      DRM_FORMAT_XRGB8888,
			      LOCAL_DRM_FORMAT_MOD_NONE,
			      &data->primary_fb);

	primary = igt_output_get_plane_type(data->output, DRM_PLANE_TYPE_PRIMARY);
	igt_plane_set_fb(primary, &data->primary_fb);

	igt_display_commit2(display, display->is_atomic ? COMMIT_ATOMIC : COMMIT_LEGACY);

	data->jump_x = (mode->hdisplay - data->curw) / 2;
	data->jump_y = (mode->vdisplay - data->curh) / 2;

	/* create the pipe_crc object for this pipe */
	data->pipe_crc = igt_pipe_crc_new_nonblock(data->drm_fd, data->pipe,
						   INTEL_PIPE_CRC_SOURCE_AUTO);

	/* get reference crc w/o cursor */
	igt_pipe_crc_start(data->pipe_crc);
	igt_pipe_crc_get_single(data->pipe_crc, &data->ref_crc);
}

static void test_crtc(data_t *data, unsigned int edges)
{
	prepare_crtc(data);

	create_cursor_fb(data, data->curw, data->curh);

	test_edges(data, edges);
}

static int opt_handler(int opt, int opt_index, void *_data)
{
	data_t *data = _data;

	switch (opt) {
	case 'c':
		data->colored = true;
		break;
	case 'd':
		data->disable = true;
		break;
	case 'j':
		data->jump = true;
		break;
	default:
		return IGT_OPT_HANDLER_ERROR;
	}

	return IGT_OPT_HANDLER_SUCCESS;
}

static data_t data;
static uint64_t max_curw = 64, max_curh = 64;
static const struct option long_opts[] = {
	{ .name = "colored", .val = 'c' },
	{ .name = "disable", .val = 'd'},
	{ .name = "jump", .val = 'j' },
	{}
};
static const char *help_str =
	"  --colored\t\tUse a colored cursor (disables CRC checks)\n"
	"  --disable\t\tDisable the cursor between each step\n"
	"  --jump\t\tJump the cursor to middle of the screen between each step)\n";

igt_main_args("", long_opts, help_str, opt_handler, &data)
{
	igt_skip_on_simulation();

	igt_fixture {
		int ret;

		data.drm_fd = drm_open_driver_master(DRIVER_INTEL);

		data.devid = intel_get_drm_devid(data.drm_fd);

		ret = drmGetCap(data.drm_fd, DRM_CAP_CURSOR_WIDTH, &max_curw);
		igt_assert(ret == 0 || errno == EINVAL);
		/* Not making use of cursor_height since it is same as width, still reading */
		ret = drmGetCap(data.drm_fd, DRM_CAP_CURSOR_HEIGHT, &max_curh);
		igt_assert(ret == 0 || errno == EINVAL);

		kmstest_set_vt_graphics_mode();

		igt_require_pipe_crc(data.drm_fd);

		igt_display_require(&data.display, data.drm_fd);
	}

	for_each_pipe_static(data.pipe) {
		igt_subtest_group {
			igt_fixture {
				igt_display_require_output_on_pipe(&data.display, data.pipe);
				data.output = igt_get_single_output_for_pipe(&data.display, data.pipe);
			}

			for (data.curw = 64; data.curw <= 256; data.curw *= 2) {
				data.curh = data.curw;

				igt_fixture
					igt_require(data.curw <= max_curw && data.curh <= max_curh);

				igt_subtest_f("pipe-%s-%dx%d-left-edge",
					kmstest_pipe_name(data.pipe),
					data.curw, data.curh)
					test_crtc(&data, EDGE_LEFT);

				igt_subtest_f("pipe-%s-%dx%d-right-edge",
					kmstest_pipe_name(data.pipe),
					data.curw, data.curh)
					test_crtc(&data, EDGE_RIGHT);

				igt_subtest_f("pipe-%s-%dx%d-top-edge",
					kmstest_pipe_name(data.pipe),
					data.curw, data.curh)
					test_crtc(&data, EDGE_TOP);

				igt_subtest_f("pipe-%s-%dx%d-bottom-edge",
					kmstest_pipe_name(data.pipe),
					data.curw, data.curh)
					test_crtc(&data, EDGE_BOTTOM);
			}
		}
	}

	igt_fixture
		igt_display_fini(&data.display);
}
