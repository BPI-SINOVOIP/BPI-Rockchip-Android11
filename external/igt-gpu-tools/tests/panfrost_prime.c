/*
 * Copyright © 2016 Broadcom
 * Copyright © 2019 Collabora, Ltd.
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
#include "igt_panfrost.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "panfrost_drm.h"

igt_main
{
	int fd, kms_fd;

	igt_fixture {
		kms_fd = drm_open_driver_master(DRIVER_ANY);
		fd = drm_open_driver(DRIVER_PANFROST);
	}

	igt_subtest("gem-prime-import") {
		struct panfrost_bo *bo;
		uint32_t handle, dumb_handle;
	        struct drm_panfrost_get_bo_offset get_bo_offset = {0,};
		int dmabuf_fd;

		/* Just to be sure that when we import the dumb buffer it has
		 * a non-NULL address.
		 */
		bo = igt_panfrost_gem_new(fd, 1024);

		dumb_handle = kmstest_dumb_create(kms_fd, 1024, 1024, 32, NULL, NULL);

		dmabuf_fd = prime_handle_to_fd(kms_fd, dumb_handle);

		handle = prime_fd_to_handle(fd, dmabuf_fd);

		get_bo_offset.handle = handle;
		do_ioctl(fd, DRM_IOCTL_PANFROST_GET_BO_OFFSET, &get_bo_offset);
		igt_assert(get_bo_offset.offset);

		gem_close(fd, handle);

		kmstest_dumb_destroy(kms_fd, dumb_handle);

		igt_panfrost_free_bo(fd, bo);
	}

	igt_fixture {
		close(fd);
		close(kms_fd);
	}
}
