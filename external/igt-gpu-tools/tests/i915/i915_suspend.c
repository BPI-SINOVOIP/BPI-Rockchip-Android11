/*
 * Copyright © 2013, 2015 Intel Corporation
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
 *    David Weinehall <david.weinehall@intel.com>
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

#include <drm.h>

#include "igt_device.h"

#define OBJECT_SIZE (16*1024*1024)

static void
test_fence_restore(int fd, bool tiled2untiled, bool hibernate)
{
	uint32_t handle1, handle2, handle_tiled;
	uint32_t *ptr1, *ptr2, *ptr_tiled;
	int i;

	/* We wall the tiled object with untiled canary objects to make sure
	 * that we detect tile leaking in both directions. */
	handle1 = gem_create(fd, OBJECT_SIZE);
	handle2 = gem_create(fd, OBJECT_SIZE);
	handle_tiled = gem_create(fd, OBJECT_SIZE);

	/* Access the buffer objects in the order we want to have the laid out. */
	ptr1 = gem_mmap__gtt(fd, handle1, OBJECT_SIZE, PROT_READ | PROT_WRITE);
	gem_set_domain(fd, handle1, I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	for (i = 0; i < OBJECT_SIZE/sizeof(uint32_t); i++)
		ptr1[i] = i;

	ptr_tiled = gem_mmap__gtt(fd, handle_tiled, OBJECT_SIZE,
				  PROT_READ | PROT_WRITE);
	gem_set_domain(fd, handle_tiled,
		       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	if (tiled2untiled)
		gem_set_tiling(fd, handle_tiled, I915_TILING_X, 2048);
	for (i = 0; i < OBJECT_SIZE/sizeof(uint32_t); i++)
		ptr_tiled[i] = i;

	ptr2 = gem_mmap__gtt(fd, handle2, OBJECT_SIZE, PROT_READ | PROT_WRITE);
	gem_set_domain(fd, handle2, I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	for (i = 0; i < OBJECT_SIZE/sizeof(uint32_t); i++)
		ptr2[i] = i;

	if (tiled2untiled)
		gem_set_tiling(fd, handle_tiled, I915_TILING_NONE, 2048);
	else
		gem_set_tiling(fd, handle_tiled, I915_TILING_X, 2048);

	if (hibernate)
		igt_system_suspend_autoresume(SUSPEND_STATE_DISK,
					      SUSPEND_TEST_NONE);
	else
		igt_system_suspend_autoresume(SUSPEND_STATE_MEM,
					      SUSPEND_TEST_NONE);

	igt_info("checking the first canary object\n");
	for (i = 0; i < OBJECT_SIZE/sizeof(uint32_t); i++)
		igt_assert(ptr1[i] == i);

	igt_info("checking the second canary object\n");
	for (i = 0; i < OBJECT_SIZE/sizeof(uint32_t); i++)
		igt_assert(ptr2[i] == i);

	gem_close(fd, handle1);
	gem_close(fd, handle2);
	gem_close(fd, handle_tiled);

	munmap(ptr1, OBJECT_SIZE);
	munmap(ptr2, OBJECT_SIZE);
	munmap(ptr_tiled, OBJECT_SIZE);
}

static void
test_debugfs_reader(int fd, bool hibernate)
{
	struct igt_helper_process reader = {};
	reader.use_SIGKILL = true;

	igt_fork_helper(&reader) {
		static const char dfs_base[] = "/sys/kernel/debug/dri";
		static char tmp[1024];

		snprintf(tmp, sizeof(tmp) - 1,
			 "while true; do find %s/%i/ -type f ! -path \"*/crc/*\" | xargs cat > /dev/null 2>&1; done",
			 dfs_base, igt_device_get_card_index(fd));
		igt_assert(execl("/bin/sh", "sh", "-c", tmp, (char *) NULL) != -1);
	}

	sleep(1);

	if (hibernate)
		igt_system_suspend_autoresume(SUSPEND_STATE_DISK,
					      SUSPEND_TEST_NONE);
	else
		igt_system_suspend_autoresume(SUSPEND_STATE_MEM,
					      SUSPEND_TEST_NONE);

	sleep(1);

	igt_stop_helper(&reader);
}

static void
test_sysfs_reader(int fd, bool hibernate)
{
	struct igt_helper_process reader = {};
	reader.use_SIGKILL = true;

	igt_fork_helper(&reader) {
		static const char dfs_base[] = "/sys/class/drm/card";
		static char tmp[1024];

		snprintf(tmp, sizeof(tmp) - 1,
			 "while true; do find %s%i*/ -type f | xargs cat > /dev/null 2>&1; done",
			 dfs_base, igt_device_get_card_index(fd));
		igt_assert(execl("/bin/sh", "sh", "-c", tmp, (char *) NULL) != -1);
	}

	sleep(1);

	if (hibernate)
		igt_system_suspend_autoresume(SUSPEND_STATE_DISK,
					      SUSPEND_TEST_NONE);
	else
		igt_system_suspend_autoresume(SUSPEND_STATE_MEM,
					      SUSPEND_TEST_NONE);

	sleep(1);

	igt_stop_helper(&reader);
}

static void
test_shrink(int fd, unsigned int mode)
{
	size_t size;
	void *mem;

	gem_quiescent_gpu(fd);
	intel_purge_vm_caches(fd);

	mem = intel_get_total_pinnable_mem(&size);
	igt_assert(mem != MAP_FAILED);

	intel_purge_vm_caches(fd);
	igt_system_suspend_autoresume(mode, SUSPEND_TEST_NONE);

	munmap(mem, size);
}

static void
test_forcewake(int fd, bool hibernate)
{
	int fw_fd;

	fw_fd = igt_open_forcewake_handle(fd);
	igt_assert_lte(0, fw_fd);

	if (hibernate)
		igt_system_suspend_autoresume(SUSPEND_STATE_DISK,
					      SUSPEND_TEST_NONE);
	else
		igt_system_suspend_autoresume(SUSPEND_STATE_MEM,
					      SUSPEND_TEST_NONE);

	close (fw_fd);
}

int fd;

igt_main
{
	igt_skip_on_simulation();

	igt_fixture
		fd = drm_open_driver(DRIVER_INTEL);

	igt_subtest("fence-restore-tiled2untiled")
		test_fence_restore(fd, true, false);

	igt_subtest("fence-restore-untiled")
		test_fence_restore(fd, false, false);

	igt_subtest("debugfs-reader")
		test_debugfs_reader(fd, false);

	igt_subtest("sysfs-reader")
		test_sysfs_reader(fd, false);

	igt_subtest("shrink")
		test_shrink(fd, SUSPEND_STATE_MEM);

	igt_subtest("forcewake")
		test_forcewake(fd, false);

	igt_subtest("fence-restore-tiled2untiled-hibernate")
		test_fence_restore(fd, true, true);

	igt_subtest("fence-restore-untiled-hibernate")
		test_fence_restore(fd, false, true);

	igt_subtest("debugfs-reader-hibernate")
		test_debugfs_reader(fd, true);

	igt_subtest("sysfs-reader-hibernate")
		test_sysfs_reader(fd, true);

	igt_subtest("forcewake-hibernate")
		test_forcewake(fd, true);

	igt_fixture
		close(fd);
}
