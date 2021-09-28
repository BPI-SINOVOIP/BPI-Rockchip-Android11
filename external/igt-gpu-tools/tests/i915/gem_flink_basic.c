/*
 * Copyright © 2008 Intel Corporation
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
 *    Eric Anholt <eric@anholt.net>
 *
 */

#include "igt.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "drm.h"

IGT_TEST_DESCRIPTION("Tests for flink - a way to export a gem object by name");

static void
test_flink(int fd)
{
	struct drm_i915_gem_create create;
	struct drm_gem_flink flink;
	struct drm_gem_open open_struct;
	int ret;

	igt_debug("Testing flink and open.\n");

	memset(&create, 0, sizeof(create));
	create.size = 16 * 1024;
	ret = ioctl(fd, DRM_IOCTL_I915_GEM_CREATE, &create);
	igt_assert_eq(ret, 0);

	flink.handle = create.handle;
	ret = ioctl(fd, DRM_IOCTL_GEM_FLINK, &flink);
	igt_assert_eq(ret, 0);

	open_struct.name = flink.name;
	ret = ioctl(fd, DRM_IOCTL_GEM_OPEN, &open_struct);
	igt_assert_eq(ret, 0);
	igt_assert(open_struct.handle != 0);
}

static void
test_double_flink(int fd)
{
	struct drm_i915_gem_create create;
	struct drm_gem_flink flink;
	struct drm_gem_flink flink2;
	int ret;

	igt_debug("Testing repeated flink.\n");

	memset(&create, 0, sizeof(create));
	create.size = 16 * 1024;
	ret = ioctl(fd, DRM_IOCTL_I915_GEM_CREATE, &create);
	igt_assert_eq(ret, 0);

	flink.handle = create.handle;
	ret = ioctl(fd, DRM_IOCTL_GEM_FLINK, &flink);
	igt_assert_eq(ret, 0);

	flink2.handle = create.handle;
	ret = ioctl(fd, DRM_IOCTL_GEM_FLINK, &flink2);
	igt_assert_eq(ret, 0);

	/* flinks for same gem object share the same name */
	igt_assert(flink2.name == flink.name);
}

static void
test_bad_flink(int fd)
{
	struct drm_gem_flink flink;
	int ret;

	igt_debug("Testing error return on bad flink ioctl.\n");

	flink.handle = 0x10101010;
	ret = ioctl(fd, DRM_IOCTL_GEM_FLINK, &flink);
	igt_assert(ret == -1 && errno == ENOENT);
}

static void
test_bad_open(int fd)
{
	struct drm_gem_open open_struct;
	int ret;

	igt_debug("Testing error return on bad open ioctl.\n");

	open_struct.name = 0x10101010;
	ret = ioctl(fd, DRM_IOCTL_GEM_OPEN, &open_struct);

	igt_assert(ret == -1 && errno == ENOENT);
}

static void
test_flink_lifetime(int fd)
{
	struct drm_i915_gem_create create;
	struct drm_gem_flink flink;
	struct drm_gem_open open_struct;
	int ret, fd2;

	igt_debug("Testing flink lifetime.\n");

	fd2 = drm_open_driver(DRIVER_INTEL);

	memset(&create, 0, sizeof(create));
	create.size = 16 * 1024;
	ret = ioctl(fd2, DRM_IOCTL_I915_GEM_CREATE, &create);
	igt_assert_eq(ret, 0);

	flink.handle = create.handle;
	ret = ioctl(fd2, DRM_IOCTL_GEM_FLINK, &flink);
	igt_assert_eq(ret, 0);

	/* Open a second reference to the gem object with different fd */
	open_struct.name = flink.name;
	ret = ioctl(fd, DRM_IOCTL_GEM_OPEN, &open_struct);
	igt_assert_eq(ret, 0);
	igt_assert(open_struct.handle != 0);

	close(fd2);
	fd2 = drm_open_driver(DRIVER_INTEL);

	/* Flink name remains valid due to the second reference */
	open_struct.name = flink.name;
	ret = ioctl(fd2, DRM_IOCTL_GEM_OPEN, &open_struct);
	igt_assert_eq(ret, 0);
	igt_assert(open_struct.handle != 0);
}

int fd;

igt_main
{
	igt_fixture
		fd = drm_open_driver(DRIVER_INTEL);

	igt_subtest("basic")
		test_flink(fd);
	igt_subtest("double-flink")
		test_double_flink(fd);
	igt_subtest("bad-flink")
		test_bad_flink(fd);
	igt_subtest("bad-open")
		test_bad_open(fd);

	/* Flink lifetime is limited to that of the gem object it points to */
	igt_subtest("flink-lifetime")
		test_flink_lifetime(fd);
}
