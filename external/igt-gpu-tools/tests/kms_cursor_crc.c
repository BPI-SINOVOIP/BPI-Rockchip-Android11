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
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>


IGT_TEST_DESCRIPTION(
   "Use the display CRC support to validate cursor plane functionality. "
   "The test will position the cursor plane either fully onscreen, "
   "partially onscreen, or fully offscreen, using either a fully opaque "
   "or fully transparent surface. In each case it then reads the PF CRC "
   "and compares it with the CRC value obtained when the cursor plane "
   "was disabled.");

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
	int left, right, top, bottom;
	int screenw, screenh;
	int refresh;
	int curw, curh; /* cursor size */
	int cursor_max_w, cursor_max_h;
	igt_pipe_crc_t *pipe_crc;
	unsigned flags;
} data_t;

#define TEST_DPMS (1<<0)
#define TEST_SUSPEND (1<<1)

static void draw_cursor(cairo_t *cr, int x, int y, int cw, int ch, double a)
{
	int wl, wr, ht, hb;

	/* deal with odd cursor width/height */
	wl = cw / 2;
	wr = (cw + 1) / 2;
	ht = ch / 2;
	hb = (ch + 1) / 2;

	/* Cairo doesn't like to be fed numbers that are too wild */
	if ((x < SHRT_MIN) || (x > SHRT_MAX) || (y < SHRT_MIN) || (y > SHRT_MAX))
		return;
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	/* 4 color rectangles in the corners, RGBY */
	igt_paint_color_alpha(cr, x,      y,      wl, ht, 1.0, 0.0, 0.0, a);
	igt_paint_color_alpha(cr, x + wl, y,      wr, ht, 0.0, 1.0, 0.0, a);
	igt_paint_color_alpha(cr, x,      y + ht, wl, hb, 0.0, 0.0, 1.0, a);
	igt_paint_color_alpha(cr, x + wl, y + ht, wr, hb, 0.5, 0.5, 0.5, a);
}

static void cursor_enable(data_t *data)
{
	igt_output_t *output = data->output;
	igt_plane_t *cursor =
		igt_output_get_plane_type(output, DRM_PLANE_TYPE_CURSOR);

	igt_plane_set_fb(cursor, &data->fb);
	igt_plane_set_size(cursor, data->curw, data->curh);
	igt_fb_set_size(&data->fb, cursor, data->curw, data->curh);
}

static void cursor_disable(data_t *data)
{
	igt_output_t *output = data->output;
	igt_plane_t *cursor =
		igt_output_get_plane_type(output, DRM_PLANE_TYPE_CURSOR);

	igt_plane_set_fb(cursor, NULL);
	igt_plane_set_position(cursor, 0, 0);
}

static bool chv_cursor_broken(data_t *data, int x)
{
	uint32_t devid;

	if (!is_i915_device(data->drm_fd))
		return false;

	devid = intel_get_drm_devid(data->drm_fd);

	/*
	 * CHV gets a FIFO underrun on pipe C when cursor x coordinate
	 * is negative and the cursor visible.
	 *
	 * i915 is fixed to return -EINVAL on cursor updates with those
	 * negative coordinates, so require cursor update to fail with
	 * -EINVAL in that case.
	 *
	 * See also kms_chv_cursor_fail.c
	 */
	if (x >= 0)
		return false;

	return IS_CHERRYVIEW(devid) && data->pipe == PIPE_C;
}

static bool cursor_visible(data_t *data, int x, int y)
{
	if (x + data->curw <= 0 || y + data->curh <= 0)
		return false;

	if (x >= data->screenw || y >= data->screenh)
		return false;

	return true;
}

static void do_single_test(data_t *data, int x, int y)
{
	igt_display_t *display = &data->display;
	igt_pipe_crc_t *pipe_crc = data->pipe_crc;
	igt_crc_t crc, ref_crc;
	igt_plane_t *cursor =
		igt_output_get_plane_type(data->output, DRM_PLANE_TYPE_CURSOR);
	cairo_t *cr;
	int ret = 0;

	igt_print_activity();

	/* Hardware test */
	cr = igt_get_cairo_ctx(data->drm_fd, &data->primary_fb);
	igt_paint_test_pattern(cr, data->screenw, data->screenh);
	igt_put_cairo_ctx(data->drm_fd, &data->primary_fb, cr);

	cursor_enable(data);
	igt_plane_set_position(cursor, x, y);

	if (chv_cursor_broken(data, x) && cursor_visible(data, x, y)) {
		ret = igt_display_try_commit2(display, COMMIT_LEGACY);
		igt_assert_eq(ret, -EINVAL);
		igt_plane_set_position(cursor, 0, y);

		return;
	}

	igt_display_commit(display);

	igt_wait_for_vblank(data->drm_fd, data->pipe);
	igt_pipe_crc_collect_crc(pipe_crc, &crc);

	if (data->flags & (TEST_DPMS | TEST_SUSPEND)) {
		igt_crc_t crc_after;

		if (data->flags & TEST_DPMS) {
			igt_debug("dpms off/on cycle\n");
			kmstest_set_connector_dpms(data->drm_fd,
						   data->output->config.connector,
						   DRM_MODE_DPMS_OFF);
			kmstest_set_connector_dpms(data->drm_fd,
						   data->output->config.connector,
						   DRM_MODE_DPMS_ON);
		}

		if (data->flags & TEST_SUSPEND)
			igt_system_suspend_autoresume(SUSPEND_STATE_MEM,
						      SUSPEND_TEST_NONE);

		igt_pipe_crc_collect_crc(pipe_crc, &crc_after);
		igt_assert_crc_equal(&crc, &crc_after);
	}

	cursor_disable(data);
	igt_display_commit(display);

	/* Now render the same in software and collect crc */
	cr = igt_get_cairo_ctx(data->drm_fd, &data->primary_fb);
	draw_cursor(cr, x, y, data->curw, data->curh, 1.0);
	igt_put_cairo_ctx(data->drm_fd, &data->primary_fb, cr);
	igt_display_commit(display);

	igt_wait_for_vblank(data->drm_fd, data->pipe);
	igt_pipe_crc_collect_crc(pipe_crc, &ref_crc);
	igt_assert_crc_equal(&crc, &ref_crc);

	/* Clear screen afterwards */
	cr = igt_get_cairo_ctx(data->drm_fd, &data->primary_fb);
	igt_paint_color(cr, 0, 0, data->screenw, data->screenh, 0.0, 0.0, 0.0);
	igt_put_cairo_ctx(data->drm_fd, &data->primary_fb, cr);
}

static void do_fail_test(data_t *data, int x, int y, int expect)
{
	igt_display_t *display = &data->display;
	igt_plane_t *cursor =
		igt_output_get_plane_type(data->output, DRM_PLANE_TYPE_CURSOR);
	cairo_t *cr;
	int ret;

	igt_print_activity();

	/* Hardware test */
	cr = igt_get_cairo_ctx(data->drm_fd, &data->primary_fb);
	igt_paint_test_pattern(cr, data->screenw, data->screenh);
	igt_put_cairo_ctx(data->drm_fd, &data->primary_fb, cr);

	cursor_enable(data);
	igt_plane_set_position(cursor, x, y);
	ret = igt_display_try_commit2(display, COMMIT_LEGACY);

	igt_plane_set_position(cursor, 0, 0);
	cursor_disable(data);
	igt_display_commit(display);

	igt_assert_eq(ret, expect);
}

static void do_test(data_t *data,
		    int left, int right, int top, int bottom)
{
	do_single_test(data, left, top);
	do_single_test(data, right, top);
	do_single_test(data, right, bottom);
	do_single_test(data, left, bottom);
}

static void test_crc_onscreen(data_t *data)
{
	int left = data->left;
	int right = data->right;
	int top = data->top;
	int bottom = data->bottom;
	int cursor_w = data->curw;
	int cursor_h = data->curh;

	/* fully inside  */
	do_test(data, left, right, top, bottom);

	/* 2 pixels inside */
	do_test(data, left - (cursor_w-2), right + (cursor_w-2), top               , bottom               );
	do_test(data, left               , right               , top - (cursor_h-2), bottom + (cursor_h-2));
	do_test(data, left - (cursor_w-2), right + (cursor_w-2), top - (cursor_h-2), bottom + (cursor_h-2));

	/* 1 pixel inside */
	do_test(data, left - (cursor_w-1), right + (cursor_w-1), top               , bottom               );
	do_test(data, left               , right               , top - (cursor_h-1), bottom + (cursor_h-1));
	do_test(data, left - (cursor_w-1), right + (cursor_w-1), top - (cursor_h-1), bottom + (cursor_h-1));
}

static void test_crc_offscreen(data_t *data)
{
	int left = data->left;
	int right = data->right;
	int top = data->top;
	int bottom = data->bottom;
	int cursor_w = data->curw;
	int cursor_h = data->curh;

	/* fully outside */
	do_test(data, left - (cursor_w), right + (cursor_w), top             , bottom             );
	do_test(data, left             , right             , top - (cursor_h), bottom + (cursor_h));
	do_test(data, left - (cursor_w), right + (cursor_w), top - (cursor_h), bottom + (cursor_h));

	/* fully outside by 1 extra pixels */
	do_test(data, left - (cursor_w+1), right + (cursor_w+1), top               , bottom               );
	do_test(data, left               , right               , top - (cursor_h+1), bottom + (cursor_h+1));
	do_test(data, left - (cursor_w+1), right + (cursor_w+1), top - (cursor_h+1), bottom + (cursor_h+1));

	/* fully outside by 2 extra pixels */
	do_test(data, left - (cursor_w+2), right + (cursor_w+2), top               , bottom               );
	do_test(data, left               , right               , top - (cursor_h+2), bottom + (cursor_h+2));
	do_test(data, left - (cursor_w+2), right + (cursor_w+2), top - (cursor_h+2), bottom + (cursor_h+2));

	/* fully outside by a lot of extra pixels */
	do_test(data, left - (cursor_w+512), right + (cursor_w+512), top                 , bottom                 );
	do_test(data, left                 , right                 , top - (cursor_h+512), bottom + (cursor_h+512));
	do_test(data, left - (cursor_w+512), right + (cursor_w+512), top - (cursor_h+512), bottom + (cursor_h+512));

	/* go nuts */
	do_test(data, INT_MIN, INT_MAX - cursor_w, INT_MIN, INT_MAX - cursor_h);
	do_test(data, SHRT_MIN, SHRT_MAX, SHRT_MIN, SHRT_MAX);

	/* Make sure we get -ERANGE on integer overflow */
	do_fail_test(data, INT_MAX - cursor_w + 1, INT_MAX - cursor_h + 1, -ERANGE);
}

static void test_crc_sliding(data_t *data)
{
	int i;

	/* Make sure cursor moves smoothly and pixel-by-pixel, and that there are
	 * no alignment issues. Horizontal, vertical and diagonal test.
	 */
	for (i = 0; i < 16; i++) {
		do_single_test(data, i, 0);
		do_single_test(data, 0, i);
		do_single_test(data, i, i);
	}
}

static void test_crc_random(data_t *data)
{
	int i, max;

	max = data->flags & (TEST_DPMS | TEST_SUSPEND) ? 2 : 50;

	/* Random cursor placement */
	for (i = 0; i < max; i++) {
		int x = rand() % (data->screenw + data->curw * 2) - data->curw;
		int y = rand() % (data->screenh + data->curh * 2) - data->curh;
		do_single_test(data, x, y);
	}
}

static void cleanup_crtc(data_t *data)
{
	igt_display_t *display = &data->display;

	igt_pipe_crc_free(data->pipe_crc);
	data->pipe_crc = NULL;

	igt_remove_fb(data->drm_fd, &data->primary_fb);

	igt_display_reset(display);
}

static void prepare_crtc(data_t *data, igt_output_t *output,
			 int cursor_w, int cursor_h)
{
	drmModeModeInfo *mode;
	igt_display_t *display = &data->display;
	igt_plane_t *primary;

	cleanup_crtc(data);

	/* select the pipe we want to use */
	igt_output_set_pipe(output, data->pipe);

	/* create and set the primary plane fb */
	mode = igt_output_get_mode(output);
	igt_create_color_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
			    DRM_FORMAT_XRGB8888,
			    LOCAL_DRM_FORMAT_MOD_NONE,
			    0.0, 0.0, 0.0,
			    &data->primary_fb);

	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);
	igt_plane_set_fb(primary, &data->primary_fb);

	igt_display_commit(display);

	/* create the pipe_crc object for this pipe */
	data->pipe_crc = igt_pipe_crc_new(data->drm_fd, data->pipe,
					  INTEL_PIPE_CRC_SOURCE_AUTO);

	/* x/y position where the cursor is still fully visible */
	data->left = 0;
	data->right = mode->hdisplay - cursor_w;
	data->top = 0;
	data->bottom = mode->vdisplay - cursor_h;
	data->screenw = mode->hdisplay;
	data->screenh = mode->vdisplay;
	data->curw = cursor_w;
	data->curh = cursor_h;
	data->refresh = mode->vrefresh;

	/* get reference crc w/o cursor */
	igt_pipe_crc_collect_crc(data->pipe_crc, &data->ref_crc);
}

static void test_cursor_alpha(data_t *data, double a)
{
	igt_display_t *display = &data->display;
	igt_pipe_crc_t *pipe_crc = data->pipe_crc;
	igt_crc_t crc, ref_crc;
	cairo_t *cr;
	uint32_t fb_id;
	int curw = data->curw;
	int curh = data->curh;

	/*alpha cursor fb*/
	fb_id = igt_create_fb(data->drm_fd, curw, curh,
				    DRM_FORMAT_ARGB8888,
				    LOCAL_DRM_FORMAT_MOD_NONE,
				    &data->fb);
	igt_assert(fb_id);
	cr = igt_get_cairo_ctx(data->drm_fd, &data->fb);
	igt_paint_color_alpha(cr, 0, 0, curw, curh, 1.0, 1.0, 1.0, a);
	igt_put_cairo_ctx(data->drm_fd, &data->fb, cr);

	/*Hardware Test*/
	cursor_enable(data);
	igt_display_commit(display);
	igt_wait_for_vblank(data->drm_fd, data->pipe);
	igt_pipe_crc_collect_crc(pipe_crc, &crc);
	cursor_disable(data);
	igt_remove_fb(data->drm_fd, &data->fb);

	/*Software Test*/
	cr = igt_get_cairo_ctx(data->drm_fd, &data->primary_fb);
	igt_paint_color_alpha(cr, 0, 0, curw, curh, 1.0, 1.0, 1.0, a);
	igt_put_cairo_ctx(data->drm_fd, &data->primary_fb, cr);

	igt_display_commit(display);
	igt_wait_for_vblank(data->drm_fd, data->pipe);
	igt_pipe_crc_collect_crc(pipe_crc, &ref_crc);
	igt_assert_crc_equal(&crc, &ref_crc);

	/*Clear Screen*/
	cr = igt_get_cairo_ctx(data->drm_fd, &data->primary_fb);
	igt_paint_color(cr, 0, 0, data->screenw, data->screenh,
			0.0, 0.0, 0.0);
	igt_put_cairo_ctx(data->drm_fd, &data->primary_fb, cr);
}

static void test_cursor_transparent(data_t *data)
{
	test_cursor_alpha(data, 0.0);

}

static void test_cursor_opaque(data_t *data)
{
	test_cursor_alpha(data, 1.0);
}

static void run_test(data_t *data, void (*testfunc)(data_t *), int cursor_w, int cursor_h)
{
	prepare_crtc(data, data->output, cursor_w, cursor_h);
	testfunc(data);
}

static void create_cursor_fb(data_t *data, int cur_w, int cur_h)
{
	cairo_t *cr;
	uint32_t fb_id;

	/*
	 * Make the FB slightly taller and leave the extra
	 * line opaque white, so that we can see that the
	 * hardware won't scan beyond what it should (esp.
	 * with non-square cursors).
	 */
	fb_id = igt_create_color_fb(data->drm_fd, cur_w, cur_h + 1,
				    DRM_FORMAT_ARGB8888,
				    LOCAL_DRM_FORMAT_MOD_NONE,
				    1.0, 1.0, 1.0,
				    &data->fb);

	igt_assert(fb_id);

	cr = igt_get_cairo_ctx(data->drm_fd, &data->fb);
	draw_cursor(cr, 0, 0, cur_w, cur_h, 1.0);
	igt_put_cairo_ctx(data->drm_fd, &data->fb, cr);
}

static bool has_nonsquare_cursors(data_t *data)
{
	uint32_t devid;

	if (!is_i915_device(data->drm_fd))
		return false;

	devid = intel_get_drm_devid(data->drm_fd);

	/*
	 * Test non-square cursors a bit on the platforms
	 * that support such things.
	 */
	if (devid == PCI_CHIP_845_G || devid == PCI_CHIP_I865_G)
		return true;

	if (IS_VALLEYVIEW(devid) || IS_CHERRYVIEW(devid))
		return false;

	return intel_gen(devid) >= 7;
}

static void test_cursor_size(data_t *data)
{
	igt_display_t *display = &data->display;
	igt_pipe_crc_t *pipe_crc = data->pipe_crc;
	igt_crc_t crc[10], ref_crc;
	cairo_t *cr;
	uint32_t fb_id;
	int i, size;
	int cursor_max_size = data->cursor_max_w;
	igt_plane_t *cursor =
		igt_output_get_plane_type(data->output, DRM_PLANE_TYPE_CURSOR);

	/* Create a maximum size cursor, then change the size in flight to
	 * smaller ones to see that the size is applied correctly
	 */
	fb_id = igt_create_fb(data->drm_fd, cursor_max_size, cursor_max_size,
			      DRM_FORMAT_ARGB8888, LOCAL_DRM_FORMAT_MOD_NONE,
			      &data->fb);
	igt_assert(fb_id);

	/* Use a solid white rectangle as the cursor */
	cr = igt_get_cairo_ctx(data->drm_fd, &data->fb);
	igt_paint_color_alpha(cr, 0, 0, cursor_max_size, cursor_max_size, 1.0, 1.0, 1.0, 1.0);
	igt_put_cairo_ctx(data->drm_fd, &data->fb, cr);

	/* Hardware test loop */
	cursor_enable(data);
	for (i = 0, size = cursor_max_size; size >= 64; size /= 2, i++) {
		/* Change size in flight: */
		igt_plane_set_size(cursor, size, size);
		igt_fb_set_size(&data->fb, cursor, size, size);
		igt_display_commit(display);
		igt_wait_for_vblank(data->drm_fd, data->pipe);
		igt_pipe_crc_collect_crc(pipe_crc, &crc[i]);
	}
	cursor_disable(data);
	igt_display_commit(display);
	igt_remove_fb(data->drm_fd, &data->fb);
	/* Software test loop */
	for (i = 0, size = cursor_max_size; size >= 64; size /= 2, i++) {
		/* Now render the same in software and collect crc */
		cr = igt_get_cairo_ctx(data->drm_fd, &data->primary_fb);
		igt_paint_color_alpha(cr, 0, 0, size, size, 1.0, 1.0, 1.0, 1.0);
		igt_put_cairo_ctx(data->drm_fd, &data->primary_fb, cr);

		igt_display_commit(display);
		igt_wait_for_vblank(data->drm_fd, data->pipe);
		igt_pipe_crc_collect_crc(pipe_crc, &ref_crc);
		/* Clear screen afterwards */
		cr = igt_get_cairo_ctx(data->drm_fd, &data->primary_fb);
		igt_paint_color(cr, 0, 0, data->screenw, data->screenh,
				0.0, 0.0, 0.0);
		igt_put_cairo_ctx(data->drm_fd, &data->primary_fb, cr);
		igt_assert_crc_equal(&crc[i], &ref_crc);
	}
}

static void test_rapid_movement(data_t *data)
{
	struct timeval start, end, delta;
	int x = 0, y = 0;
	long usec;
	igt_display_t *display = &data->display;
	igt_plane_t *cursor =
		igt_output_get_plane_type(data->output, DRM_PLANE_TYPE_CURSOR);

	cursor_enable(data);

	gettimeofday(&start, NULL);
	for ( ; x < 100; x++) {
		igt_plane_set_position(cursor, x, y);
		igt_display_commit(display);
	}
	for ( ; y < 100; y++) {
		igt_plane_set_position(cursor, x, y);
		igt_display_commit(display);
	}
	for ( ; x > 0; x--) {
		igt_plane_set_position(cursor, x, y);
		igt_display_commit(display);
	}
	for ( ; y > 0; y--) {
		igt_plane_set_position(cursor, x, y);
		igt_display_commit(display);
	}
	gettimeofday(&end, NULL);

	/*
	 * We've done 400 cursor updates now.  If we're being throttled to
	 * vblank, then that would take roughly 400/refresh seconds.  If the
	 * elapsed time is greater than 90% of that value, we'll consider it
	 * a failure (since cursor updates shouldn't be throttled).
	 */
	timersub(&end, &start, &delta);
	usec = delta.tv_usec + 1000000 * delta.tv_sec;
	igt_assert_lt(usec, 0.9 * 400 * 1000000 / data->refresh);
}

static void run_tests_on_pipe(data_t *data, enum pipe pipe)
{
	int cursor_size;

	igt_fixture {
		data->pipe = pipe;
		data->output = igt_get_single_output_for_pipe(&data->display, pipe);
		igt_require(data->output);
	}

	igt_subtest_f("pipe-%s-cursor-size-change", kmstest_pipe_name(pipe))
		run_test(data, test_cursor_size,
			 data->cursor_max_w, data->cursor_max_h);

	igt_subtest_f("pipe-%s-cursor-alpha-opaque", kmstest_pipe_name(pipe))
		run_test(data, test_cursor_opaque, data->cursor_max_w, data->cursor_max_h);

	igt_subtest_f("pipe-%s-cursor-alpha-transparent", kmstest_pipe_name(pipe))
		run_test(data, test_cursor_transparent, data->cursor_max_w, data->cursor_max_h);

	igt_fixture
		create_cursor_fb(data, data->cursor_max_w, data->cursor_max_h);

	igt_subtest_f("pipe-%s-cursor-dpms", kmstest_pipe_name(pipe)) {
		data->flags = TEST_DPMS;
		run_test(data, test_crc_random, data->cursor_max_w, data->cursor_max_h);
	}
	data->flags = 0;

	igt_subtest_f("pipe-%s-cursor-suspend", kmstest_pipe_name(pipe)) {
		data->flags = TEST_SUSPEND;
		run_test(data, test_crc_random, data->cursor_max_w, data->cursor_max_h);
	}
	data->flags = 0;

	igt_fixture
		igt_remove_fb(data->drm_fd, &data->fb);

	for (cursor_size = 64; cursor_size <= 512; cursor_size *= 2) {
		int w = cursor_size;
		int h = cursor_size;

		igt_fixture {
			igt_require(w <= data->cursor_max_w &&
				    h <= data->cursor_max_h);

			create_cursor_fb(data, w, h);
		}

		/* Using created cursor FBs to test cursor support */
		igt_subtest_f("pipe-%s-cursor-%dx%d-onscreen", kmstest_pipe_name(pipe), w, h)
			run_test(data, test_crc_onscreen, w, h);
		igt_subtest_f("pipe-%s-cursor-%dx%d-offscreen", kmstest_pipe_name(pipe), w, h)
			run_test(data, test_crc_offscreen, w, h);
		igt_subtest_f("pipe-%s-cursor-%dx%d-sliding", kmstest_pipe_name(pipe), w, h)
			run_test(data, test_crc_sliding, w, h);
		igt_subtest_f("pipe-%s-cursor-%dx%d-random", kmstest_pipe_name(pipe), w, h)
			run_test(data, test_crc_random, w, h);

		igt_subtest_f("pipe-%s-cursor-%dx%d-rapid-movement", kmstest_pipe_name(pipe), w, h) {
			run_test(data, test_rapid_movement, w, h);
		}

		igt_fixture
			igt_remove_fb(data->drm_fd, &data->fb);

		/*
		 * Test non-square cursors a bit on the platforms
		 * that support such things. And make it a bit more
		 * interesting by using a non-pot height.
		 */
		h /= 3;

		igt_fixture {
			if (has_nonsquare_cursors(data))
				create_cursor_fb(data, w, h);
		}

		/* Using created cursor FBs to test cursor support */
		igt_subtest_f("pipe-%s-cursor-%dx%d-onscreen", kmstest_pipe_name(pipe), w, h) {
			igt_require(has_nonsquare_cursors(data));
			run_test(data, test_crc_onscreen, w, h);
		}
		igt_subtest_f("pipe-%s-cursor-%dx%d-offscreen", kmstest_pipe_name(pipe), w, h) {
			igt_require(has_nonsquare_cursors(data));
			run_test(data, test_crc_offscreen, w, h);
		}
		igt_subtest_f("pipe-%s-cursor-%dx%d-sliding", kmstest_pipe_name(pipe), w, h) {
			igt_require(has_nonsquare_cursors(data));
			run_test(data, test_crc_sliding, w, h);
		}
		igt_subtest_f("pipe-%s-cursor-%dx%d-random", kmstest_pipe_name(pipe), w, h) {
			igt_require(has_nonsquare_cursors(data));
			run_test(data, test_crc_random, w, h);
		}

		igt_fixture
			igt_remove_fb(data->drm_fd, &data->fb);
	}
}

static data_t data;

igt_main
{
	uint64_t cursor_width = 64, cursor_height = 64;
	int ret;
	enum pipe pipe;

	igt_skip_on_simulation();

	igt_fixture {
		data.drm_fd = drm_open_driver_master(DRIVER_ANY);

		ret = drmGetCap(data.drm_fd, DRM_CAP_CURSOR_WIDTH, &cursor_width);
		igt_assert(ret == 0 || errno == EINVAL);
		/* Not making use of cursor_height since it is same as width, still reading */
		ret = drmGetCap(data.drm_fd, DRM_CAP_CURSOR_HEIGHT, &cursor_height);
		igt_assert(ret == 0 || errno == EINVAL);

		/* We assume width and height are same so max is assigned width */
		igt_assert_eq(cursor_width, cursor_height);

		kmstest_set_vt_graphics_mode();

		igt_require_pipe_crc(data.drm_fd);

		igt_display_require(&data.display, data.drm_fd);
	}

	data.cursor_max_w = cursor_width;
	data.cursor_max_h = cursor_height;

	for_each_pipe_static(pipe)
		igt_subtest_group
			run_tests_on_pipe(&data, pipe);

	igt_fixture {
		igt_display_fini(&data.display);
	}
}
