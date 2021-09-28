/*
 * Copyright © 2013 Intel Corporation
 * Copyright © 2018 Collabora, Ltd.
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
 *    Daniel Vetter <daniel.vetter@ffwll.ch>
 *    Daniel Stone <daniels@collabora.com>
 *
 */

#include "igt.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "drm.h"
#include "drm_fourcc.h"

static bool has_getfb_iface(int fd)
{
	struct drm_mode_fb_cmd arg = { };
	int err;

	err = 0;
	if (drmIoctl(fd, DRM_IOCTL_MODE_GETFB, &arg))
		err = -errno;
	switch (err) {
	case -ENOTTY: /* ioctl unrecognised (kernel too old) */
	case -ENOTSUP: /* driver doesn't support KMS */
		return false;
	default:
		return true;
	}
}

static bool has_addfb2_iface(int fd)
{
	struct drm_mode_fb_cmd2 arg = { };
	int err;

	err = 0;
	if (drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &arg))
		err = -errno;
	switch (err) {
	case -ENOTTY: /* ioctl unrecognised (kernel too old) */
	case -ENOTSUP: /* driver doesn't support KMS */
		return false;
	default:
		return true;
	}
}

static void get_ccs_fb(int fd, struct drm_mode_fb_cmd2 *ret)
{
	struct drm_mode_fb_cmd2 add = {
		.width = 1024,
		.height = 1024,
		.pixel_format = DRM_FORMAT_XRGB8888,
		.flags = DRM_MODE_FB_MODIFIERS,
		.modifier = {
			I915_FORMAT_MOD_Y_TILED_CCS,
			I915_FORMAT_MOD_Y_TILED_CCS,
		},
	};
	int size;

	igt_require(has_addfb2_iface(fd));
	igt_require_intel(fd);

	/* An explanation of the magic numbers can be found in kms_ccs.c. */
	add.pitches[0] = ALIGN(add.width * 4, 128);
	add.offsets[1] = add.pitches[0] * ALIGN(add.height, 32);
	add.pitches[1] = ALIGN(ALIGN(add.width * 4, 32) / 32, 128);

	size = add.offsets[1];
	size += add.pitches[1] * ALIGN(ALIGN(add.height, 16) / 16, 32);

	add.handles[0] = gem_create(fd, size);
	igt_require(add.handles[0] != 0);
	add.handles[1] = add.handles[0];

	if (drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &add) == 0)
		*ret = add;
	else
		gem_close(fd, add.handles[0]);
}

/**
 * Find and return an arbitrary valid property ID.
 */
static uint32_t get_any_prop_id(int fd)
{
	igt_display_t display;

	igt_display_require(&display, fd);
	for (int i = 0; i < display.n_outputs; i++) {
		igt_output_t *output = &display.outputs[i];
		if (output->props[IGT_CONNECTOR_DPMS] != 0)
			return output->props[IGT_CONNECTOR_DPMS];
	}

	return 0;
}

static void test_handle_input(int fd)
{
	struct drm_mode_fb_cmd2 add = {};

	igt_fixture {
		add.width = 1024;
		add.height = 1024;
		add.pixel_format = DRM_FORMAT_XRGB8888;
		add.pitches[0] = 1024*4;
		add.handles[0] = igt_create_bo_with_dimensions(fd, 1024, 1024,
			DRM_FORMAT_XRGB8888, 0, 0, NULL, NULL, NULL);
		igt_require(add.handles[0] != 0);
		do_ioctl(fd, DRM_IOCTL_MODE_ADDFB2, &add);
	}

	igt_subtest("getfb-handle-zero") {
		struct drm_mode_fb_cmd get = { .fb_id = 0 };
		do_ioctl_err(fd, DRM_IOCTL_MODE_GETFB, &get, ENOENT);
	}

	igt_subtest("getfb-handle-valid") {
		struct drm_mode_fb_cmd get = { .fb_id = add.fb_id };
		do_ioctl(fd, DRM_IOCTL_MODE_GETFB, &get);
		igt_assert_neq_u32(get.handle, 0);
		igt_assert_eq_u32(get.width, add.width);
		igt_assert_eq_u32(get.height, add.height);
		igt_assert_eq_u32(get.pitch, add.pitches[0]);
		igt_assert_eq_u32(get.depth, 24);
		igt_assert_eq_u32(get.bpp, 32);
		gem_close(fd, get.handle);
	}

	igt_subtest("getfb-handle-closed") {
		struct drm_mode_fb_cmd get = { .fb_id = add.fb_id };
		do_ioctl(fd, DRM_IOCTL_MODE_RMFB, &add.fb_id);
		do_ioctl_err(fd, DRM_IOCTL_MODE_GETFB, &get, ENOENT);
	}

	igt_subtest("getfb-handle-not-fb") {
		struct drm_mode_fb_cmd get = { .fb_id = get_any_prop_id(fd) };
		igt_require(get.fb_id > 0);
		do_ioctl_err(fd, DRM_IOCTL_MODE_GETFB, &get, ENOENT);
	}

	igt_fixture
		gem_close(fd, add.handles[0]);
}

static void test_duplicate_handles(int fd)
{
	struct drm_mode_fb_cmd2 add = {};

	igt_fixture {
		add.width = 1024;
		add.height = 1024;
		add.pixel_format = DRM_FORMAT_XRGB8888;
		add.pitches[0] = 1024*4;
		add.handles[0] = igt_create_bo_with_dimensions(fd, 1024, 1024,
			DRM_FORMAT_XRGB8888, 0, 0, NULL, NULL, NULL);
		igt_assert(add.handles[0]);
		do_ioctl(fd, DRM_IOCTL_MODE_ADDFB2, &add);
	}

	igt_subtest("getfb-addfb-different-handles") {
		struct drm_mode_fb_cmd get = { .fb_id = add.fb_id };

		do_ioctl(fd, DRM_IOCTL_MODE_GETFB, &get);
		igt_assert_neq_u32(get.handle, add.handles[0]);
		gem_close(fd, get.handle);
	}

	igt_subtest("getfb-repeated-different-handles") {
		struct drm_mode_fb_cmd get1 = { .fb_id = add.fb_id };
		struct drm_mode_fb_cmd get2 = { .fb_id = add.fb_id };

		do_ioctl(fd, DRM_IOCTL_MODE_GETFB, &get1);
		do_ioctl(fd, DRM_IOCTL_MODE_GETFB, &get2);
		igt_assert_neq_u32(get1.handle, get2.handle);

		gem_close(fd, get1.handle);
		gem_close(fd, get2.handle);
	}

	igt_subtest("getfb-reject-ccs") {
		struct drm_mode_fb_cmd2 add_ccs = { };
		struct drm_mode_fb_cmd get = { };

		get_ccs_fb(fd, &add_ccs);
		igt_require(add_ccs.handles[0] != 0);
		get.fb_id = add_ccs.fb_id;
		do_ioctl_err(fd, DRM_IOCTL_MODE_GETFB, &get, EINVAL);

		do_ioctl(fd, DRM_IOCTL_MODE_RMFB, &add_ccs.fb_id);
		gem_close(fd, add_ccs.handles[0]);
	}

	igt_fixture {
		do_ioctl(fd, DRM_IOCTL_MODE_RMFB, &add.fb_id);
		gem_close(fd, add.handles[0]);
	}
}

igt_main
{
	int fd;

	igt_fixture {
		fd = drm_open_driver_master(DRIVER_ANY);
		igt_require(has_getfb_iface(fd));
	}

	igt_subtest_group
		test_handle_input(fd);

	igt_subtest_group
		test_duplicate_handles(fd);

	igt_fixture
		close(fd);
}
