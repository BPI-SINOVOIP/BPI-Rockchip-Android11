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

/** @file gem_ringfill.c
 *
 * This is a test of doing many tiny batchbuffer operations, in the hope of
 * catching failure to manage the ring properly near full.
 */

#include "igt.h"
#include "igt_device.h"
#include "igt_gt.h"
#include "igt_vgem.h"
#include "i915/gem_ring.h"

#include <signal.h>
#include <sys/ioctl.h>

#define INTERRUPTIBLE 0x1
#define HANG 0x2
#define CHILD 0x8
#define FORKED 0x8
#define BOMB 0x10
#define SUSPEND 0x20
#define HIBERNATE 0x40
#define NEWFD 0x80

static unsigned int ring_size;

static void check_bo(int fd, uint32_t handle)
{
	uint32_t *map;
	int i;

	igt_debug("Verifying result\n");
	map = gem_mmap__cpu(fd, handle, 0, 4096, PROT_READ);
	gem_set_domain(fd, handle, I915_GEM_DOMAIN_CPU, 0);
	for (i = 0; i < 1024; i++)
		igt_assert_eq(map[i], i);
	munmap(map, 4096);
}

static void fill_ring(int fd,
		      struct drm_i915_gem_execbuffer2 *execbuf,
		      unsigned flags, unsigned timeout)
{
	/* The ring we've been using is 128k, and each rendering op
	 * will use at least 8 dwords:
	 *
	 * BATCH_START
	 * BATCH_START offset
	 * MI_FLUSH
	 * STORE_DATA_INDEX
	 * STORE_DATA_INDEX offset
	 * STORE_DATA_INDEX value
	 * MI_USER_INTERRUPT
	 * (padding)
	 *
	 * So iterate just a little more than that -- if we don't fill the ring
	 * doing this, we aren't likely to with this test.
	 */
	igt_debug("Executing execbuf %d times\n", 128*1024/(8*4));
	igt_until_timeout(timeout) {
		igt_while_interruptible(flags & INTERRUPTIBLE) {
			for (typeof(ring_size) i = 0; i < ring_size; i++)
				gem_execbuf(fd, execbuf);
		}
	}
}

static int setup_execbuf(int fd,
			 struct drm_i915_gem_execbuffer2 *execbuf,
			 struct drm_i915_gem_exec_object2 *obj,
			 struct drm_i915_gem_relocation_entry *reloc,
			 unsigned int ring)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	uint32_t *batch, *b;
	int ret;
	int i;

	memset(execbuf, 0, sizeof(*execbuf));
	memset(obj, 0, 2*sizeof(*obj));
	memset(reloc, 0, 1024*sizeof(*reloc));

	execbuf->buffers_ptr = to_user_pointer(obj);
	execbuf->flags = ring | (1 << 11) | (1 << 12);

	if (gen > 3 && gen < 6)
		execbuf->flags |= I915_EXEC_SECURE;

	obj[0].handle = gem_create(fd, 4096);
	gem_write(fd, obj[0].handle, 0, &bbe, sizeof(bbe));
	execbuf->buffer_count = 1;
	ret = __gem_execbuf(fd, execbuf);
	if (ret)
		return ret;

	obj[0].flags |= EXEC_OBJECT_WRITE;
	obj[1].handle = gem_create(fd, 1024*16 + 4096);

	obj[1].relocs_ptr = to_user_pointer(reloc);
	obj[1].relocation_count = 1024;

	batch = gem_mmap__cpu(fd, obj[1].handle, 0, 16*1024 + 4096,
			      PROT_WRITE | PROT_READ);
	gem_set_domain(fd, obj[1].handle,
		       I915_GEM_DOMAIN_CPU, I915_GEM_DOMAIN_CPU);

	b = batch;
	for (i = 0; i < 1024; i++) {
		uint64_t offset;

		reloc[i].presumed_offset = obj[0].offset;
		reloc[i].offset = (b - batch + 1) * sizeof(*batch);
		reloc[i].delta = i * sizeof(uint32_t);
		reloc[i].read_domains = I915_GEM_DOMAIN_INSTRUCTION;
		reloc[i].write_domain = I915_GEM_DOMAIN_INSTRUCTION;

		offset = obj[0].offset + reloc[i].delta;
		*b++ = MI_STORE_DWORD_IMM;
		if (gen >= 8) {
			*b++ = offset;
			*b++ = offset >> 32;
		} else if (gen >= 4) {
			if (gen < 6)
				b[-1] |= 1 << 22;
			*b++ = 0;
			*b++ = offset;
			reloc[i].offset += sizeof(*batch);
		} else {
			b[-1] |= 1 << 22;
			b[-1] -= 1;
			*b++ = offset;
		}
		*b++ = i;
	}
	*b++ = MI_BATCH_BUFFER_END;
	munmap(batch, 16*1024+4096);

	execbuf->buffer_count = 2;
	gem_execbuf(fd, execbuf);

	check_bo(fd, obj[0].handle);
	return 0;
}

static void run_test(int fd, unsigned ring, unsigned flags, unsigned timeout)
{
	struct drm_i915_gem_exec_object2 obj[2];
	struct drm_i915_gem_relocation_entry reloc[1024];
	struct drm_i915_gem_execbuffer2 execbuf;
	igt_hang_t hang;

	gem_require_ring(fd, ring);
	igt_require(gem_can_store_dword(fd, ring));

	if (flags & (SUSPEND | HIBERNATE))
		run_test(fd, ring, 0, 0);

	gem_quiescent_gpu(fd);
	igt_require(setup_execbuf(fd, &execbuf, obj, reloc, ring) == 0);

	memset(&hang, 0, sizeof(hang));
	if (flags & HANG)
		hang = igt_hang_ring(fd, ring & ~(3<<13));

	if (flags & (CHILD | FORKED | BOMB)) {
		int nchild;

		if (flags & FORKED)
			nchild = sysconf(_SC_NPROCESSORS_ONLN);
		else if (flags & BOMB)
			nchild = 8*sysconf(_SC_NPROCESSORS_ONLN);
		else
			nchild = 1;

		igt_debug("Forking %d children\n", nchild);
		igt_fork(child, nchild) {
			if (flags & NEWFD) {
				fd = drm_open_driver(DRIVER_INTEL);
				setup_execbuf(fd, &execbuf, obj, reloc, ring);
			}
			fill_ring(fd, &execbuf, flags, timeout);
		}

		if (flags & SUSPEND)
			igt_system_suspend_autoresume(SUSPEND_STATE_MEM,
						      SUSPEND_TEST_NONE);

		if (flags & HIBERNATE)
			igt_system_suspend_autoresume(SUSPEND_STATE_DISK,
						      SUSPEND_TEST_NONE);

		if (flags & NEWFD)
			fill_ring(fd, &execbuf, flags, timeout);

		igt_waitchildren();
	} else
		fill_ring(fd, &execbuf, flags, timeout);

	if (flags & HANG)
		igt_post_hang_ring(fd, hang);
	else
		check_bo(fd, obj[0].handle);

	gem_close(fd, obj[1].handle);
	gem_close(fd, obj[0].handle);

	gem_quiescent_gpu(fd);

	if (flags & (SUSPEND | HIBERNATE))
		run_test(fd, ring, 0, 0);
}

igt_main
{
	const struct {
		const char *suffix;
		unsigned flags;
		unsigned timeout;
		bool basic;
	} modes[] = {
		{ "", 0, 0, true},
		{ "-interruptible", INTERRUPTIBLE, 1, true },
		{ "-hang", HANG, 10, true },
		{ "-child", CHILD, 0 },
		{ "-forked", FORKED, 0, true },
		{ "-fd", FORKED | NEWFD, 0, true },
		{ "-bomb", BOMB | NEWFD | INTERRUPTIBLE, 150 },
		{ "-S3", BOMB | SUSPEND, 30 },
		{ "-S4", BOMB | HIBERNATE, 30 },
		{ NULL }
	}, *m;
	bool master = false;
	int fd = -1;

	igt_fixture {
		int gen;

		fd = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(fd);
		igt_require(gem_can_store_dword(fd, 0));
		gen = intel_gen(intel_get_drm_devid(fd));
		if (gen > 3 && gen < 6) { /* ctg and ilk need secure batches */
			igt_device_set_master(fd);
			master = true;
		}

		ring_size = gem_measure_ring_inflight(fd, ALL_ENGINES, 0);
		igt_info("Ring size: %d batches\n", ring_size);
		igt_require(ring_size);
	}

	for (m = modes; m->suffix; m++) {
		const struct intel_execution_engine *e;

		for (e = intel_execution_engines; e->name; e++) {
			igt_subtest_f("%s%s%s",
				      m->basic && !e->exec_id ? "basic-" : "",
				      e->name,
				      m->suffix) {
				igt_skip_on(m->flags & NEWFD && master);
				if (m->flags & (HANG|SUSPEND|HIBERNATE))
					igt_skip_on_simulation();
				run_test(fd, e->exec_id | e->flags,
					 m->flags, m->timeout);
			}
		}
	}

	igt_fixture
		close(fd);
}
