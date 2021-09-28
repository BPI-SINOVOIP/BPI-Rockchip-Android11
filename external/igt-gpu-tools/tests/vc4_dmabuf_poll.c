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
poll_write_bo_test(int fd, int poll_flag)
{
	size_t size = 1024 * 1024 * 4;
	uint32_t clearval = 0xaabbccdd;
	/* Get a BO that's being rendered to. */
	int handle = igt_vc4_get_cleared_bo(fd, size, clearval);
	int dmabuf_fd = prime_handle_to_fd(fd, handle);
	struct pollfd p = {
		.fd = dmabuf_fd,
		.events = POLLIN,
	};
	struct drm_vc4_wait_bo wait = {
		.handle = handle,
		.timeout_ns = 0,
	};

	/* Block for a couple of minutes waiting for rendering to complete. */
	int poll_ret = poll(&p, 1, 120 * 1000);
	igt_assert(poll_ret == 1);

	/* Now that we've waited for idle, a nonblocking wait for the
	 * BO should pass.
	 */
	do_ioctl(fd, DRM_IOCTL_VC4_WAIT_BO, &wait);

	close(dmabuf_fd);
	gem_close(fd, handle);
}

igt_main
{
	int fd;

	igt_fixture
		fd = drm_open_driver(DRIVER_VC4);

	igt_subtest("poll-write-waits-until-write-done") {
		poll_write_bo_test(fd, POLLOUT);
	}

	igt_subtest("poll-read-waits-until-write-done") {
		poll_write_bo_test(fd, POLLIN);
	}

	igt_fixture
		close(fd);
}
