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
 *
 */

#include <sys/stat.h>
#include <sys/sysmacros.h>
#include "igt.h"
#include "igt_device.h"

int __igt_device_set_master(int fd)
{
	int err;

	err = 0;
	if (drmIoctl(fd, DRM_IOCTL_SET_MASTER, NULL)) {
		err = -errno;
		igt_assume(err);
	}

	errno = 0;
	return err;
}

static void show_clients(int fd)
{
	__igt_debugfs_dump(fd, "clients", IGT_LOG_WARN);
}

/**
 * igt_device_set_master: Set the device fd to be DRM master
 * @fd: the device
 *
 * Tell the kernel to make this device fd become DRM master or skip the test.
 */
void igt_device_set_master(int fd)
{
	if (__igt_device_set_master(fd)) {
		show_clients(fd);
		igt_require_f(__igt_device_set_master(fd) == 0,
			      "Can't become DRM master, "
			      "please check if no other DRM client is running.\n");
	}
}

int __igt_device_drop_master(int fd)
{
	int err;

	err = 0;
	if (drmIoctl(fd, DRM_IOCTL_DROP_MASTER, NULL)) {
		err = -errno;
		igt_assume(err);
	}

	errno = 0;
	return err;
}

/**
 * igt_device_drop_master: Drop DRM master
 * @fd: the device
 *
 * Tell the kernel we no longer want this device fd to be the DRM master;
 * asserting that we lose the privilege. Return if we are master already.
 */
void igt_device_drop_master(int fd)
{
	/* Check if we are master before dropping */
	if (__igt_device_set_master(fd))
		return;

	if (__igt_device_drop_master(fd)) {
		show_clients(fd);
		igt_assert_f(__igt_device_drop_master(fd) == 0,
			      "Failed to drop DRM master.\n");
	}
}

/**
 * igt_device_get_card_index:
 * @fd: the device
 *
 * Returns:
 * Index (N) of /dev/dri/cardN or /dev/dri/renderDN corresponding with fd.
 *
 */
int igt_device_get_card_index(int fd)
{
	struct stat st;

	igt_fail_on(fstat(fd, &st) || !S_ISCHR(st.st_mode));

	return minor(st.st_rdev);
}
