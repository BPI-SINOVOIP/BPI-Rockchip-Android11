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

static void
set_label(int fd, int handle, const char *name, int err)
{
	struct drm_vc4_label_bo label = {
		.handle = handle,
		.len = strlen(name),
		.name = (uintptr_t)name,
	};

	if (err)
		do_ioctl_err(fd, DRM_IOCTL_VC4_LABEL_BO, &label, err);
	else
		do_ioctl(fd, DRM_IOCTL_VC4_LABEL_BO, &label);
}

igt_main
{
	int fd;

	igt_fixture
		fd = drm_open_driver(DRIVER_VC4);

	igt_subtest("set-label") {
		int handle = igt_vc4_create_bo(fd, 4096);
		set_label(fd, handle, "a test label", 0);
		set_label(fd, handle, "a new test label", 0);
		gem_close(fd, handle);
	}

	igt_subtest("set-bad-handle") {
		set_label(fd, 0xd0d0d0d0, "bad handle", ENOENT);
	}

	igt_subtest("set-bad-name") {
		int handle = igt_vc4_create_bo(fd, 4096);

		struct drm_vc4_label_bo label = {
			.handle = handle,
			.len = 1000,
			.name = 0,
		};

		do_ioctl_err(fd, DRM_IOCTL_VC4_LABEL_BO, &label, EFAULT);

		gem_close(fd, handle);
	}

	igt_subtest("set-kernel-name") {
		int handle = igt_vc4_create_bo(fd, 4096);
		set_label(fd, handle, "BCL", 0);
		set_label(fd, handle, "a test label", 0);
		set_label(fd, handle, "BCL", 0);
		gem_close(fd, handle);
	}

	igt_fixture
		close(fd);
}
