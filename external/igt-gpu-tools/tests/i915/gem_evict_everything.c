/*
 * Copyright © 2011,2012,2014 Intel Corporation
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
 *    Daniel Vetter <daniel.vetter@ffwll.ch>
 *
 */

/*
 * Testcase: run a couple of big batches to force the eviction code.
 */

#include "igt.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include "drm.h"

#include "eviction_common.c"

IGT_TEST_DESCRIPTION("Run a couple of big batches to force the eviction"
		     " code.");

#define HEIGHT 256
#define WIDTH 1024

static int
copy(int fd, uint32_t dst, uint32_t src, uint32_t *all_bo, int n_bo)
{
	uint32_t batch[12];
	struct drm_i915_gem_relocation_entry reloc[2];
	struct drm_i915_gem_exec_object2 *obj;
	struct drm_i915_gem_execbuffer2 exec;
	uint32_t handle;
	int n, ret, i=0;

	batch[i++] = (XY_SRC_COPY_BLT_CMD |
		    XY_SRC_COPY_BLT_WRITE_ALPHA |
		    XY_SRC_COPY_BLT_WRITE_RGB | 6);
	if (intel_gen(intel_get_drm_devid(fd)) >= 8)
		batch[i - 1] += 2;
	batch[i++] = (3 << 24) | /* 32 bits */
		  (0xcc << 16) | /* copy ROP */
		  WIDTH*4;
	batch[i++] = 0; /* dst x1,y1 */
	batch[i++] = (HEIGHT << 16) | WIDTH; /* dst x2,y2 */
	batch[i++] = 0; /* dst reloc */
	if (intel_gen(intel_get_drm_devid(fd)) >= 8)
		batch[i++] = 0; /* FIXME */
	batch[i++] = 0; /* src x1,y1 */
	batch[i++] = WIDTH*4;
	batch[i++] = 0; /* src reloc */
	if (intel_gen(intel_get_drm_devid(fd)) >= 8)
		batch[i++] = 0; /* FIXME */
	batch[i++] = MI_BATCH_BUFFER_END;
	batch[i++] = MI_NOOP;

	handle = gem_create(fd, 4096);
	gem_write(fd, handle, 0, batch, sizeof(batch));

	reloc[0].target_handle = dst;
	reloc[0].delta = 0;
	reloc[0].offset = 4 * sizeof(batch[0]);
	reloc[0].presumed_offset = 0;
	reloc[0].read_domains = I915_GEM_DOMAIN_RENDER;
	reloc[0].write_domain = I915_GEM_DOMAIN_RENDER;

	reloc[1].target_handle = src;
	reloc[1].delta = 0;
	reloc[1].offset = 7 * sizeof(batch[0]);
	if (intel_gen(intel_get_drm_devid(fd)) >= 8)
		reloc[1].offset += sizeof(batch[0]);
	reloc[1].presumed_offset = 0;
	reloc[1].read_domains = I915_GEM_DOMAIN_RENDER;
	reloc[1].write_domain = 0;

	obj = calloc(n_bo + 1, sizeof(*obj));
	for (n = 0; n < n_bo; n++)
		obj[n].handle = all_bo[n];
	obj[n].handle = handle;
	obj[n].relocation_count = 2;
	obj[n].relocs_ptr = to_user_pointer(reloc);

	memset(&exec, 0, sizeof(exec));
	exec.buffers_ptr = to_user_pointer(obj);
	exec.buffer_count = n_bo + 1;
	if (HAS_BLT_RING(intel_get_drm_devid(fd)))
		exec.flags |= I915_EXEC_BLT;
	ret = __gem_execbuf(fd, &exec);

	gem_close(fd, handle);
	free(obj);

	return ret;
}

static void clear(int fd, uint32_t handle, uint64_t size)
{
	void *base = gem_mmap__cpu(fd, handle, 0, size, PROT_WRITE);
	memset(base, 0, size);
	munmap(base, size);
}

static struct igt_eviction_test_ops fault_ops = {
	.create = gem_create,
	.close = gem_close,
	.copy = copy,
	.clear = clear,
};

static void test_forking_evictions(int fd, uint64_t size, uint64_t count,
				   unsigned flags)
{
	uint64_t trash_count;

	trash_count = intel_get_total_ram_mb() * 11 / 10;
	intel_require_memory(trash_count, size, CHECK_RAM | CHECK_SWAP);

	forking_evictions(fd, &fault_ops, size, count, trash_count, flags);
}

static void test_mlocked_evictions(int fd, uint64_t size, uint64_t count)
{
	mlocked_evictions(fd, &fault_ops, size, count);
}

static void test_swapping_evictions(int fd, uint64_t size, uint64_t count)
{
	int trash_count;

	trash_count = intel_get_total_ram_mb() * 11 / 10;
	intel_require_memory(trash_count, size, CHECK_RAM | CHECK_SWAP);

	swapping_evictions(fd, &fault_ops, size, count, trash_count);
}

static void test_minor_evictions(int fd, uint64_t size, uint64_t count)
{
	minor_evictions(fd, &fault_ops, size, count);
}

static void test_major_evictions(int fd, uint64_t size, uint64_t count)
{
	major_evictions(fd, &fault_ops, size, count);
}

#define MAX_32b ((1ull << 32) - 4096)

igt_main
{
	uint64_t size, count;
	int fd;

	size = count = 0;
	fd = -1;

	igt_fixture {
		fd = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(fd);

		size = 1024 * 1024;
		count = gem_aperture_size(fd);
		if (count >> 32)
			count = MAX_32b;
		count = 3 * count / size / 4;

		igt_fork_hang_detector(fd);
	}

	for (unsigned flags = 0; flags < ALL_FORKING_EVICTIONS + 1; flags++) {
		igt_subtest_f("forked%s%s%s-%s",
		    flags & FORKING_EVICTIONS_SWAPPING ? "-swapping" : "",
		    flags & FORKING_EVICTIONS_DUP_DRMFD ? "-multifd" : "",
		    flags & FORKING_EVICTIONS_MEMORY_PRESSURE ?
				"-mempressure" : "",
		    flags & FORKING_EVICTIONS_INTERRUPTIBLE ?
				"interruptible" : "normal") {
			test_forking_evictions(fd, size, count, flags);
		}
	}

	igt_subtest("mlocked-normal")
		test_mlocked_evictions(fd, size, count);

	igt_subtest("swapping-normal")
		test_swapping_evictions(fd, size, count);

	igt_subtest("minor-normal")
		test_minor_evictions(fd, size, count);

	igt_subtest("major-normal") {
		size = gem_aperture_size(fd);
		if (size >> 32)
			size = MAX_32b;
		size = 3 * size / 4;
		count = 4;
		test_major_evictions(fd, size, count);
	}

	igt_fixture {
		size = 1024 * 1024;
		count = gem_aperture_size(fd);
		if (count >> 32)
			count = MAX_32b;
		count = 3 * count / size / 4;
	}

	igt_fork_signal_helper();

	igt_subtest("mlocked-interruptible")
		test_mlocked_evictions(fd, size, count);

	igt_subtest("swapping-interruptible")
		test_swapping_evictions(fd, size, count);

	igt_subtest("minor-interruptible")
		test_minor_evictions(fd, size, count);

	igt_subtest("major-interruptible") {
		size = gem_aperture_size(fd);
		if (size >> 32)
			size = MAX_32b;
		size = 3 * size / 4;
		count = 4;
		test_major_evictions(fd, size, count);
	}

	igt_fixture {
		igt_stop_hang_detector();
		igt_fork_hang_helper();

		size = 1024 * 1024;
		count = gem_aperture_size(fd);
		if (count >> 32)
			count = MAX_32b;
		count = 3 * count / size / 4;
	}

	igt_subtest("mlocked-hang")
		test_mlocked_evictions(fd, size, count);

	igt_subtest("swapping-hang")
		test_swapping_evictions(fd, size, count);

	igt_subtest("minor-hang")
		test_minor_evictions(fd, size, count);

	igt_subtest("major-hang") {
		size = gem_aperture_size(fd);
		if (size >> 32)
			size = MAX_32b;
		size = 3 * size / 4;
		count = 4;
		test_major_evictions(fd, size, count);
	}

	igt_stop_signal_helper();

	igt_fixture {
		igt_stop_hang_helper();
		close(fd);
	}
}
