/*
 * Copyright © 2015 Intel Corporation
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
 *    Ankitprasad Sharma <ankitprasad.r.sharma at intel.com>
 *
 */

/** @file gem_create.c
 *
 * This is a test for the extended and old gem_create ioctl, that
 * includes allocation of object from stolen memory and shmem.
 *
 * The goal is to simply ensure that basics work and invalid input
 * combinations are rejected.
 */

#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <getopt.h>
#include <pthread.h>
#include <stdatomic.h>

#include <drm.h>

#include "ioctl_wrappers.h"
#include "intel_bufmgr.h"
#include "intel_batchbuffer.h"
#include "intel_io.h"
#include "intel_chipset.h"
#include "igt_aux.h"
#include "drmtest.h"
#include "drm.h"
#include "i915_drm.h"

IGT_TEST_DESCRIPTION("This is a test for the extended & old gem_create ioctl,"
		     " that includes allocation of object from stolen memory"
		     " and shmem.");

#define CLEAR(s) memset(&s, 0, sizeof(s))
#define PAGE_SIZE 4096

struct local_i915_gem_create_v2 {
	uint64_t size;
	uint32_t handle;
	uint32_t pad;
#define I915_CREATE_PLACEMENT_STOLEN (1<<0)
	uint32_t flags;
} create_v2;

#define LOCAL_IOCTL_I915_GEM_CREATE       DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_CREATE, struct local_i915_gem_create_v2)

static void invalid_flag_test(int fd)
{
	int ret;

	gem_require_stolen_support(fd);

	create_v2.handle = 0;
	create_v2.size = PAGE_SIZE;
	create_v2.flags = ~I915_CREATE_PLACEMENT_STOLEN;
	ret = drmIoctl(fd, LOCAL_IOCTL_I915_GEM_CREATE, &create_v2);

	igt_assert(ret <= 0);

	create_v2.flags = ~0;
	ret = drmIoctl(fd, LOCAL_IOCTL_I915_GEM_CREATE, &create_v2);

	igt_assert(ret <= 0);
}

static int create_ioctl(int fd, struct drm_i915_gem_create *create)
{
        int err = 0;

        if (igt_ioctl(fd, DRM_IOCTL_I915_GEM_CREATE, create)) {
                err = -errno;
                igt_assume(err != 0);
        }

        errno = 0;
        return err;
}

static void invalid_size_test(int fd)
{
	struct drm_i915_gem_create create = {
		.size = 0,
	};

	igt_assert_eq(create_ioctl(fd, &create), -EINVAL);
}

/*
 * Creating an object with non-aligned size and trying to access it with an
 * offset, which is greater than the requested size but smaller than the
 * object's last page boundary. pwrite here must be successful.
 */
static void valid_nonaligned_size(int fd)
{
	struct drm_i915_gem_create create = {
		.size = PAGE_SIZE / 2,
	};
	char buf[PAGE_SIZE];

	igt_assert_eq(create_ioctl(fd, &create), 0);

	gem_write(fd, create.handle, PAGE_SIZE / 2, buf, PAGE_SIZE / 2);

	gem_close(fd, create.handle);
}

/*
 * Creating an object with non-aligned size and trying to access it with an
 * offset, which is greater than the requested size and larger than the
 * object's last page boundary. pwrite here must fail.
 */
static void invalid_nonaligned_size(int fd)
{
	struct drm_i915_gem_create create = {
		.size = PAGE_SIZE / 2,
	};
	char buf[PAGE_SIZE];

	igt_assert_eq(create_ioctl(fd, &create), 0);

	/* This should fail. Hence cannot use gem_write. */
	igt_assert(__gem_write(fd, create.handle,
			       PAGE_SIZE / 2, buf, PAGE_SIZE));

	gem_close(fd, create.handle);
}

static uint64_t atomic_compare_swap_u64(_Atomic(uint64_t) *ptr,
					uint64_t oldval, uint64_t newval)
{
	atomic_compare_exchange_strong(ptr, &oldval, newval);
	return oldval;
}

static uint64_t get_npages(_Atomic(uint64_t) *global, uint64_t npages)
{
	uint64_t try, old, max;

	max = *global;
	do {
		old = max;
		try = 1 + npages % (max / 2);
		max -= try;
	} while ((max = atomic_compare_swap_u64(global, old, max)) != old);

	return try;
}

struct thread_clear {
	_Atomic(uint64_t) max;
	int timeout;
	int i915;
};

static void *thread_clear(void *data)
{
	struct thread_clear *arg = data;
	unsigned long checked = 0;
	int i915 = arg->i915;

	igt_until_timeout(arg->timeout) {
		struct drm_i915_gem_create create = {};
		uint64_t npages;

		npages = random();
		npages <<= 32;
		npages |= random();
		npages = get_npages(&arg->max, npages);
		create.size = npages << 12;

		create_ioctl(i915, &create);
		for (uint64_t page = 0; page < npages; page++) {
			uint64_t x;

			gem_read(i915, create.handle,
				 page * 4096 + (page % (4096 - sizeof(x))),
				 &x, sizeof(x));
			igt_assert_eq_u64(x, 0);
		}
		gem_close(i915, create.handle);
		checked += npages;

		atomic_fetch_add(&arg->max, npages);
	}

	return (void *)(uintptr_t)checked;
}

static void always_clear(int i915, int timeout)
{
	struct thread_clear arg = {
		.i915 = i915,
		.timeout = timeout,
		.max = intel_get_avail_ram_mb() << (20 - 12), /* in pages */
	};
	const int ncpus = sysconf(_SC_NPROCESSORS_ONLN);
	unsigned long checked;
	pthread_t thread[ncpus];
	void *result;

	for (int i = 0; i < ncpus; i++)
		pthread_create(&thread[i], NULL, thread_clear, &arg);

	checked = 0;
	for (int i = 0; i < ncpus; i++) {
		pthread_join(thread[i], &result);
		checked += (uintptr_t)result;
	}
	igt_info("Checked %'lu page allocations\n", checked);
}

static void size_update(int fd)
{
	int size_initial_nonaligned = 15;

	struct drm_i915_gem_create create = {
		.size = size_initial_nonaligned,
	};

	igt_assert_eq(create_ioctl(fd, &create), 0);
	igt_assert_neq(create.size, size_initial_nonaligned);
}

igt_main
{
	int fd = -1;

	igt_skip_on_simulation();

	igt_fixture {
		fd = drm_open_driver(DRIVER_INTEL);
	}

	igt_subtest("stolen-invalid-flag")
		invalid_flag_test(fd);

	igt_subtest("create-invalid-size")
		invalid_size_test(fd);

	igt_subtest("create-valid-nonaligned")
		valid_nonaligned_size(fd);

	igt_subtest("create-invalid-nonaligned")
		invalid_nonaligned_size(fd);

	igt_subtest("create-size-update")
		size_update(fd);

	igt_subtest("create-clear")
		always_clear(fd, 30);
}
