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
#include "igt_sysfs.h"
#include "igt_vgem.h"
#include "sw_sync.h"
#include "i915/gem_ring.h"

#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/signal.h>

IGT_TEST_DESCRIPTION("Check that execbuf waits for explicit fences");

#define LOCAL_EXEC_FENCE_IN (1 << 16)
#define LOCAL_EXEC_FENCE_OUT (1 << 17)
#define LOCAL_EXEC_FENCE_SUBMIT (1 << 20)

#define LOCAL_EXEC_FENCE_ARRAY (1 << 19)
struct local_gem_exec_fence {
	uint32_t handle;
	uint32_t flags;
#define LOCAL_EXEC_FENCE_WAIT (1 << 0)
#define LOCAL_EXEC_FENCE_SIGNAL (1 << 1)
};

#ifndef SYNC_IOC_MERGE
struct sync_merge_data {
	char    name[32];
	int32_t fd2;
	int32_t fence;
	uint32_t        flags;
	uint32_t        pad;
};
#define SYNC_IOC_MAGIC '>'
#define SYNC_IOC_MERGE _IOWR(SYNC_IOC_MAGIC, 3, struct sync_merge_data)
#endif

static void store(int fd, unsigned ring, int fence, uint32_t target, unsigned offset_value)
{
	const int SCRATCH = 0;
	const int BATCH = 1;
	const int gen = intel_gen(intel_get_drm_devid(fd));
	struct drm_i915_gem_exec_object2 obj[2];
	struct drm_i915_gem_relocation_entry reloc;
	struct drm_i915_gem_execbuffer2 execbuf;
	uint32_t batch[16];
	int i;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = 2;
	execbuf.flags = ring | LOCAL_EXEC_FENCE_IN;
	execbuf.rsvd2 = fence;
	if (gen < 6)
		execbuf.flags |= I915_EXEC_SECURE;

	memset(obj, 0, sizeof(obj));
	obj[SCRATCH].handle = target;

	obj[BATCH].handle = gem_create(fd, 4096);
	obj[BATCH].relocs_ptr = to_user_pointer(&reloc);
	obj[BATCH].relocation_count = 1;
	memset(&reloc, 0, sizeof(reloc));

	i = 0;
	reloc.target_handle = obj[SCRATCH].handle;
	reloc.presumed_offset = -1;
	reloc.offset = sizeof(uint32_t) * (i + 1);
	reloc.delta = sizeof(uint32_t) * offset_value;
	reloc.read_domains = I915_GEM_DOMAIN_INSTRUCTION;
	reloc.write_domain = I915_GEM_DOMAIN_INSTRUCTION;
	batch[i] = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
	if (gen >= 8) {
		batch[++i] = reloc.delta;
		batch[++i] = 0;
	} else if (gen >= 4) {
		batch[++i] = 0;
		batch[++i] = reloc.delta;
		reloc.offset += sizeof(uint32_t);
	} else {
		batch[i]--;
		batch[++i] = reloc.delta;
	}
	batch[++i] = offset_value;
	batch[++i] = MI_BATCH_BUFFER_END;
	gem_write(fd, obj[BATCH].handle, 0, batch, sizeof(batch));
	gem_execbuf(fd, &execbuf);
	gem_close(fd, obj[BATCH].handle);
}

static bool fence_busy(int fence)
{
	return poll(&(struct pollfd){fence, POLLIN}, 1, 0) == 0;
}

#define HANG 0x1
#define NONBLOCK 0x2
#define WAIT 0x4

static void test_fence_busy(int fd, unsigned ring, unsigned flags)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	struct drm_i915_gem_exec_object2 obj;
	struct drm_i915_gem_relocation_entry reloc;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct timespec tv;
	uint32_t *batch;
	int fence, i, timeout;

	gem_quiescent_gpu(fd);

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;
	execbuf.flags = ring | LOCAL_EXEC_FENCE_OUT;

	memset(&obj, 0, sizeof(obj));
	obj.handle = gem_create(fd, 4096);

	obj.relocs_ptr = to_user_pointer(&reloc);
	obj.relocation_count = 1;
	memset(&reloc, 0, sizeof(reloc));

	batch = gem_mmap__wc(fd, obj.handle, 0, 4096, PROT_WRITE);
	gem_set_domain(fd, obj.handle,
		       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);

	reloc.target_handle = obj.handle; /* recurse */
	reloc.presumed_offset = 0;
	reloc.offset = sizeof(uint32_t);
	reloc.delta = 0;
	reloc.read_domains = I915_GEM_DOMAIN_COMMAND;
	reloc.write_domain = 0;

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

	execbuf.rsvd2 = -1;
	gem_execbuf_wr(fd, &execbuf);
	fence = execbuf.rsvd2 >> 32;
	igt_assert(fence != -1);

	igt_assert(gem_bo_busy(fd, obj.handle));
	igt_assert(fence_busy(fence));

	timeout = 120;
	if ((flags & HANG) == 0) {
		*batch = MI_BATCH_BUFFER_END;
		__sync_synchronize();
		timeout = 1;
	}
	munmap(batch, 4096);

	if (flags & WAIT) {
		struct pollfd pfd = { .fd = fence, .events = POLLIN };
		igt_assert(poll(&pfd, 1, timeout*1000) == 1);
	} else {
		memset(&tv, 0, sizeof(tv));
		while (fence_busy(fence))
			igt_assert(igt_seconds_elapsed(&tv) < timeout);
	}

	igt_assert(!gem_bo_busy(fd, obj.handle));
	igt_assert_eq(sync_fence_status(fence),
		      flags & HANG ? -EIO : SYNC_FENCE_OK);

	close(fence);
	gem_close(fd, obj.handle);

	gem_quiescent_gpu(fd);
}

static void test_fence_busy_all(int fd, unsigned flags)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	struct drm_i915_gem_exec_object2 obj;
	struct drm_i915_gem_relocation_entry reloc;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct timespec tv;
	uint32_t *batch;
	unsigned int engine;
	int all, i, timeout;

	gem_quiescent_gpu(fd);

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;

	memset(&obj, 0, sizeof(obj));
	obj.handle = gem_create(fd, 4096);

	obj.relocs_ptr = to_user_pointer(&reloc);
	obj.relocation_count = 1;
	memset(&reloc, 0, sizeof(reloc));

	batch = gem_mmap__wc(fd, obj.handle, 0, 4096, PROT_WRITE);
	gem_set_domain(fd, obj.handle,
		       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);

	reloc.target_handle = obj.handle; /* recurse */
	reloc.presumed_offset = 0;
	reloc.offset = sizeof(uint32_t);
	reloc.delta = 0;
	reloc.read_domains = I915_GEM_DOMAIN_COMMAND;
	reloc.write_domain = 0;

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

	all = -1;
	for_each_engine(fd, engine) {
		int fence, new;

		execbuf.flags = engine | LOCAL_EXEC_FENCE_OUT;
		execbuf.rsvd2 = -1;
		gem_execbuf_wr(fd, &execbuf);
		fence = execbuf.rsvd2 >> 32;
		igt_assert(fence != -1);

		if (all < 0) {
			all = fence;
			continue;
		}

		new = sync_fence_merge(all, fence);
		igt_assert_lte(0, new);
		close(all);
		close(fence);

		all = new;
	}

	igt_assert(gem_bo_busy(fd, obj.handle));
	igt_assert(fence_busy(all));

	timeout = 120;
	if ((flags & HANG) == 0) {
		*batch = MI_BATCH_BUFFER_END;
		__sync_synchronize();
		timeout = 1;
	}
	munmap(batch, 4096);

	if (flags & WAIT) {
		struct pollfd pfd = { .fd = all, .events = POLLIN };
		igt_assert(poll(&pfd, 1, timeout*1000) == 1);
	} else {
		memset(&tv, 0, sizeof(tv));
		while (fence_busy(all))
			igt_assert(igt_seconds_elapsed(&tv) < timeout);
	}

	igt_assert(!gem_bo_busy(fd, obj.handle));
	igt_assert_eq(sync_fence_status(all),
		      flags & HANG ? -EIO : SYNC_FENCE_OK);

	close(all);
	gem_close(fd, obj.handle);

	gem_quiescent_gpu(fd);
}

static void test_fence_await(int fd, unsigned ring, unsigned flags)
{
	uint32_t scratch = gem_create(fd, 4096);
	igt_spin_t *spin;
	unsigned engine;
	uint32_t *out;
	int i;

	igt_require(gem_can_store_dword(fd, 0));

	out = gem_mmap__wc(fd, scratch, 0, 4096, PROT_WRITE);
	gem_set_domain(fd, scratch,
			I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);

	spin = igt_spin_new(fd,
			    .engine = ring,
			    .flags = IGT_SPIN_FENCE_OUT);
	igt_assert(spin->out_fence != -1);

	i = 0;
	for_each_physical_engine(fd, engine) {
		if (!gem_can_store_dword(fd, engine))
			continue;

		if (flags & NONBLOCK) {
			store(fd, engine, spin->out_fence, scratch, i);
		} else {
			igt_fork(child, 1)
				store(fd, engine, spin->out_fence, scratch, i);
		}

		i++;
	}

	sleep(1);

	/* Check for invalidly completing the task early */
	igt_assert(fence_busy(spin->out_fence));
	for (int n = 0; n < i; n++)
		igt_assert_eq_u32(out[n], 0);

	if ((flags & HANG) == 0)
		igt_spin_end(spin);

	igt_waitchildren();

	gem_set_domain(fd, scratch, I915_GEM_DOMAIN_GTT, 0);
	while (i--)
		igt_assert_eq_u32(out[i], i);
	munmap(out, 4096);

	igt_spin_free(fd, spin);
	gem_close(fd, scratch);
}

static void resubmit(int fd, uint32_t handle, unsigned int ring, int count)
{
	struct drm_i915_gem_exec_object2 obj = { .handle = handle };
	struct drm_i915_gem_execbuffer2 execbuf = {
		.buffers_ptr = to_user_pointer(&obj),
		.buffer_count = 1,
		.flags = ring,
	};
	while (count--)
		gem_execbuf(fd, &execbuf);
}

static void alarm_handler(int sig)
{
}

static int __execbuf(int fd, struct drm_i915_gem_execbuffer2 *execbuf)
{
	int err;

	err = 0;
	if (ioctl(fd, DRM_IOCTL_I915_GEM_EXECBUFFER2_WR, execbuf))
		err = -errno;

	errno = 0;
	return err;
}

static void test_parallel(int fd, unsigned int master)
{
	const int SCRATCH = 0;
	const int BATCH = 1;
	const int gen = intel_gen(intel_get_drm_devid(fd));
	struct drm_i915_gem_exec_object2 obj[2];
	struct drm_i915_gem_relocation_entry reloc[2];
	struct drm_i915_gem_execbuffer2 execbuf;
	uint32_t scratch = gem_create(fd, 4096);
	uint32_t *out = gem_mmap__wc(fd, scratch, 0, 4096, PROT_READ);
	uint32_t handle[16];
	uint32_t batch[16];
	igt_spin_t *spin;
	unsigned engine;
	IGT_CORK_HANDLE(c);
	uint32_t plug;
	int i, x = 0;

	plug = igt_cork_plug(&c, fd);

	/* Fill the queue with many requests so that the next one has to
	 * wait before it can be executed by the hardware.
	 */
	spin = igt_spin_new(fd, .engine = master, .dependency = plug);
	resubmit(fd, spin->handle, master, 16);

	/* Now queue the master request and its secondaries */
	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = 2;
	execbuf.flags = master | LOCAL_EXEC_FENCE_OUT;
	if (gen < 6)
		execbuf.flags |= I915_EXEC_SECURE;

	memset(obj, 0, sizeof(obj));
	obj[SCRATCH].handle = scratch;

	obj[BATCH].handle = gem_create(fd, 4096);
	handle[x] = obj[BATCH].handle;
	obj[BATCH].relocs_ptr = to_user_pointer(&reloc);
	obj[BATCH].relocation_count = 2;
	memset(reloc, 0, sizeof(reloc));

	i = 0;

	reloc[0].target_handle = obj[SCRATCH].handle;
	reloc[0].presumed_offset = -1;
	reloc[0].offset = sizeof(uint32_t) * (i + 1);
	reloc[0].delta = sizeof(uint32_t) * x++;
	reloc[0].read_domains = I915_GEM_DOMAIN_INSTRUCTION;
	reloc[0].write_domain = 0; /* lies */

	batch[i] = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
	if (gen >= 8) {
		batch[++i] = reloc[0].presumed_offset + reloc[0].delta;
		batch[++i] = (reloc[0].presumed_offset + reloc[0].delta) >> 32;
	} else if (gen >= 4) {
		batch[++i] = 0;
		batch[++i] = reloc[0].presumed_offset + reloc[0].delta;
		reloc[0].offset += sizeof(uint32_t);
	} else {
		batch[i]--;
		batch[++i] = reloc[0].presumed_offset + reloc[0].delta;
	}
	batch[++i] = ~0u ^ x;

	reloc[1].target_handle = obj[BATCH].handle; /* recurse */
	reloc[1].presumed_offset = 0;
	reloc[1].offset = sizeof(uint32_t) * (i + 2);
	reloc[1].delta = 0;
	reloc[1].read_domains = I915_GEM_DOMAIN_COMMAND;
	reloc[1].write_domain = 0;

	batch[++i] = MI_BATCH_BUFFER_START;
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
			reloc[1].delta = 1;
		}
	}
	batch[++i] = MI_BATCH_BUFFER_END;
	igt_assert(i < sizeof(batch)/sizeof(batch[0]));
	gem_write(fd, obj[BATCH].handle, 0, batch, sizeof(batch));
	gem_execbuf_wr(fd, &execbuf);

	igt_assert(execbuf.rsvd2);
	execbuf.rsvd2 >>= 32; /* out fence -> in fence */
	obj[BATCH].relocation_count = 1;

	/* Queue all secondaries */
	for_each_physical_engine(fd, engine) {
		if (engine == master)
			continue;

		execbuf.flags = engine | LOCAL_EXEC_FENCE_SUBMIT;
		if (gen < 6)
			execbuf.flags |= I915_EXEC_SECURE;

		obj[BATCH].handle = gem_create(fd, 4096);
		handle[x] = obj[BATCH].handle;

		i = 0;
		reloc[0].delta = sizeof(uint32_t) * x++;
		batch[i] = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
		if (gen >= 8) {
			batch[++i] = reloc[0].presumed_offset + reloc[0].delta;
			batch[++i] = (reloc[0].presumed_offset + reloc[0].delta) >> 32;
		} else if (gen >= 4) {
			batch[++i] = 0;
			batch[++i] = reloc[0].presumed_offset + reloc[0].delta;
		} else {
			batch[i]--;
			batch[++i] = reloc[0].presumed_offset + reloc[0].delta;
		}
		batch[++i] = ~0u ^ x;
		batch[++i] = MI_BATCH_BUFFER_END;
		gem_write(fd, obj[BATCH].handle, 0, batch, sizeof(batch));
		gem_execbuf(fd, &execbuf);
	}
	igt_assert(gem_bo_busy(fd, spin->handle));
	close(execbuf.rsvd2);

	/* No secondary should be executed since master is stalled. If there
	 * was no dependency chain at all, the secondaries would start
	 * immediately.
	 */
	for (i = 0; i < x; i++) {
		igt_assert_eq_u32(out[i], 0);
		igt_assert(gem_bo_busy(fd, handle[i]));
	}

	/* Unblock the master */
	igt_cork_unplug(&c);
	gem_close(fd, plug);
	igt_spin_end(spin);

	/* Wait for all secondaries to complete. If we used a regular fence
	 * then the secondaries would not start until the master was complete.
	 * In this case that can only happen with a GPU reset, and so we run
	 * under the hang detector and double check that the master is still
	 * running afterwards.
	 */
	for (i = 1; i < x; i++) {
		while (gem_bo_busy(fd, handle[i]))
			sleep(0);

		igt_assert_f(out[i], "Missing output from engine %d\n", i);
		gem_close(fd, handle[i]);
	}
	munmap(out, 4096);
	gem_close(fd, obj[SCRATCH].handle);

	/* Master should still be spinning, but all output should be written */
	igt_assert(gem_bo_busy(fd, handle[0]));
	out = gem_mmap__wc(fd, handle[0], 0, 4096, PROT_WRITE);
	out[0] = MI_BATCH_BUFFER_END;
	munmap(out, 4096);
	gem_close(fd, handle[0]);
}

static uint32_t batch_create(int fd)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	uint32_t handle;

	handle = gem_create(fd, 4096);
	gem_write(fd, handle, 0, &bbe, sizeof(bbe));

	return handle;
}

static inline uint32_t lower_32_bits(uint64_t x)
{
	return x & 0xffffffff;
}

static inline uint32_t upper_32_bits(uint64_t x)
{
	return x >> 32;
}

static void test_keep_in_fence(int fd, unsigned int engine, unsigned int flags)
{
	struct sigaction sa = { .sa_handler = alarm_handler };
	struct drm_i915_gem_exec_object2 obj = {
		.handle = batch_create(fd),
	};
	struct drm_i915_gem_execbuffer2 execbuf = {
		.buffers_ptr = to_user_pointer(&obj),
		.buffer_count = 1,
		.flags = engine | LOCAL_EXEC_FENCE_OUT,
	};
	unsigned long count, last;
	struct itimerval itv;
	igt_spin_t *spin;
	int fence;

	spin = igt_spin_new(fd, .engine = engine);

	gem_execbuf_wr(fd, &execbuf);
	fence = upper_32_bits(execbuf.rsvd2);

	sigaction(SIGALRM, &sa, NULL);
	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 1000;
	itv.it_value.tv_sec = 0;
	itv.it_value.tv_usec = 10000;
	setitimer(ITIMER_REAL, &itv, NULL);

	execbuf.flags |= LOCAL_EXEC_FENCE_IN;
	execbuf.rsvd2 = fence;

	last = -1;
	count = 0;
	do {
		int err = __execbuf(fd, &execbuf);

		igt_assert_eq(lower_32_bits(execbuf.rsvd2), fence);

		if (err == 0) {
			close(fence);

			fence = upper_32_bits(execbuf.rsvd2);
			execbuf.rsvd2 = fence;

			count++;
			continue;
		}

		igt_assert_eq(err, -EINTR);
		igt_assert_eq(upper_32_bits(execbuf.rsvd2), 0);

		if (last == count)
			break;

		last = count;
	} while (1);

	memset(&itv, 0, sizeof(itv));
	setitimer(ITIMER_REAL, &itv, NULL);

	gem_close(fd, obj.handle);
	close(fence);

	igt_spin_free(fd, spin);
	gem_quiescent_gpu(fd);
}

#define EXPIRED 0x10000
static void test_long_history(int fd, long ring_size, unsigned flags)
{
	const uint32_t sz = 1 << 20;
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 obj[2];
	struct drm_i915_gem_execbuffer2 execbuf;
	unsigned int engines[16], engine;
	unsigned int nengine, n, s;
	unsigned long limit;
	int all_fences;
	IGT_CORK_HANDLE(c);

	limit = -1;
	if (!gem_uses_full_ppgtt(fd))
		limit = ring_size / 3;

	nengine = 0;
	for_each_physical_engine(fd, engine)
		engines[nengine++] = engine;
	igt_require(nengine);

	gem_quiescent_gpu(fd);

	memset(obj, 0, sizeof(obj));
	obj[1].handle = gem_create(fd, sz);
	gem_write(fd, obj[1].handle, sz - sizeof(bbe), &bbe, sizeof(bbe));

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj[1]);
	execbuf.buffer_count = 1;
	execbuf.flags = LOCAL_EXEC_FENCE_OUT;

	gem_execbuf_wr(fd, &execbuf);
	all_fences = execbuf.rsvd2 >> 32;

	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = 2;

	obj[0].handle = igt_cork_plug(&c, fd);

	igt_until_timeout(5) {
		execbuf.rsvd1 = gem_context_create(fd);

		for (n = 0; n < nengine; n++) {
			struct sync_merge_data merge;

			execbuf.flags = engines[n] | LOCAL_EXEC_FENCE_OUT;
			if (__gem_execbuf_wr(fd, &execbuf))
				continue;

			memset(&merge, 0, sizeof(merge));
			merge.fd2 = execbuf.rsvd2 >> 32;
			strcpy(merge.name, "igt");

			do_ioctl(all_fences, SYNC_IOC_MERGE, &merge);

			close(all_fences);
			close(merge.fd2);

			all_fences = merge.fence;
		}

		gem_context_destroy(fd, execbuf.rsvd1);
		if (!--limit)
			break;
	}
	igt_cork_unplug(&c);

	igt_info("History depth = %d\n", sync_fence_count(all_fences));

	if (flags & EXPIRED)
		gem_sync(fd, obj[1].handle);

	execbuf.buffers_ptr = to_user_pointer(&obj[1]);
	execbuf.buffer_count = 1;
	execbuf.rsvd2 = all_fences;
	execbuf.rsvd1 = 0;

	for (s = 0; s < ring_size; s++) {
		for (n = 0; n < nengine; n++) {
			execbuf.flags = engines[n] | LOCAL_EXEC_FENCE_IN;
			if (__gem_execbuf_wr(fd, &execbuf))
				continue;
		}
	}

	close(all_fences);

	gem_sync(fd, obj[1].handle);
	gem_close(fd, obj[1].handle);
	gem_close(fd, obj[0].handle);
}

static void test_fence_flip(int i915)
{
	igt_skip_on_f(1, "no fence-in for atomic flips\n");
}

static bool has_submit_fence(int fd)
{
	struct drm_i915_getparam gp;
	int value = 0;

	memset(&gp, 0, sizeof(gp));
	gp.param = 0xdeadbeef ^ 51; /* I915_PARAM_HAS_EXEC_SUBMIT_FENCE */
	gp.value = &value;

	ioctl(fd, DRM_IOCTL_I915_GETPARAM, &gp, sizeof(gp));
	errno = 0;

	return value;
}

static bool has_syncobj(int fd)
{
	struct drm_get_cap cap = { .capability = 0x13 };
	ioctl(fd, DRM_IOCTL_GET_CAP, &cap);
	return cap.value;
}

static bool exec_has_fence_array(int fd)
{
	struct drm_i915_getparam gp;
	int value = 0;

	memset(&gp, 0, sizeof(gp));
	gp.param = 49; /* I915_PARAM_HAS_EXEC_FENCE_ARRAY */
	gp.value = &value;

	ioctl(fd, DRM_IOCTL_I915_GETPARAM, &gp, sizeof(gp));
	errno = 0;

	return value;
}

static void test_invalid_fence_array(int fd)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj;
	struct local_gem_exec_fence fence;
	void *ptr;

	/* create an otherwise valid execbuf */
	memset(&obj, 0, sizeof(obj));
	obj.handle = gem_create(fd, 4096);
	gem_write(fd, obj.handle, 0, &bbe, sizeof(bbe));
	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;
	gem_execbuf(fd, &execbuf);

	execbuf.flags |= LOCAL_EXEC_FENCE_ARRAY;
	gem_execbuf(fd, &execbuf);

	/* Now add a few invalid fence-array pointers */
	if (sizeof(execbuf.num_cliprects) == sizeof(size_t)) {
		execbuf.num_cliprects = -1;
		igt_assert_eq(__gem_execbuf(fd, &execbuf), -EINVAL);
	}

	execbuf.num_cliprects = 1;
	execbuf.cliprects_ptr = -1;
	igt_assert_eq(__gem_execbuf(fd, &execbuf), -EFAULT);

	memset(&fence, 0, sizeof(fence));
	execbuf.cliprects_ptr = to_user_pointer(&fence);
	igt_assert_eq(__gem_execbuf(fd, &execbuf), -ENOENT);

	ptr = mmap(NULL, 4096, PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
	igt_assert(ptr != MAP_FAILED);
	execbuf.cliprects_ptr = to_user_pointer(ptr);
	igt_assert_eq(__gem_execbuf(fd, &execbuf), -ENOENT);

	do_or_die(mprotect(ptr, 4096, PROT_READ));
	igt_assert_eq(__gem_execbuf(fd, &execbuf), -ENOENT);

	do_or_die(mprotect(ptr, 4096, PROT_NONE));
	igt_assert_eq(__gem_execbuf(fd, &execbuf), -EFAULT);

	munmap(ptr, 4096);
}

static uint32_t __syncobj_create(int fd)
{
	struct local_syncobj_create {
		uint32_t handle, flags;
	} arg;
#define LOCAL_IOCTL_SYNCOBJ_CREATE        DRM_IOWR(0xBF, struct local_syncobj_create)

	memset(&arg, 0, sizeof(arg));
	igt_ioctl(fd, LOCAL_IOCTL_SYNCOBJ_CREATE, &arg);

	return arg.handle;
}

static uint32_t syncobj_create(int fd)
{
	uint32_t ret;

	igt_assert_neq((ret = __syncobj_create(fd)), 0);

	return ret;
}

static int __syncobj_destroy(int fd, uint32_t handle)
{
	struct local_syncobj_destroy {
		uint32_t handle, flags;
	} arg;
#define LOCAL_IOCTL_SYNCOBJ_DESTROY        DRM_IOWR(0xC0, struct local_syncobj_destroy)
	int err = 0;

	memset(&arg, 0, sizeof(arg));
	arg.handle = handle;
	if (igt_ioctl(fd, LOCAL_IOCTL_SYNCOBJ_DESTROY, &arg))
		err = -errno;

	errno = 0;
	return err;
}

static void syncobj_destroy(int fd, uint32_t handle)
{
	igt_assert_eq(__syncobj_destroy(fd, handle), 0);
}

static int __syncobj_to_sync_file(int fd, uint32_t handle)
{
	struct local_syncobj_handle {
		uint32_t handle;
		uint32_t flags;
		int32_t fd;
		uint32_t pad;
	} arg;
#define LOCAL_IOCTL_SYNCOBJ_HANDLE_TO_FD  DRM_IOWR(0xC1, struct local_syncobj_handle)

	memset(&arg, 0, sizeof(arg));
	arg.handle = handle;
	arg.flags = 1 << 0; /* EXPORT_SYNC_FILE */
	if (igt_ioctl(fd, LOCAL_IOCTL_SYNCOBJ_HANDLE_TO_FD, &arg))
		arg.fd = -errno;

	errno = 0;
	return arg.fd;
}

static int syncobj_to_sync_file(int fd, uint32_t handle)
{
	int ret;

	igt_assert_lte(0, (ret = __syncobj_to_sync_file(fd, handle)));

	return ret;
}

static int __syncobj_from_sync_file(int fd, uint32_t handle, int sf)
{
	struct local_syncobj_handle {
		uint32_t handle;
		uint32_t flags;
		int32_t fd;
		uint32_t pad;
	} arg;
#define LOCAL_IOCTL_SYNCOBJ_FD_TO_HANDLE  DRM_IOWR(0xC2, struct local_syncobj_handle)
	int err = 0;

	memset(&arg, 0, sizeof(arg));
	arg.handle = handle;
	arg.fd = sf;
	arg.flags = 1 << 0; /* IMPORT_SYNC_FILE */
	if (igt_ioctl(fd, LOCAL_IOCTL_SYNCOBJ_FD_TO_HANDLE, &arg))
		err = -errno;

	errno = 0;
	return err;
}

static void syncobj_from_sync_file(int fd, uint32_t handle, int sf)
{
	igt_assert_eq(__syncobj_from_sync_file(fd, handle, sf), 0);
}

static int __syncobj_export(int fd, uint32_t handle, int *syncobj)
{
	struct local_syncobj_handle {
		uint32_t handle;
		uint32_t flags;
		int32_t fd;
		uint32_t pad;
	} arg;
	int err;

	memset(&arg, 0, sizeof(arg));
	arg.handle = handle;

	err = 0;
	if (igt_ioctl(fd, LOCAL_IOCTL_SYNCOBJ_HANDLE_TO_FD, &arg))
		err = -errno;

	errno = 0;
	*syncobj = arg.fd;
	return err;
}

static int syncobj_export(int fd, uint32_t handle)
{
	int syncobj;

	igt_assert_eq(__syncobj_export(fd, handle, &syncobj), 0);

	return syncobj;
}

static int __syncobj_import(int fd, int syncobj, uint32_t *handle)
{
	struct local_syncobj_handle {
		uint32_t handle;
		uint32_t flags;
		int32_t fd;
		uint32_t pad;
	} arg;
#define LOCAL_IOCTL_SYNCOBJ_FD_TO_HANDLE  DRM_IOWR(0xC2, struct local_syncobj_handle)
	int err;

	memset(&arg, 0, sizeof(arg));
	arg.fd = syncobj;

	err = 0;
	if (igt_ioctl(fd, LOCAL_IOCTL_SYNCOBJ_FD_TO_HANDLE, &arg))
		err = -errno;

	errno = 0;
	*handle = arg.handle;
	return err;
}

static uint32_t syncobj_import(int fd, int syncobj)
{
	uint32_t handle;

	igt_assert_eq(__syncobj_import(fd, syncobj, &handle), 0);


	return handle;
}

static bool syncobj_busy(int fd, uint32_t handle)
{
	bool result;
	int sf;

	sf = syncobj_to_sync_file(fd, handle);
	result = poll(&(struct pollfd){sf, POLLIN}, 1, 0) == 0;
	close(sf);

	return result;
}

static void test_syncobj_unused_fence(int fd)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 obj;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct local_gem_exec_fence fence = {
		.handle = syncobj_create(fd),
	};
	igt_spin_t *spin = igt_spin_new(fd);

	/* sanity check our syncobj_to_sync_file interface */
	igt_assert_eq(__syncobj_to_sync_file(fd, 0), -ENOENT);

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;
	execbuf.flags = LOCAL_EXEC_FENCE_ARRAY;
	execbuf.cliprects_ptr = to_user_pointer(&fence);
	execbuf.num_cliprects = 1;

	memset(&obj, 0, sizeof(obj));
	obj.handle = gem_create(fd, 4096);
	gem_write(fd, obj.handle, 0, &bbe, sizeof(bbe));

	gem_execbuf(fd, &execbuf);

	/* no flags, the fence isn't created */
	igt_assert_eq(__syncobj_to_sync_file(fd, fence.handle), -EINVAL);
	igt_assert(gem_bo_busy(fd, obj.handle));

	gem_close(fd, obj.handle);
	syncobj_destroy(fd, fence.handle);

	igt_spin_free(fd, spin);
}

static void test_syncobj_invalid_wait(int fd)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 obj;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct local_gem_exec_fence fence = {
		.handle = syncobj_create(fd),
	};

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;
	execbuf.flags = LOCAL_EXEC_FENCE_ARRAY;
	execbuf.cliprects_ptr = to_user_pointer(&fence);
	execbuf.num_cliprects = 1;

	memset(&obj, 0, sizeof(obj));
	obj.handle = gem_create(fd, 4096);
	gem_write(fd, obj.handle, 0, &bbe, sizeof(bbe));

	/* waiting before the fence is set is invalid */
	fence.flags = LOCAL_EXEC_FENCE_WAIT;
	igt_assert_eq(__gem_execbuf(fd, &execbuf), -EINVAL);

	gem_close(fd, obj.handle);
	syncobj_destroy(fd, fence.handle);
}

static void test_syncobj_invalid_flags(int fd)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 obj;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct local_gem_exec_fence fence = {
		.handle = syncobj_create(fd),
	};

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;
	execbuf.flags = LOCAL_EXEC_FENCE_ARRAY;
	execbuf.cliprects_ptr = to_user_pointer(&fence);
	execbuf.num_cliprects = 1;

	memset(&obj, 0, sizeof(obj));
	obj.handle = gem_create(fd, 4096);
	gem_write(fd, obj.handle, 0, &bbe, sizeof(bbe));

	/* set all flags to hit an invalid one */
	fence.flags = ~0;
	igt_assert_eq(__gem_execbuf(fd, &execbuf), -EINVAL);

	gem_close(fd, obj.handle);
	syncobj_destroy(fd, fence.handle);
}

static void test_syncobj_signal(int fd)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 obj;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct local_gem_exec_fence fence = {
		.handle = syncobj_create(fd),
	};
	igt_spin_t *spin = igt_spin_new(fd);

	/* Check that the syncobj is signaled only when our request/fence is */

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;
	execbuf.flags = LOCAL_EXEC_FENCE_ARRAY;
	execbuf.cliprects_ptr = to_user_pointer(&fence);
	execbuf.num_cliprects = 1;

	memset(&obj, 0, sizeof(obj));
	obj.handle = gem_create(fd, 4096);
	gem_write(fd, obj.handle, 0, &bbe, sizeof(bbe));

	fence.flags = LOCAL_EXEC_FENCE_SIGNAL;
	gem_execbuf(fd, &execbuf);

	igt_assert(gem_bo_busy(fd, obj.handle));
	igt_assert(syncobj_busy(fd, fence.handle));

	igt_spin_free(fd, spin);

	gem_sync(fd, obj.handle);
	igt_assert(!gem_bo_busy(fd, obj.handle));
	igt_assert(!syncobj_busy(fd, fence.handle));

	gem_close(fd, obj.handle);
	syncobj_destroy(fd, fence.handle);
}

static void test_syncobj_wait(int fd)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 obj;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct local_gem_exec_fence fence = {
		.handle = syncobj_create(fd),
	};
	igt_spin_t *spin;
	unsigned engine;
	unsigned handle[16];
	int n;

	/* Check that we can use the syncobj to asynchronous wait prior to
	 * execution.
	 */

	gem_quiescent_gpu(fd);

	spin = igt_spin_new(fd);

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;

	memset(&obj, 0, sizeof(obj));
	obj.handle = gem_create(fd, 4096);
	gem_write(fd, obj.handle, 0, &bbe, sizeof(bbe));

	/* Queue a signaler from the blocked engine */
	execbuf.flags = LOCAL_EXEC_FENCE_ARRAY;
	execbuf.cliprects_ptr = to_user_pointer(&fence);
	execbuf.num_cliprects = 1;
	fence.flags = LOCAL_EXEC_FENCE_SIGNAL;
	gem_execbuf(fd, &execbuf);
	igt_assert(gem_bo_busy(fd, spin->handle));

	gem_close(fd, obj.handle);
	obj.handle = gem_create(fd, 4096);
	gem_write(fd, obj.handle, 0, &bbe, sizeof(bbe));

	n = 0;
	for_each_engine(fd, engine) {
		obj.handle = gem_create(fd, 4096);
		gem_write(fd, obj.handle, 0, &bbe, sizeof(bbe));

		/* No inter-engine synchronisation, will complete */
		if (engine == I915_EXEC_BLT) {
			execbuf.flags = engine;
			execbuf.cliprects_ptr = 0;
			execbuf.num_cliprects = 0;
			gem_execbuf(fd, &execbuf);
			gem_sync(fd, obj.handle);
			igt_assert(gem_bo_busy(fd, spin->handle));
		}
		igt_assert(gem_bo_busy(fd, spin->handle));

		/* Now wait upon the blocked engine */
		execbuf.flags = LOCAL_EXEC_FENCE_ARRAY | engine;
		execbuf.cliprects_ptr = to_user_pointer(&fence);
		execbuf.num_cliprects = 1;
		fence.flags = LOCAL_EXEC_FENCE_WAIT;
		gem_execbuf(fd, &execbuf);

		igt_assert(gem_bo_busy(fd, obj.handle));
		handle[n++] = obj.handle;
	}
	syncobj_destroy(fd, fence.handle);

	for (int i = 0; i < n; i++)
		igt_assert(gem_bo_busy(fd, handle[i]));

	igt_spin_free(fd, spin);

	for (int i = 0; i < n; i++) {
		gem_sync(fd, handle[i]);
		gem_close(fd, handle[i]);
	}
}

static void test_syncobj_export(int fd)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 obj;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct local_gem_exec_fence fence = {
		.handle = syncobj_create(fd),
	};
	int export[2];
	igt_spin_t *spin = igt_spin_new(fd);

	/* Check that if we export the syncobj prior to use it picks up
	 * the later fence. This allows a syncobj to establish a channel
	 * between clients that may be updated to a later fence by either
	 * end.
	 */
	for (int n = 0; n < ARRAY_SIZE(export); n++)
		export[n] = syncobj_export(fd, fence.handle);

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;
	execbuf.flags = LOCAL_EXEC_FENCE_ARRAY;
	execbuf.cliprects_ptr = to_user_pointer(&fence);
	execbuf.num_cliprects = 1;

	memset(&obj, 0, sizeof(obj));
	obj.handle = gem_create(fd, 4096);
	gem_write(fd, obj.handle, 0, &bbe, sizeof(bbe));

	fence.flags = LOCAL_EXEC_FENCE_SIGNAL;
	gem_execbuf(fd, &execbuf);

	igt_assert(syncobj_busy(fd, fence.handle));
	igt_assert(gem_bo_busy(fd, obj.handle));

	for (int n = 0; n < ARRAY_SIZE(export); n++) {
		uint32_t import = syncobj_import(fd, export[n]);
		igt_assert(syncobj_busy(fd, import));
		syncobj_destroy(fd, import);
	}

	igt_spin_free(fd, spin);

	gem_sync(fd, obj.handle);
	igt_assert(!gem_bo_busy(fd, obj.handle));
	igt_assert(!syncobj_busy(fd, fence.handle));

	gem_close(fd, obj.handle);
	syncobj_destroy(fd, fence.handle);

	for (int n = 0; n < ARRAY_SIZE(export); n++) {
		uint32_t import = syncobj_import(fd, export[n]);
		igt_assert(!syncobj_busy(fd, import));
		syncobj_destroy(fd, import);
		close(export[n]);
	}
}

static void test_syncobj_repeat(int fd)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	const unsigned nfences = 4096;
	struct drm_i915_gem_exec_object2 obj;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct local_gem_exec_fence *fence;
	int export;
	igt_spin_t *spin = igt_spin_new(fd);

	/* Check that we can wait on the same fence multiple times */
	fence = calloc(nfences, sizeof(*fence));
	fence->handle = syncobj_create(fd);
	export = syncobj_export(fd, fence->handle);
	for (int i = 1; i < nfences; i++)
		fence[i].handle = syncobj_import(fd, export);
	close(export);

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;
	execbuf.flags = LOCAL_EXEC_FENCE_ARRAY;
	execbuf.cliprects_ptr = to_user_pointer(fence);
	execbuf.num_cliprects = nfences;

	memset(&obj, 0, sizeof(obj));
	obj.handle = gem_create(fd, 4096);
	gem_write(fd, obj.handle, 0, &bbe, sizeof(bbe));

	for (int i = 0; i < nfences; i++)
		fence[i].flags = LOCAL_EXEC_FENCE_SIGNAL;

	gem_execbuf(fd, &execbuf);

	for (int i = 0; i < nfences; i++) {
		igt_assert(syncobj_busy(fd, fence[i].handle));
		fence[i].flags |= LOCAL_EXEC_FENCE_WAIT;
	}
	igt_assert(gem_bo_busy(fd, obj.handle));

	gem_execbuf(fd, &execbuf);

	for (int i = 0; i < nfences; i++)
		igt_assert(syncobj_busy(fd, fence[i].handle));
	igt_assert(gem_bo_busy(fd, obj.handle));

	igt_spin_free(fd, spin);

	gem_sync(fd, obj.handle);
	gem_close(fd, obj.handle);

	for (int i = 0; i < nfences; i++) {
		igt_assert(!syncobj_busy(fd, fence[i].handle));
		syncobj_destroy(fd, fence[i].handle);
	}
	free(fence);
}

static void test_syncobj_import(int fd)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 obj;
	struct drm_i915_gem_execbuffer2 execbuf;
	igt_spin_t *spin = igt_spin_new(fd);
	uint32_t sync = syncobj_create(fd);
	int fence;

	/* Check that we can create a syncobj from an explicit fence (which
	 * uses sync_file) and that it acts just like a regular fence.
	 */

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;
	execbuf.flags = LOCAL_EXEC_FENCE_OUT;
	execbuf.rsvd2 = -1;

	memset(&obj, 0, sizeof(obj));
	obj.handle = gem_create(fd, 4096);
	gem_write(fd, obj.handle, 0, &bbe, sizeof(bbe));

	gem_execbuf_wr(fd, &execbuf);

	fence = execbuf.rsvd2 >> 32;
	igt_assert(fence_busy(fence));
	syncobj_from_sync_file(fd, sync, fence);
	close(fence);

	igt_assert(gem_bo_busy(fd, obj.handle));
	igt_assert(syncobj_busy(fd, sync));

	igt_spin_free(fd, spin);

	gem_sync(fd, obj.handle);
	igt_assert(!gem_bo_busy(fd, obj.handle));
	igt_assert(!syncobj_busy(fd, sync));

	gem_close(fd, obj.handle);
	syncobj_destroy(fd, sync);
}

static void test_syncobj_channel(int fd)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 obj;
	struct drm_i915_gem_execbuffer2 execbuf;
	unsigned int *control;
	int syncobj[3];

	/* Create a pair of channels (like a pipe) between two clients
	 * and try to create races on the syncobj.
	 */

	control = mmap(NULL, 4096, PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
	igt_assert(control != MAP_FAILED);

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;
	execbuf.flags = LOCAL_EXEC_FENCE_OUT;
	execbuf.rsvd2 = -1;

	memset(&obj, 0, sizeof(obj));
	obj.handle = gem_create(fd, 4096);
	gem_write(fd, obj.handle, 0, &bbe, sizeof(bbe));

	for (int i = 0; i < ARRAY_SIZE(syncobj); i++) {
		struct local_gem_exec_fence fence;

		execbuf.flags = LOCAL_EXEC_FENCE_ARRAY;
		execbuf.cliprects_ptr = to_user_pointer(&fence);
		execbuf.num_cliprects = 1;

		/* Create a primed fence */
		fence.handle = syncobj_create(fd);
		fence.flags = LOCAL_EXEC_FENCE_SIGNAL;

		gem_execbuf(fd, &execbuf);

		syncobj[i] = fence.handle;
	}

	/* Two processes in ping-pong unison (pipe), one out of sync */
	igt_fork(child, 1) {
		struct local_gem_exec_fence fence[3];
		unsigned long count;

		execbuf.flags = LOCAL_EXEC_FENCE_ARRAY;
		execbuf.cliprects_ptr = to_user_pointer(fence);
		execbuf.num_cliprects = 3;

		fence[0].handle = syncobj[0];
		fence[0].flags = LOCAL_EXEC_FENCE_SIGNAL;

		fence[1].handle = syncobj[1];
		fence[1].flags = LOCAL_EXEC_FENCE_WAIT;

		fence[2].handle = syncobj[2];
		fence[2].flags = LOCAL_EXEC_FENCE_WAIT;

		count = 0;
		while (!*(volatile unsigned *)control) {
			gem_execbuf(fd, &execbuf);
			count++;
		}

		control[1] = count;
	}
	igt_fork(child, 1) {
		struct local_gem_exec_fence fence[3];
		unsigned long count;

		execbuf.flags = LOCAL_EXEC_FENCE_ARRAY;
		execbuf.cliprects_ptr = to_user_pointer(fence);
		execbuf.num_cliprects = 3;

		fence[0].handle = syncobj[0];
		fence[0].flags = LOCAL_EXEC_FENCE_WAIT;

		fence[1].handle = syncobj[1];
		fence[1].flags = LOCAL_EXEC_FENCE_SIGNAL;

		fence[2].handle = syncobj[2];
		fence[2].flags = LOCAL_EXEC_FENCE_WAIT;

		count = 0;
		while (!*(volatile unsigned *)control) {
			gem_execbuf(fd, &execbuf);
			count++;
		}
		control[2] = count;
	}
	igt_fork(child, 1) {
		struct local_gem_exec_fence fence;
		unsigned long count;

		execbuf.flags = LOCAL_EXEC_FENCE_ARRAY;
		execbuf.cliprects_ptr = to_user_pointer(&fence);
		execbuf.num_cliprects = 1;

		fence.handle = syncobj[2];
		fence.flags = LOCAL_EXEC_FENCE_SIGNAL;

		count = 0;
		while (!*(volatile unsigned *)control) {
			gem_execbuf(fd, &execbuf);
			count++;
		}
		control[3] = count;
	}

	sleep(1);
	*control = 1;
	igt_waitchildren();

	igt_info("Pipe=[%u, %u], gooseberry=%u\n",
		 control[1], control[2], control[3]);
	munmap(control, 4096);

	gem_sync(fd, obj.handle);
	gem_close(fd, obj.handle);

	for (int i = 0; i < ARRAY_SIZE(syncobj); i++)
		syncobj_destroy(fd, syncobj[i]);
}

igt_main
{
	const struct intel_execution_engine *e;
	int i915 = -1;

	igt_fixture {
		i915 = drm_open_driver_master(DRIVER_INTEL);
		igt_require_gem(i915);
		igt_require(gem_has_exec_fence(i915));
		gem_require_mmap_wc(i915);

		gem_submission_print_method(i915);
	}

	igt_subtest_group {
		igt_fixture {
			igt_fork_hang_detector(i915);
		}

		igt_subtest("basic-busy-all")
			test_fence_busy_all(i915, 0);
		igt_subtest("basic-wait-all")
			test_fence_busy_all(i915, WAIT);

		igt_fixture {
			igt_stop_hang_detector();
		}

		igt_subtest("busy-hang-all")
			test_fence_busy_all(i915, HANG);
		igt_subtest("wait-hang-all")
			test_fence_busy_all(i915, WAIT | HANG);
	}

	for (e = intel_execution_engines; e->name; e++) {
		igt_subtest_group {
			igt_fixture {
				igt_require(gem_has_ring(i915, e->exec_id | e->flags));
				igt_require(gem_can_store_dword(i915, e->exec_id | e->flags));
			}

			igt_subtest_group {
				igt_fixture {
					igt_fork_hang_detector(i915);
				}

				igt_subtest_f("%sbusy-%s",
						e->exec_id == 0 ? "basic-" : "",
						e->name)
					test_fence_busy(i915, e->exec_id | e->flags, 0);
				igt_subtest_f("%swait-%s",
						e->exec_id == 0 ? "basic-" : "",
						e->name)
					test_fence_busy(i915, e->exec_id | e->flags, WAIT);
				igt_subtest_f("%sawait-%s",
						e->exec_id == 0 ? "basic-" : "",
						e->name)
					test_fence_await(i915, e->exec_id | e->flags, 0);
				igt_subtest_f("nb-await-%s", e->name)
					test_fence_await(i915, e->exec_id | e->flags, NONBLOCK);

				igt_subtest_f("keep-in-fence-%s", e->name)
					test_keep_in_fence(i915, e->exec_id | e->flags, 0);

				if (e->exec_id &&
				    !(e->exec_id == I915_EXEC_BSD && !e->flags)) {
					igt_subtest_f("parallel-%s", e->name) {
						igt_require(has_submit_fence(i915));
						igt_until_timeout(2)
							test_parallel(i915, e->exec_id | e->flags);
					}
				}

				igt_fixture {
					igt_stop_hang_detector();
				}
			}

			igt_subtest_group {
				igt_hang_t hang;

				igt_skip_on_simulation();

				igt_fixture {
					hang = igt_allow_hang(i915, 0, 0);
				}

				igt_subtest_f("busy-hang-%s", e->name)
					test_fence_busy(i915, e->exec_id | e->flags, HANG);
				igt_subtest_f("wait-hang-%s", e->name)
					test_fence_busy(i915, e->exec_id | e->flags, HANG | WAIT);
				igt_subtest_f("await-hang-%s", e->name)
					test_fence_await(i915, e->exec_id | e->flags, HANG);
				igt_subtest_f("nb-await-hang-%s", e->name)
					test_fence_await(i915, e->exec_id | e->flags, NONBLOCK | HANG);
				igt_fixture {
					igt_disallow_hang(i915, hang);
				}
			}
		}
	}

	igt_subtest_group {
		long ring_size = 0;

		igt_fixture {
			ring_size = gem_measure_ring_inflight(i915, ALL_ENGINES, 0) - 1;
			igt_info("Ring size: %ld batches\n", ring_size);
			igt_require(ring_size);

			gem_require_contexts(i915);
		}

		igt_subtest("long-history")
			test_long_history(i915, ring_size, 0);

		igt_subtest("expired-history")
			test_long_history(i915, ring_size, EXPIRED);
	}

	igt_subtest("flip") {
		gem_quiescent_gpu(i915);
		test_fence_flip(i915);
	}

	igt_subtest_group { /* syncobj */
		igt_fixture {
			igt_require(exec_has_fence_array(i915));
			igt_assert(has_syncobj(i915));
			igt_fork_hang_detector(i915);
		}

		igt_subtest("invalid-fence-array")
			test_invalid_fence_array(i915);

		igt_subtest("syncobj-unused-fence")
			test_syncobj_unused_fence(i915);

		igt_subtest("syncobj-invalid-wait")
			test_syncobj_invalid_wait(i915);

		igt_subtest("syncobj-invalid-flags")
			test_syncobj_invalid_flags(i915);

		igt_subtest("syncobj-signal")
			test_syncobj_signal(i915);

		igt_subtest("syncobj-wait")
			test_syncobj_wait(i915);

		igt_subtest("syncobj-export")
			test_syncobj_export(i915);

		igt_subtest("syncobj-repeat")
			test_syncobj_repeat(i915);

		igt_subtest("syncobj-import")
			test_syncobj_import(i915);

		igt_subtest("syncobj-channel")
			test_syncobj_channel(i915);

		igt_fixture {
			igt_stop_hang_detector();
		}
	}

	igt_fixture {
		close(i915);
	}
}
