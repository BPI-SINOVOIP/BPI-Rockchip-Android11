/*
 * Copyright © 2009 Intel Corporation
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
 *
 */

/** @file gem_tiled_blits.c
 *
 * This is a test of doing many tiled blits, with a working set
 * larger than the aperture size.
 *
 * The goal is to catch a couple types of failure;
 * - Fence management problems on pre-965.
 * - A17 or L-shaped memory tiling workaround problems in acceleration.
 *
 * The model is to fill a collection of 1MB objects in a way that can't trip
 * over A6 swizzling -- upload data to a non-tiled object, blit to the tiled
 * object.  Then, copy the 1MB objects randomly between each other for a while.
 * Finally, download their data through linear objects again and see what
 * resulted.
 */

#include "igt.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <drm.h>


IGT_TEST_DESCRIPTION("Test doing many tiled blits, with a working set larger"
		     " than the aperture size.");

static drm_intel_bufmgr *bufmgr;
struct intel_batchbuffer *batch;
static int width = 512, height = 512;

static drm_intel_bo *
create_bo(uint32_t start_val)
{
	drm_intel_bo *bo, *linear_bo;
	uint32_t *linear;
	uint32_t tiling = I915_TILING_X;
	int i;

	bo = drm_intel_bo_alloc(bufmgr, "tiled bo", 1024 * 1024, 4096);
	do_or_die(drm_intel_bo_set_tiling(bo, &tiling, width * 4));
	igt_assert(tiling == I915_TILING_X);

	linear_bo = drm_intel_bo_alloc(bufmgr, "linear src", 1024 * 1024, 4096);

	/* Fill the BO with dwords starting at start_val */
	do_or_die(drm_intel_bo_map(linear_bo, 1));
	linear = linear_bo->virtual;
	for (i = 0; i < 1024 * 1024 / 4; i++)
		linear[i] = start_val++;
	drm_intel_bo_unmap(linear_bo);

	intel_copy_bo (batch, bo, linear_bo, width*height*4);

	drm_intel_bo_unreference(linear_bo);

	return bo;
}

static void
check_bo(drm_intel_bo *bo, uint32_t val)
{
	drm_intel_bo *linear_bo;
	uint32_t *linear;
	int num_errors;
	int i;

	linear_bo = drm_intel_bo_alloc(bufmgr, "linear dst", 1024 * 1024, 4096);

	intel_copy_bo(batch, linear_bo, bo, width*height*4);

	do_or_die(drm_intel_bo_map(linear_bo, 0));
	linear = linear_bo->virtual;

	num_errors = 0;
	for (i = 0; i < 1024 * 1024 / 4; i++) {
		if (linear[i] != val && num_errors++ < 32)
			igt_warn("[%08x] Expected 0x%08x, found 0x%08x (difference 0x%08x)\n",
				 i * 4, val, linear[i], val ^ linear[i]);
		val++;
	}
	igt_assert_eq(num_errors, 0);
	drm_intel_bo_unmap(linear_bo);

	drm_intel_bo_unreference(linear_bo);
}

static void run_test(int count)
{
	drm_intel_bo **bo;
	uint32_t *bo_start_val;
	uint32_t start = 0;
	int i;

	igt_debug("Using %d 1MiB buffers\n", count);

	bo = malloc(sizeof(drm_intel_bo *)*count);
	bo_start_val = malloc(sizeof(uint32_t)*count);

	for (i = 0; i < count; i++) {
		bo[i] = create_bo(start);
		bo_start_val[i] = start;
		start += 1024 * 1024 / 4;
	}
	igt_info("Verifying initialisation...\n");
	for (i = 0; i < count; i++)
		check_bo(bo[i], bo_start_val[i]);

	igt_info("Cyclic blits, forward...\n");
	for (i = 0; i < count * 4; i++) {
		int src = i % count;
		int dst = (i+1) % count;

		if (src == dst)
			continue;

		intel_copy_bo(batch, bo[dst], bo[src], width*height*4);
		bo_start_val[dst] = bo_start_val[src];
	}
	for (i = 0; i < count; i++)
		check_bo(bo[i], bo_start_val[i]);

	if (igt_run_in_simulation()) {
		for (i = 0; i < count; i++)
			drm_intel_bo_unreference(bo[i]);
		free(bo_start_val);
		free(bo);
		return;
	}

	igt_info("Cyclic blits, backward...\n");
	for (i = 0; i < count * 4; i++) {
		int src = (i+1) % count;
		int dst = i % count;

		if (src == dst)
			continue;

		intel_copy_bo(batch, bo[dst], bo[src], width*height*4);
		bo_start_val[dst] = bo_start_val[src];
	}
	for (i = 0; i < count; i++)
		check_bo(bo[i], bo_start_val[i]);

	igt_info("Random blits...\n");
	for (i = 0; i < count * 4; i++) {
		int src = random() % count;
		int dst = random() % count;

		if (src == dst)
			continue;

		intel_copy_bo(batch, bo[dst], bo[src], width*height*4);
		bo_start_val[dst] = bo_start_val[src];
	}
	for (i = 0; i < count; i++) {
		check_bo(bo[i], bo_start_val[i]);
		drm_intel_bo_unreference(bo[i]);
	}

	free(bo_start_val);
	free(bo);
}

#define MAX_32b ((1ull << 32) - 4096)

int fd;

igt_main
{
	igt_fixture {
		fd = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(fd);

		bufmgr = drm_intel_bufmgr_gem_init(fd, 4096);
		drm_intel_bufmgr_gem_enable_reuse(bufmgr);
		drm_intel_bufmgr_gem_set_vma_cache_size(bufmgr, 32);
		batch = intel_batchbuffer_alloc(bufmgr, intel_get_drm_devid(fd));
	}

	igt_subtest("basic")
		run_test(2);

	igt_subtest("normal") {
		uint64_t count;

		count = gem_aperture_size(fd);
		if (count >> 32)
			count = MAX_32b;
		count = 3 * count / (1024*1024) / 2;
		count += (count & 1) == 0;
		intel_require_memory(count, 1024*1024, CHECK_RAM);

		run_test(count);
	}

	igt_subtest("interruptible") {
		uint64_t count;

		count = gem_aperture_size(fd);
		if (count >> 32)
			count = MAX_32b;
		count = 3 * count / (1024*1024) / 2;
		count += (count & 1) == 0;
		intel_require_memory(count, 1024*1024, CHECK_RAM);

		igt_fork_signal_helper();
		run_test(count);
		igt_stop_signal_helper();
	}

	igt_fixture {
		intel_batchbuffer_free(batch);
		drm_intel_bufmgr_destroy(bufmgr);

		close(fd);
	}
}
