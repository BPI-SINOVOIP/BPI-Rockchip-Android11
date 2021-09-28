/*
 * Copyright © 2017 Broadcom
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
#include "igt_vc4.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <poll.h>
#include "vc4_drm.h"

igt_main
{
	int fd;

	igt_fixture
		fd = drm_open_driver(DRIVER_VC4);

	igt_subtest("get-bad-handle") {
		struct drm_vc4_get_tiling get = {
			.handle = 0xd0d0d0d0,
		};
		do_ioctl_err(fd, DRM_IOCTL_VC4_GET_TILING, &get, ENOENT);
	}

	igt_subtest("set-bad-handle") {
		struct drm_vc4_set_tiling set = {
			.handle = 0xd0d0d0d0,
			.modifier = DRM_FORMAT_MOD_NONE,
		};
		do_ioctl_err(fd, DRM_IOCTL_VC4_SET_TILING, &set, ENOENT);
	}

	igt_subtest("get-bad-flags") {
		int bo = igt_vc4_create_bo(fd, 4096);
		struct drm_vc4_get_tiling get = {
			.handle = bo,
			.flags = 0xd0d0d0d0,
		};
		do_ioctl_err(fd, DRM_IOCTL_VC4_GET_TILING, &get, EINVAL);
		gem_close(fd, bo);
	}

	igt_subtest("set-bad-flags") {
		int bo = igt_vc4_create_bo(fd, 4096);
		struct drm_vc4_set_tiling set = {
			.handle = bo,
			.flags = 0xd0d0d0d0,
			.modifier = DRM_FORMAT_MOD_NONE,
		};
		do_ioctl_err(fd, DRM_IOCTL_VC4_SET_TILING, &set, EINVAL);
		gem_close(fd, bo);
	}

	igt_subtest("get-bad-modifier") {
		int bo = igt_vc4_create_bo(fd, 4096);
		struct drm_vc4_get_tiling get = {
			.handle = bo,
			.modifier = 0xd0d0d0d0,
		};
		do_ioctl_err(fd, DRM_IOCTL_VC4_GET_TILING, &get, EINVAL);
		gem_close(fd, bo);
	}

	igt_subtest("set-bad-modifier") {
		int bo = igt_vc4_create_bo(fd, 4096);
		struct drm_vc4_set_tiling set = {
			.handle = bo,
			.modifier = 0xd0d0d0d0,
		};
		do_ioctl_err(fd, DRM_IOCTL_VC4_SET_TILING, &set, EINVAL);
		gem_close(fd, bo);
	}

	igt_subtest("set-get") {
		int bo = igt_vc4_create_bo(fd, 4096);

		/* Default is untiled. */
		igt_assert(igt_vc4_get_tiling(fd, bo) == DRM_FORMAT_MOD_NONE);

		/* Set to tiled and check. */
		igt_vc4_set_tiling(fd, bo, DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED);
		igt_assert(igt_vc4_get_tiling(fd, bo) ==
			   DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED);

		/* Set it back and check. */
		igt_vc4_set_tiling(fd, bo, DRM_FORMAT_MOD_NONE);
		igt_assert(igt_vc4_get_tiling(fd, bo) == DRM_FORMAT_MOD_NONE);

		gem_close(fd, bo);
	}

	igt_subtest("get-after-free") {
		/* Some size that probably nobody else is using, to
		 * encourage getting the same BO back from the cache.
		 */
		int size = 91 * 4096;
		int bo;

		bo = igt_vc4_create_bo(fd, size);
		igt_vc4_set_tiling(fd, bo, DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED);
		gem_close(fd, bo);

		bo = igt_vc4_create_bo(fd, size);
		igt_assert(igt_vc4_get_tiling(fd, bo) == DRM_FORMAT_MOD_NONE);
		gem_close(fd, bo);
	}

	igt_fixture
		close(fd);
}
