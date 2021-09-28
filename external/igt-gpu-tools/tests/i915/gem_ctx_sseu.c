/*
 * Copyright © 2017-2018 Intel Corporation
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
 *    Lionel Landwerlin <lionel.g.landwerlin@intel.com>
 *
 */

#include "igt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include "igt_dummyload.h"
#include "igt_perf.h"
#include "igt_sysfs.h"
#include "ioctl_wrappers.h"

IGT_TEST_DESCRIPTION("Test context render powergating programming.");

static unsigned int __intel_gen__, __intel_devid__;
static uint64_t __slice_mask__, __subslice_mask__;
static unsigned int __slice_count__, __subslice_count__;

static uint64_t mask_minus_one(uint64_t mask)
{
	unsigned int i;

	for (i = 0; i < (sizeof(mask) * 8 - 1); i++) {
		if ((1ULL << i) & mask)
			return mask & ~(1ULL << i);
	}

	igt_assert(0);
	return 0;
}

static uint64_t mask_plus_one(uint64_t mask)
{
	unsigned int i;

	for (i = 0; i < (sizeof(mask) * 8 - 1); i++) {
		if (((1ULL << i) & mask) == 0)
			return mask | (1ULL << i);
	}

	igt_assert(0);
	return 0;
}

static uint64_t mask_minus(uint64_t mask, int n)
{
	unsigned int i;

	for (i = 0; i < n; i++)
		mask = mask_minus_one(mask);

	return mask;
}

static uint64_t mask_plus(uint64_t mask, int n)
{
	unsigned int i;

	for (i = 0; i < n; i++)
		mask = mask_plus_one(mask);

	return mask;
}

static bool
kernel_has_per_context_sseu_support(int fd)
{
	struct drm_i915_gem_context_param_sseu sseu = { };
	struct drm_i915_gem_context_param arg = {
		.param = I915_CONTEXT_PARAM_SSEU,
		.size = sizeof(sseu),
		.value = to_user_pointer(&sseu),
	};
	int ret;

	if (__gem_context_get_param(fd, &arg))
		return false;

	arg.value = to_user_pointer(&sseu);

	ret = __gem_context_set_param(fd, &arg);

	igt_assert(ret == 0 || ret == -ENODEV || ret == -EINVAL);

	return ret == 0;
}

static bool has_engine(int fd, unsigned int class, unsigned int instance)
{
	int pmu = perf_i915_open(I915_PMU_ENGINE_BUSY(class, instance));

	if (pmu >= 0)
		close(pmu);

	return pmu >= 0;
}

/*
 * Verify that invalid engines are rejected and valid ones are accepted.
 */
static void test_engines(int fd)
{
	struct drm_i915_gem_context_param_sseu sseu = { };
	struct drm_i915_gem_context_param arg = {
		.param = I915_CONTEXT_PARAM_SSEU,
		.ctx_id = gem_context_create(fd),
		.size = sizeof(sseu),
		.value = to_user_pointer(&sseu)
	};
	unsigned int class, instance;
	int last_with_engines;

	/* get_param */

	sseu.engine.engine_instance = -1; /* Assumed invalid. */
	igt_assert_eq(__gem_context_get_param(fd, &arg), -EINVAL);

	sseu.engine.engine_class = I915_ENGINE_CLASS_INVALID; /* Both invalid. */
	igt_assert_eq(__gem_context_get_param(fd, &arg), -EINVAL);

	sseu.engine.engine_instance = 0; /* Class invalid. */
	igt_assert_eq(__gem_context_get_param(fd, &arg), -EINVAL);
	sseu.engine.engine_class = I915_ENGINE_CLASS_RENDER;

	last_with_engines = -1;
	for (class = 0; class < ~0; class++) {
		for (instance = 0; instance < ~0; instance++) {
			int ret;

			sseu.engine.engine_class = class;
			sseu.engine.engine_instance = instance;

			ret = __gem_context_get_param(fd, &arg);

			if (has_engine(fd, class, instance)) {
				igt_assert_eq(ret, 0);
				last_with_engines = class;
			} else {
				igt_assert_eq(ret, -EINVAL);
				if (instance > 8) /* Skip over some instance holes. */
					break;
			}
		}

		if (class - last_with_engines > 8) /* Skip over some class holes. */
			break;
	}

	/*
	 * Get some proper values before trying to reprogram them onto
	 * an invalid engine.
	 */
	sseu.engine.engine_class = 0;
	sseu.engine.engine_instance = 0;
	gem_context_get_param(fd, &arg);

	/* set_param */

	sseu.engine.engine_instance = -1; /* Assumed invalid. */
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);

	sseu.engine.engine_class = I915_ENGINE_CLASS_INVALID; /* Both invalid. */
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);

	sseu.engine.engine_instance = 0; /* Class invalid. */
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);

	last_with_engines = -1;
	for (class = 0; class < ~0; class++) {
		for (instance = 0; instance < ~0; instance++) {
			int ret;

			sseu.engine.engine_class = class;
			sseu.engine.engine_instance = instance;

			ret = __gem_context_set_param(fd, &arg);

			if (has_engine(fd, class, instance)) {
				igt_assert(ret == 0 || ret == -ENODEV);
				last_with_engines = class;
			} else {
				igt_assert_eq(ret, -EINVAL);
				if (instance > 8) /* Skip over some instance holes. */
					break;
			}
		}

		if (class - last_with_engines > 8) /* Skip over some class holes. */
			break;
	}

	gem_context_destroy(fd, arg.ctx_id);
}

/*
 * Verify that invalid arguments are rejected.
 */
static void
test_invalid_args(int fd)
{
	struct drm_i915_gem_context_param arg = {
		.param = I915_CONTEXT_PARAM_SSEU,
		.ctx_id = gem_context_create(fd),
	};
	struct drm_i915_gem_context_param_sseu sseu = { };
	unsigned char *page[2];
	unsigned char *addr;
	unsigned int sz;

	/* get param */

	/* Invalid size. */
	arg.size = 1;
	igt_assert_eq(__gem_context_get_param(fd, &arg), -EINVAL);

	/* Query size. */
	arg.size = 0;
	igt_assert_eq(__gem_context_get_param(fd, &arg), 0);
	sz = arg.size;

	/* Bad pointers. */
	igt_assert_eq(__gem_context_get_param(fd, &arg), -EFAULT);
	arg.value = -1;
	igt_assert_eq(__gem_context_get_param(fd, &arg), -EFAULT);
	arg.value = 1;
	igt_assert_eq(__gem_context_get_param(fd, &arg), -EFAULT);

	/* Unmapped. */
	page[0] = mmap(0, 4096, PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	igt_assert(page[0] != MAP_FAILED);
	memset(page[0], 0, sizeof(sseu));
	munmap(page[0], 4096);
	arg.value = to_user_pointer(page[0]);
	igt_assert_eq(__gem_context_get_param(fd, &arg), -EFAULT);

	/* Straddle into unmapped area. */
	page[0] = mmap(0, 8192, PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	igt_assert(page[0] != MAP_FAILED);
	munmap(page[0], 8192);
	page[0] = mmap(page[0], 4096,
		       PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
	igt_assert(page[0] != MAP_FAILED);
	memset(page[0], 0, sizeof(sseu));
	page[1] = mmap((void *)((unsigned long)page[0] + 4096), 4096,
		       PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
	igt_assert(page[1] != MAP_FAILED);
	memset(page[1], 0, sizeof(sseu));
	munmap(page[1], 4096);
	arg.value = to_user_pointer(page[1]) -
		    sizeof(struct drm_i915_gem_context_param_sseu) + 4;
	igt_assert_eq(__gem_context_get_param(fd, &arg), -EFAULT);
	munmap(page[0], 4096);

	/* Straddle into read-only area. */
	page[0] = mmap(0, 8192, PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	igt_assert(page[0] != MAP_FAILED);
	munmap(page[0], 8192);
	page[0] = mmap(page[0], 4096,
		       PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
	igt_assert(page[0] != MAP_FAILED);
	memset(page[0], 0, sizeof(sseu));
	page[1] = mmap((void *)((unsigned long)page[0] + 4096), 4096,
		       PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
	igt_assert(page[1] != MAP_FAILED);
	memset(page[1], 0, sizeof(sseu));
	igt_assert(mprotect(page[1], 4096, PROT_READ) == 0);
	arg.value = to_user_pointer(page[1] - sizeof(sseu) + 4);
	igt_assert_eq(__gem_context_get_param(fd, &arg), -EFAULT);
	munmap(page[0], 4096);
	munmap(page[1], 4096);

	/* set param */

	/* Invalid sizes. */
	arg.size = 1;
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);

	arg.size = 0;
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);
	arg.size = sz;

	/* Bad pointers. */
	arg.value = 0;
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EFAULT);
	arg.value = -1;
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EFAULT);
	arg.value = 1;
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EFAULT);

	/* Get valid SSEU. */
	arg.value = to_user_pointer(&sseu);
	igt_assert_eq(__gem_context_get_param(fd, &arg), 0);

	/* Invalid MBZ. */
	sseu.flags = -1;
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);
	sseu.rsvd = -1;
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);
	sseu.flags = 0;
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);
	sseu.rsvd = 0;

	/* Unmapped. */
	page[0] = mmap(0, 4096, PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	igt_assert(page[0] != MAP_FAILED);
	memcpy(page[0], &sseu, sizeof(sseu));
	munmap(page[0], 4096);
	arg.value = to_user_pointer(page[0]);
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EFAULT);

	/* Straddle into unmapped area. */
	page[0] = mmap(0, 8192, PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	igt_assert(page[0] != MAP_FAILED);
	munmap(page[0], 8192);
	page[0] = mmap(page[0], 4096,
		       PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
	igt_assert(page[0] != MAP_FAILED);
	page[1] = mmap((void *)((unsigned long)page[0] + 4096), 4096,
		       PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
	igt_assert(page[1] != MAP_FAILED);
	addr = page[1] - sizeof(sseu) + 4;
	memcpy(addr, &sseu, sizeof(sseu));
	munmap(page[1], 4096);
	arg.value = to_user_pointer(addr);
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EFAULT);
	munmap(page[0], 4096);

	gem_context_destroy(fd, arg.ctx_id);
}

/*
 * Verify that ggtt mapped area can be used as the sseu pointer.
 */
static void
test_ggtt_args(int fd)
{
	struct drm_i915_gem_context_param_sseu *sseu;
	struct drm_i915_gem_context_param arg = {
		.param = I915_CONTEXT_PARAM_SSEU,
		.ctx_id = gem_context_create(fd),
		.size = sizeof(*sseu),
	};
	uint32_t bo;

	bo = gem_create(fd, 4096);
	arg.value = to_user_pointer(gem_mmap__gtt(fd, bo, 4096,
						  PROT_READ | PROT_WRITE));

	igt_assert_eq(__gem_context_get_param(fd, &arg), 0);
	igt_assert_eq(__gem_context_set_param(fd, &arg), 0);

	munmap((void *)(uintptr_t)arg.value, 4096);
	gem_close(fd, bo);
	gem_context_destroy(fd, arg.ctx_id);
}

/*
 * Verify that invalid SSEU values are rejected.
 */
static void
test_invalid_sseu(int fd)
{
	struct drm_i915_gem_context_param_sseu device_sseu = { };
	struct drm_i915_gem_context_param_sseu sseu = { };
	struct drm_i915_gem_context_param arg = {
		.param = I915_CONTEXT_PARAM_SSEU,
		.ctx_id = gem_context_create(fd),
		.size = sizeof(sseu),
	};
	unsigned int i;

	/* Fetch the device defaults. */
	arg.value = to_user_pointer(&device_sseu);
	gem_context_get_param(fd, &arg);

	arg.value = to_user_pointer(&sseu);

	/* Try all slice masks known to be invalid. */
	sseu = device_sseu;
	for (i = 1; i <= (8 - __slice_count__); i++) {
		sseu.slice_mask = mask_plus(__slice_mask__, i);
		igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);
	}

	/* 0 slices. */
	sseu.slice_mask = 0;
	igt_assert_eq(-EINVAL, __gem_context_set_param(fd, &arg));

	/* Try all subslice masks known to be invalid. */
	sseu = device_sseu;
	for (i = 1; i <= (8 - __subslice_count__); i++) {
		sseu.subslice_mask = mask_plus(__subslice_mask__, i);
		igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);
	}

	/* 0 subslices. */
	sseu.subslice_mask = 0;
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);

	/* Try number of EUs superior to the max available. */
	sseu = device_sseu;
	sseu.min_eus_per_subslice = device_sseu.max_eus_per_subslice + 1;
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);

	sseu = device_sseu;
	sseu.max_eus_per_subslice = device_sseu.max_eus_per_subslice + 1;
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);

	/* Try to program 0 max EUs. */
	sseu = device_sseu;
	sseu.max_eus_per_subslice = 0;
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);

	/* Min > max */
	sseu = device_sseu;
	sseu.min_eus_per_subslice = sseu.max_eus_per_subslice;
	sseu.max_eus_per_subslice = 1;
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);

	if (__intel_gen__ != 11)
		goto out;

	/* Subset of subslices but slice mask greater than one. */
	if (__slice_count__ > 1) {
		sseu = device_sseu;
		sseu.subslice_mask = mask_minus_one(sseu.subslice_mask);
		igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);
	}

	/* Odd subslices above four. */
	sseu = device_sseu;
	sseu.slice_mask = 0x1;
	sseu.subslice_mask = mask_minus_one(sseu.subslice_mask);
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);

	/* More than half subslices with one slice. */
	sseu = device_sseu;
	sseu.slice_mask = 0x1;
	sseu.subslice_mask = mask_minus(sseu.subslice_mask,
					__subslice_count__ / 2 - 1);
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);

	/* VME */

	/* Slice count between one and max. */
	if (__slice_count__ > 2) {
		sseu = device_sseu;
		sseu.slice_mask = mask_minus_one(sseu.slice_mask);
		igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);
	}

	/* Less than half subslices with one slice. */
	sseu = device_sseu;
	sseu.slice_mask = 0x1;
	sseu.subslice_mask = mask_minus(sseu.subslice_mask,
					__subslice_count__ / 2 + 1);
	igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);

out:
	gem_context_destroy(fd, arg.ctx_id);
}

igt_main
{
	int fd;

	igt_fixture {
		fd = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(fd);

		__intel_devid__ = intel_get_drm_devid(fd);
		__intel_gen__ = intel_gen(__intel_devid__);

		igt_require(kernel_has_per_context_sseu_support(fd));
	}

	igt_subtest_group {
		igt_fixture {
			drm_i915_getparam_t gp;

			gp.param = I915_PARAM_SLICE_MASK;
			gp.value = (int *) &__slice_mask__;
			do_ioctl(fd, DRM_IOCTL_I915_GETPARAM, &gp);
			__slice_count__ = __builtin_popcount(__slice_mask__);

			gp.param = I915_PARAM_SUBSLICE_MASK;
			gp.value = (int *) &__subslice_mask__;
			do_ioctl(fd, DRM_IOCTL_I915_GETPARAM, &gp);
			__subslice_count__ =
				__builtin_popcount(__subslice_mask__);
		}

		igt_subtest("invalid-args")
			test_invalid_args(fd);

		igt_subtest("invalid-sseu")
			test_invalid_sseu(fd);

		igt_subtest("ggtt-args")
			test_ggtt_args(fd);

		igt_subtest("engines")
			test_engines(fd);
	}

	igt_fixture {
		close(fd);
	}
}
