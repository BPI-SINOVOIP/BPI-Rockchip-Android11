/*
 * Copyright © 2016 Intel Corporation
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
#include "igt_vgem.h"
#include "igt_debugfs.h"
#include "igt_sysfs.h"

#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <dirent.h>

IGT_TEST_DESCRIPTION("Extended sanity check of Virtual GEM module (vGEM).");

static bool has_prime_export(int fd)
{
	uint64_t value;

	if (drmGetCap(fd, DRM_CAP_PRIME, &value))
		return false;

	return value & DRM_PRIME_CAP_EXPORT;
}


static void test_nohang(int fd)
{
	struct vgem_bo bo;
	uint32_t fence;
	struct pollfd pfd;

	/* A vGEM fence must expire automatically to prevent driver hangs */

	igt_require(has_prime_export(fd));
	igt_require(vgem_has_fences(fd));

	bo.width = 1;
	bo.height = 1;
	bo.bpp = 32;
	vgem_create(fd, &bo);

	pfd.fd = prime_handle_to_fd(fd, bo.handle);
	pfd.events = POLLOUT;

	fence = vgem_fence_attach(fd, &bo, 0);

	igt_assert(poll(&pfd, 1, 0) == 0);
	igt_assert(poll(&pfd, 1, 60*1000) == 1);

	igt_assert_eq(__vgem_fence_signal(fd, fence), -110);
	close(pfd.fd);
	gem_close(fd, bo.handle);
}

igt_main
{
	int fd = -1;

	igt_fixture {
		fd = drm_open_driver(DRIVER_VGEM);
	}

	igt_subtest_f("nohang")
		test_nohang(fd);

	igt_fixture {
		close(fd);
	}
}
