/*
 * Copyright © 2016 Intel Corporation
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
 */

#include "igt.h"

#define LOCAL_OBJECT_ASYNC (1 << 6)
#define LOCAL_PARAM_HAS_EXEC_ASYNC 43

IGT_TEST_DESCRIPTION("Check that we can issue concurrent writes across the engines.");

static void store_dword(int fd, unsigned ring,
			uint32_t target, uint32_t offset, uint32_t value)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	struct drm_i915_gem_exec_object2 obj[2];
	struct drm_i915_gem_relocation_entry reloc;
	struct drm_i915_gem_execbuffer2 execbuf;
	uint32_t batch[16];
	int i;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = 2;
	execbuf.flags = ring;
	if (gen < 6)
		execbuf.flags |= I915_EXEC_SECURE;

	memset(obj, 0, sizeof(obj));
	obj[0].handle = target;
	obj[0].flags = LOCAL_OBJECT_ASYNC;
	obj[1].handle = gem_create(fd, 4096);

	memset(&reloc, 0, sizeof(reloc));
	reloc.target_handle = obj[0].handle;
	reloc.presumed_offset = 0;
	reloc.offset = sizeof(uint32_t);
	reloc.delta = offset;
	reloc.read_domains = I915_GEM_DOMAIN_INSTRUCTION;
	reloc.write_domain = I915_GEM_DOMAIN_INSTRUCTION;
	obj[1].relocs_ptr = to_user_pointer(&reloc);
	obj[1].relocation_count = 1;

	i = 0;
	batch[i] = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
	if (gen >= 8) {
		batch[++i] = offset;
		batch[++i] = 0;
	} else if (gen >= 4) {
		batch[++i] = 0;
		batch[++i] = offset;
		reloc.offset += sizeof(uint32_t);
	} else {
		batch[i]--;
		batch[++i] = offset;
	}
	batch[++i] = value;
	batch[++i] = MI_BATCH_BUFFER_END;
	gem_write(fd, obj[1].handle, 0, batch, sizeof(batch));
	gem_execbuf(fd, &execbuf);
	gem_close(fd, obj[1].handle);
}

static void one(int fd, unsigned ring, uint32_t flags)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	struct drm_i915_gem_exec_object2 obj[2];
#define SCRATCH 0
#define BATCH 1
	struct drm_i915_gem_relocation_entry reloc;
	struct drm_i915_gem_execbuffer2 execbuf;
	unsigned int other;
	uint32_t *batch;
	int i;

	/* On the target ring, create a looping batch that marks
	 * the scratch for write. Then on the other rings try and
	 * write into that target. If it blocks we hang the GPU...
	 */

	memset(obj, 0, sizeof(obj));
	obj[SCRATCH].handle = gem_create(fd, 4096);

	obj[BATCH].handle = gem_create(fd, 4096);
	obj[BATCH].relocs_ptr = to_user_pointer(&reloc);
	obj[BATCH].relocation_count = 1;

	memset(&reloc, 0, sizeof(reloc));
	reloc.target_handle = obj[BATCH].handle; /* recurse */
	reloc.presumed_offset = 0;
	reloc.offset = sizeof(uint32_t);
	reloc.delta = 0;
	reloc.read_domains = I915_GEM_DOMAIN_COMMAND;
	reloc.write_domain = 0;

	batch = gem_mmap__wc(fd, obj[BATCH].handle, 0, 4096, PROT_WRITE);
	gem_set_domain(fd, obj[BATCH].handle,
			I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);

	i = 0;
	batch[i] = MI_BATCH_BUFFER_START;
	if (gen >= 8) {
		batch[i] |= 1 << 8 | 1;
		batch[++i] = 0;
		batch[++i] = 0;
	} else if (gen >= 6) {
		batch[i] |= 1 << 8;
		batch[++i] = 0;
	} else {
		batch[i] |= 2 << 6;
		batch[++i] = 0;
		if (gen < 4) {
			batch[i] |= 1;
			reloc.delta = 1;
		}
	}
	i++;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = 2;
	execbuf.flags = ring | flags;
	igt_require(__gem_execbuf(fd, &execbuf) == 0);
	gem_close(fd, obj[BATCH].handle);

	i = 0;
	for_each_physical_engine(fd, other) {
		if (other == ring)
			continue;

		if (!gem_can_store_dword(fd, other))
			continue;

		store_dword(fd, other, obj[SCRATCH].handle, 4*i, i);
		i++;
	}

	*batch = MI_BATCH_BUFFER_END;
	__sync_synchronize();
	munmap(batch, 4096);

	batch = gem_mmap__wc(fd, obj[SCRATCH].handle, 0, 4096, PROT_READ);
	/* The kernel only tracks the last *submitted* write (but all reads),
	 * so to ensure *all* rings are flushed, we flush all reads even
	 * though we only need read access for ourselves.
	 */
	gem_set_domain(fd, obj[SCRATCH].handle,
		       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	gem_close(fd, obj[SCRATCH].handle);
	while (i--)
		igt_assert_eq_u32(batch[i], i);
	munmap(batch, 4096);
}

static bool has_async_execbuf(int fd)
{
	drm_i915_getparam_t gp;
	int async = -1;

	gp.param = LOCAL_PARAM_HAS_EXEC_ASYNC;
	gp.value = &async;
	drmIoctl(fd, DRM_IOCTL_I915_GETPARAM, &gp);

	return async > 0;
}

igt_main
{
	const struct intel_execution_engine *e;
	int fd = -1;

	igt_skip_on_simulation();

	igt_fixture {
		fd = drm_open_driver_master(DRIVER_INTEL);
		igt_require_gem(fd);
		gem_require_mmap_wc(fd);
		igt_require(has_async_execbuf(fd));
		igt_require(gem_can_store_dword(fd, 0));
		igt_fork_hang_detector(fd);
	}

	for (e = intel_execution_engines; e->name; e++) {
		/* default exec-id is purely symbolic */
		if (e->exec_id == 0)
			continue;

		igt_subtest_f("concurrent-writes-%s", e->name) {
			igt_require(gem_ring_has_physical_engine(fd, e->exec_id | e->flags));
			igt_require(gem_can_store_dword(fd, e->exec_id | e->flags));
			one(fd, e->exec_id, e->flags);
		}
	}

	igt_fixture {
		igt_stop_hang_detector();
		close(fd);
	}
}
