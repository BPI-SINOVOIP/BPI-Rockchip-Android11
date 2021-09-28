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

/** @file gem_tiled_pread_pwrite.c
 *
 * This is a test of pread's behavior on tiled objects with respect to the
 * reported swizzling value.
 *
 * The goal is to exercise the slow_bit17_copy path for reading on bit17
 * machines, but will also be useful for catching swizzling value bugs on
 * other systems.
 */

/*
 * Testcase: Test swizzling by testing pwrite does the inverse of pread
 *
 * Together with the explicit pread testcase, this should cover our swizzle
 * handling.
 *
 * Note that this test will use swap in an effort to test all of ram.
 */

#include "igt.h"
#include "igt_x86.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <drm.h>


IGT_TEST_DESCRIPTION("Test swizzling by testing pwrite does the inverse of"
		     " pread.");

#define WIDTH 512
#define HEIGHT 512
static uint32_t linear[WIDTH * HEIGHT];
static uint32_t current_tiling_mode;

#define PAGE_SIZE 4096

static uint32_t
create_bo_and_fill(int fd)
{
	uint32_t handle;
	uint32_t *data;
	int i;

	handle = gem_create(fd, sizeof(linear));
	gem_set_tiling(fd, handle, current_tiling_mode, WIDTH * sizeof(uint32_t));

	/* Fill the BO with dwords starting at start_val */
	data = gem_mmap__gtt(fd, handle, sizeof(linear),
			     PROT_READ | PROT_WRITE);
	gem_set_domain(fd, handle, I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	for (i = 0; i < WIDTH*HEIGHT; i++)
		data[i] = i;
	munmap(data, sizeof(linear));

	return handle;
}

static uint32_t
create_bo(int fd)
{
	uint32_t handle;

	handle = gem_create(fd, sizeof(linear));
	gem_set_tiling(fd, handle, current_tiling_mode, WIDTH * sizeof(uint32_t));

	return handle;
}

static void copy_wc_page(void *dst, const void *src)
{
	igt_memcpy_from_wc(dst, src, PAGE_SIZE);
}

igt_simple_main
{
	uint32_t tiling, swizzle;
	int count;
	int fd;
	
	fd = drm_open_driver(DRIVER_INTEL);
	count = SLOW_QUICK(intel_get_total_ram_mb() * 9 / 10, 8) ;

	for (int i = 0; i < count/2; i++) {
		uint32_t handle, handle_target;
		char *data;
		int n;

		current_tiling_mode = I915_TILING_X;

		handle = create_bo_and_fill(fd);
		igt_require(gem_get_tiling(fd, handle, &tiling, &swizzle));

		gem_read(fd, handle, 0, linear, sizeof(linear));

		handle_target = create_bo(fd);
		gem_write(fd, handle_target, 0, linear, sizeof(linear));

		/* Check the target bo's contents. */
		data = gem_mmap__gtt(fd, handle_target, sizeof(linear), PROT_READ);
		n = 0;
		for (int pfn = 0; pfn < sizeof(linear)/PAGE_SIZE; pfn++) {
			uint32_t page[PAGE_SIZE/sizeof(uint32_t)];
			copy_wc_page(page, data + PAGE_SIZE*pfn);
			for (int j = 0; j < PAGE_SIZE/sizeof(uint32_t); j++) {
				igt_assert_f(page[j] == n,
					     "mismatch at %i: %i\n",
					     n, page[j]);
				n++;
			}
		}
		munmap(data, sizeof(linear));

		/* Leak both bos so that we use all of system mem! */
		gem_madvise(fd, handle_target, I915_MADV_DONTNEED);
		gem_madvise(fd, handle, I915_MADV_DONTNEED);

		igt_progress("gem_tiled_pread_pwrite: ", i, count/2);
	}

	close(fd);
}
