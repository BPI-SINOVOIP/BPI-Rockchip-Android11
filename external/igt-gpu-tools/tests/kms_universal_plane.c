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
 */

#include "igt.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>


typedef struct {
	int drm_fd;
	igt_display_t display;
	int gen;
} data_t;

typedef struct {
	data_t *data;
	igt_pipe_crc_t *pipe_crc;
	igt_crc_t crc_1, crc_2, crc_3, crc_4, crc_5, crc_6, crc_7, crc_8,
		  crc_9, crc_10;
	struct igt_fb red_fb, blue_fb, black_fb, yellow_fb;
	drmModeModeInfo *mode;
} functional_test_t;

typedef struct {
	data_t *data;
	drmModeResPtr moderes;
	struct igt_fb blue_fb, oversized_fb, undersized_fb;
} sanity_test_t;

typedef struct {
	data_t *data;
	struct igt_fb red_fb, blue_fb;
} pageflip_test_t;

typedef struct {
	data_t *data;
	int x, y;
	int w, h;
	struct igt_fb biggreen_fb, smallred_fb, smallblue_fb;
} gen9_test_t;

static void
functional_test_init(functional_test_t *test, igt_output_t *output, enum pipe pipe)
{
	data_t *data = test->data;
	drmModeModeInfo *mode;

	test->pipe_crc = igt_pipe_crc_new(data->drm_fd, pipe, INTEL_PIPE_CRC_SOURCE_AUTO);

	igt_output_set_pipe(output, pipe);

	mode = igt_output_get_mode(output);
	igt_create_color_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
				DRM_FORMAT_XRGB8888,
				LOCAL_DRM_FORMAT_MOD_NONE,
				0.0, 0.0, 0.0,
				&test->black_fb);
	igt_create_color_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
				DRM_FORMAT_XRGB8888,
				LOCAL_DRM_FORMAT_MOD_NONE,
				0.0, 0.0, 1.0,
				&test->blue_fb);
	igt_create_color_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
				DRM_FORMAT_XRGB8888,
				LOCAL_DRM_FORMAT_MOD_NONE,
				1.0, 1.0, 0.0,
				&test->yellow_fb);
	igt_create_color_fb(data->drm_fd, 100, 100,
				DRM_FORMAT_XRGB8888,
				LOCAL_DRM_FORMAT_MOD_NONE,
				1.0, 0.0, 0.0,
				&test->red_fb);

	test->mode = mode;
}

static void
functional_test_fini(functional_test_t *test, igt_output_t *output)
{
	igt_pipe_crc_free(test->pipe_crc);

	igt_remove_fb(test->data->drm_fd, &test->black_fb);
	igt_remove_fb(test->data->drm_fd, &test->blue_fb);
	igt_remove_fb(test->data->drm_fd, &test->red_fb);
	igt_remove_fb(test->data->drm_fd, &test->yellow_fb);

	igt_output_set_pipe(output, PIPE_ANY);
	igt_display_commit2(&test->data->display, COMMIT_LEGACY);
}

/*
 * Universal plane functional testing.
 *   - Black primary plane via traditional interfaces, red sprite, grab CRC:1.
 *   - Blue primary plane via traditional interfaces, red sprite, grab CRC:2.
 *   - Yellow primary via traditional interfaces
 *   - Blue primary plane, red sprite via universal planes, grab CRC:3 and compare
 *     with CRC:2 (should be the same)
 *   - Disable primary plane, grab CRC:4 (should be same as CRC:1)
 *   - Reenable primary, grab CRC:5 (should be same as CRC:2 and CRC:3)
 *   - Yellow primary, no sprite
 *   - Disable CRTC
 *   - Program red sprite (while CRTC off)
 *   - Program blue primary (while CRTC off)
 *   - Enable CRTC, grab CRC:6 (should be same as CRC:2)
 */
static void
functional_test_pipe(data_t *data, enum pipe pipe, igt_output_t *output)
{
	functional_test_t test = { .data = data };
	igt_display_t *display = &data->display;
	igt_plane_t *primary, *sprite;
	int num_primary = 0, num_cursor = 0;
	int i;

	igt_skip_on(pipe >= display->n_pipes);

	igt_info("Testing connector %s using pipe %s\n", igt_output_name(output),
		 kmstest_pipe_name(pipe));

	functional_test_init(&test, output, pipe);

	/*
	 * Make sure we have no more than one primary or cursor plane per crtc.
	 * If the kernel accidentally calls drm_plane_init() rather than
	 * drm_universal_plane_init(), the type enum can get interpreted as a
	 * boolean and show up in userspace as the wrong type.
	 */
	for (i = 0; i < display->pipes[pipe].n_planes; i++)
		if (display->pipes[pipe].planes[i].type == DRM_PLANE_TYPE_PRIMARY)
			num_primary++;
		else if (display->pipes[pipe].planes[i].type == DRM_PLANE_TYPE_CURSOR)
			num_cursor++;

	igt_assert_eq(num_primary, 1);
	igt_assert_lte(num_cursor, 1);

	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);
	sprite = igt_output_get_plane_type(output, DRM_PLANE_TYPE_OVERLAY);
	if (!sprite) {
		functional_test_fini(&test, output);
		igt_skip("No sprite plane available\n");
	}

	igt_plane_set_position(sprite, 100, 100);

	/* Step 1: Legacy API's, black primary, red sprite (CRC 1) */
	igt_plane_set_fb(primary, &test.black_fb);
	igt_plane_set_fb(sprite, &test.red_fb);
	igt_display_commit2(display, COMMIT_LEGACY);
	igt_pipe_crc_collect_crc(test.pipe_crc, &test.crc_1);

	/* Step 2: Legacy API', blue primary, red sprite (CRC 2) */
	igt_plane_set_fb(primary, &test.blue_fb);
	igt_plane_set_fb(sprite, &test.red_fb);
	igt_display_commit2(display, COMMIT_LEGACY);
	igt_pipe_crc_collect_crc(test.pipe_crc, &test.crc_2);

	/* Step 3: Legacy API's, yellow primary (CRC 3) */
	igt_plane_set_fb(primary, &test.yellow_fb);
	igt_display_commit2(display, COMMIT_LEGACY);
	igt_pipe_crc_collect_crc(test.pipe_crc, &test.crc_3);

	/* Step 4: Universal API's, blue primary, red sprite (CRC 4) */
	igt_plane_set_fb(primary, &test.blue_fb);
	igt_plane_set_fb(sprite, &test.red_fb);
	igt_display_commit2(display, COMMIT_UNIVERSAL);
	igt_pipe_crc_collect_crc(test.pipe_crc, &test.crc_4);

	/* Step 5: Universal API's, disable primary plane (CRC 5) */
	igt_plane_set_fb(primary, NULL);
	igt_display_commit2(display, COMMIT_UNIVERSAL);
	igt_pipe_crc_collect_crc(test.pipe_crc, &test.crc_5);

	/* Step 6: Universal API's, re-enable primary with blue (CRC 6) */
	igt_plane_set_fb(primary, &test.blue_fb);
	igt_display_commit2(display, COMMIT_UNIVERSAL);
	igt_pipe_crc_collect_crc(test.pipe_crc, &test.crc_6);

	/* Step 7: Legacy API's, yellow primary, no sprite */
	igt_plane_set_fb(primary, &test.yellow_fb);
	igt_plane_set_fb(sprite, NULL);
	igt_display_commit2(display, COMMIT_LEGACY);

	/* Step 8: Disable CRTC */
	igt_plane_set_fb(primary, NULL);
	igt_display_commit2(display, COMMIT_LEGACY);

	/* Step 9: Universal API's with crtc off:
	 *  - red sprite
	 *  - multiple primary fb's, ending in blue
	 */
	igt_plane_set_fb(sprite, &test.red_fb);
	igt_display_commit2(display, COMMIT_UNIVERSAL);
	igt_plane_set_fb(primary, &test.yellow_fb);
	igt_display_commit2(display, COMMIT_UNIVERSAL);
	igt_plane_set_fb(primary, &test.black_fb);
	igt_display_commit2(display, COMMIT_UNIVERSAL);
	igt_plane_set_fb(primary, &test.blue_fb);
	igt_display_commit2(display, COMMIT_UNIVERSAL);

	/* Step 10: Enable crtc (fb = -1), take CRC (CRC 7) */
	igt_assert(drmModeSetCrtc(data->drm_fd, output->config.crtc->crtc_id, -1,
				  0, 0, &output->config.connector->connector_id,
				  1, test.mode) == 0);
	igt_pipe_crc_collect_crc(test.pipe_crc, &test.crc_7);

	/* Step 11: Disable primary plane */
	igt_plane_set_fb(primary, NULL);
	igt_display_commit2(display, COMMIT_UNIVERSAL);

	/* Step 12: Legacy modeset to yellow FB (CRC 8) */
	igt_plane_set_fb(primary, &test.yellow_fb);
	igt_display_commit2(display, COMMIT_LEGACY);
	igt_pipe_crc_collect_crc(test.pipe_crc, &test.crc_8);

	/* Step 13: Legacy API', blue primary, red sprite */
	igt_plane_set_fb(primary, &test.blue_fb);
	igt_plane_set_fb(sprite, &test.red_fb);
	igt_display_commit2(display, COMMIT_LEGACY);

	/* Step 14: Universal API, set primary completely offscreen (CRC 9) */
	igt_assert(drmModeSetPlane(data->drm_fd, primary->drm_plane->plane_id,
				   output->config.crtc->crtc_id,
				   test.blue_fb.fb_id, 0,
				   9000, 9000,
				   test.mode->hdisplay,
				   test.mode->vdisplay,
				   IGT_FIXED(0,0), IGT_FIXED(0,0),
				   IGT_FIXED(test.mode->hdisplay,0),
				   IGT_FIXED(test.mode->vdisplay,0)) == 0);
	igt_pipe_crc_collect_crc(test.pipe_crc, &test.crc_9);

	/*
	 * Step 15: Explicitly disable primary after it's already been
	 * implicitly disabled (CRC 10).
	 */
	igt_plane_set_fb(primary, NULL);
	igt_display_commit2(display, COMMIT_UNIVERSAL);
	igt_pipe_crc_collect_crc(test.pipe_crc, &test.crc_10);

	/* Step 16: Legacy API's, blue primary, red sprite */
	igt_plane_set_fb(primary, &test.blue_fb);
	igt_plane_set_fb(sprite, &test.red_fb);
	igt_display_commit2(display, COMMIT_LEGACY);

	/* Blue bg + red sprite should be same under both types of API's */
	igt_assert_crc_equal(&test.crc_2, &test.crc_4);

	/* Disabling primary plane should be same as black primary */
	igt_assert_crc_equal(&test.crc_1, &test.crc_5);

	/* Re-enabling primary should return to blue properly */
	igt_assert_crc_equal(&test.crc_2, &test.crc_6);

	/*
	 * We should be able to setup plane FB's while CRTC is disabled and
	 * then have them pop up correctly when the CRTC is re-enabled.
	 */
	igt_assert_crc_equal(&test.crc_2, &test.crc_7);

	/*
	 * We should be able to modeset with the primary plane off
	 * successfully
	 */
	igt_assert_crc_equal(&test.crc_3, &test.crc_8);

	/*
	 * We should be able to move the primary plane completely offscreen
	 * and have it disable successfully.
	 */
	igt_assert_crc_equal(&test.crc_5, &test.crc_9);

	/*
	 * We should be able to explicitly disable an already
	 * implicitly-disabled primary plane
	 */
	igt_assert_crc_equal(&test.crc_5, &test.crc_10);

	igt_plane_set_fb(primary, NULL);
	igt_plane_set_fb(sprite, NULL);

	functional_test_fini(&test, output);
}

static void
sanity_test_init(sanity_test_t *test, igt_output_t *output, enum pipe pipe)
{
	data_t *data = test->data;
	drmModeModeInfo *mode;

	igt_output_set_pipe(output, pipe);

	mode = igt_output_get_mode(output);
	igt_create_color_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
			    DRM_FORMAT_XRGB8888,
			    LOCAL_DRM_FORMAT_MOD_NONE,
			    0.0, 0.0, 1.0,
			    &test->blue_fb);
	igt_create_color_fb(data->drm_fd,
			    mode->hdisplay + 100, mode->vdisplay + 100,
			    DRM_FORMAT_XRGB8888,
			    LOCAL_DRM_FORMAT_MOD_NONE,
			    0.0, 0.0, 1.0,
			    &test->oversized_fb);
	igt_create_color_fb(data->drm_fd,
			    mode->hdisplay - 100, mode->vdisplay - 100,
			    DRM_FORMAT_XRGB8888,
			    LOCAL_DRM_FORMAT_MOD_NONE,
			    0.0, 0.0, 1.0,
			    &test->undersized_fb);

	test->moderes = drmModeGetResources(data->drm_fd);
	igt_assert(test->moderes);
}

static void
sanity_test_fini(sanity_test_t *test, igt_output_t *output)
{
	drmModeFreeResources(test->moderes);

	igt_remove_fb(test->data->drm_fd, &test->oversized_fb);
	igt_remove_fb(test->data->drm_fd, &test->undersized_fb);
	igt_remove_fb(test->data->drm_fd, &test->blue_fb);

	igt_output_set_pipe(output, PIPE_ANY);
	igt_display_commit2(&test->data->display, COMMIT_LEGACY);
}

/*
 * Universal plane sanity testing.
 *   - Primary doesn't cover CRTC
 *   - Primary plane tries to scale down
 *   - Primary plane tries to scale up
 */
static void
sanity_test_pipe(data_t *data, enum pipe pipe, igt_output_t *output)
{
	sanity_test_t test = { .data = data };
	igt_plane_t *primary;
	drmModeModeInfo *mode;
	int i;
	int expect;

	igt_skip_on(pipe >= data->display.n_pipes);

	igt_output_set_pipe(output, pipe);
	mode = igt_output_get_mode(output);

	sanity_test_init(&test, output, pipe);

	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);

	/* Use legacy API to set a mode with a blue FB */
	igt_plane_set_fb(primary, &test.blue_fb);
	igt_display_commit2(&data->display, COMMIT_LEGACY);

	/*
	 * Try to use universal plane API to set primary plane that
	 * doesn't cover CRTC (should fail on pre-gen9 and succeed on
	 * gen9+).
	 */
	igt_plane_set_fb(primary, &test.undersized_fb);
	expect = (data->gen < 9) ? -EINVAL : 0;
	igt_assert(igt_display_try_commit2(&data->display, COMMIT_UNIVERSAL) == expect);

	/* Same as above, but different plane positioning. */
	igt_plane_set_position(primary, 100, 100);
	igt_assert(igt_display_try_commit2(&data->display, COMMIT_UNIVERSAL) == expect);

	igt_plane_set_position(primary, 0, 0);

	/* Try to use universal plane API to scale down (should fail on pre-gen9) */
	expect = (data->gen < 9) ? -ERANGE : 0;
	igt_assert(drmModeSetPlane(data->drm_fd, primary->drm_plane->plane_id,
				   output->config.crtc->crtc_id,
				   test.oversized_fb.fb_id, 0,
				   0, 0,
				   mode->hdisplay + 100,
				   mode->vdisplay + 100,
				   IGT_FIXED(0,0), IGT_FIXED(0,0),
				   IGT_FIXED(mode->hdisplay,0),
				   IGT_FIXED(mode->vdisplay,0)) == expect);

	/* Try to use universal plane API to scale up (should fail on pre-gen9) */
	igt_assert(drmModeSetPlane(data->drm_fd, primary->drm_plane->plane_id,
				   output->config.crtc->crtc_id,
				   test.oversized_fb.fb_id, 0,
				   0, 0,
				   mode->hdisplay,
				   mode->vdisplay,
				   IGT_FIXED(0,0), IGT_FIXED(0,0),
				   IGT_FIXED(mode->hdisplay - 100,0),
				   IGT_FIXED(mode->vdisplay - 100,0)) == expect);

	/* Find other crtcs and try to program our primary plane on them */
	for (i = 0; i < test.moderes->count_crtcs; i++)
		if (test.moderes->crtcs[i] != output->config.crtc->crtc_id) {
			igt_assert(drmModeSetPlane(data->drm_fd,
						   primary->drm_plane->plane_id,
						   test.moderes->crtcs[i],
						   test.blue_fb.fb_id, 0,
						   0, 0,
						   mode->hdisplay,
						   mode->vdisplay,
						   IGT_FIXED(0,0), IGT_FIXED(0,0),
						   IGT_FIXED(mode->hdisplay,0),
						   IGT_FIXED(mode->vdisplay,0)) == -EINVAL);
		}

	igt_plane_set_fb(primary, NULL);
	sanity_test_fini(&test, output);
}

static void
pageflip_test_init(pageflip_test_t *test, igt_output_t *output, enum pipe pipe)
{
	data_t *data = test->data;
	drmModeModeInfo *mode;

	igt_output_set_pipe(output, pipe);

	mode = igt_output_get_mode(output);
	igt_create_color_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
			    DRM_FORMAT_XRGB8888,
			    LOCAL_DRM_FORMAT_MOD_NONE,
			    1.0, 0.0, 0.0,
			    &test->red_fb);
	igt_create_color_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
			    DRM_FORMAT_XRGB8888,
			    LOCAL_DRM_FORMAT_MOD_NONE,
			    0.0, 0.0, 1.0,
			    &test->blue_fb);
}

static void
pageflip_test_fini(pageflip_test_t *test, igt_output_t *output)
{
	igt_remove_fb(test->data->drm_fd, &test->red_fb);
	igt_remove_fb(test->data->drm_fd, &test->blue_fb);

	igt_output_set_pipe(output, PIPE_ANY);
	igt_display_commit2(&test->data->display, COMMIT_LEGACY);
}

static void
pageflip_test_pipe(data_t *data, enum pipe pipe, igt_output_t *output)
{
	pageflip_test_t test = { .data = data };
	igt_plane_t *primary;
	struct timeval timeout = { .tv_sec = 0, .tv_usec = 500 };
	drmEventContext evctx = { .version = 2 };

	fd_set fds;
	int ret = 0;

	igt_skip_on(pipe >= data->display.n_pipes);

	igt_output_set_pipe(output, pipe);

	pageflip_test_init(&test, output, pipe);

	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);

	/* Use legacy API to set a mode with a blue FB */
	igt_plane_set_fb(primary, &test.blue_fb);
	igt_display_commit2(&data->display, COMMIT_LEGACY);

	/* Disable the primary plane */
	igt_plane_set_fb(primary, NULL);
	igt_display_commit2(&data->display, COMMIT_UNIVERSAL);

	/*
	 * Issue a pageflip to red FB
	 *
	 * Note that crtc->primary->fb = NULL causes flip to return EBUSY for
	 * historical reasons...
	 */
	igt_assert(drmModePageFlip(data->drm_fd, output->config.crtc->crtc_id,
				   test.red_fb.fb_id, 0, NULL) == -EBUSY);

	/* Turn primary plane back on */
	igt_plane_set_fb(primary, &test.blue_fb);
	igt_display_commit2(&data->display, COMMIT_UNIVERSAL);

	/*
	 * Issue a pageflip to red, then immediately try to disable the primary
	 * plane, hopefully before the pageflip has a chance to complete.  The
	 * plane disable operation should wind up blocking while the pageflip
	 * completes, which we don't have a good way to specifically test for,
	 * but at least we can make sure that nothing blows up.
	 */
	igt_assert(drmModePageFlip(data->drm_fd, output->config.crtc->crtc_id,
				   test.red_fb.fb_id, DRM_MODE_PAGE_FLIP_EVENT,
				   &test) == 0);
	igt_plane_set_fb(primary, NULL);
	igt_display_commit2(&data->display, COMMIT_UNIVERSAL);

	/* Wait for pageflip completion, then consume event on fd */
	FD_ZERO(&fds);
	FD_SET(data->drm_fd, &fds);
	do {
		ret = select(data->drm_fd + 1, &fds, NULL, NULL, &timeout);
	} while (ret < 0 && errno == EINTR);
	igt_assert_eq(ret, 1);
	igt_assert(drmHandleEvent(data->drm_fd, &evctx) == 0);

	igt_plane_set_fb(primary, NULL);
	pageflip_test_fini(&test, output);
}

static void
cursor_leak_test_fini(data_t *data,
		      igt_output_t *output,
		      struct igt_fb *bg,
		      struct igt_fb *curs)
{
	int i;

	igt_remove_fb(data->drm_fd, bg);
	for (i = 0; i < 10; i++)
		igt_remove_fb(data->drm_fd, &curs[i]);

	igt_output_set_pipe(output, PIPE_ANY);
}

static int
i915_gem_fb_count(data_t *data)
{
	char buf[1024];
	FILE *fp;
	int fd;
	int count = 0;

	fd = igt_debugfs_open(data->drm_fd, "i915_gem_framebuffer", O_RDONLY);
	fp = fdopen(fd, "r");
	igt_require(fp);
	while (fgets(buf, sizeof(buf), fp) != NULL)
		count++;
	fclose(fp);
	close(fd);

	return count;
}

static void
cursor_leak_test_pipe(data_t *data, enum pipe pipe, igt_output_t *output)
{
	igt_display_t *display = &data->display;
	igt_plane_t *primary, *cursor;
	drmModeModeInfo *mode;
	struct igt_fb background_fb;
	struct igt_fb cursor_fb[10];
	int i;
	int r, g, b;
	int count1, count2;

	igt_skip_on(pipe >= display->n_pipes);
	igt_require(display->has_cursor_plane);

	igt_output_set_pipe(output, pipe);
	mode = igt_output_get_mode(output);

	/* Count GEM framebuffers before creating our cursor FB's */
	count1 = i915_gem_fb_count(data);

	/* Black background FB */
	igt_create_color_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
			    DRM_FORMAT_XRGB8888,
			    false,
			    0.0, 0.0, 0.0,
			    &background_fb);

	/* Random color cursors */
	for (i = 0; i < 10; i++) {
		r = rand() % 0xFF;
		g = rand() % 0xFF;
		b = rand() % 0xFF;
		igt_create_color_fb(data->drm_fd, 64, 64,
				    DRM_FORMAT_ARGB8888,
				    false,
				    (double)r / 0xFF,
				    (double)g / 0xFF,
				    (double)b / 0xFF,
				    &cursor_fb[i]);
	}

	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);
	cursor = igt_output_get_plane_type(output, DRM_PLANE_TYPE_CURSOR);
	if (!primary || !cursor) {
		cursor_leak_test_fini(data, output, &background_fb, cursor_fb);
		igt_skip("Primary and/or cursor are unavailable\n");
	}


	igt_plane_set_fb(primary, &background_fb);
	igt_display_commit2(display, COMMIT_LEGACY);

	igt_plane_set_position(cursor, 100, 100);

	/*
	 * Exercise both legacy and universal code paths.  Note that legacy
	 * handling in the kernel redirects through universal codepaths
	 * internally, so that redirection is where we're most worried about
	 * leaking.
	 */
	for (i = 0; i < 10; i++) {
		igt_plane_set_fb(cursor, &cursor_fb[i]);
		igt_display_commit2(display, COMMIT_UNIVERSAL);
	}
	for (i = 0; i < 10; i++) {
		igt_plane_set_fb(cursor, &cursor_fb[i]);
		igt_display_commit2(display, COMMIT_LEGACY);
	}

	/* Release our framebuffer handles before we take a second count */
	igt_plane_set_fb(primary, NULL);
	igt_plane_set_fb(cursor, NULL);
	igt_display_commit2(display, COMMIT_LEGACY);
	cursor_leak_test_fini(data, output, &background_fb, cursor_fb);

	/* We should be back to the same framebuffer count as when we started */
	count2 = i915_gem_fb_count(data);

	igt_assert_eq(count1, count2);
}

static void
gen9_test_init(gen9_test_t *test, igt_output_t *output, enum pipe pipe)
{
	data_t *data = test->data;
	drmModeModeInfo *mode;

	igt_output_set_pipe(output, pipe);

	mode = igt_output_get_mode(output);
	test->w = mode->hdisplay / 2;
	test->h = mode->vdisplay / 2;
	test->x = mode->hdisplay / 4;
	test->y = mode->vdisplay / 4;

	/* Initial framebuffer of full CRTC size */
	igt_create_color_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
			    DRM_FORMAT_XRGB8888,
			    LOCAL_DRM_FORMAT_MOD_NONE,
			    0.0, 1.0, 0.0,
			    &test->biggreen_fb);

	/* Framebuffers that only cover a quarter of the CRTC size */
	igt_create_color_fb(data->drm_fd, test->w, test->h,
			    DRM_FORMAT_XRGB8888,
			    LOCAL_DRM_FORMAT_MOD_NONE,
			    1.0, 0.0, 0.0,
			    &test->smallred_fb);
	igt_create_color_fb(data->drm_fd, test->w, test->h,
			    DRM_FORMAT_XRGB8888,
			    LOCAL_DRM_FORMAT_MOD_NONE,
			    0.0, 0.0, 1.0,
			    &test->smallblue_fb);
}

static void
gen9_test_fini(gen9_test_t *test, igt_output_t *output)
{
	igt_remove_fb(test->data->drm_fd, &test->biggreen_fb);
	igt_remove_fb(test->data->drm_fd, &test->smallred_fb);
	igt_remove_fb(test->data->drm_fd, &test->smallblue_fb);

	igt_output_set_pipe(output, PIPE_ANY);
	igt_display_commit2(&test->data->display, COMMIT_LEGACY);
}

/*
 * Test features specific to gen9+ platforms (i.e., primary plane
 * windowing)
 */
static void
gen9_test_pipe(data_t *data, enum pipe pipe, igt_output_t *output)
{
	gen9_test_t test = { .data = data };
	igt_plane_t *primary;

	int ret = 0;

	igt_skip_on(data->gen < 9);
	igt_skip_on(pipe >= data->display.n_pipes);

	igt_output_set_pipe(output, pipe);

	gen9_test_init(&test, output, pipe);

	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);

	/* Start with a full-screen primary plane */
	igt_plane_set_fb(primary, &test.biggreen_fb);
	igt_display_commit2(&data->display, COMMIT_LEGACY);

	/* Set primary to windowed size/position */
	igt_plane_set_fb(primary, &test.smallblue_fb);
	igt_plane_set_position(primary, test.x, test.y);
	igt_plane_set_size(primary, test.w, test.h);
	igt_display_commit2(&data->display, COMMIT_UNIVERSAL);

	/*
	 * SetPlane update to another framebuffer of the same size
	 * should succeed
	 */
	igt_plane_set_fb(primary, &test.smallred_fb);
	igt_plane_set_position(primary, test.x, test.y);
	igt_plane_set_size(primary, test.w, test.h);
	igt_display_commit2(&data->display, COMMIT_UNIVERSAL);

	/* PageFlip should also succeed */
	ret = drmModePageFlip(data->drm_fd, output->config.crtc->crtc_id,
			      test.smallblue_fb.fb_id, 0, NULL);
	igt_assert_eq(ret, 0);

	igt_plane_set_fb(primary, NULL);
	igt_plane_set_position(primary, 0, 0);
	gen9_test_fini(&test, output);
}

static void
run_tests_for_pipe(data_t *data, enum pipe pipe)
{
	igt_output_t *output;

	igt_fixture {
		int valid_tests = 0;

		igt_skip_on(pipe >= data->display.n_pipes);

		for_each_valid_output_on_pipe(&data->display, pipe, output)
			valid_tests++;

		igt_require_f(valid_tests, "no valid crtc/connector combinations found\n");
	}

	igt_subtest_f("universal-plane-pipe-%s-functional",
		      kmstest_pipe_name(pipe))
		for_each_valid_output_on_pipe(&data->display, pipe, output)
			functional_test_pipe(data, pipe, output);

	igt_subtest_f("universal-plane-pipe-%s-sanity",
		      kmstest_pipe_name(pipe))
		for_each_valid_output_on_pipe(&data->display, pipe, output)
			sanity_test_pipe(data, pipe, output);

	igt_subtest_f("disable-primary-vs-flip-pipe-%s",
		      kmstest_pipe_name(pipe))
		for_each_valid_output_on_pipe(&data->display, pipe, output)
			pageflip_test_pipe(data, pipe, output);

	igt_subtest_f("cursor-fb-leak-pipe-%s",
		      kmstest_pipe_name(pipe))
		for_each_valid_output_on_pipe(&data->display, pipe, output)
			cursor_leak_test_pipe(data, pipe, output);

	igt_subtest_f("universal-plane-gen9-features-pipe-%s",
		      kmstest_pipe_name(pipe))
		for_each_valid_output_on_pipe(&data->display, pipe, output)
			gen9_test_pipe(data, pipe, output);
}

static data_t data;

igt_main
{
	enum pipe pipe;

	igt_skip_on_simulation();

	igt_fixture {
		data.drm_fd = drm_open_driver_master(DRIVER_INTEL);
		data.gen = intel_gen(intel_get_drm_devid(data.drm_fd));

		kmstest_set_vt_graphics_mode();

		igt_require_pipe_crc(data.drm_fd);
		igt_display_require(&data.display, data.drm_fd);
	}

	for_each_pipe_static(pipe) {
		igt_subtest_group
			run_tests_for_pipe(&data, pipe);
	}

	igt_fixture {
		igt_display_fini(&data.display);
	}
}
