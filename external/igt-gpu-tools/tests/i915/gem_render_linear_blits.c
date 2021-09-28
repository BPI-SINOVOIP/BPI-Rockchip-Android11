/*
 * Copyright © 2011 Intel Corporation
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
 *    Chris Wilson <chris@chris-wilson.co.uk>
 *
 */

/** @file gem_linear_render_blits.c
 *
 * This is a test of doing many blits, with a working set
 * larger than the aperture size.
 *
 * The goal is to simply ensure the basics work.
 */

#include "config.h"

#include "igt.h"
#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <drm.h>

#include "intel_bufmgr.h"

#define WIDTH 512
#define STRIDE (WIDTH*4)
#define HEIGHT 512
#define SIZE (HEIGHT*STRIDE)

static uint32_t linear[WIDTH*HEIGHT];
static igt_render_copyfunc_t render_copy;

static void
check_bo(int fd, uint32_t handle, uint32_t val)
{
	int i;

	gem_read(fd, handle, 0, linear, sizeof(linear));
	for (i = 0; i < WIDTH*HEIGHT; i++) {
		igt_assert_f(linear[i] == val,
			"Expected 0x%08x, found 0x%08x "
			"at offset 0x%08x\n",
			val, linear[i], i * 4);
		val++;
	}
}

static void run_test (int fd, int count)
{
	drm_intel_bufmgr *bufmgr;
	struct intel_batchbuffer *batch;
	uint32_t *start_val;
	drm_intel_bo **bo;
	uint32_t start = 0;
	int i, j;

	render_copy = igt_get_render_copyfunc(intel_get_drm_devid(fd));
	igt_require(render_copy);

	bufmgr = drm_intel_bufmgr_gem_init(fd, 4096);
	batch = intel_batchbuffer_alloc(bufmgr, intel_get_drm_devid(fd));

	bo = malloc(sizeof(*bo)*count);
	start_val = malloc(sizeof(*start_val)*count);

	for (i = 0; i < count; i++) {
		bo[i] = drm_intel_bo_alloc(bufmgr, "", SIZE, 4096);
		start_val[i] = start;
		for (j = 0; j < WIDTH*HEIGHT; j++)
			linear[j] = start++;
		gem_write(fd, bo[i]->handle, 0, linear, sizeof(linear));
	}

	igt_info("Verifying initialisation - %d buffers of %d bytes\n", count, SIZE);
	for (i = 0; i < count; i++)
		check_bo(fd, bo[i]->handle, start_val[i]);

	igt_info("Cyclic blits, forward...\n");
	for (i = 0; i < count * 4; i++) {
		struct igt_buf src = {}, dst = {};

		src.bo = bo[i % count];
		src.stride = STRIDE;
		src.tiling = I915_TILING_NONE;
		src.size = SIZE;
		src.bpp = 32;

		dst.bo = bo[(i + 1) % count];
		dst.stride = STRIDE;
		dst.tiling = I915_TILING_NONE;
		dst.size = SIZE;
		dst.bpp = 32;

		render_copy(batch, NULL, &src, 0, 0, WIDTH, HEIGHT, &dst, 0, 0);
		start_val[(i + 1) % count] = start_val[i % count];
	}
	for (i = 0; i < count; i++)
		check_bo(fd, bo[i]->handle, start_val[i]);

	if (igt_run_in_simulation())
		return;

	igt_info("Cyclic blits, backward...\n");
	for (i = 0; i < count * 4; i++) {
		struct igt_buf src = {}, dst = {};

		src.bo = bo[(i + 1) % count];
		src.stride = STRIDE;
		src.tiling = I915_TILING_NONE;
		src.size = SIZE;
		src.bpp = 32;

		dst.bo = bo[i % count];
		dst.stride = STRIDE;
		dst.tiling = I915_TILING_NONE;
		dst.size = SIZE;
		dst.bpp = 32;

		render_copy(batch, NULL, &src, 0, 0, WIDTH, HEIGHT, &dst, 0, 0);
		start_val[i % count] = start_val[(i + 1) % count];
	}
	for (i = 0; i < count; i++)
		check_bo(fd, bo[i]->handle, start_val[i]);

	igt_info("Random blits...\n");
	for (i = 0; i < count * 4; i++) {
		struct igt_buf src = {}, dst = {};
		int s = random() % count;
		int d = random() % count;

		if (s == d)
			continue;

		src.bo = bo[s];
		src.stride = STRIDE;
		src.tiling = I915_TILING_NONE;
		src.size = SIZE;
		src.bpp = 32;

		dst.bo = bo[d];
		dst.stride = STRIDE;
		dst.tiling = I915_TILING_NONE;
		dst.size = SIZE;
		dst.bpp = 32;

		render_copy(batch, NULL, &src, 0, 0, WIDTH, HEIGHT, &dst, 0, 0);
		start_val[d] = start_val[s];
	}
	for (i = 0; i < count; i++)
		check_bo(fd, bo[i]->handle, start_val[i]);

	/* release resources */
	for (i = 0; i < count; i++) {
		drm_intel_bo_unreference(bo[i]);
	}
	intel_batchbuffer_free(batch);
	drm_intel_bufmgr_destroy(bufmgr);
}

igt_main
{
	static int fd = 0;
	int count=0;

	igt_fixture {
		fd = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(fd);
	}

	igt_subtest("basic") {
		run_test(fd, 2);
	}

	/* the rest of the tests are too long for simulation */
	igt_skip_on_simulation();

	igt_subtest("aperture-thrash") {
		count = 3 * gem_aperture_size(fd) / SIZE / 2;
		intel_require_memory(count, SIZE, CHECK_RAM);
		run_test(fd, count);
	}

	igt_subtest("aperture-shrink") {
		igt_fork_shrink_helper(fd);

		count = 3 * gem_aperture_size(fd) / SIZE / 2;
		intel_require_memory(count, SIZE, CHECK_RAM);
		run_test(fd, count);

		igt_stop_shrink_helper();
	}

	igt_subtest("swap-thrash") {
		uint64_t swap_mb = intel_get_total_swap_mb();
		igt_require(swap_mb > 0);
		count = ((intel_get_avail_ram_mb() + (swap_mb / 2)) * 1024*1024) / SIZE;
		intel_require_memory(count, SIZE, CHECK_RAM | CHECK_SWAP);
		run_test(fd, count);
	}
}
