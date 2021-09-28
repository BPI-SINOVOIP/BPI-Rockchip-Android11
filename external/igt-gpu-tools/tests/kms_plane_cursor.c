/*
 * Copyright 2019 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "igt.h"

/*
 * This file tests cursor interactions with primary and overlay planes.
 *
 * Some assumptions made:
 * - Cursor planes can be placed on top of overlay planes
 * - DRM index indicates z-ordering, higher index = higher z-order
 */

typedef struct {
	int x;
	int y;
} pos_t;

typedef struct {
	int x;
	int y;
	int w;
	int h;
} rect_t;

/* Common test data. */
typedef struct data {
	igt_display_t display;
	igt_plane_t *primary;
	igt_plane_t *overlay;
	igt_plane_t *cursor;
	igt_output_t *output;
	igt_pipe_t *pipe;
	igt_pipe_crc_t *pipe_crc;
	drmModeModeInfo *mode;
	enum pipe pipe_id;
	int drm_fd;
	rect_t or;
} data_t;

/* Common test setup. */
static void test_init(data_t *data, enum pipe pipe_id)
{
	igt_display_t *display = &data->display;

	data->pipe_id = pipe_id;
	data->pipe = &data->display.pipes[data->pipe_id];

	igt_display_reset(display);

	data->output = igt_get_single_output_for_pipe(&data->display, pipe_id);
	igt_require(data->output);

	data->mode = igt_output_get_mode(data->output);

	data->primary = igt_pipe_get_plane_type(data->pipe, DRM_PLANE_TYPE_PRIMARY);
	data->overlay = igt_pipe_get_plane_type(data->pipe, DRM_PLANE_TYPE_OVERLAY);
	data->cursor = igt_pipe_get_plane_type(data->pipe, DRM_PLANE_TYPE_CURSOR);

	data->pipe_crc = igt_pipe_crc_new(data->drm_fd, data->pipe_id,
					  INTEL_PIPE_CRC_SOURCE_AUTO);

	igt_output_set_pipe(data->output, data->pipe_id);

	/* Overlay rectangle for a rect in the center of the screen */
	data->or.x = data->mode->hdisplay / 4;
	data->or.y = data->mode->vdisplay / 4;
	data->or.w = data->mode->hdisplay / 2;
	data->or.h = data->mode->vdisplay / 2;
}

/* Common test cleanup. */
static void test_fini(data_t *data)
{
	igt_pipe_crc_free(data->pipe_crc);
	igt_display_reset(&data->display);
}

/* Fills a FB with the solid color given. */
static void draw_color(igt_fb_t *fb, double r, double g, double b)
{
	cairo_t *cr = igt_get_cairo_ctx(fb->fd, fb);

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	igt_paint_color(cr, 0, 0, fb->width, fb->height, r, g, b);
	igt_put_cairo_ctx(fb->fd, fb, cr);
}

/*
 * Draws white and gray (if overlay FB is given) on the primary FB.
 * Draws a magenta square where the cursor should be over top both planes.
 * Takes this as the reference CRC.
 *
 * Draws white on the primary FB and gray on the overlay FB if given.
 * Places the cursor where the magenta square should be with a magenta FB.
 * Takes this as the test CRC and compares it to the reference.
 */
static void test_cursor_pos(data_t *data, igt_fb_t *pfb, igt_fb_t *ofb,
			    igt_fb_t *cfb, const rect_t *or, int x, int y)
{
	igt_crc_t ref_crc, test_crc;
	cairo_t *cr;
	int cw = cfb->width;
	int ch = cfb->height;

	cr = igt_get_cairo_ctx(pfb->fd, pfb);
	igt_paint_color(cr, 0, 0, pfb->width, pfb->height, 1.0, 1.0, 1.0);

	if (ofb)
		igt_paint_color(cr, or->x, or->y, or->w, or->h, 0.5, 0.5, 0.5);

	igt_paint_color(cr, x, y, cw, ch, 1.0, 0.0, 1.0);
	igt_put_cairo_ctx(pfb->fd, pfb, cr);

	igt_plane_set_fb(data->overlay, NULL);
	igt_plane_set_fb(data->cursor, NULL);
	igt_display_commit_atomic(&data->display, 0, NULL);

	igt_pipe_crc_start(data->pipe_crc);
	igt_pipe_crc_get_current(data->drm_fd, data->pipe_crc, &ref_crc);

	draw_color(pfb, 1.0, 1.0, 1.0);

	if (ofb) {
		igt_plane_set_fb(data->overlay, ofb);
		igt_plane_set_position(data->overlay, or->x, or->y);
		igt_plane_set_size(data->overlay, or->w, or->h);
		igt_fb_set_size(ofb, data->overlay, or->w, or->h);
		igt_fb_set_position(ofb, data->overlay,
				    (ofb->width - or->w) / 2,
				    (ofb->height - or->h) / 2);
	}

	igt_plane_set_fb(data->cursor, cfb);
	igt_plane_set_position(data->cursor, x, y);
	igt_display_commit_atomic(&data->display, 0, NULL);

	igt_pipe_crc_get_current(data->drm_fd, data->pipe_crc, &test_crc);
	igt_pipe_crc_stop(data->pipe_crc);

	igt_assert_crc_equal(&ref_crc, &test_crc);
}

/*
 * Tests the cursor on a variety of positions on the screen.
 * Specific edge cases that should be captured here are the negative edges
 * of each plane and the centers.
 */
static void test_cursor_spots(data_t *data, igt_fb_t *pfb, igt_fb_t *ofb,
			      igt_fb_t *cfb, const rect_t *or, int size)
{
	int sw = data->mode->hdisplay;
	int sh = data->mode->vdisplay;
	int i;
	const pos_t pos[] = {
		/* Test diagonally from top left to bottom right. */
		{ -size / 3, -size / 3 },
		{ 0, 0 },
		{ or->x - size, or->y - size },
		{ or->x - size / 3, or->y - size / 3 },
		{ or->x, or->y },
		{ or->x + size, or->y + size },
		{ sw / 2, sh / 2 },
		{ or->x + or->w - size, or->y + or->h - size },
		{ or->x + or->w - size / 3, or->y + or->h - size / 3 },
		{ or->x + or->w + size, or->y + or->h + size },
		{ sw - size, sh - size },
		{ sw - size / 3, sh - size / 3 },
		/* Test remaining corners. */
		{ sw - size, 0 },
		{ 0, sh - size },
		{ or->x + or->w - size, or->y },
		{ or->x, or->y + or->h - size }
	};

	for (i = 0; i < ARRAY_SIZE(pos); ++i) {
		test_cursor_pos(data, pfb, ofb, cfb, or, pos[i].x, pos[i].y);
	}
}

/*
 * Tests atomic cursor positioning on a primary and overlay plane.
 * Assumes the cursor can be placed on top of the overlay.
 */
static void test_cursor_overlay(data_t *data, int size, enum pipe pipe_id)
{
	igt_fb_t pfb, ofb, cfb;
	int sw, sh;

	test_init(data, pipe_id);
	igt_require(data->overlay);

	sw = data->mode->hdisplay;
	sh = data->mode->vdisplay;

	igt_create_color_fb(data->drm_fd, sw, sh, DRM_FORMAT_XRGB8888, 0,
			    1.0, 1.0, 1.0, &pfb);

	igt_create_color_fb(data->drm_fd, data->or.w, data->or.h,
			    DRM_FORMAT_XRGB8888, 0, 0.5, 0.5, 0.5, &ofb);

	igt_create_color_fb(data->drm_fd, size, size, DRM_FORMAT_ARGB8888, 0,
			    1.0, 0.0, 1.0, &cfb);

	igt_plane_set_fb(data->primary, &pfb);
	igt_display_commit2(&data->display, COMMIT_ATOMIC);

	test_cursor_spots(data, &pfb, &ofb, &cfb, &data->or, size);

	test_fini(data);

	igt_remove_fb(data->drm_fd, &cfb);
	igt_remove_fb(data->drm_fd, &ofb);
	igt_remove_fb(data->drm_fd, &pfb);
}

/* Tests atomic cursor positioning on a primary plane. */
static void test_cursor_primary(data_t *data, int size, enum pipe pipe_id)
{
	igt_fb_t pfb, cfb;
	int sw, sh;

	test_init(data, pipe_id);

	sw = data->mode->hdisplay;
	sh = data->mode->vdisplay;

	igt_create_color_fb(data->drm_fd, sw, sh, DRM_FORMAT_XRGB8888, 0,
			    1.0, 1.0, 1.0, &pfb);

	igt_create_color_fb(data->drm_fd, size, size, DRM_FORMAT_ARGB8888, 0,
			    1.0, 0.0, 1.0, &cfb);

	igt_plane_set_fb(data->primary, &pfb);
	igt_display_commit2(&data->display, COMMIT_ATOMIC);

	test_cursor_spots(data, &pfb, NULL, &cfb, &data->or, size);

	test_fini(data);

	igt_remove_fb(data->drm_fd, &cfb);
	igt_remove_fb(data->drm_fd, &pfb);
}

/*
 * Tests atomic cursor positioning on a primary and overlay plane.
 * The overlay's buffer is larger than the viewport actually used
 * for display.
 */
static void test_cursor_viewport(data_t *data, int size, enum pipe pipe_id)
{
	igt_fb_t pfb, ofb, cfb;
	int sw, sh;
	int pad = 128;

	test_init(data, pipe_id);
	igt_require(data->overlay);

	sw = data->mode->hdisplay;
	sh = data->mode->vdisplay;

	igt_create_color_fb(data->drm_fd, sw, sh, DRM_FORMAT_XRGB8888, 0,
			    1.0, 1.0, 1.0, &pfb);

	igt_create_color_fb(data->drm_fd, data->or.w + pad, data->or.h + pad,
			    DRM_FORMAT_XRGB8888, 0, 0.5, 0.5, 0.5, &ofb);

	igt_create_color_fb(data->drm_fd, size, size, DRM_FORMAT_ARGB8888, 0,
			    1.0, 0.0, 1.0, &cfb);

	igt_plane_set_fb(data->primary, &pfb);
	igt_display_commit2(&data->display, COMMIT_ATOMIC);

	test_cursor_spots(data, &pfb, &ofb, &cfb, &data->or, size);

	test_fini(data);

	igt_remove_fb(data->drm_fd, &cfb);
	igt_remove_fb(data->drm_fd, &ofb);
	igt_remove_fb(data->drm_fd, &pfb);
}

igt_main
{
	static const int cursor_sizes[] = { 64, 128, 256 };
	data_t data = { 0 };
	enum pipe pipe;
	int i;

	igt_skip_on_simulation();

	igt_fixture {
		data.drm_fd = drm_open_driver_master(DRIVER_ANY);

		kmstest_set_vt_graphics_mode();

		igt_display_require(&data.display, data.drm_fd);
		igt_require(data.display.is_atomic);
		igt_display_require_output(&data.display);
	}

	for_each_pipe_static(pipe)
		for (i = 0; i < ARRAY_SIZE(cursor_sizes); ++i) {
			int size = cursor_sizes[i];

			igt_subtest_f("pipe-%s-overlay-size-%d",
				      kmstest_pipe_name(pipe), size)
				test_cursor_overlay(&data, size, pipe);

			igt_subtest_f("pipe-%s-primary-size-%d",
				      kmstest_pipe_name(pipe), size)
				test_cursor_primary(&data, size, pipe);

			igt_subtest_f("pipe-%s-viewport-size-%d",
				      kmstest_pipe_name(pipe), size)
				test_cursor_viewport(&data, size, pipe);
		}

	igt_fixture {
		igt_display_fini(&data.display);
	}
}
