/*
 * Copyright © 2017-2019 Intel Corporation
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

#include <drm.h>

#include "igt_rand.h"
#include "igt_vgem.h"
#include "sync_file.h"

#define LO 0
#define HI 1
#define NOISE 2

#define MAX_PRIO LOCAL_I915_CONTEXT_MAX_USER_PRIORITY
#define MIN_PRIO LOCAL_I915_CONTEXT_MIN_USER_PRIORITY

static int priorities[] = {
	[LO] = MIN_PRIO / 2,
	[HI] = MAX_PRIO / 2,
};

#define MAX_ELSP_QLEN 16

IGT_TEST_DESCRIPTION("Test shared contexts.");

static void create_shared_gtt(int i915, unsigned int flags)
#define DETACHED 0x1
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 obj = {
		.handle = gem_create(i915, 4096),
	};
	struct drm_i915_gem_execbuffer2 execbuf = {
		.buffers_ptr = to_user_pointer(&obj),
		.buffer_count = 1,
	};
	uint32_t parent, child;

	gem_write(i915, obj.handle, 0, &bbe, sizeof(bbe));
	gem_execbuf(i915, &execbuf);
	gem_sync(i915, obj.handle);

	child = flags & DETACHED ? gem_context_create(i915) : 0;
	igt_until_timeout(2) {
		parent = flags & DETACHED ? child : 0;
		child = gem_context_clone(i915,
					  parent, I915_CONTEXT_CLONE_VM,
					  0);
		execbuf.rsvd1 = child;
		gem_execbuf(i915, &execbuf);

		if (flags & DETACHED) {
			gem_context_destroy(i915, parent);
			gem_execbuf(i915, &execbuf);
		} else {
			parent = child;
			gem_context_destroy(i915, parent);
		}

		execbuf.rsvd1 = parent;
		igt_assert_eq(__gem_execbuf(i915, &execbuf), -ENOENT);
		igt_assert_eq(__gem_context_clone(i915,
						  parent, I915_CONTEXT_CLONE_VM,
						  0, &parent), -ENOENT);
	}
	if (flags & DETACHED)
		gem_context_destroy(i915, child);

	gem_sync(i915, obj.handle);
	gem_close(i915, obj.handle);
}

static void disjoint_timelines(int i915)
{
	IGT_CORK_HANDLE(cork);
	igt_spin_t *spin[2];
	uint32_t plug, child;

	igt_require(gem_has_execlists(i915));

	/*
	 * Each context, although they share a vm, are expected to be
	 * distinct timelines. A request queued to one context should be
	 * independent of any shared contexts.
	 */
	child = gem_context_clone(i915, 0, I915_CONTEXT_CLONE_VM, 0);
	plug = igt_cork_plug(&cork, i915);

	spin[0] = __igt_spin_new(i915, .ctx = 0, .dependency = plug);
	spin[1] = __igt_spin_new(i915, .ctx = child);

	/* Wait for the second spinner, will hang if stuck behind the first */
	igt_spin_end(spin[1]);
	gem_sync(i915, spin[1]->handle);

	igt_cork_unplug(&cork);

	igt_spin_free(i915, spin[1]);
	igt_spin_free(i915, spin[0]);
}

static void exhaust_shared_gtt(int i915, unsigned int flags)
#define EXHAUST_LRC 0x1
{
	i915 = gem_reopen_driver(i915);

	igt_fork(pid, 1) {
		const uint32_t bbe = MI_BATCH_BUFFER_END;
		struct drm_i915_gem_exec_object2 obj = {
			.handle = gem_create(i915, 4096)
		};
		struct drm_i915_gem_execbuffer2 execbuf = {
			.buffers_ptr = to_user_pointer(&obj),
			.buffer_count = 1,
		};
		uint32_t parent, child;
		unsigned long count = 0;
		int err;

		gem_write(i915, obj.handle, 0, &bbe, sizeof(bbe));

		child = 0;
		for (;;) {
			parent = child;
			err = __gem_context_clone(i915,
						  parent, I915_CONTEXT_CLONE_VM,
						  0, &child);
			if (err)
				break;

			if (flags & EXHAUST_LRC) {
				execbuf.rsvd1 = child;
				err = __gem_execbuf(i915, &execbuf);
				if (err)
					break;
			}

			count++;
		}
		gem_sync(i915, obj.handle);

		igt_info("Created %lu shared contexts, before %d (%s)\n",
			 count, err, strerror(-err));
	}
	close(i915);
	igt_waitchildren();
}

static void exec_shared_gtt(int i915, unsigned int ring)
{
	const int gen = intel_gen(intel_get_drm_devid(i915));
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 obj = {};
	struct drm_i915_gem_execbuffer2 execbuf = {
		.buffers_ptr = to_user_pointer(&obj),
		.buffer_count = 1,
		.flags = ring,
	};
	uint32_t scratch, *s;
	uint32_t batch, cs[16];
	uint64_t offset;
	int i;

	gem_require_ring(i915, ring);
	igt_require(gem_can_store_dword(i915, ring));

	/* Find a hole big enough for both objects later */
	scratch = gem_create(i915, 16384);
	gem_write(i915, scratch, 0, &bbe, sizeof(bbe));
	obj.handle = scratch;
	gem_execbuf(i915, &execbuf);
	gem_close(i915, scratch);
	obj.flags |= EXEC_OBJECT_PINNED; /* reuse this address */

	scratch = gem_create(i915, 4096);
	s = gem_mmap__wc(i915, scratch, 0, 4096, PROT_WRITE);

	gem_set_domain(i915, scratch, I915_GEM_DOMAIN_WC, I915_GEM_DOMAIN_WC);
	s[0] = bbe;
	s[64] = bbe;

	/* Load object into place in the GTT */
	obj.handle = scratch;
	gem_execbuf(i915, &execbuf);
	offset = obj.offset;

	/* Presume nothing causes an eviction in the meantime! */

	batch = gem_create(i915, 4096);

	i = 0;
	cs[i] = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
	if (gen >= 8) {
		cs[++i] = obj.offset;
		cs[++i] = obj.offset >> 32;
	} else if (gen >= 4) {
		cs[++i] = 0;
		cs[++i] = obj.offset;
	} else {
		cs[i]--;
		cs[++i] = obj.offset;
	}
	cs[++i] = 0xc0ffee;
	cs[++i] = bbe;
	gem_write(i915, batch, 0, cs, sizeof(cs));

	obj.handle = batch;
	obj.offset += 8192; /* make sure we don't cause an eviction! */
	execbuf.rsvd1 = gem_context_clone(i915, 0, I915_CONTEXT_CLONE_VM, 0);
	if (gen > 3 && gen < 6)
		execbuf.flags |= I915_EXEC_SECURE;
	gem_execbuf(i915, &execbuf);

	/* Check the scratch didn't move */
	obj.handle = scratch;
	obj.offset = -1;
	obj.flags &= ~EXEC_OBJECT_PINNED;
	execbuf.batch_start_offset = 64 * sizeof(s[0]);
	gem_execbuf(i915, &execbuf);
	igt_assert_eq_u64(obj.offset, offset);
	gem_context_destroy(i915, execbuf.rsvd1);

	gem_sync(i915, batch); /* write hazard lies */
	gem_close(i915, batch);

	/*
	 * If we created the new context with the old GTT, the write
	 * into the stale location of scratch will have landed in the right
	 * object. Otherwise, it should read the previous value of
	 * MI_BATCH_BUFFER_END.
	 */
	igt_assert_eq_u32(*s, 0xc0ffee);

	munmap(s, 4096);
	gem_close(i915, scratch);
}

static int nop_sync(int i915, uint32_t ctx, unsigned int ring, int64_t timeout)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 obj = {
		.handle = gem_create(i915, 4096),
	};
	struct drm_i915_gem_execbuffer2 execbuf = {
		.buffers_ptr = to_user_pointer(&obj),
		.buffer_count = 1,
		.flags = ring,
		.rsvd1 = ctx,
	};
	int err;

	gem_write(i915, obj.handle, 0, &bbe, sizeof(bbe));
	gem_execbuf(i915, &execbuf);
	err = gem_wait(i915, obj.handle, &timeout);
	gem_close(i915, obj.handle);

	return err;
}

static bool has_single_timeline(int i915)
{
	uint32_t ctx;

	__gem_context_clone(i915, 0, 0,
			    I915_CONTEXT_CREATE_FLAGS_SINGLE_TIMELINE,
			    &ctx);
	if (ctx)
		gem_context_destroy(i915, ctx);

	return ctx != 0;
}

static void single_timeline(int i915)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 obj = {
		.handle = gem_create(i915, 4096),
	};
	struct drm_i915_gem_execbuffer2 execbuf = {
		.buffers_ptr = to_user_pointer(&obj),
		.buffer_count = 1,
	};
	struct sync_fence_info rings[16];
	struct sync_file_info sync_file_info = {
		.num_fences = 1,
	};
	unsigned int engine;
	int n;

	igt_require(has_single_timeline(i915));

	gem_write(i915, obj.handle, 0, &bbe, sizeof(bbe));
	gem_execbuf(i915, &execbuf);
	gem_sync(i915, obj.handle);

	/*
	 * For a "single timeline" context, each ring is on the common
	 * timeline, unlike a normal context where each ring has an
	 * independent timeline. That is no matter which engine we submit
	 * to, it reports the same timeline name and fence context. However,
	 * the fence context is not reported through the sync_fence_info.
	 */
	execbuf.rsvd1 =
		gem_context_clone(i915, 0, 0,
				  I915_CONTEXT_CREATE_FLAGS_SINGLE_TIMELINE);
	execbuf.flags = I915_EXEC_FENCE_OUT;
	n = 0;
	for_each_engine(i915, engine) {
		gem_execbuf_wr(i915, &execbuf);
		sync_file_info.sync_fence_info = to_user_pointer(&rings[n]);
		do_ioctl(execbuf.rsvd2 >> 32, SYNC_IOC_FILE_INFO, &sync_file_info);
		close(execbuf.rsvd2 >> 32);

		igt_info("ring[%d] fence: %s %s\n",
			 n, rings[n].driver_name, rings[n].obj_name);
		n++;
	}
	gem_sync(i915, obj.handle);
	gem_close(i915, obj.handle);

	for (int i = 1; i < n; i++) {
		igt_assert(!strcmp(rings[0].driver_name, rings[i].driver_name));
		igt_assert(!strcmp(rings[0].obj_name, rings[i].obj_name));
	}
}

static void exec_single_timeline(int i915, unsigned int engine)
{
	unsigned int other;
	igt_spin_t *spin;
	uint32_t ctx;

	igt_require(gem_ring_has_physical_engine(i915, engine));
	igt_require(has_single_timeline(i915));

	/*
	 * On an ordinary context, a blockage on one engine doesn't prevent
	 * execution on an other.
	 */
	ctx = 0;
	spin = NULL;
	for_each_physical_engine(i915, other) {
		if (other == engine)
			continue;

		if (spin == NULL) {
			spin = __igt_spin_new(i915, .ctx = ctx, .engine = other);
		} else {
			struct drm_i915_gem_execbuffer2 execbuf = {
				.buffers_ptr = spin->execbuf.buffers_ptr,
				.buffer_count = spin->execbuf.buffer_count,
				.flags = other,
				.rsvd1 = ctx,
			};
			gem_execbuf(i915, &execbuf);
		}
	}
	igt_require(spin);
	igt_assert_eq(nop_sync(i915, ctx, engine, NSEC_PER_SEC), 0);
	igt_spin_free(i915, spin);

	/*
	 * But if we create a context with just a single shared timeline,
	 * then it will block waiting for the earlier requests on the
	 * other engines.
	 */
	ctx = gem_context_clone(i915, 0, 0,
				I915_CONTEXT_CREATE_FLAGS_SINGLE_TIMELINE);
	spin = NULL;
	for_each_physical_engine(i915, other) {
		if (other == engine)
			continue;

		if (spin == NULL) {
			spin = __igt_spin_new(i915, .ctx = ctx, .engine = other);
		} else {
			struct drm_i915_gem_execbuffer2 execbuf = {
				.buffers_ptr = spin->execbuf.buffers_ptr,
				.buffer_count = spin->execbuf.buffer_count,
				.flags = other,
				.rsvd1 = ctx,
			};
			gem_execbuf(i915, &execbuf);
		}
	}
	igt_assert(spin);
	igt_assert_eq(nop_sync(i915, ctx, engine, NSEC_PER_SEC), -ETIME);
	igt_spin_free(i915, spin);
}

static void store_dword(int i915, uint32_t ctx, unsigned ring,
			uint32_t target, uint32_t offset, uint32_t value,
			uint32_t cork, unsigned write_domain)
{
	const int gen = intel_gen(intel_get_drm_devid(i915));
	struct drm_i915_gem_exec_object2 obj[3];
	struct drm_i915_gem_relocation_entry reloc;
	struct drm_i915_gem_execbuffer2 execbuf;
	uint32_t batch[16];
	int i;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj + !cork);
	execbuf.buffer_count = 2 + !!cork;
	execbuf.flags = ring;
	if (gen < 6)
		execbuf.flags |= I915_EXEC_SECURE;
	execbuf.rsvd1 = ctx;

	memset(obj, 0, sizeof(obj));
	obj[0].handle = cork;
	obj[1].handle = target;
	obj[2].handle = gem_create(i915, 4096);

	memset(&reloc, 0, sizeof(reloc));
	reloc.target_handle = obj[1].handle;
	reloc.presumed_offset = 0;
	reloc.offset = sizeof(uint32_t);
	reloc.delta = offset;
	reloc.read_domains = I915_GEM_DOMAIN_INSTRUCTION;
	reloc.write_domain = write_domain;
	obj[2].relocs_ptr = to_user_pointer(&reloc);
	obj[2].relocation_count = 1;

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
	gem_write(i915, obj[2].handle, 0, batch, sizeof(batch));
	gem_execbuf(i915, &execbuf);
	gem_close(i915, obj[2].handle);
}

static uint32_t create_highest_priority(int i915)
{
	uint32_t ctx = gem_context_create(i915);

	/*
	 * If there is no priority support, all contexts will have equal
	 * priority (and therefore the max user priority), so no context
	 * can overtake us, and we effectively can form a plug.
	 */
	__gem_context_set_priority(i915, ctx, MAX_PRIO);

	return ctx;
}

static void unplug_show_queue(int i915, struct igt_cork *c, unsigned int engine)
{
	igt_spin_t *spin[MAX_ELSP_QLEN];

	for (int n = 0; n < ARRAY_SIZE(spin); n++) {
		const struct igt_spin_factory opts = {
			.ctx = create_highest_priority(i915),
			.engine = engine,
		};
		spin[n] = __igt_spin_factory(i915, &opts);
		gem_context_destroy(i915, opts.ctx);
	}

	igt_cork_unplug(c); /* batches will now be queued on the engine */
	igt_debugfs_dump(i915, "i915_engine_info");

	for (int n = 0; n < ARRAY_SIZE(spin); n++)
		igt_spin_free(i915, spin[n]);
}

static uint32_t store_timestamp(int i915,
				uint32_t ctx, unsigned ring,
				unsigned mmio_base,
				int offset)
{
	const bool r64b = intel_gen(intel_get_drm_devid(i915)) >= 8;
	struct drm_i915_gem_exec_object2 obj = {
		.handle = gem_create(i915, 4096),
		.relocation_count = 1,
	};
	struct drm_i915_gem_relocation_entry reloc = {
		.target_handle = obj.handle,
		.offset = 2 * sizeof(uint32_t),
		.delta = offset * sizeof(uint32_t),
		.read_domains = I915_GEM_DOMAIN_INSTRUCTION,
	};
	struct drm_i915_gem_execbuffer2 execbuf = {
		.buffers_ptr = to_user_pointer(&obj),
		.buffer_count = 1,
		.flags = ring,
		.rsvd1 = ctx,
	};
	uint32_t batch[] = {
		0x24 << 23 | (1 + r64b), /* SRM */
		mmio_base + 0x358,
		offset * sizeof(uint32_t),
		0,
		MI_BATCH_BUFFER_END
	};

	igt_require(intel_gen(intel_get_drm_devid(i915)) >= 7);

	gem_write(i915, obj.handle, 0, batch, sizeof(batch));
	obj.relocs_ptr = to_user_pointer(&reloc);

	gem_execbuf(i915, &execbuf);

	return obj.handle;
}

static void independent(int i915, unsigned ring, unsigned flags)
{
	const int TIMESTAMP = 1023;
	uint32_t handle[ARRAY_SIZE(priorities)];
	igt_spin_t *spin[MAX_ELSP_QLEN];
	unsigned int mmio_base;

	/* XXX i915_query()! */
	switch (ring) {
	case I915_EXEC_DEFAULT:
	case I915_EXEC_RENDER:
		mmio_base = 0x2000;
		break;
#if 0
	case I915_EXEC_BSD:
		mmio_base = 0x12000;
		break;
#endif
	case I915_EXEC_BLT:
		mmio_base = 0x22000;
		break;

#define GEN11_VECS0_BASE 0x1c8000
#define GEN11_VECS1_BASE 0x1d8000
	case I915_EXEC_VEBOX:
		if (intel_gen(intel_get_drm_devid(i915)) >= 11)
			mmio_base = GEN11_VECS0_BASE;
		else
			mmio_base = 0x1a000;
		break;

	default:
		igt_skip("mmio base not known\n");
	}

	for (int n = 0; n < ARRAY_SIZE(spin); n++) {
		const struct igt_spin_factory opts = {
			.ctx = create_highest_priority(i915),
			.engine = ring,
		};
		spin[n] = __igt_spin_factory(i915, &opts);
		gem_context_destroy(i915, opts.ctx);
	}

	for (int i = 0; i < ARRAY_SIZE(priorities); i++) {
		uint32_t ctx = gem_queue_create(i915);
		gem_context_set_priority(i915, ctx, priorities[i]);
		handle[i] = store_timestamp(i915, ctx, ring, mmio_base, TIMESTAMP);
		gem_context_destroy(i915, ctx);
	}

	for (int n = 0; n < ARRAY_SIZE(spin); n++)
		igt_spin_free(i915, spin[n]);

	for (int i = 0; i < ARRAY_SIZE(priorities); i++) {
		uint32_t *ptr;

		ptr = gem_mmap__gtt(i915, handle[i], 4096, PROT_READ);
		gem_set_domain(i915, handle[i], /* no write hazard lies! */
			       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
		gem_close(i915, handle[i]);

		handle[i] = ptr[TIMESTAMP];
		munmap(ptr, 4096);

		igt_debug("ctx[%d] .prio=%d, timestamp=%u\n",
			  i, priorities[i], handle[i]);
	}

	igt_assert((int32_t)(handle[HI] - handle[LO]) < 0);
}

static void reorder(int i915, unsigned ring, unsigned flags)
#define EQUAL 1
{
	IGT_CORK_HANDLE(cork);
	uint32_t scratch;
	uint32_t *ptr;
	uint32_t ctx[2];
	uint32_t plug;

	ctx[LO] = gem_queue_create(i915);
	gem_context_set_priority(i915, ctx[LO], MIN_PRIO);

	ctx[HI] = gem_queue_create(i915);
	gem_context_set_priority(i915, ctx[HI], flags & EQUAL ? MIN_PRIO : 0);

	scratch = gem_create(i915, 4096);
	plug = igt_cork_plug(&cork, i915);

	/* We expect the high priority context to be executed first, and
	 * so the final result will be value from the low priority context.
	 */
	store_dword(i915, ctx[LO], ring, scratch, 0, ctx[LO], plug, 0);
	store_dword(i915, ctx[HI], ring, scratch, 0, ctx[HI], plug, 0);

	unplug_show_queue(i915, &cork, ring);
	gem_close(i915, plug);

	gem_context_destroy(i915, ctx[LO]);
	gem_context_destroy(i915, ctx[HI]);

	ptr = gem_mmap__gtt(i915, scratch, 4096, PROT_READ);
	gem_set_domain(i915, scratch, /* no write hazard lies! */
		       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	gem_close(i915, scratch);

	if (flags & EQUAL) /* equal priority, result will be fifo */
		igt_assert_eq_u32(ptr[0], ctx[HI]);
	else
		igt_assert_eq_u32(ptr[0], ctx[LO]);
	munmap(ptr, 4096);
}

static void promotion(int i915, unsigned ring)
{
	IGT_CORK_HANDLE(cork);
	uint32_t result, dep;
	uint32_t *ptr;
	uint32_t ctx[3];
	uint32_t plug;

	ctx[LO] = gem_queue_create(i915);
	gem_context_set_priority(i915, ctx[LO], MIN_PRIO);

	ctx[HI] = gem_queue_create(i915);
	gem_context_set_priority(i915, ctx[HI], 0);

	ctx[NOISE] = gem_queue_create(i915);
	gem_context_set_priority(i915, ctx[NOISE], MIN_PRIO/2);

	result = gem_create(i915, 4096);
	dep = gem_create(i915, 4096);

	plug = igt_cork_plug(&cork, i915);

	/* Expect that HI promotes LO, so the order will be LO, HI, NOISE.
	 *
	 * fifo would be NOISE, LO, HI.
	 * strict priority would be  HI, NOISE, LO
	 */
	store_dword(i915, ctx[NOISE], ring, result, 0, ctx[NOISE], plug, 0);
	store_dword(i915, ctx[LO], ring, result, 0, ctx[LO], plug, 0);

	/* link LO <-> HI via a dependency on another buffer */
	store_dword(i915, ctx[LO], ring, dep, 0, ctx[LO], 0, I915_GEM_DOMAIN_INSTRUCTION);
	store_dword(i915, ctx[HI], ring, dep, 0, ctx[HI], 0, 0);

	store_dword(i915, ctx[HI], ring, result, 0, ctx[HI], 0, 0);

	unplug_show_queue(i915, &cork, ring);
	gem_close(i915, plug);

	gem_context_destroy(i915, ctx[NOISE]);
	gem_context_destroy(i915, ctx[LO]);
	gem_context_destroy(i915, ctx[HI]);

	ptr = gem_mmap__gtt(i915, dep, 4096, PROT_READ);
	gem_set_domain(i915, dep, /* no write hazard lies! */
			I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	gem_close(i915, dep);

	igt_assert_eq_u32(ptr[0], ctx[HI]);
	munmap(ptr, 4096);

	ptr = gem_mmap__gtt(i915, result, 4096, PROT_READ);
	gem_set_domain(i915, result, /* no write hazard lies! */
			I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	gem_close(i915, result);

	igt_assert_eq_u32(ptr[0], ctx[NOISE]);
	munmap(ptr, 4096);
}

static void smoketest(int i915, unsigned ring, unsigned timeout)
{
	const int ncpus = sysconf(_SC_NPROCESSORS_ONLN);
	unsigned engines[16];
	unsigned nengine;
	unsigned engine;
	uint32_t scratch;
	uint32_t *ptr;

	nengine = 0;
	for_each_physical_engine(i915, engine)
		engines[nengine++] = engine;
	igt_require(nengine);

	scratch = gem_create(i915, 4096);
	igt_fork(child, ncpus) {
		unsigned long count = 0;
		uint32_t ctx;

		hars_petruska_f54_1_random_perturb(child);

		ctx = gem_queue_create(i915);
		igt_until_timeout(timeout) {
			int prio;

			prio = hars_petruska_f54_1_random_unsafe_max(MAX_PRIO - MIN_PRIO) + MIN_PRIO;
			gem_context_set_priority(i915, ctx, prio);

			engine = engines[hars_petruska_f54_1_random_unsafe_max(nengine)];
			store_dword(i915, ctx, engine, scratch,
				    8*child + 0, ~child,
				    0, 0);
			for (unsigned int step = 0; step < 8; step++)
				store_dword(i915, ctx, engine, scratch,
					    8*child + 4, count++,
					    0, 0);
		}
		gem_context_destroy(i915, ctx);
	}
	igt_waitchildren();

	ptr = gem_mmap__gtt(i915, scratch, 4096, PROT_READ);
	gem_set_domain(i915, scratch, /* no write hazard lies! */
			I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	gem_close(i915, scratch);

	for (unsigned n = 0; n < ncpus; n++) {
		igt_assert_eq_u32(ptr[2*n], ~n);
		/*
		 * Note this count is approximate due to unconstrained
		 * ordering of the dword writes between engines.
		 *
		 * Take the result with a pinch of salt.
		 */
		igt_info("Child[%d] completed %u cycles\n",  n, ptr[2*n+1]);
	}
	munmap(ptr, 4096);
}

igt_main
{
	const struct intel_execution_engine *e;
	int i915 = -1;

	igt_fixture {
		i915 = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(i915);
	}

	igt_subtest_group {
		igt_fixture {
			igt_require(gem_contexts_has_shared_gtt(i915));
			igt_fork_hang_detector(i915);
		}

		igt_subtest("create-shared-gtt")
			create_shared_gtt(i915, 0);

		igt_subtest("detached-shared-gtt")
			create_shared_gtt(i915, DETACHED);

		igt_subtest("disjoint-timelines")
			disjoint_timelines(i915);

		igt_subtest("single-timeline")
			single_timeline(i915);

		igt_subtest("exhaust-shared-gtt")
			exhaust_shared_gtt(i915, 0);

		igt_subtest("exhaust-shared-gtt-lrc")
			exhaust_shared_gtt(i915, EXHAUST_LRC);

		for (e = intel_execution_engines; e->name; e++) {
			igt_subtest_f("exec-shared-gtt-%s", e->name)
				exec_shared_gtt(i915, e->exec_id | e->flags);

			igt_subtest_f("exec-single-timeline-%s", e->name)
				exec_single_timeline(i915,
						     e->exec_id | e->flags);

			/*
			 * Check that the shared contexts operate independently,
			 * that is requests on one ("queue") can be scheduled
			 * around another queue. We only check the basics here,
			 * enough to reduce the queue into just another context,
			 * and so rely on gem_exec_schedule to prove the rest.
			 */
			igt_subtest_group {
				igt_fixture {
					gem_require_ring(i915, e->exec_id | e->flags);
					igt_require(gem_can_store_dword(i915, e->exec_id | e->flags));
					igt_require(gem_scheduler_enabled(i915));
					igt_require(gem_scheduler_has_ctx_priority(i915));
				}

				igt_subtest_f("Q-independent-%s", e->name)
					independent(i915, e->exec_id | e->flags, 0);

				igt_subtest_f("Q-in-order-%s", e->name)
					reorder(i915, e->exec_id | e->flags, EQUAL);

				igt_subtest_f("Q-out-order-%s", e->name)
					reorder(i915, e->exec_id | e->flags, 0);

				igt_subtest_f("Q-promotion-%s", e->name)
					promotion(i915, e->exec_id | e->flags);

				igt_subtest_f("Q-smoketest-%s", e->name)
					smoketest(i915, e->exec_id | e->flags, 5);
			}
		}

		igt_subtest("Q-smoketest-all") {
			igt_require(gem_scheduler_enabled(i915));
			igt_require(gem_scheduler_has_ctx_priority(i915));
			smoketest(i915, -1, 30);
		}

		igt_fixture {
			igt_stop_hang_detector();
		}
	}
}
