/*
 * Copyright © 2009,2011 Intel Corporation
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

/** @file gem_tiled_fence_blits.c
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
#include "igt_x86.h"

enum { width = 512, height = 512 };
static uint32_t linear[width * height];
static const int bo_size = sizeof(linear);

static uint32_t create_bo(int fd, uint32_t start_val)
{
	uint32_t handle;
	uint32_t *ptr;

	handle = gem_create(fd, bo_size);
	gem_set_tiling(fd, handle, I915_TILING_X, width * 4);

	/* Fill the BO with dwords starting at start_val */
	ptr = gem_mmap__gtt(fd, handle, bo_size, PROT_WRITE);
	gem_set_domain(fd, handle, I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	for (int i = 0; i < width * height; i++)
		ptr[i] = start_val++;
	munmap(ptr, bo_size);

	return handle;
}

static void check_bo(int fd, uint32_t handle, uint32_t start_val)
{
	uint32_t *ptr;

	ptr = gem_mmap__gtt(fd, handle, bo_size, PROT_READ);
	gem_set_domain(fd, handle, I915_GEM_DOMAIN_GTT, 0);
	igt_memcpy_from_wc(linear, ptr, bo_size);
	munmap(ptr, bo_size);

	for (int i = 0; i < width * height; i++) {
		igt_assert_f(linear[i] == start_val,
			     "Expected 0x%08x, found 0x%08x "
			     "at offset 0x%08x\n",
			     start_val, linear[i], i * 4);
		start_val++;
	}
}

static uint32_t
create_batch(int fd, struct drm_i915_gem_relocation_entry *reloc)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	const bool has_64b_reloc = gen >= 8;
	uint32_t *batch;
	uint32_t handle;
	uint32_t pitch;
	int i = 0;

	handle = gem_create(fd, 4096);
	batch = gem_mmap__cpu(fd, handle, 0, 4096, PROT_WRITE);

	batch[i] = (XY_SRC_COPY_BLT_CMD |
		    XY_SRC_COPY_BLT_WRITE_ALPHA |
		    XY_SRC_COPY_BLT_WRITE_RGB);
	if (gen >= 4) {
		batch[i] |= (XY_SRC_COPY_BLT_SRC_TILED |
			     XY_SRC_COPY_BLT_DST_TILED);
		pitch = width;
	} else {
		pitch = 4 * width;
	}
	batch[i++] |= 6 + 2 * has_64b_reloc;

	batch[i++] = 3 << 24 | 0xcc << 16 | pitch;
	batch[i++] = 0; /* dst (x1, y1) */
	batch[i++] = height << 16 | width; /* dst (x2 y2) */
	reloc[0].offset = sizeof(*batch) * i;
	reloc[0].read_domains = I915_GEM_DOMAIN_RENDER;
	reloc[0].write_domain = I915_GEM_DOMAIN_RENDER;
	batch[i++] = 0;
	if (has_64b_reloc)
		batch[i++] = 0;

	batch[i++] = 0; /* src (x1, y1) */
	batch[i++] = pitch;
	reloc[1].offset = sizeof(*batch) * i;
	reloc[1].read_domains = I915_GEM_DOMAIN_RENDER;
	batch[i++] = 0;
	if (has_64b_reloc)
		batch[i++] = 0;

	batch[i++] = MI_BATCH_BUFFER_END;
	munmap(batch, 4096);

	return handle;
}

static void run_test(int fd, int count)
{
	struct drm_i915_gem_relocation_entry reloc[2];
	struct drm_i915_gem_exec_object2 obj[3];
	struct drm_i915_gem_execbuffer2 eb;
	uint32_t *bo, *bo_start_val;
	uint32_t start = 0;

	memset(reloc, 0, sizeof(reloc));
	memset(obj, 0, sizeof(obj));
	obj[0].flags = EXEC_OBJECT_NEEDS_FENCE;
	obj[1].flags = EXEC_OBJECT_NEEDS_FENCE;
	obj[2].handle = create_batch(fd, reloc);
	obj[2].relocs_ptr = to_user_pointer(reloc);
	obj[2].relocation_count = ARRAY_SIZE(reloc);

	memset(&eb, 0, sizeof(eb));
	eb.buffers_ptr = to_user_pointer(obj);
	eb.buffer_count = ARRAY_SIZE(obj);
	if (intel_gen(intel_get_drm_devid(fd)) >= 6)
		eb.flags = I915_EXEC_BLT;

	count |= 1;
	igt_info("Using %d 1MiB buffers\n", count);

	bo = malloc(count * (sizeof(*bo) + sizeof(*bo_start_val)));
	igt_assert(bo);
	bo_start_val = bo + count;

	for (int i = 0; i < count; i++) {
		bo[i] = create_bo(fd, start);
		bo_start_val[i] = start;
		start += width * height;
	}

	for (int dst = 0; dst < count; dst++) {
		int src = count - dst - 1;

		if (src == dst)
			continue;

		reloc[0].target_handle = obj[0].handle = bo[dst];
		reloc[1].target_handle = obj[1].handle = bo[src];

		gem_execbuf(fd, &eb);
		bo_start_val[dst] = bo_start_val[src];
	}

	for (int i = 0; i < count * 4; i++) {
		int src = random() % count;
		int dst = random() % count;

		if (src == dst)
			continue;

		reloc[0].target_handle = obj[0].handle = bo[dst];
		reloc[1].target_handle = obj[1].handle = bo[src];

		gem_execbuf(fd, &eb);
		bo_start_val[dst] = bo_start_val[src];
	}

	for (int i = 0; i < count; i++) {
		check_bo(fd, bo[i], bo_start_val[i]);
		gem_close(fd, bo[i]);
	}
	free(bo);

	gem_close(fd, obj[2].handle);
}

#define MAX_32b ((1ull << 32) - 4096)

igt_main
{
	int fd;

	igt_fixture {
		fd = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(fd);
	}

	igt_subtest("basic")
		run_test (fd, 2);

	/* the rest of the tests are too long for simulation */
	igt_skip_on_simulation();

	igt_subtest("normal") {
		uint64_t count;

		count = gem_aperture_size(fd);
		if (count >> 32)
			count = MAX_32b;
		count = 3 * count / bo_size / 2;
		intel_require_memory(count, bo_size, CHECK_RAM);
		run_test(fd, count);
	}

	igt_fixture
		close(fd);
}
