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
 *    Daniel Vetter <daniel.vetter@ffwll.ch>
 *
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


IGT_TEST_DESCRIPTION("Test pwrite/pread consistency when touching partial"
		     " cachelines.");

/*
 * Testcase: pwrite/pread consistency when touching partial cachelines
 *
 * Some fancy new pwrite/pread optimizations clflush in-line while
 * reading/writing. Check whether all required clflushes happen.
 *
 */

static drm_intel_bufmgr *bufmgr;
struct intel_batchbuffer *batch;

drm_intel_bo *scratch_bo;
drm_intel_bo *staging_bo;
#define BO_SIZE (4*4096)
uint32_t devid;
int fd;

static void
copy_bo(drm_intel_bo *src, drm_intel_bo *dst)
{
	BLIT_COPY_BATCH_START(0);
	OUT_BATCH((3 << 24) | /* 32 bits */
		  (0xcc << 16) | /* copy ROP */
		  4096);
	OUT_BATCH(0 << 16 | 0);
	OUT_BATCH((BO_SIZE/4096) << 16 | 1024);
	OUT_RELOC_FENCED(dst, I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER, 0);
	OUT_BATCH(0 << 16 | 0);
	OUT_BATCH(4096);
	OUT_RELOC_FENCED(src, I915_GEM_DOMAIN_RENDER, 0, 0);
	ADVANCE_BATCH();

	intel_batchbuffer_flush(batch);
}

static void
blt_bo_fill(drm_intel_bo *tmp_bo, drm_intel_bo *bo, uint8_t val)
{
	uint8_t *gtt_ptr;
	int i;

	do_or_die(drm_intel_gem_bo_map_gtt(tmp_bo));
	gtt_ptr = tmp_bo->virtual;

	for (i = 0; i < BO_SIZE; i++)
		gtt_ptr[i] = val;

	drm_intel_gem_bo_unmap_gtt(tmp_bo);

	igt_drop_caches_set(fd, DROP_BOUND);

	copy_bo(tmp_bo, bo);
}

#define MAX_BLT_SIZE 128
#define ROUNDS 1000
uint8_t tmp[BO_SIZE];

static void get_range(int *start, int *len)
{
	*start = random() % (BO_SIZE - 1);
	*len = random() % (BO_SIZE - *start - 1) + 1;
}

static void test_partial_reads(void)
{
	int i, j;

	igt_info("checking partial reads\n");
	for (i = 0; i < ROUNDS; i++) {
		uint8_t val = i;
		int start, len;

		blt_bo_fill(staging_bo, scratch_bo, val);

		get_range(&start, &len);
		do_or_die(drm_intel_bo_get_subdata(scratch_bo, start, len, tmp));
		for (j = 0; j < len; j++) {
			igt_assert_f(tmp[j] == val,
				     "mismatch at %i [%i + %i], got: %i, expected: %i\n",
				     j, start, len, tmp[j], val);
		}

		igt_progress("partial reads test: ", i, ROUNDS);
	}
}

static void test_partial_writes(void)
{
	int i, j;
	uint8_t *gtt_ptr;

	igt_info("checking partial writes\n");
	for (i = 0; i < ROUNDS; i++) {
		uint8_t val = i;
		int start, len;

		blt_bo_fill(staging_bo, scratch_bo, val);

		memset(tmp, i + 63, BO_SIZE);

		get_range(&start, &len);
		drm_intel_bo_subdata(scratch_bo, start, len, tmp);

		copy_bo(scratch_bo, staging_bo);
		drm_intel_gem_bo_map_gtt(staging_bo);
		gtt_ptr = staging_bo->virtual;

		for (j = 0; j < start; j++) {
			igt_assert_f(gtt_ptr[j] == val,
				     "mismatch at %i (start=%i), got: %i, expected: %i\n",
				     j, start, tmp[j], val);
		}
		for (; j < start + len; j++) {
			igt_assert_f(gtt_ptr[j] == tmp[0],
				     "mismatch at %i (%i/%i), got: %i, expected: %i\n",
				     j, j-start, len, tmp[j], i);
		}
		for (; j < BO_SIZE; j++) {
			igt_assert_f(gtt_ptr[j] == val,
				     "mismatch at %i (end=%i), got: %i, expected: %i\n",
				     j, start+len, tmp[j], val);
		}
		drm_intel_gem_bo_unmap_gtt(staging_bo);

		igt_progress("partial writes test: ", i, ROUNDS);
	}
}

static void test_partial_read_writes(void)
{
	int i, j;
	uint8_t *gtt_ptr;

	igt_info("checking partial writes after partial reads\n");
	for (i = 0; i < ROUNDS; i++) {
		uint8_t val = i;
		int start, len;

		blt_bo_fill(staging_bo, scratch_bo, val);

		/* partial read */
		get_range(&start, &len);
		drm_intel_bo_get_subdata(scratch_bo, start, len, tmp);
		for (j = 0; j < len; j++) {
			igt_assert_f(tmp[j] == val,
				     "mismatch in read at %i [%i + %i], got: %i, expected: %i\n",
				     j, start, len, tmp[j], val);
		}

		/* Change contents through gtt to make the pread cachelines
		 * stale. */
		val += 17;
		blt_bo_fill(staging_bo, scratch_bo, val);

		/* partial write */
		memset(tmp, i + 63, BO_SIZE);

		get_range(&start, &len);
		drm_intel_bo_subdata(scratch_bo, start, len, tmp);

		copy_bo(scratch_bo, staging_bo);
		do_or_die(drm_intel_gem_bo_map_gtt(staging_bo));
		gtt_ptr = staging_bo->virtual;

		for (j = 0; j < start; j++) {
			igt_assert_f(gtt_ptr[j] == val,
				     "mismatch at %i (start=%i), got: %i, expected: %i\n",
				     j, start, tmp[j], val);
		}
		for (; j < start + len; j++) {
			igt_assert_f(gtt_ptr[j] == tmp[0],
				     "mismatch at %i (%i/%i), got: %i, expected: %i\n",
				     j, j - start, len, tmp[j], tmp[0]);
		}
		for (; j < BO_SIZE; j++) {
			igt_assert_f(gtt_ptr[j] == val,
				     "mismatch at %i (end=%i), got: %i, expected: %i\n",
				     j, start + len, tmp[j], val);
		}
		drm_intel_gem_bo_unmap_gtt(staging_bo);

		igt_progress("partial read/writes test: ", i, ROUNDS);
	}
}

static void do_tests(int cache_level, const char *suffix)
{
	igt_fixture {
		if (cache_level != -1)
			gem_set_caching(fd, scratch_bo->handle, cache_level);
	}

	igt_subtest_f("reads%s", suffix)
		test_partial_reads();

	igt_subtest_f("write%s", suffix)
		test_partial_writes();

	igt_subtest_f("writes-after-reads%s", suffix)
		test_partial_read_writes();
}

igt_main
{
	srandom(0xdeadbeef);

	igt_skip_on_simulation();

	igt_fixture {
		fd = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(fd);

		bufmgr = drm_intel_bufmgr_gem_init(fd, 4096);
		devid = intel_get_drm_devid(fd);
		batch = intel_batchbuffer_alloc(bufmgr, devid);

		/* overallocate the buffers we're actually using because */
		scratch_bo = drm_intel_bo_alloc(bufmgr, "scratch bo", BO_SIZE, 4096);
		staging_bo = drm_intel_bo_alloc(bufmgr, "staging bo", BO_SIZE, 4096);
	}

	do_tests(-1, "");

	/* Repeat the tests using different levels of snooping */
	do_tests(0, "-uncached");
	do_tests(1, "-snoop");
	do_tests(2, "-display");

	igt_fixture {
		drm_intel_bufmgr_destroy(bufmgr);

		close(fd);
	}
}
