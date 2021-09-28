/*
 * Copyright © 2017 Red Hat
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
#include "igt_syncobj.h"
#include <unistd.h>
#include <sys/ioctl.h>
#include "drm.h"

IGT_TEST_DESCRIPTION("Basic check for drm sync objects.");

/* destroy a random handle */
static void
test_bad_destroy(int fd)
{
	struct drm_syncobj_destroy destroy;
	int ret;

	destroy.handle = 0xdeadbeef;
	destroy.pad = 0;

	ret = ioctl(fd, DRM_IOCTL_SYNCOBJ_DESTROY, &destroy);

	igt_assert(ret == -1 && errno == EINVAL);
}

/* handle to fd a bad handle */
static void
test_bad_handle_to_fd(int fd)
{
	struct drm_syncobj_handle handle;

	handle.handle = 0xdeadbeef;
	handle.flags = 0;

	igt_assert_eq(__syncobj_handle_to_fd(fd, &handle), -EINVAL);
}

/* fd to handle a bad fd */
static void
test_bad_fd_to_handle(int fd)
{
	struct drm_syncobj_handle handle;

	handle.fd = -1;
	handle.flags = 0;

	igt_assert_eq(__syncobj_fd_to_handle(fd, &handle), -EINVAL);
}

/* fd to handle an fd but not a sync file one */
static void
test_illegal_fd_to_handle(int fd)
{
	struct drm_syncobj_handle handle;

	handle.fd = fd;
	handle.flags = 0;

	igt_assert_eq(__syncobj_fd_to_handle(fd, &handle), -EINVAL);
}

static void
test_bad_flags_fd_to_handle(int fd)
{
	struct drm_syncobj_handle handle = { 0 };

	handle.flags = 0xdeadbeef;
	igt_assert_eq(__syncobj_fd_to_handle(fd, &handle), -EINVAL);
}

static void
test_bad_flags_handle_to_fd(int fd)
{
	struct drm_syncobj_handle handle = { 0 };

	handle.flags = 0xdeadbeef;
	igt_assert_eq(__syncobj_handle_to_fd(fd, &handle), -EINVAL);
}

static void
test_bad_pad_handle_to_fd(int fd)
{
	struct drm_syncobj_handle handle = { 0 };

	handle.pad = 0xdeadbeef;
	igt_assert_eq(__syncobj_handle_to_fd(fd, &handle), -EINVAL);
}

static void
test_bad_pad_fd_to_handle(int fd)
{
	struct drm_syncobj_handle handle = { 0 };

	handle.pad = 0xdeadbeef;
	igt_assert_eq(__syncobj_fd_to_handle(fd, &handle), -EINVAL);
}



/* destroy with data in the padding */
static void
test_bad_destroy_pad(int fd)
{
	struct drm_syncobj_destroy destroy;
	int ret;

	destroy.handle = syncobj_create(fd, 0);
	destroy.pad = 0xdeadbeef;

	ret = ioctl(fd, DRM_IOCTL_SYNCOBJ_DESTROY, &destroy);

	igt_assert(ret == -1 && errno == EINVAL);

	syncobj_destroy(fd, destroy.handle);
}

static void
test_bad_create_flags(int fd)
{
	struct drm_syncobj_create create = { 0 };
	int ret;

	create.flags = 0xdeadbeef;
	ret = ioctl(fd, DRM_IOCTL_SYNCOBJ_CREATE, &create);
	igt_assert(ret == -1 && errno == EINVAL);
}

static void
test_create_signaled(int fd)
{
	uint32_t syncobj = syncobj_create(fd, LOCAL_SYNCOBJ_CREATE_SIGNALED);

	igt_assert_eq(syncobj_wait_err(fd, &syncobj, 1, 0, 0), 0);

	syncobj_destroy(fd, syncobj);
}

/*
 * currently don't do handle deduplication
 * test we get a different handle back.
 */
static void
test_valid_cycle(int fd)
{
	uint32_t first_handle, second_handle;
	int syncobj_fd;

	first_handle = syncobj_create(fd, 0);
	syncobj_fd = syncobj_handle_to_fd(fd, first_handle, 0);
	second_handle = syncobj_fd_to_handle(fd, syncobj_fd, 0);
	close(syncobj_fd);

	igt_assert(first_handle != second_handle);

	syncobj_destroy(fd, first_handle);
	syncobj_destroy(fd, second_handle);
}

static bool has_syncobj(int fd)
{
	uint64_t value;
	if (drmGetCap(fd, DRM_CAP_SYNCOBJ, &value))
		return false;
	return value ? true : false;
}

igt_main
{
	int fd = -1;

	igt_fixture {
		fd = drm_open_driver(DRIVER_ANY);
		igt_require(has_syncobj(fd));
	}


	igt_subtest("bad-destroy")
		test_bad_destroy(fd);

	igt_subtest("bad-create-flags")
		test_bad_create_flags(fd);

	igt_subtest("bad-handle-to-fd")
		test_bad_handle_to_fd(fd);

	igt_subtest("bad-fd-to-handle")
		test_bad_fd_to_handle(fd);

	igt_subtest("bad-flags-handle-to-fd")
		test_bad_flags_handle_to_fd(fd);

	igt_subtest("bad-flags-fd-to-handle")
		test_bad_flags_fd_to_handle(fd);

	igt_subtest("bad-pad-handle-to-fd")
		test_bad_pad_handle_to_fd(fd);

	igt_subtest("bad-pad-fd-to-handle")
		test_bad_pad_fd_to_handle(fd);

	igt_subtest("illegal-fd-to-handle")
		test_illegal_fd_to_handle(fd);

	igt_subtest("bad-destroy-pad")
		test_bad_destroy_pad(fd);

	igt_subtest("create-signaled")
		test_create_signaled(fd);

	igt_subtest("test-valid-cycle")
		test_valid_cycle(fd);

}
