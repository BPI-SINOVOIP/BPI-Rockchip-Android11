/*
 * Copyright © 2017 Intel Corporation
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

#include <signal.h>

#include "igt.h"
#include "drmtest.h"
#include "sw_sync.h"

IGT_TEST_DESCRIPTION("Tests that interrupt various atomic ioctls.");

enum plane_test_type
{
	test_legacy_modeset,
	test_atomic_modeset,
	test_legacy_dpms,
	test_setplane,
	test_setcursor,
	test_pageflip
};

static int block_plane(igt_display_t *display, igt_output_t *output, enum plane_test_type test_type, igt_plane_t *plane)
{
	int timeline = sw_sync_timeline_create();

	igt_fork(child, 1) {
		/* Ignore the signal helper, we need to block indefinitely on the fence. */
		signal(SIGCONT, SIG_IGN);

		if (test_type == test_legacy_modeset || test_type == test_atomic_modeset) {
			igt_output_set_pipe(output, PIPE_NONE);
			igt_plane_set_fb(plane, NULL);
		}
		igt_plane_set_fence_fd(plane, sw_sync_timeline_create_fence(timeline, 1));

		igt_display_commit2(display, COMMIT_ATOMIC);
	}

	return timeline;
}

static void unblock(int block)
{
	sw_sync_timeline_inc(block, 1);
	close(block);
}

static void ev_page_flip(int fd, unsigned seq, unsigned tv_sec, unsigned tv_usec, void *user_data)
{
	igt_debug("Retrieved vblank seq: %u on unk\n", seq);
}

static drmEventContext drm_events = {
	.version = 2,
	.page_flip_handler = ev_page_flip
};

static void run_plane_test(igt_display_t *display, enum pipe pipe, igt_output_t *output,
			   enum plane_test_type test_type, unsigned plane_type)
{
	drmModeModeInfo *mode;
	igt_fb_t fb, fb2;
	igt_plane_t *primary, *plane;
	int block;

	/*
	 * Make sure we start with everything disabled to force a real modeset.
	 * igt_display_require only sets sw state, and assumes the first test
	 * doesn't care about hw state.
	 */
	igt_display_commit2(display, COMMIT_ATOMIC);

	igt_output_set_pipe(output, pipe);

	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);
	plane = igt_output_get_plane_type(output, plane_type);
	mode = igt_output_get_mode(output);

	igt_create_fb(display->drm_fd, mode->hdisplay, mode->vdisplay,
		      DRM_FORMAT_XRGB8888, LOCAL_DRM_FORMAT_MOD_NONE, &fb);

	switch (plane_type) {
	case DRM_PLANE_TYPE_PRIMARY:
		igt_create_fb(display->drm_fd, mode->hdisplay, mode->vdisplay,
			      DRM_FORMAT_XRGB8888, LOCAL_DRM_FORMAT_MOD_NONE, &fb2);
		break;
	case DRM_PLANE_TYPE_CURSOR:
		igt_create_fb(display->drm_fd, 64, 64,
		      DRM_FORMAT_ARGB8888, LOCAL_DRM_FORMAT_MOD_NONE, &fb2);
		break;
	}

	if (test_type != test_legacy_modeset && test_type != test_atomic_modeset) {
		igt_plane_set_fb(primary, &fb);
		igt_display_commit2(display, COMMIT_ATOMIC);
	}

	igt_plane_set_fb(plane, &fb2);

	block = block_plane(display, output, test_type, plane);

	/* wait for the block to complete in block_plane */
	sleep(1);

	igt_fork(child, 1) {
		signal(SIGCONT, SIG_IGN);

		 /* unblock after 5 seconds to allow the ioctl to complete,
		  * instead of failing with -EINTR.
		  */
		igt_assert(sleep(5) == 0);

		unblock(block);
	}

	/* run the test */
	igt_while_interruptible(true) {
		switch (test_type) {
			case test_legacy_modeset: {
				struct drm_mode_crtc crtc = {
					.set_connectors_ptr = (uint64_t)(uintptr_t)&output->id,
					.count_connectors = 1,
					.crtc_id = primary->pipe->crtc_id,
					.fb_id = fb2.fb_id,
					.mode_valid = 1,
					.mode = *(struct drm_mode_modeinfo*)mode,
				};

				do_ioctl(display->drm_fd, DRM_IOCTL_MODE_SETCRTC, &crtc);
				break;
			}
			case test_atomic_modeset: {
				uint32_t objs[3] = {
					plane->pipe->crtc_id,
					output->id,
					plane->drm_plane->plane_id
				};
				uint32_t count_props[3] = { 2, 1, 6 };
				uint32_t props[] = {
					/* crtc: 2 props */
					plane->pipe->props[IGT_CRTC_MODE_ID],
					plane->pipe->props[IGT_CRTC_ACTIVE],
					/* connector: 1 prop */
					output->props[IGT_CONNECTOR_CRTC_ID],
					/* plane: remainder props */
					plane->props[IGT_PLANE_CRTC_ID],
					plane->props[IGT_PLANE_FB_ID],
					plane->props[IGT_PLANE_SRC_W],
					plane->props[IGT_PLANE_SRC_H],
					plane->props[IGT_PLANE_CRTC_W],
					plane->props[IGT_PLANE_CRTC_H]
				};
				uint64_t prop_vals[] = {
					/* crtc */
					0, /* mode_id, filled in below */
					true,
					/* connector */
					plane->pipe->crtc_id,
					/* plane */
					plane->pipe->crtc_id,
					fb2.fb_id,
					IGT_FIXED(fb2.width, 0),
					IGT_FIXED(fb2.height, 0),
					fb2.width,
					fb2.height
				};
				uint32_t mode_blob;

				struct drm_mode_atomic atm = {
					.flags = DRM_MODE_ATOMIC_ALLOW_MODESET,
					.count_objs = 3, /* crtc, connector, plane */
					.objs_ptr = (uint64_t)(uintptr_t)&objs,
					.count_props_ptr = (uint64_t)(uintptr_t)&count_props,
					.props_ptr = (uint64_t)(uintptr_t)&props,
					.prop_values_ptr = (uint64_t)(uintptr_t)&prop_vals,
				};

				do_or_die(drmModeCreatePropertyBlob(display->drm_fd, mode, sizeof(*mode), &mode_blob));
				prop_vals[0] = mode_blob;

				do_ioctl(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &atm);

				do_or_die(drmModeDestroyPropertyBlob(display->drm_fd, mode_blob));
				break;
			}
			case test_legacy_dpms: {
				struct drm_mode_connector_set_property prop = {
					.value = DRM_MODE_DPMS_OFF,
					.prop_id = output->props[IGT_CONNECTOR_DPMS],
					.connector_id = output->id,
				};

				do_ioctl(display->drm_fd, DRM_IOCTL_MODE_SETPROPERTY, &prop);
				break;
			}
			case test_setcursor: {
				struct drm_mode_cursor cur = {
					.flags = DRM_MODE_CURSOR_BO,
					.crtc_id = plane->pipe->crtc_id,
					.width = fb2.width,
					.height = fb2.height,
					.handle = fb2.gem_handle,
				};
				do_ioctl(display->drm_fd, DRM_IOCTL_MODE_CURSOR, &cur);
				break;
			}
			case test_setplane: {
				struct drm_mode_set_plane setplane = {
					.plane_id = plane->drm_plane->plane_id,
					.crtc_id = plane->pipe->crtc_id,
					.fb_id = fb2.fb_id,
					.crtc_w = fb2.width,
					.crtc_h = fb2.height,
					.src_w = IGT_FIXED(fb2.width, 0),
					.src_h = IGT_FIXED(fb2.height, 0),
				};

				do_ioctl(display->drm_fd, DRM_IOCTL_MODE_SETPLANE, &setplane);
				break;
			}
			case test_pageflip: {
				struct drm_mode_crtc_page_flip pageflip = {
					.crtc_id = plane->pipe->crtc_id,
					.fb_id = fb2.fb_id,
					.flags = DRM_MODE_PAGE_FLIP_EVENT,
				};

				do_ioctl(display->drm_fd, DRM_IOCTL_MODE_PAGE_FLIP, &pageflip);

				drmHandleEvent(display->drm_fd, &drm_events);
				break;
			}
		}
	}

	igt_waitchildren();

	/* The mode is unset by the forked helper, force a refresh here */
	if (test_type == test_legacy_modeset || test_type == test_atomic_modeset)
		igt_pipe_refresh(display, pipe, true);

	igt_plane_set_fb(plane, NULL);
	igt_plane_set_fb(primary, NULL);
	igt_output_set_pipe(output, PIPE_NONE);
	igt_display_commit2(display, COMMIT_ATOMIC);
	igt_remove_fb(display->drm_fd, &fb);
}

igt_main
{
	igt_display_t display;
	igt_output_t *output;
	enum pipe pipe;

	igt_skip_on_simulation();

	igt_fixture {
		display.drm_fd = drm_open_driver_master(DRIVER_ANY);

		kmstest_set_vt_graphics_mode();

		igt_display_require(&display, display.drm_fd);
		igt_require(display.is_atomic);
		igt_display_require_output(&display);

		igt_require_sw_sync();
	}

	igt_subtest("legacy-setmode")
		for_each_pipe_with_valid_output(&display, pipe, output) {
			run_plane_test(&display, pipe, output, test_legacy_modeset, DRM_PLANE_TYPE_PRIMARY);
			break;
		}

	igt_subtest("atomic-setmode")
		for_each_pipe_with_valid_output(&display, pipe, output) {
			run_plane_test(&display, pipe, output, test_atomic_modeset, DRM_PLANE_TYPE_PRIMARY);
			break;
		}

	igt_subtest("legacy-dpms")
		for_each_pipe_with_valid_output(&display, pipe, output) {
			run_plane_test(&display, pipe, output, test_legacy_dpms, DRM_PLANE_TYPE_PRIMARY);
			break;
		}

	igt_subtest("legacy-pageflip")
		for_each_pipe_with_valid_output(&display, pipe, output) {
			run_plane_test(&display, pipe, output, test_pageflip, DRM_PLANE_TYPE_PRIMARY);
			break;
		}

	igt_subtest("legacy-cursor")
		for_each_pipe_with_valid_output(&display, pipe, output) {
			run_plane_test(&display, pipe, output, test_setcursor, DRM_PLANE_TYPE_CURSOR);
			break;
		}

	igt_subtest("universal-setplane-primary")
		for_each_pipe_with_valid_output(&display, pipe, output) {
			run_plane_test(&display, pipe, output, test_setplane, DRM_PLANE_TYPE_PRIMARY);
			break;
		}

	igt_subtest("universal-setplane-cursor")
		for_each_pipe_with_valid_output(&display, pipe, output) {
			run_plane_test(&display, pipe, output, test_setplane, DRM_PLANE_TYPE_CURSOR);
			break;
		}

	/* TODO: legacy gamma_set/get, object set/getprop, getcrtc, getconnector */
	igt_fixture {
		igt_display_fini(&display);
	}
}
