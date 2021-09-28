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

/** @file gem_pread_after_blit.c
 *
 * This is a test of pread's behavior when getting values out of just-drawn-to
 * buffers.
 *
 * The goal is to catch failure in the whole-buffer-flush or
 * ranged-buffer-flush paths in the kernel.
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


IGT_TEST_DESCRIPTION("Test pread behavior when getting values out of"
		     " just-drawn-to buffers.");

static drm_intel_bufmgr *bufmgr;
struct intel_batchbuffer *batch;
static const int width = 512, height = 512;
static const int size = 1024 * 1024;

#define PAGE_SIZE 4096

static drm_intel_bo *
create_bo(uint32_t val)
{
	drm_intel_bo *bo;
	uint32_t *vaddr;
	int i;

	bo = drm_intel_bo_alloc(bufmgr, "src bo", size, 4096);

	/* Fill the BO with dwords starting at start_val */
	drm_intel_bo_map(bo, 1);
	vaddr = bo->virtual;

	for (i = 0; i < 1024 * 1024 / 4; i++)
		vaddr[i] = val++;

	drm_intel_bo_unmap(bo);

	return bo;
}

static void
verify_large_read(drm_intel_bo *bo, uint32_t val)
{
	uint32_t buf[size / 4];
	int i;

	drm_intel_bo_get_subdata(bo, 0, size, buf);

	for (i = 0; i < size / 4; i++) {
		igt_assert_f(buf[i] == val,
			     "Unexpected value 0x%08x instead of "
			     "0x%08x at offset 0x%08x (%p)\n",
			     buf[i], val, i * 4, buf);
		val++;
	}
}

/** This reads at the size that Mesa usees for software fallbacks. */
static void
verify_small_read(drm_intel_bo *bo, uint32_t val)
{
	uint32_t buf[4096 / 4];
	int offset, i;

	for (i = 0; i < 4096 / 4; i++)
		buf[i] = 0x00c0ffee;

	for (offset = 0; offset < size; offset += PAGE_SIZE) {
		drm_intel_bo_get_subdata(bo, offset, PAGE_SIZE, buf);

		for (i = 0; i < PAGE_SIZE; i += 4) {
			igt_assert_f(buf[i / 4] == val,
				     "Unexpected value 0x%08x instead of "
				     "0x%08x at offset 0x%08x\n",
				     buf[i / 4], val, i * 4);
			val++;
		}
	}
}

typedef igt_hang_t (*do_hang)(int fd);

static igt_hang_t no_hang(int fd)
{
	return (igt_hang_t){0};
}

static igt_hang_t bcs_hang(int fd)
{
	return igt_hang_ring(fd, batch->gen >= 6 ? I915_EXEC_BLT : I915_EXEC_DEFAULT);
}

static void do_test(int fd, int cache_level,
		    drm_intel_bo *src[2],
		    const uint32_t start[2],
		    drm_intel_bo *tmp[2],
		    int loop, do_hang do_hang_func)
{
	igt_hang_t hang;

	if (cache_level != -1) {
		gem_set_caching(fd, tmp[0]->handle, cache_level);
		gem_set_caching(fd, tmp[1]->handle, cache_level);
	}

	do {
		/* First, do a full-buffer read after blitting */
		intel_copy_bo(batch, tmp[0], src[0], width*height*4);
		hang = do_hang_func(fd);
		verify_large_read(tmp[0], start[0]);
		igt_post_hang_ring(fd, hang);
		intel_copy_bo(batch, tmp[0], src[1], width*height*4);
		hang = do_hang_func(fd);
		verify_large_read(tmp[0], start[1]);
		igt_post_hang_ring(fd, hang);

		intel_copy_bo(batch, tmp[0], src[0], width*height*4);
		hang = do_hang_func(fd);
		verify_small_read(tmp[0], start[0]);
		igt_post_hang_ring(fd, hang);
		intel_copy_bo(batch, tmp[0], src[1], width*height*4);
		hang = do_hang_func(fd);
		verify_small_read(tmp[0], start[1]);
		igt_post_hang_ring(fd, hang);

		intel_copy_bo(batch, tmp[0], src[0], width*height*4);
		hang = do_hang_func(fd);
		verify_large_read(tmp[0], start[0]);
		igt_post_hang_ring(fd, hang);

		intel_copy_bo(batch, tmp[0], src[0], width*height*4);
		intel_copy_bo(batch, tmp[1], src[1], width*height*4);
		hang = do_hang_func(fd);
		verify_large_read(tmp[0], start[0]);
		verify_large_read(tmp[1], start[1]);
		igt_post_hang_ring(fd, hang);

		intel_copy_bo(batch, tmp[0], src[0], width*height*4);
		intel_copy_bo(batch, tmp[1], src[1], width*height*4);
		hang = do_hang_func(fd);
		verify_large_read(tmp[1], start[1]);
		verify_large_read(tmp[0], start[0]);
		igt_post_hang_ring(fd, hang);

		intel_copy_bo(batch, tmp[1], src[0], width*height*4);
		intel_copy_bo(batch, tmp[0], src[1], width*height*4);
		hang = do_hang_func(fd);
		verify_large_read(tmp[0], start[1]);
		verify_large_read(tmp[1], start[0]);
		igt_post_hang_ring(fd, hang);
	} while (--loop);
}

drm_intel_bo *src[2], *dst[2];
int fd;

igt_main
{
	const uint32_t start[2] = {0, 1024 * 1024 / 4};
	const struct {
		const char *name;
		int cache;
	} tests[] = {
		{ "default", -1 },
		{ "uncached", 0 },
		{ "snooped", 1 },
		{ "display", 2 },
		{ NULL, -1 },
	}, *t;

	igt_skip_on_simulation();

	igt_fixture {
		fd = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(fd);

		bufmgr = drm_intel_bufmgr_gem_init(fd, 4096);
		drm_intel_bufmgr_gem_enable_reuse(bufmgr);
		batch = intel_batchbuffer_alloc(bufmgr, intel_get_drm_devid(fd));

		src[0] = create_bo(start[0]);
		src[1] = create_bo(start[1]);

		dst[0] = drm_intel_bo_alloc(bufmgr, "dst bo", size, 4096);
		dst[1] = drm_intel_bo_alloc(bufmgr, "dst bo", size, 4096);
	}

	for (t = tests; t->name; t++) {
		igt_subtest_f("%s-normal", t->name)
			do_test(fd, t->cache, src, start, dst, 1, no_hang);

		igt_fork_signal_helper();
		igt_subtest_f("%s-interruptible", t->name)
			do_test(fd, t->cache, src, start, dst, 100, no_hang);
		igt_stop_signal_helper();

		igt_subtest_f("%s-hang", t->name)
			do_test(fd, t->cache, src, start, dst, 1, bcs_hang);
	}

	igt_fixture {
		drm_intel_bo_unreference(src[0]);
		drm_intel_bo_unreference(src[1]);
		drm_intel_bo_unreference(dst[0]);
		drm_intel_bo_unreference(dst[1]);

		intel_batchbuffer_free(batch);
		drm_intel_bufmgr_destroy(bufmgr);
	}

	igt_fixture
		close(fd);
}
