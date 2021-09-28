/*
 * Copyright © 2007, 2011, 2013, 2014 Intel Corporation
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
 *    Eric Anholt <eric@anholt.net>
 *    Daniel Vetter <daniel.vetter@ffwll.ch>
 *    Tvrtko Ursulin <tvrtko.ursulin@intel.com>
 *
 */

#include "igt.h"
#include <stdlib.h>


struct igt_eviction_test_ops {
	uint32_t (*create)(int fd, uint64_t size);
	void (*flink)(uint32_t old_handle, uint32_t new_handle);
	void	 (*close)(int fd, uint32_t bo);
	int	 (*copy)(int fd, uint32_t dst, uint32_t src,
			 uint32_t *all_bo, int nr_bos);
	void	 (*clear)(int fd, uint32_t bo, uint64_t size);
};

#define FORKING_EVICTIONS_INTERRUPTIBLE	  (1 << 0)
#define FORKING_EVICTIONS_SWAPPING	  (1 << 1)
#define FORKING_EVICTIONS_DUP_DRMFD	  (1 << 2)
#define FORKING_EVICTIONS_MEMORY_PRESSURE (1 << 3)
#define ALL_FORKING_EVICTIONS	(FORKING_EVICTIONS_INTERRUPTIBLE | \
				 FORKING_EVICTIONS_SWAPPING | \
				 FORKING_EVICTIONS_DUP_DRMFD | \
				 FORKING_EVICTIONS_MEMORY_PRESSURE)

static void exchange_uint32_t(void *array, unsigned i, unsigned j)
{
	uint32_t *i_arr = array;

	igt_swap(i_arr[i], i_arr[j]);
}

static int minor_evictions(int fd, struct igt_eviction_test_ops *ops,
			   uint64_t surface_size,
			   uint64_t nr_surfaces)
{
	uint32_t *bo, *sel;
	uint64_t n, m, total_surfaces;
	int pass, fail;

	/* Make sure nr_surfaces is not divisible by seven
	 * to avoid duplicates in the selection loop below.
	 */
	nr_surfaces /= 7;
	nr_surfaces *= 7;
	nr_surfaces += 3;

	total_surfaces = gem_aperture_size(fd) / surface_size + 1;
	igt_require(nr_surfaces < total_surfaces);
	intel_require_memory(total_surfaces, surface_size, CHECK_RAM);

	bo = malloc((nr_surfaces + total_surfaces)*sizeof(*bo));
	igt_assert(bo);

	for (n = 0; n < total_surfaces; n++)
		bo[n] = ops->create(fd, surface_size);

	sel = bo + n;
	for (fail = 0, m = 0; fail < 10; fail++) {
		int ret;
		for (pass = 0; pass < 100; pass++) {
			for (n = 0; n < nr_surfaces; n++, m += 7)
				sel[n] = bo[m%total_surfaces];
			ret = ops->copy(fd, sel[0], sel[1], sel, nr_surfaces);
			igt_assert_eq(ret, 0);
		}
		ret = ops->copy(fd, bo[0], bo[0], bo, total_surfaces);
		igt_assert_eq(ret, -ENOSPC);
	}

	for (n = 0; n < total_surfaces; n++)
		ops->close(fd, bo[n]);
	free(bo);

	return 0;
}

static int major_evictions(int fd, struct igt_eviction_test_ops *ops,
			   uint64_t surface_size, uint64_t nr_surfaces)
{
	uint64_t n, m;
	uint32_t *bo;
	int ret, loop;

	intel_require_memory(nr_surfaces, surface_size, CHECK_RAM);

	bo = malloc(nr_surfaces*sizeof(*bo));
	igt_assert(bo);

	for (n = 0; n < nr_surfaces; n++)
		bo[n] = ops->create(fd, surface_size);

	for (loop = 0, m = 0; loop < 100; loop++, m += 17) {
		n = m % nr_surfaces;
		ret = ops->copy(fd, bo[n], bo[n], &bo[n], 1);
		igt_assert_eq(ret, 0);
	}

	for (n = 0; n < nr_surfaces; n++)
		ops->close(fd, bo[n]);
	free(bo);

	return 0;
}

static void mlocked_evictions(int fd, struct igt_eviction_test_ops *ops,
			      uint64_t surface_size,
			      uint64_t surface_count)
{
	uint64_t sz, pin, total;
	void *mem;

	intel_require_memory(surface_count, surface_size, CHECK_RAM);

	mem = intel_get_total_pinnable_mem(&total);
	igt_assert(mem != MAP_FAILED);
	pin = *(uint64_t *)mem;
	igt_assert(!munlock(mem, pin));

	sz = surface_size * surface_count;
	igt_require(pin > sz);

	igt_fork(child, 1) {
		uint32_t *bo;
		uint64_t n;
		int ret;
		size_t lock = pin - sz;

		bo = malloc(surface_count * sizeof(*bo));
		igt_assert(bo);
		lock -= ALIGN(surface_count * sizeof(*bo), 4096);

		igt_debug("Locking %'zu B (%'zu MiB)\n",
			  lock, lock >> 20);
		igt_assert(!mlock(mem, lock));
		igt_info("Locked %'zu B (%'zu MiB)\n",
			 lock, lock >> 20);

		for (n = 0; n < surface_count; n++)
			bo[n] = ops->create(fd, surface_size);

		for (n = 0; n < surface_count - 2; n++) {
			igt_permute_array(bo, surface_count, exchange_uint32_t);
			ret = ops->copy(fd, bo[0], bo[1], bo, surface_count-n);
			if (ret)
				exit(ret);

			/* Having used the surfaces (and so pulled out of
			 * our pages into memory), start a memory hog to
			 * force evictions.
			 */
			lock += surface_size;
			igt_assert(!mlock(mem, lock));
			igt_debug("Total locked %'zu B (%'zu MiB)\n",
				  lock,
				  lock >> 20);
		}

		for (n = 0; n < surface_count; n++)
			ops->close(fd, bo[n]);
	}
	igt_waitchildren();

	munmap(mem, total);
}

static int swapping_evictions(int fd, struct igt_eviction_test_ops *ops,
			      uint64_t surface_size,
			      uint64_t working_surfaces,
			      uint64_t trash_surfaces)
{
	uint32_t *bo;
	uint64_t i, n;
	int pass, ret;

	intel_require_memory(working_surfaces, surface_size, CHECK_RAM);

	if (trash_surfaces < working_surfaces)
		trash_surfaces = working_surfaces;

	intel_require_memory(trash_surfaces, surface_size, CHECK_RAM | CHECK_SWAP);

	bo = malloc(trash_surfaces*sizeof(*bo));
	igt_assert(bo);

	for (n = 0; n < trash_surfaces; n++)
		bo[n] = ops->create(fd, surface_size);

	for (i = 0; i < trash_surfaces/32; i++) {
		igt_permute_array(bo, trash_surfaces, exchange_uint32_t);

		for (pass = 0; pass < 100; pass++) {
			ret = ops->copy(fd, bo[0], bo[1], bo, working_surfaces);
			igt_assert_eq(ret, 0);
		}
	}

	for (n = 0; n < trash_surfaces; n++)
		ops->close(fd, bo[n]);
	free(bo);

	return 0;
}

static int forking_evictions(int fd, struct igt_eviction_test_ops *ops,
			     uint64_t surface_size,
			     uint64_t working_surfaces,
			     uint64_t trash_surfaces,
			     unsigned flags)
{
	const int num_threads = sysconf(_SC_NPROCESSORS_ONLN);
	uint64_t bo_count, n, l;
	uint32_t *bo;
	int pass, ret;

	intel_require_memory(working_surfaces, surface_size, CHECK_RAM);

	if (flags & FORKING_EVICTIONS_SWAPPING) {
		bo_count = trash_surfaces;
		if (bo_count < working_surfaces)
			bo_count = working_surfaces;

	} else
		bo_count = working_surfaces;

	igt_assert_lte(working_surfaces, bo_count);
	intel_require_memory(bo_count, surface_size, CHECK_RAM | CHECK_SWAP);

	bo = malloc(bo_count*sizeof(*bo));
	igt_assert(bo);

	for (n = 0; n < bo_count; n++)
		bo[n] = ops->create(fd, surface_size);

	igt_fork(i, min(num_threads * 4, 12)) {
		int realfd = fd;
		int num_passes = flags & FORKING_EVICTIONS_SWAPPING ? 10 : 100;

		/* Every fork should have a different permutation! */
		srand(i * 63);

		if (flags & FORKING_EVICTIONS_INTERRUPTIBLE)
			igt_fork_signal_helper();

		igt_permute_array(bo, bo_count, exchange_uint32_t);

		if (flags & FORKING_EVICTIONS_DUP_DRMFD) {
			realfd = drm_open_driver(DRIVER_INTEL);

			/* We can overwrite the bo array since we're forked. */
			for (l = 0; l < bo_count; l++) {
				uint32_t handle = bo[l];
				uint32_t flink = gem_flink(fd, bo[l]);

				bo[l] = gem_open(realfd, flink);
				if (ops->flink)
					ops->flink(handle, bo[l]);
			}
		}

		for (pass = 0; pass < num_passes; pass++) {
			ret = ops->copy(realfd, bo[0], bo[1], bo, working_surfaces);
			igt_assert_eq(ret, 0);

			for (l = 0; l < working_surfaces &&
			  (flags & FORKING_EVICTIONS_MEMORY_PRESSURE);
			    l++) {
				ops->clear(realfd, bo[l], surface_size);
			}
		}

		if (flags & FORKING_EVICTIONS_INTERRUPTIBLE)
			igt_stop_signal_helper();

		/* drmfd closing will take care of additional bo refs */
		if (flags & FORKING_EVICTIONS_DUP_DRMFD)
			close(realfd);
	}

	igt_waitchildren();

	for (n = 0; n < bo_count; n++)
		ops->close(fd, bo[n]);
	free(bo);

	return 0;
}
