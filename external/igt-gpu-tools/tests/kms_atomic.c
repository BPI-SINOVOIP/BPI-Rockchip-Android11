/*
 * Copyright © 2015 Intel Corporation
 * Copyright © 2014-2015 Collabora, Ltd.
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
 * Authors:
 *    Micah Fedke <micah.fedke@collabora.co.uk>
 *    Daniel Stone <daniels@collabora.com>
 *    Pekka Paalanen <pekka.paalanen@collabora.co.uk>
 */

/*
 * Testcase: testing atomic modesetting API
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <xf86drmMode.h>
#include <cairo.h>
#include "drm.h"
#include "ioctl_wrappers.h"
#include "drmtest.h"
#include "igt.h"
#include "igt_aux.h"
#include "sw_sync.h"

#ifndef DRM_CAP_CURSOR_WIDTH
#define DRM_CAP_CURSOR_WIDTH 0x8
#endif

#ifndef DRM_CAP_CURSOR_HEIGHT
#define DRM_CAP_CURSOR_HEIGHT 0x9
#endif

IGT_TEST_DESCRIPTION("Test atomic modesetting API");

enum kms_atomic_check_relax {
	ATOMIC_RELAX_NONE = 0,
	CRTC_RELAX_MODE = (1 << 0),
	PLANE_RELAX_FB = (1 << 1)
};

static bool plane_filter(enum igt_atomic_plane_properties prop)
{
	if ((1 << prop) & IGT_PLANE_COORD_CHANGED_MASK)
		return false;

	if (prop == IGT_PLANE_CRTC_ID || prop == IGT_PLANE_FB_ID)
		return false;

	if (prop == IGT_PLANE_IN_FENCE_FD)
		return false;

	/* Don't care about other properties */
	return true;
}

static void plane_get_current_state(igt_plane_t *plane, uint64_t *values)
{
	int i;

	for (i = 0; i < IGT_NUM_PLANE_PROPS; i++) {
		if (plane_filter(i)) {
			values[i] = 0;
			continue;
		}

		values[i] = igt_plane_get_prop(plane, i);
	}
}

static void plane_check_current_state(igt_plane_t *plane, const uint64_t *values,
				      enum kms_atomic_check_relax relax)
{
	drmModePlanePtr legacy;
	uint64_t current_values[IGT_NUM_PLANE_PROPS];
	int i;

	legacy = drmModeGetPlane(plane->pipe->display->drm_fd, plane->drm_plane->plane_id);
	igt_assert(legacy);

	igt_assert_eq_u32(legacy->crtc_id, values[IGT_PLANE_CRTC_ID]);

	if (!(relax & PLANE_RELAX_FB))
		igt_assert_eq_u32(legacy->fb_id, values[IGT_PLANE_FB_ID]);

	plane_get_current_state(plane, current_values);

	/* Legacy cursor ioctls create their own, unknowable, internal
	 * framebuffer which we can't reason about. */
	if (relax & PLANE_RELAX_FB)
		current_values[IGT_PLANE_FB_ID] = values[IGT_PLANE_FB_ID];

	for (i = 0; i < IGT_NUM_PLANE_PROPS; i++)
		if (!plane_filter(i))
			igt_assert_eq_u64(current_values[i], values[i]);

	drmModeFreePlane(legacy);
}

static void plane_commit(igt_plane_t *plane, enum igt_commit_style s,
                                enum kms_atomic_check_relax relax)
{
	igt_display_commit2(plane->pipe->display, s);
	plane_check_current_state(plane, plane->values, relax);
}

static void plane_commit_atomic_err(igt_plane_t *plane,
				    enum kms_atomic_check_relax relax,
				    int err)
{
	uint64_t current_values[IGT_NUM_PLANE_PROPS];

	plane_get_current_state(plane, current_values);

	igt_assert_eq(-err, igt_display_try_commit2(plane->pipe->display, COMMIT_ATOMIC));

	plane_check_current_state(plane, current_values, relax);
}

static bool crtc_filter(enum igt_atomic_crtc_properties prop)
{
	if (prop == IGT_CRTC_MODE_ID || prop == IGT_CRTC_ACTIVE)
		return false;

	return true;
}

static void crtc_get_current_state(igt_pipe_t *pipe, uint64_t *values)
{
	int i;

	for (i = 0; i < IGT_NUM_CRTC_PROPS; i++) {
		if (crtc_filter(i)) {
			values[i] = 0;
			continue;
		}

		values[i] = igt_pipe_obj_get_prop(pipe, i);
	}
}

static void crtc_check_current_state(igt_pipe_t *pipe,
				     const uint64_t *pipe_values,
				     const uint64_t *primary_values,
				     enum kms_atomic_check_relax relax)
{
	uint64_t current_pipe_values[IGT_NUM_CRTC_PROPS];
	drmModeCrtcPtr legacy;
	drmModePropertyBlobRes *mode_prop = NULL;
	struct drm_mode_modeinfo *mode = NULL;

	if (pipe_values[IGT_CRTC_MODE_ID]) {
		mode_prop = drmModeGetPropertyBlob(pipe->display->drm_fd,
						   pipe_values[IGT_CRTC_MODE_ID]);

		igt_assert(mode_prop);

		igt_assert_eq(mode_prop->length,
		              sizeof(struct drm_mode_modeinfo));
		mode = mode_prop->data;
	}

	legacy = drmModeGetCrtc(pipe->display->drm_fd, pipe->crtc_id);
	igt_assert(legacy);

	igt_assert_eq_u32(legacy->crtc_id, pipe->crtc_id);
	igt_assert_eq_u32(legacy->x, primary_values[IGT_PLANE_SRC_X] >> 16);
	igt_assert_eq_u32(legacy->y, primary_values[IGT_PLANE_SRC_Y] >> 16);

	igt_assert_eq_u32(legacy->buffer_id, primary_values[IGT_PLANE_FB_ID]);

	if (legacy->mode_valid) {
		igt_assert(mode_prop);

		do_or_die(memcmp(&legacy->mode, mode, sizeof(*mode)));

		igt_assert_eq(legacy->width, legacy->mode.hdisplay);
		igt_assert_eq(legacy->height, legacy->mode.vdisplay);

		igt_assert_neq(pipe_values[IGT_CRTC_MODE_ID], 0);
	} else {
		igt_assert(!mode_prop);
	}

	crtc_get_current_state(pipe, current_pipe_values);

	/* Optionally relax the check for MODE_ID: using the legacy SetCrtc
	 * API can potentially change MODE_ID even if the mode itself remains
	 * unchanged. */
	if (relax & CRTC_RELAX_MODE && mode && current_pipe_values[IGT_CRTC_MODE_ID] &&
	    current_pipe_values[IGT_CRTC_MODE_ID] != pipe_values[IGT_CRTC_MODE_ID]) {
		drmModePropertyBlobRes *cur_prop =
			drmModeGetPropertyBlob(pipe->display->drm_fd,
					       current_pipe_values[IGT_CRTC_MODE_ID]);

		igt_assert(cur_prop);
		igt_assert_eq(cur_prop->length, sizeof(struct drm_mode_modeinfo));

		if (!memcmp(cur_prop->data, mode, sizeof(*mode)))
			current_pipe_values[IGT_CRTC_MODE_ID] = pipe_values[IGT_CRTC_MODE_ID];

		drmModeFreePropertyBlob(cur_prop);
	}

	do_or_die(memcmp(pipe_values, current_pipe_values, sizeof(current_pipe_values)));

	drmModeFreeCrtc(legacy);
	drmModeFreePropertyBlob(mode_prop);
}

static void crtc_commit(igt_pipe_t *pipe, igt_plane_t *plane,
			enum igt_commit_style s,
			enum kms_atomic_check_relax relax)
{
	igt_display_commit2(pipe->display, s);

	crtc_check_current_state(pipe, pipe->values, plane->values, relax);
	plane_check_current_state(plane, plane->values, relax);
}

static void crtc_commit_atomic_flags_err(igt_pipe_t *pipe, igt_plane_t *plane,
					 unsigned flags,
					 enum kms_atomic_check_relax relax,
					 int err)
{
	uint64_t current_pipe_values[IGT_NUM_CRTC_PROPS];
	uint64_t current_plane_values[IGT_NUM_PLANE_PROPS];

	crtc_get_current_state(pipe, current_pipe_values);
	plane_get_current_state(plane, current_plane_values);

	igt_assert_eq(-err, igt_display_try_commit_atomic(pipe->display, flags, NULL));

	crtc_check_current_state(pipe, current_pipe_values, current_plane_values, relax);
	plane_check_current_state(plane, current_plane_values, relax);
}

#define crtc_commit_atomic_err(pipe, plane, relax, err) \
	crtc_commit_atomic_flags_err(pipe, plane, DRM_MODE_ATOMIC_ALLOW_MODESET, relax, err)

static uint32_t plane_get_igt_format(igt_plane_t *plane)
{
	drmModePlanePtr plane_kms;
	int i;

	plane_kms = plane->drm_plane;

	for (i = 0; i < plane_kms->count_formats; i++) {
		if (igt_fb_supported_format(plane_kms->formats[i]))
			return plane_kms->formats[i];
	}

	return 0;
}

static void
plane_primary_overlay_zpos(igt_pipe_t *pipe, igt_output_t *output,
			   igt_plane_t *primary, igt_plane_t *overlay,
			   uint32_t format_primary, uint32_t format_overlay)
{
	struct igt_fb fb_primary, fb_overlay;
	drmModeModeInfo *mode = igt_output_get_mode(output);
	cairo_t *cr;

	/* for primary */
	uint32_t w = mode->hdisplay;
	uint32_t h = mode->vdisplay;

	/* for overlay */
	uint32_t w_overlay = mode->hdisplay / 2;
	uint32_t h_overlay = mode->vdisplay / 2;

	igt_create_color_pattern_fb(pipe->display->drm_fd,
				    w, h, format_primary, I915_TILING_NONE,
				    0.2, 0.2, 0.2, &fb_primary);

	igt_create_color_pattern_fb(pipe->display->drm_fd,
				    w_overlay, h_overlay,
				    format_overlay, I915_TILING_NONE,
				    0.2, 0.2, 0.2, &fb_overlay);

#if defined(USE_CAIRO_PIXMAN)
	/* Draw a hole in the overlay */
	cr = igt_get_cairo_ctx(pipe->display->drm_fd, &fb_overlay);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	igt_paint_color_alpha(cr, w_overlay / 4, h_overlay / 4,
			      w_overlay / 2, h_overlay / 2,
			      0.0, 0.0, 0.0, 0.0);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	igt_put_cairo_ctx(pipe->display->drm_fd, &fb_overlay, cr);
#endif

	igt_plane_set_fb(primary, &fb_primary);
	igt_plane_set_fb(overlay, &fb_overlay);

	igt_plane_set_position(overlay, w_overlay / 2, h_overlay / 2);

	igt_plane_set_prop_value(primary, IGT_PLANE_ZPOS, 0);
	igt_plane_set_prop_value(overlay, IGT_PLANE_ZPOS, 1);

	igt_info("Committing with overlay on top, it has a hole "\
		  "through which the primary should be seen\n");
	plane_commit(primary, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);

	igt_assert_eq_u64(igt_plane_get_prop(primary, IGT_PLANE_ZPOS), 0);
	igt_assert_eq_u64(igt_plane_get_prop(overlay, IGT_PLANE_ZPOS), 1);

	igt_plane_set_prop_value(primary, IGT_PLANE_ZPOS, 1);
	igt_plane_set_prop_value(overlay, IGT_PLANE_ZPOS, 0);

	igt_info("Committing with primary on top, only the primary "\
		 "should be visible\n");
	plane_commit(primary, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);

	igt_assert_eq_u64(igt_plane_get_prop(primary, IGT_PLANE_ZPOS), 1);
	igt_assert_eq_u64(igt_plane_get_prop(overlay, IGT_PLANE_ZPOS), 0);

	/* Draw a hole in the primary exactly on top of the overlay plane */
#if defined(USE_CAIRO_PIXMAN)
	cr = igt_get_cairo_ctx(pipe->display->drm_fd, &fb_primary);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	igt_paint_color_alpha(cr, w_overlay / 2, h_overlay / 2,
			      w_overlay, h_overlay,
			      0.0, 0.0, 0.0, 0.5);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	igt_put_cairo_ctx(pipe->display->drm_fd, &fb_primary, cr);
#endif

	igt_info("Committing with a hole in the primary through "\
		  "which the underlay should be seen\n");
	plane_commit(primary, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);

	/* reset it back to initial state */
	igt_plane_set_prop_value(primary, IGT_PLANE_ZPOS, 0);
	igt_plane_set_prop_value(overlay, IGT_PLANE_ZPOS, 1);
	plane_commit(primary, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);

	igt_assert_eq_u64(igt_plane_get_prop(primary, IGT_PLANE_ZPOS), 0);
	igt_assert_eq_u64(igt_plane_get_prop(overlay, IGT_PLANE_ZPOS), 1);
}

static void plane_overlay(igt_pipe_t *pipe, igt_output_t *output, igt_plane_t *plane)
{
	drmModeModeInfo *mode = igt_output_get_mode(output);
	uint32_t format = plane_get_igt_format(plane);
	struct igt_fb fb;
	uint32_t w = mode->hdisplay / 2;
	uint32_t h = mode->vdisplay / 2;

	igt_require(format != 0);

	igt_create_pattern_fb(pipe->display->drm_fd, w, h,
			      format, I915_TILING_NONE, &fb);

	igt_plane_set_fb(plane, &fb);
	igt_plane_set_position(plane, w/2, h/2);

	/* Enable the overlay plane using the atomic API, and double-check
	 * state is what we think it should be. */
	plane_commit(plane, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);

	/* Disable the plane and check the state matches the old. */
	igt_plane_set_fb(plane, NULL);
	igt_plane_set_position(plane, 0, 0);
	plane_commit(plane, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);

	/* Re-enable the plane through the legacy plane API, and verify through
	 * atomic. */
	igt_plane_set_fb(plane, &fb);
	igt_plane_set_position(plane, w/2, h/2);
	plane_commit(plane, COMMIT_LEGACY, ATOMIC_RELAX_NONE);

	/* Restore the plane to its original settings through the legacy plane
	 * API, and verify through atomic. */
	igt_plane_set_fb(plane, NULL);
	igt_plane_set_position(plane, 0, 0);
	plane_commit(plane, COMMIT_LEGACY, ATOMIC_RELAX_NONE);

	igt_remove_fb(pipe->display->drm_fd, &fb);
}

static void plane_primary(igt_pipe_t *pipe, igt_plane_t *plane, struct igt_fb *fb)
{
	struct igt_fb fb2;

	igt_create_color_pattern_fb(pipe->display->drm_fd,
				    fb->width, fb->height,
				    fb->drm_format, I915_TILING_NONE,
				    0.2, 0.2, 0.2, &fb2);

	/* Flip the primary plane using the atomic API, and double-check
	 * state is what we think it should be. */
	igt_plane_set_fb(plane, &fb2);
	crtc_commit(pipe, plane, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);

	/* Restore the primary plane and check the state matches the old. */
	igt_plane_set_fb(plane, fb);
	crtc_commit(pipe, plane, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);

	/* Set the plane through the legacy CRTC/primary-plane API, and
	 * verify through atomic. */
	igt_plane_set_fb(plane, &fb2);
	crtc_commit(pipe, plane, COMMIT_LEGACY, CRTC_RELAX_MODE);

	/* Restore the plane to its original settings through the legacy CRTC
	 * API, and verify through atomic. */
	igt_plane_set_fb(plane, fb);
	crtc_commit(pipe, plane, COMMIT_LEGACY, CRTC_RELAX_MODE);

	/* Set the plane through the universal setplane API, and
	 * verify through atomic. */
	igt_plane_set_fb(plane, &fb2);
	plane_commit(plane, COMMIT_UNIVERSAL, ATOMIC_RELAX_NONE);
}

/* test to ensure that DRM_MODE_ATOMIC_TEST_ONLY really only touches the
 * free-standing state objects and nothing else.
 */
static void test_only(igt_pipe_t *pipe_obj,
		      igt_plane_t *primary,
		      igt_output_t *output)
{
	drmModeModeInfo *mode = igt_output_get_mode(output);
	uint32_t format = plane_get_igt_format(primary);
	struct igt_fb fb;
	uint64_t old_plane_values[IGT_NUM_PLANE_PROPS], old_crtc_values[IGT_NUM_CRTC_PROPS];

	igt_require(format != 0);

	plane_get_current_state(primary, old_plane_values);
	crtc_get_current_state(pipe_obj, old_crtc_values);

	igt_assert(!old_crtc_values[IGT_CRTC_MODE_ID]);

	igt_create_pattern_fb(pipe_obj->display->drm_fd,
			     mode->hdisplay, mode->vdisplay,
			     format, I915_TILING_NONE, &fb);
	igt_plane_set_fb(primary, &fb);
	igt_output_set_pipe(output, pipe_obj->pipe);

	igt_display_commit_atomic(pipe_obj->display, DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);

	/* check the state, should still be old state */
	crtc_check_current_state(pipe_obj, old_crtc_values, old_plane_values, ATOMIC_RELAX_NONE);
	plane_check_current_state(primary, old_plane_values, ATOMIC_RELAX_NONE);

	/*
	 * Enable the plane through the legacy CRTC/primary-plane API, and
	 * verify through atomic.
	 */
	crtc_commit(pipe_obj, primary, COMMIT_LEGACY, CRTC_RELAX_MODE);

	/* Same for disable.. */
	plane_get_current_state(primary, old_plane_values);
	crtc_get_current_state(pipe_obj, old_crtc_values);

	igt_plane_set_fb(primary, NULL);
	igt_output_set_pipe(output, PIPE_NONE);

	igt_display_commit_atomic(pipe_obj->display, DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);

	/* for extra stress, go through dpms off/on cycle */
	kmstest_set_connector_dpms(output->display->drm_fd, output->config.connector, DRM_MODE_DPMS_OFF);
	kmstest_set_connector_dpms(output->display->drm_fd, output->config.connector, DRM_MODE_DPMS_ON);

	/* check the state, should still be old state */
	crtc_check_current_state(pipe_obj, old_crtc_values, old_plane_values, ATOMIC_RELAX_NONE);
	plane_check_current_state(primary, old_plane_values, ATOMIC_RELAX_NONE);

	/* And disable the pipe and remove fb, test complete */
	crtc_commit(pipe_obj, primary, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);
	igt_remove_fb(pipe_obj->display->drm_fd, &fb);
}

static void plane_cursor(igt_pipe_t *pipe_obj,
			 igt_output_t *output,
			 igt_plane_t *cursor)
{
	drmModeModeInfo *mode = igt_output_get_mode(output);
	struct igt_fb fb;
	uint64_t width, height;
	int x = mode->hdisplay / 2;
	int y = mode->vdisplay / 2;

	/* Any kernel new enough for atomic, also has the cursor size caps. */
	do_or_die(drmGetCap(pipe_obj->display->drm_fd,
	                    DRM_CAP_CURSOR_WIDTH, &width));
	do_or_die(drmGetCap(pipe_obj->display->drm_fd,
	                    DRM_CAP_CURSOR_HEIGHT, &height));

	igt_create_color_fb(pipe_obj->display->drm_fd,
			    width, height, DRM_FORMAT_ARGB8888,
			    LOCAL_DRM_FORMAT_MOD_NONE,
			    0.0, 0.0, 0.0, &fb);

	/* Flip the cursor plane using the atomic API, and double-check
	 * state is what we think it should be. */
	igt_plane_set_fb(cursor, &fb);
	igt_plane_set_position(cursor, x, y);
	plane_commit(cursor, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);

	/* Restore the cursor plane and check the state matches the old. */
	igt_plane_set_fb(cursor, NULL);
	igt_plane_set_position(cursor, 0, 0);
	plane_commit(cursor, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);

	/* Re-enable the plane through the legacy cursor API, and verify
	 * through atomic. */
	igt_plane_set_fb(cursor, &fb);
	igt_plane_set_position(cursor, x, y);
	plane_commit(cursor, COMMIT_LEGACY, PLANE_RELAX_FB);

	/* Wiggle. */
	igt_plane_set_position(cursor, x - 16, y - 16);
	plane_commit(cursor, COMMIT_LEGACY, PLANE_RELAX_FB);

	/* Restore the plane to its original settings through the legacy cursor
	 * API, and verify through atomic. */
	igt_plane_set_fb(cursor, NULL);
	igt_plane_set_position(cursor, 0, 0);
	plane_commit(cursor, COMMIT_LEGACY, ATOMIC_RELAX_NONE);
}

static void plane_invalid_params(igt_pipe_t *pipe,
				 igt_output_t *output,
				 igt_plane_t *plane,
				 struct igt_fb *fb)
{
	struct igt_fb fb2;

	/* Pass a series of invalid object IDs for the FB ID. */
	igt_plane_set_prop_value(plane, IGT_PLANE_FB_ID, plane->drm_plane->plane_id);
	plane_commit_atomic_err(plane, ATOMIC_RELAX_NONE, EINVAL);

	igt_plane_set_prop_value(plane, IGT_PLANE_FB_ID, pipe->crtc_id);
	plane_commit_atomic_err(plane, ATOMIC_RELAX_NONE, EINVAL);

	igt_plane_set_prop_value(plane, IGT_PLANE_FB_ID, output->id);
	plane_commit_atomic_err(plane, ATOMIC_RELAX_NONE, EINVAL);

	igt_plane_set_prop_value(plane, IGT_PLANE_FB_ID, pipe->values[IGT_CRTC_MODE_ID]);
	plane_commit_atomic_err(plane, ATOMIC_RELAX_NONE, EINVAL);

	/* Valid, but invalid because CRTC_ID is set. */
	igt_plane_set_prop_value(plane, IGT_PLANE_FB_ID, 0);
	plane_commit_atomic_err(plane, ATOMIC_RELAX_NONE, EINVAL);

	igt_plane_set_fb(plane, fb);
	plane_commit(plane, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);

	/* Pass a series of invalid object IDs for the CRTC ID. */
	igt_plane_set_prop_value(plane, IGT_PLANE_CRTC_ID, plane->drm_plane->plane_id);
	plane_commit_atomic_err(plane, ATOMIC_RELAX_NONE, EINVAL);

	igt_plane_set_prop_value(plane, IGT_PLANE_CRTC_ID, fb->fb_id);
	plane_commit_atomic_err(plane, ATOMIC_RELAX_NONE, EINVAL);

	igt_plane_set_prop_value(plane, IGT_PLANE_CRTC_ID, output->id);
	plane_commit_atomic_err(plane, ATOMIC_RELAX_NONE, EINVAL);

	igt_plane_set_prop_value(plane, IGT_PLANE_CRTC_ID, pipe->values[IGT_CRTC_MODE_ID]);
	plane_commit_atomic_err(plane, ATOMIC_RELAX_NONE, EINVAL);

	/* Valid, but invalid because FB_ID is set. */
	igt_plane_set_prop_value(plane, IGT_PLANE_CRTC_ID, 0);
	plane_commit_atomic_err(plane, ATOMIC_RELAX_NONE, EINVAL);

	igt_plane_set_fb(plane, fb);
	plane_commit(plane, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);

	/* Create a framebuffer too small for the plane configuration. */
	igt_create_pattern_fb(pipe->display->drm_fd,
			      fb->width - 1, fb->height - 1,
			      fb->drm_format, I915_TILING_NONE, &fb2);

	igt_plane_set_prop_value(plane, IGT_PLANE_FB_ID, fb2.fb_id);
	plane_commit_atomic_err(plane, ATOMIC_RELAX_NONE, ENOSPC);

	/* Restore the primary plane and check the state matches the old. */
	igt_plane_set_fb(plane, fb);
	plane_commit(plane, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);
}

static void plane_invalid_params_fence(igt_pipe_t *pipe,
				       igt_output_t *output,
				       igt_plane_t *plane)
{
	int timeline, fence_fd;

	igt_require_sw_sync();

	timeline = sw_sync_timeline_create();

	/* invalid fence fd */
	igt_plane_set_fence_fd(plane, pipe->display->drm_fd);
	plane_commit_atomic_err(plane, ATOMIC_RELAX_NONE, EINVAL);

	/* Valid fence_fd but invalid CRTC */
	fence_fd = sw_sync_timeline_create_fence(timeline, 1);

	igt_plane_set_prop_value(plane, IGT_PLANE_CRTC_ID, ~0);
	igt_plane_set_fence_fd(plane, fence_fd);
	plane_commit_atomic_err(plane, ATOMIC_RELAX_NONE, EINVAL);

	sw_sync_timeline_inc(timeline, 1);
	igt_plane_set_prop_value(plane, IGT_PLANE_CRTC_ID, pipe->crtc_id);
	plane_commit(plane, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);

	close(fence_fd);
	close(timeline);
}

static void crtc_invalid_params(igt_pipe_t *pipe,
				igt_output_t *output,
				igt_plane_t *plane,
				struct igt_fb *fb)
{
	uint64_t old_mode_id = pipe->values[IGT_CRTC_MODE_ID];
	drmModeModeInfo *mode = igt_output_get_mode(output);

	/* Pass a series of invalid object IDs for the mode ID. */
	igt_pipe_obj_set_prop_value(pipe, IGT_CRTC_MODE_ID, plane->drm_plane->plane_id);
	crtc_commit_atomic_err(pipe, plane, ATOMIC_RELAX_NONE, EINVAL);

	igt_pipe_obj_set_prop_value(pipe, IGT_CRTC_MODE_ID, pipe->crtc_id);
	crtc_commit_atomic_err(pipe, plane, ATOMIC_RELAX_NONE, EINVAL);

	igt_pipe_obj_set_prop_value(pipe, IGT_CRTC_MODE_ID, output->id);
	crtc_commit_atomic_err(pipe, plane, ATOMIC_RELAX_NONE, EINVAL);

	igt_pipe_obj_set_prop_value(pipe, IGT_CRTC_MODE_ID, fb->fb_id);
	crtc_commit_atomic_err(pipe, plane, ATOMIC_RELAX_NONE, EINVAL);

	/* Can we restore mode? */
	igt_pipe_obj_set_prop_value(pipe, IGT_CRTC_MODE_ID, old_mode_id);
	crtc_commit_atomic_flags_err(pipe, plane, DRM_MODE_ATOMIC_TEST_ONLY, ATOMIC_RELAX_NONE, 0);

	/*
	 * TEST_ONLY cannot be combined with DRM_MODE_PAGE_FLIP_EVENT,
	 * but DRM_MODE_PAGE_FLIP_EVENT will always generate EINVAL
	 * without valid crtc, so test it here.
	 */
	crtc_commit_atomic_flags_err(pipe, plane,
				     DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_PAGE_FLIP_EVENT,
				     ATOMIC_RELAX_NONE, EINVAL);

	/* Create a blob which is the wrong size to be a valid mode. */
	igt_pipe_obj_replace_prop_blob(pipe, IGT_CRTC_MODE_ID, mode, sizeof(*mode) - 1);
	crtc_commit_atomic_err(pipe, plane, ATOMIC_RELAX_NONE, EINVAL);

	igt_pipe_obj_replace_prop_blob(pipe, IGT_CRTC_MODE_ID, mode, sizeof(*mode) + 1);
	crtc_commit_atomic_err(pipe, plane, ATOMIC_RELAX_NONE, EINVAL);


	/* Restore the CRTC and check the state matches the old. */
	igt_pipe_obj_replace_prop_blob(pipe, IGT_CRTC_MODE_ID, mode, sizeof(*mode));
	crtc_commit(pipe, plane, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);
}

static void crtc_invalid_params_fence(igt_pipe_t *pipe,
				      igt_output_t *output,
				      igt_plane_t *plane,
				      struct igt_fb *fb)
{
	int timeline, fence_fd;
	void *map;
	const ptrdiff_t page_size = sysconf(_SC_PAGE_SIZE);

	uint64_t old_mode_id = pipe->values[IGT_CRTC_MODE_ID];

	igt_require_sw_sync();

	timeline = sw_sync_timeline_create();

	/* invalid out_fence_ptr */
	map = mmap(NULL, page_size, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	igt_assert(map != MAP_FAILED);

	igt_pipe_obj_set_prop_value(pipe, IGT_CRTC_OUT_FENCE_PTR, (ptrdiff_t)map);
	crtc_commit_atomic_err(pipe, plane, ATOMIC_RELAX_NONE, EFAULT);
	munmap(map, page_size);

	/* invalid out_fence_ptr */
	map = mmap(NULL, page_size, PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	igt_assert(map != MAP_FAILED);

	igt_pipe_obj_set_prop_value(pipe, IGT_CRTC_OUT_FENCE_PTR, (ptrdiff_t)map);
	crtc_commit_atomic_err(pipe, plane, ATOMIC_RELAX_NONE, EFAULT);
	munmap(map, page_size);

	/* invalid out_fence_ptr */
	map = mmap(NULL, page_size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	igt_assert(map != MAP_FAILED);

	igt_pipe_obj_set_prop_value(pipe, IGT_CRTC_OUT_FENCE_PTR, (ptrdiff_t)map);
	crtc_commit_atomic_err(pipe, plane, ATOMIC_RELAX_NONE, EFAULT);
	munmap(map, page_size);

	/* valid in fence but not allowed prop on crtc */
	fence_fd = sw_sync_timeline_create_fence(timeline, 1);
	igt_plane_set_fence_fd(plane, fence_fd);

	igt_pipe_obj_set_prop_value(pipe, IGT_CRTC_ACTIVE, 0);
	igt_pipe_obj_clear_prop_changed(pipe, IGT_CRTC_OUT_FENCE_PTR);

	crtc_commit_atomic_flags_err(pipe, plane, 0, ATOMIC_RELAX_NONE, EINVAL);

	/* valid out fence ptr and flip event but not allowed prop on crtc */
	igt_pipe_request_out_fence(pipe);
	crtc_commit_atomic_flags_err(pipe, plane, DRM_MODE_PAGE_FLIP_EVENT,
				     ATOMIC_RELAX_NONE, EINVAL);

	/* valid flip event but not allowed prop on crtc */
	igt_pipe_obj_clear_prop_changed(pipe, IGT_CRTC_OUT_FENCE_PTR);
	crtc_commit_atomic_flags_err(pipe, plane, DRM_MODE_PAGE_FLIP_EVENT,
				     ATOMIC_RELAX_NONE, EINVAL);

	igt_pipe_obj_set_prop_value(pipe, IGT_CRTC_ACTIVE, 1);

	/* Configuration should be valid again */
	crtc_commit_atomic_flags_err(pipe, plane, DRM_MODE_ATOMIC_TEST_ONLY,
				     ATOMIC_RELAX_NONE, 0);

	/* Set invalid prop */
	igt_pipe_obj_set_prop_value(pipe, IGT_CRTC_MODE_ID, fb->fb_id);

	/* valid out fence but invalid prop on crtc */
	igt_pipe_request_out_fence(pipe);
	crtc_commit_atomic_flags_err(pipe, plane, 0,
				     ATOMIC_RELAX_NONE, EINVAL);

	/* valid out fence ptr and flip event but invalid prop on crtc */
	crtc_commit_atomic_flags_err(pipe, plane, DRM_MODE_PAGE_FLIP_EVENT,
				     ATOMIC_RELAX_NONE, EINVAL);

	/* valid page flip event but invalid prop on crtc */
	crtc_commit_atomic_flags_err(pipe, plane, DRM_MODE_PAGE_FLIP_EVENT,
				     ATOMIC_RELAX_NONE, EINVAL);

	/* successful TEST_ONLY with fences set */
	igt_pipe_obj_set_prop_value(pipe, IGT_CRTC_MODE_ID, old_mode_id);
	crtc_commit_atomic_flags_err(pipe, plane, DRM_MODE_ATOMIC_TEST_ONLY,
				     ATOMIC_RELAX_NONE, 0);
	igt_assert(pipe->out_fence_fd == -1);
	close(fence_fd);
	close(timeline);

	/* reset fences */
	igt_plane_set_fence_fd(plane, -1);
	igt_pipe_obj_set_prop_value(pipe, IGT_CRTC_OUT_FENCE_PTR, 0);
	igt_pipe_obj_clear_prop_changed(pipe, IGT_CRTC_OUT_FENCE_PTR);
	crtc_commit(pipe, plane, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);

	/* out fence ptr but not page flip event */
	igt_pipe_request_out_fence(pipe);
	crtc_commit(pipe, plane, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);

	igt_assert(pipe->out_fence_fd != -1);
}

/* Abuse the atomic ioctl directly in order to test various invalid conditions,
 * which the libdrm wrapper won't allow us to create. */
static void atomic_invalid_params(igt_pipe_t *pipe,
				  igt_plane_t *plane,
				  igt_output_t *output,
				  struct igt_fb *fb)
{
	igt_display_t *display = pipe->display;
	struct drm_mode_atomic ioc;
	uint32_t obj_raw[16]; /* array of objects (sized by count_objs) */
	uint32_t num_props_raw[16]; /* array of num props per obj (ditto) */
	uint32_t props_raw[256]; /* array of props (sum of count_props) */
	uint64_t values_raw[256]; /* array of values for properties (ditto) */
	int i;

	memset(&ioc, 0, sizeof(ioc));

	/* An empty request should do nothing. */
	do_ioctl(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc);

	for (i = 0; i < ARRAY_SIZE(obj_raw); i++)
		obj_raw[i] = 0;
	for (i = 0; i < ARRAY_SIZE(num_props_raw); i++)
		num_props_raw[i] = 0;
	for (i = 0; i < ARRAY_SIZE(props_raw); i++)
		props_raw[i] = 0;
	for (i = 0; i < ARRAY_SIZE(values_raw); i++)
		values_raw[i] = 0;

	ioc.objs_ptr = (uintptr_t) obj_raw;
	ioc.count_props_ptr = (uintptr_t) num_props_raw;
	ioc.props_ptr = (uintptr_t) props_raw;
	ioc.prop_values_ptr = (uintptr_t) values_raw;

	/* Valid pointers, but still should copy nothing. */
	do_ioctl(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc);

	/* Valid noop, but with event set should fail. */
	ioc.flags = DRM_MODE_PAGE_FLIP_EVENT;
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, EINVAL);

	/* Nonsense flags. */
	ioc.flags = 0xdeadbeef;
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, EINVAL);

	ioc.flags = 0;
	/* Safety check that flags is reset properly. */
	do_ioctl(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc);

	/* Reserved/MBZ. */
	ioc.reserved = 1;
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, EINVAL);
	ioc.reserved = 0;
	do_ioctl(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc);

	/* Zero is not a valid object ID. */
	ioc.count_objs = ARRAY_SIZE(obj_raw);
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, ENOENT);

	/* Invalid object type (not a thing we can set properties on). */
	ioc.count_objs = 1;
	obj_raw[0] = pipe->values[IGT_CRTC_MODE_ID];
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, ENOENT);
	obj_raw[0] = fb->fb_id;
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, ENOENT);

	/* Filled object but with no properties; no-op. */
	for (i = 0; i < ARRAY_SIZE(obj_raw); i++)
		obj_raw[i] = pipe->crtc_id;
	do_ioctl(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc);

	/* Pass in all sorts of things other than the property ID. */
	num_props_raw[0] = 1;
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, ENOENT);
	props_raw[0] = pipe->crtc_id;
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, ENOENT);
	props_raw[0] = plane->drm_plane->plane_id;
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, ENOENT);
	props_raw[0] = output->id;
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, ENOENT);
	props_raw[0] = pipe->values[IGT_CRTC_MODE_ID];
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, ENOENT);

	/* Valid property, valid value. */

	for (i = 0; i < ARRAY_SIZE(props_raw); i++) {
		props_raw[i] = pipe->props[IGT_CRTC_MODE_ID];
		values_raw[i] = pipe->values[IGT_CRTC_MODE_ID];
	}
	do_ioctl(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc);

	/* Setting the same thing multiple times is OK. */
	for (i = 0; i < ARRAY_SIZE(obj_raw); i++)
		num_props_raw[i] = ARRAY_SIZE(props_raw) / ARRAY_SIZE(obj_raw);
	do_ioctl(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc);
	ioc.count_objs = ARRAY_SIZE(obj_raw);
	do_ioctl(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc);

	/* Pass a series of outlandish addresses. */
	ioc.objs_ptr = 0;
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, EFAULT);

	ioc.objs_ptr = (uintptr_t) obj_raw;
	ioc.count_props_ptr = 0;
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, EFAULT);

	ioc.count_props_ptr = (uintptr_t) num_props_raw;
	ioc.props_ptr = 0;
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, EFAULT);

	ioc.props_ptr = (uintptr_t) props_raw;
	ioc.prop_values_ptr = 0;
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, EFAULT);

	ioc.prop_values_ptr = (uintptr_t) values_raw;
	do_ioctl(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc);

	/* Attempt to overflow and/or trip various boundary conditions. */
	ioc.count_objs = UINT32_MAX / sizeof(uint32_t);
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, ENOENT);

	ioc.count_objs = ARRAY_SIZE(obj_raw);
	ioc.objs_ptr = UINT64_MAX - sizeof(uint32_t);
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, EFAULT);
	ioc.count_objs = 1;
	ioc.objs_ptr = UINT64_MAX - sizeof(uint32_t);
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, EFAULT);

	num_props_raw[0] = UINT32_MAX / sizeof(uint32_t);
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, EFAULT);
	num_props_raw[0] = UINT32_MAX - 1;
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, EFAULT);

	for (i = 0; i < ARRAY_SIZE(obj_raw); i++)
		num_props_raw[i] = (UINT32_MAX / ARRAY_SIZE(obj_raw)) + 1;
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, EFAULT);
	for (i = 0; i < ARRAY_SIZE(obj_raw); i++)
		num_props_raw[i] = ARRAY_SIZE(props_raw) / ARRAY_SIZE(obj_raw);
	do_ioctl_err(display->drm_fd, DRM_IOCTL_MODE_ATOMIC, &ioc, EFAULT);
}

static void atomic_setup(igt_display_t *display, enum pipe pipe, igt_output_t *output, igt_plane_t *primary, struct igt_fb *fb)
{
	igt_output_set_pipe(output, pipe);
	igt_plane_set_fb(primary, fb);

	crtc_commit(primary->pipe, primary, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);
}

static void atomic_clear(igt_display_t *display, enum pipe pipe, igt_plane_t *primary, igt_output_t *output)
{
	igt_plane_t *plane;

	for_each_plane_on_pipe(display, pipe, plane) {
		igt_plane_set_fb(plane, NULL);
		igt_plane_set_position(plane, 0, 0);
	}

	igt_output_set_pipe(output, PIPE_NONE);
	crtc_commit(primary->pipe, primary, COMMIT_ATOMIC, ATOMIC_RELAX_NONE);
}

igt_main
{
	igt_display_t display;
	enum pipe pipe = PIPE_NONE;
	igt_pipe_t *pipe_obj;
	igt_output_t *output = NULL;
	igt_plane_t *primary = NULL;
	drmModeModeInfo *mode;
	struct igt_fb fb;

	igt_fixture {
		display.drm_fd = drm_open_driver_master(DRIVER_ANY);

		kmstest_set_vt_graphics_mode();

		igt_display_require(&display, display.drm_fd);
		igt_require(display.is_atomic);
		igt_display_require_output(&display);

		for_each_pipe_with_valid_output(&display, pipe, output)
			break;

		pipe_obj = &display.pipes[pipe];
		primary = igt_pipe_get_plane_type(pipe_obj, DRM_PLANE_TYPE_PRIMARY);

		mode = igt_output_get_mode(output);

		igt_create_pattern_fb(display.drm_fd,
				      mode->hdisplay, mode->vdisplay,
				      plane_get_igt_format(primary),
				      LOCAL_DRM_FORMAT_MOD_NONE, &fb);
	}

	igt_subtest("plane_overlay_legacy") {
		igt_plane_t *overlay =
			igt_pipe_get_plane_type(pipe_obj, DRM_PLANE_TYPE_OVERLAY);

		igt_require(overlay);

		atomic_setup(&display, pipe, output, primary, &fb);
		plane_overlay(pipe_obj, output, overlay);
	}

	igt_subtest("plane_primary_legacy") {
		atomic_setup(&display, pipe, output, primary, &fb);

		plane_primary(pipe_obj, primary, &fb);
	}

	igt_subtest("plane_primary_overlay_zpos") {
		uint32_t format_primary = DRM_FORMAT_ARGB8888;
		uint32_t format_overlay = DRM_FORMAT_ARGB1555;

		igt_plane_t *overlay =
			igt_pipe_get_plane_type(pipe_obj, DRM_PLANE_TYPE_OVERLAY);

		igt_require(overlay);
		igt_require(igt_plane_has_prop(primary, IGT_PLANE_ZPOS));
		igt_require(igt_plane_has_prop(overlay, IGT_PLANE_ZPOS));

		igt_require(igt_plane_has_format_mod(primary, format_primary, 0x0));
		igt_require(igt_plane_has_format_mod(overlay, format_overlay, 0x0));

		igt_output_set_pipe(output, pipe);
		plane_primary_overlay_zpos(pipe_obj, output, primary, overlay,
					   format_primary, format_overlay);
	}

	igt_subtest("test_only") {
		atomic_clear(&display, pipe, primary, output);

		test_only(pipe_obj, primary, output);
	}
	igt_subtest("plane_cursor_legacy") {
		igt_plane_t *cursor =
			igt_pipe_get_plane_type(pipe_obj, DRM_PLANE_TYPE_CURSOR);

		igt_require(cursor);

		atomic_setup(&display, pipe, output, primary, &fb);
		plane_cursor(pipe_obj, output, cursor);
	}

	igt_subtest("plane_invalid_params") {
		atomic_setup(&display, pipe, output, primary, &fb);

		plane_invalid_params(pipe_obj, output, primary, &fb);
	}

	igt_subtest("plane_invalid_params_fence") {
		atomic_setup(&display, pipe, output, primary, &fb);

		plane_invalid_params_fence(pipe_obj, output, primary);
	}

	igt_subtest("crtc_invalid_params") {
		atomic_setup(&display, pipe, output, primary, &fb);

		crtc_invalid_params(pipe_obj, output, primary, &fb);
	}

	igt_subtest("crtc_invalid_params_fence") {
		atomic_setup(&display, pipe, output, primary, &fb);

		crtc_invalid_params_fence(pipe_obj, output, primary, &fb);
	}

	igt_subtest("atomic_invalid_params") {
		atomic_setup(&display, pipe, output, primary, &fb);

		atomic_invalid_params(pipe_obj, primary, output, &fb);
	}

	igt_fixture {
		atomic_clear(&display, pipe, primary, output);
		igt_remove_fb(display.drm_fd, &fb);

		igt_display_fini(&display);
	}
}
