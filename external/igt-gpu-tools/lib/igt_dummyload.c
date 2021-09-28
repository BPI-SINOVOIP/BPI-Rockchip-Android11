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
 *
 */

#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <sys/poll.h>

#include <i915_drm.h>

#include "igt_core.h"
#include "drmtest.h"
#include "igt_device.h"
#include "igt_dummyload.h"
#include "igt_gt.h"
#include "intel_chipset.h"
#include "intel_reg.h"
#include "ioctl_wrappers.h"
#include "sw_sync.h"
#include "igt_vgem.h"
#include "i915/gem_engine_topology.h"
#include "i915/gem_mman.h"

/**
 * SECTION:igt_dummyload
 * @short_description: Library for submitting GPU workloads
 * @title: Dummyload
 * @include: igt.h
 *
 * A lot of igt testcases need some GPU workload to make sure a race window is
 * big enough. Unfortunately having a fixed amount of workload leads to
 * spurious test failures or overly long runtimes on some fast/slow platforms.
 * This library contains functionality to submit GPU workloads that should
 * consume exactly a specific amount of time.
 */

#define LOCAL_I915_EXEC_BSD_SHIFT      (13)
#define LOCAL_I915_EXEC_BSD_MASK       (3 << LOCAL_I915_EXEC_BSD_SHIFT)

#define ENGINE_MASK  (I915_EXEC_RING_MASK | LOCAL_I915_EXEC_BSD_MASK)

#define MI_ARB_CHK (0x5 << 23)

static const int BATCH_SIZE = 4096;
static const int LOOP_START_OFFSET = 64;

static IGT_LIST(spin_list);
static pthread_mutex_t list_lock = PTHREAD_MUTEX_INITIALIZER;

static int
emit_recursive_batch(igt_spin_t *spin,
		     int fd, const struct igt_spin_factory *opts)
{
#define SCRATCH 0
#define BATCH IGT_SPIN_BATCH
	const int gen = intel_gen(intel_get_drm_devid(fd));
	struct drm_i915_gem_relocation_entry relocs[2], *r;
	struct drm_i915_gem_execbuffer2 *execbuf;
	struct drm_i915_gem_exec_object2 *obj;
	unsigned int flags[GEM_MAX_ENGINES];
	unsigned int nengine;
	int fence_fd = -1;
	uint32_t *cs, *batch;
	int i;

	nengine = 0;
	if (opts->engine == ALL_ENGINES) {
		struct intel_execution_engine2 *engine;

		for_each_context_engine(fd, opts->ctx, engine) {
			if (opts->flags & IGT_SPIN_POLL_RUN &&
			    !gem_class_can_store_dword(fd, engine->class))
				continue;

			flags[nengine++] = engine->flags;
		}
	} else {
		flags[nengine++] = opts->engine;
	}
	igt_require(nengine);

	memset(&spin->execbuf, 0, sizeof(spin->execbuf));
	execbuf = &spin->execbuf;
	memset(spin->obj, 0, sizeof(spin->obj));
	obj = spin->obj;
	memset(relocs, 0, sizeof(relocs));

	obj[BATCH].handle = gem_create(fd, BATCH_SIZE);
	batch = __gem_mmap__wc(fd, obj[BATCH].handle,
			       0, BATCH_SIZE, PROT_WRITE);
	if (!batch)
		batch = gem_mmap__gtt(fd, obj[BATCH].handle,
				      BATCH_SIZE, PROT_WRITE);

	gem_set_domain(fd, obj[BATCH].handle,
		       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	execbuf->buffer_count++;
	cs = batch;

	if (opts->dependency) {
		igt_assert(!(opts->flags & IGT_SPIN_POLL_RUN));

		r = &relocs[obj[BATCH].relocation_count++];

		/* dummy write to dependency */
		obj[SCRATCH].handle = opts->dependency;
		r->presumed_offset = 0;
		r->target_handle = obj[SCRATCH].handle;
		r->offset = sizeof(uint32_t) * 1020;
		r->delta = 0;
		r->read_domains = I915_GEM_DOMAIN_RENDER;
		r->write_domain = I915_GEM_DOMAIN_RENDER;

		execbuf->buffer_count++;
	} else if (opts->flags & IGT_SPIN_POLL_RUN) {
		r = &relocs[obj[BATCH].relocation_count++];

		igt_assert(!opts->dependency);

		if (gen == 4 || gen == 5) {
			execbuf->flags |= I915_EXEC_SECURE;
			igt_require(__igt_device_set_master(fd) == 0);
		}

		spin->poll_handle = gem_create(fd, 4096);
		obj[SCRATCH].handle = spin->poll_handle;

		if (__gem_set_caching(fd, spin->poll_handle,
				      I915_CACHING_CACHED) == 0)
			spin->poll = gem_mmap__cpu(fd, spin->poll_handle,
						   0, 4096,
						   PROT_READ | PROT_WRITE);
		else
			spin->poll = gem_mmap__wc(fd, spin->poll_handle,
						  0, 4096,
						  PROT_READ | PROT_WRITE);

		igt_assert_eq(spin->poll[SPIN_POLL_START_IDX], 0);

		/* batch is first */
		r->presumed_offset = 4096;
		r->target_handle = obj[SCRATCH].handle;
		r->offset = sizeof(uint32_t) * 1;
		r->delta = sizeof(uint32_t) * SPIN_POLL_START_IDX;

		*cs++ = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);

		if (gen >= 8) {
			*cs++ = r->presumed_offset + r->delta;
			*cs++ = 0;
		} else if (gen >= 4) {
			*cs++ = 0;
			*cs++ = r->presumed_offset + r->delta;
			r->offset += sizeof(uint32_t);
		} else {
			cs[-1]--;
			*cs++ = r->presumed_offset + r->delta;
		}

		*cs++ = 1;

		execbuf->buffer_count++;
	}

	spin->handle = obj[BATCH].handle;

	igt_assert_lt(cs - batch, LOOP_START_OFFSET / sizeof(*cs));
	spin->condition = batch + LOOP_START_OFFSET / sizeof(*cs);
	cs = spin->condition;

	/* Allow ourselves to be preempted */
	if (!(opts->flags & IGT_SPIN_NO_PREEMPTION))
		*cs++ = MI_ARB_CHK;

	/* Pad with a few nops so that we do not completely hog the system.
	 *
	 * Part of the attraction of using a recursive batch is that it is
	 * hard on the system (executing the "function" call is apparently
	 * quite expensive). However, the GPU may hog the entire system for
	 * a few minutes, preventing even NMI. Quite why this is so is unclear,
	 * but presumably it relates to the PM_INTRMSK workaround on gen6/gen7.
	 * If we give the system a break by having the GPU execute a few nops
	 * between function calls, that appears enough to keep SNB out of
	 * trouble. See https://bugs.freedesktop.org/show_bug.cgi?id=102262
	 */
	if (!(opts->flags & IGT_SPIN_FAST))
		cs += 1000;

	/* recurse */
	r = &relocs[obj[BATCH].relocation_count++];
	r->target_handle = obj[BATCH].handle;
	r->offset = (cs + 1 - batch) * sizeof(*cs);
	r->read_domains = I915_GEM_DOMAIN_COMMAND;
	r->delta = LOOP_START_OFFSET;
	if (gen >= 8) {
		*cs++ = MI_BATCH_BUFFER_START | 1 << 8 | 1;
		*cs++ = r->delta;
		*cs++ = 0;
	} else if (gen >= 6) {
		*cs++ = MI_BATCH_BUFFER_START | 1 << 8;
		*cs++ = r->delta;
	} else {
		*cs++ = MI_BATCH_BUFFER_START | 2 << 6;
		if (gen < 4)
			r->delta |= 1;
		*cs = r->delta;
		cs++;
	}
	obj[BATCH].relocs_ptr = to_user_pointer(relocs);

	execbuf->buffers_ptr = to_user_pointer(obj +
					       (2 - execbuf->buffer_count));
	execbuf->rsvd1 = opts->ctx;

	if (opts->flags & IGT_SPIN_FENCE_OUT)
		execbuf->flags |= I915_EXEC_FENCE_OUT;

	for (i = 0; i < nengine; i++) {
		execbuf->flags &= ~ENGINE_MASK;
		execbuf->flags |= flags[i];

		gem_execbuf_wr(fd, execbuf);

		if (opts->flags & IGT_SPIN_FENCE_OUT) {
			int _fd = execbuf->rsvd2 >> 32;

			igt_assert(_fd >= 0);
			if (fence_fd == -1) {
				fence_fd = _fd;
			} else {
				int old_fd = fence_fd;

				fence_fd = sync_fence_merge(old_fd, _fd);
				close(old_fd);
				close(_fd);
			}
			igt_assert(fence_fd >= 0);
		}
	}

	igt_assert_lt(cs - batch, BATCH_SIZE / sizeof(*cs));

	/* Make it easier for callers to resubmit. */
	for (i = 0; i < ARRAY_SIZE(spin->obj); i++) {
		spin->obj[i].relocation_count = 0;
		spin->obj[i].relocs_ptr = 0;
		spin->obj[i].flags = EXEC_OBJECT_PINNED;
	}

	spin->cmd_precondition = *spin->condition;

	return fence_fd;
}

static igt_spin_t *
spin_create(int fd, const struct igt_spin_factory *opts)
{
	igt_spin_t *spin;

	spin = calloc(1, sizeof(struct igt_spin));
	igt_assert(spin);

	spin->out_fence = emit_recursive_batch(spin, fd, opts);

	pthread_mutex_lock(&list_lock);
	igt_list_add(&spin->link, &spin_list);
	pthread_mutex_unlock(&list_lock);

	return spin;
}

igt_spin_t *
__igt_spin_factory(int fd, const struct igt_spin_factory *opts)
{
	return spin_create(fd, opts);
}

/**
 * igt_spin_factory:
 * @fd: open i915 drm file descriptor
 * @opts: controlling options such as context, engine, dependencies etc
 *
 * Start a recursive batch on a ring. Immediately returns a #igt_spin_t that
 * contains the batch's handle that can be waited upon. The returned structure
 * must be passed to igt_spin_free() for post-processing.
 *
 * Returns:
 * Structure with helper internal state for igt_spin_free().
 */
igt_spin_t *
igt_spin_factory(int fd, const struct igt_spin_factory *opts)
{
	igt_spin_t *spin;

	igt_require_gem(fd);

	if (opts->engine != ALL_ENGINES) {
		struct intel_execution_engine2 e;
		int class;

		if (!gem_context_lookup_engine(fd, opts->engine,
					       opts->ctx, &e)) {
			class = e.class;
		} else {
			gem_require_ring(fd, opts->engine);
			class = gem_execbuf_flags_to_engine_class(opts->engine);
		}

		if (opts->flags & IGT_SPIN_POLL_RUN)
			igt_require(gem_class_can_store_dword(fd, class));
	}

	spin = spin_create(fd, opts);

	igt_assert(gem_bo_busy(fd, spin->handle));
	if (opts->flags & IGT_SPIN_FENCE_OUT) {
		struct pollfd pfd = { spin->out_fence, POLLIN };

		igt_assert(poll(&pfd, 1, 0) == 0);
	}

	return spin;
}

static void notify(union sigval arg)
{
	igt_spin_t *spin = arg.sival_ptr;

	igt_spin_end(spin);
}

/**
 * igt_spin_set_timeout:
 * @spin: spin state from igt_spin_new()
 * @ns: amount of time in nanoseconds the batch continues to execute
 *      before finishing.
 *
 * Specify a timeout. This ends the recursive batch associated with @spin after
 * the timeout has elapsed.
 */
void igt_spin_set_timeout(igt_spin_t *spin, int64_t ns)
{
	timer_t timer;
	struct sigevent sev;
	struct itimerspec its;

	igt_assert(ns > 0);
	if (!spin)
		return;

	igt_assert(!spin->timer);

	memset(&sev, 0, sizeof(sev));
	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_value.sival_ptr = spin;
	sev.sigev_notify_function = notify;
	igt_assert(timer_create(CLOCK_MONOTONIC, &sev, &timer) == 0);
	igt_assert(timer);

	memset(&its, 0, sizeof(its));
	its.it_value.tv_sec = ns / NSEC_PER_SEC;
	its.it_value.tv_nsec = ns % NSEC_PER_SEC;
	igt_assert(timer_settime(timer, 0, &its, NULL) == 0);

	spin->timer = timer;
}

/**
 * igt_spin_reset:
 * @spin: spin state from igt_spin_new()
 *
 * Reset the state of spin, allowing its reuse.
 */
void igt_spin_reset(igt_spin_t *spin)
{
	if (igt_spin_has_poll(spin))
		spin->poll[SPIN_POLL_START_IDX] = 0;

	*spin->condition = spin->cmd_precondition;
	__sync_synchronize();
}

/**
 * igt_spin_end:
 * @spin: spin state from igt_spin_new()
 *
 * End the spinner associated with @spin manually.
 */
void igt_spin_end(igt_spin_t *spin)
{
	if (!spin)
		return;

	*spin->condition = MI_BATCH_BUFFER_END;
	__sync_synchronize();
}

/**
 * igt_spin_free:
 * @fd: open i915 drm file descriptor
 * @spin: spin state from igt_spin_new()
 *
 * This function does the necessary post-processing after starting a
 * spin with igt_spin_new() and then frees it.
 */
void igt_spin_free(int fd, igt_spin_t *spin)
{
	if (!spin)
		return;

	pthread_mutex_lock(&list_lock);
	igt_list_del(&spin->link);
	pthread_mutex_unlock(&list_lock);

	if (spin->timer)
		timer_delete(spin->timer);

	igt_spin_end(spin);
	gem_munmap((void *)((unsigned long)spin->condition & (~4095UL)),
		   BATCH_SIZE);

	if (spin->poll) {
		gem_munmap(spin->poll, 4096);
		gem_close(fd, spin->poll_handle);
	}

	gem_close(fd, spin->handle);

	if (spin->out_fence >= 0)
		close(spin->out_fence);

	free(spin);
}

void igt_terminate_spins(void)
{
	struct igt_spin *iter;

	pthread_mutex_lock(&list_lock);
	igt_list_for_each(iter, &spin_list, link)
		igt_spin_end(iter);
	pthread_mutex_unlock(&list_lock);
}

void igt_unshare_spins(void)
{
	struct igt_spin *it, *n;

	/* Disable the automatic termination on inherited spinners */
	igt_list_for_each_safe(it, n, &spin_list, link)
		igt_list_init(&it->link);
	igt_list_init(&spin_list);
}

static uint32_t plug_vgem_handle(struct igt_cork *cork, int fd)
{
	struct vgem_bo bo;
	int dmabuf;
	uint32_t handle;

	cork->vgem.device = drm_open_driver(DRIVER_VGEM);
	igt_require(vgem_has_fences(cork->vgem.device));

	bo.width = bo.height = 1;
	bo.bpp = 4;
	vgem_create(cork->vgem.device, &bo);
	cork->vgem.fence = vgem_fence_attach(cork->vgem.device, &bo, VGEM_FENCE_WRITE);

	dmabuf = prime_handle_to_fd(cork->vgem.device, bo.handle);
	handle = prime_fd_to_handle(fd, dmabuf);
	close(dmabuf);

	return handle;
}

static void unplug_vgem_handle(struct igt_cork *cork)
{
	vgem_fence_signal(cork->vgem.device, cork->vgem.fence);
	close(cork->vgem.device);
}

static uint32_t plug_sync_fd(struct igt_cork *cork)
{
	int fence;

	igt_require_sw_sync();

	cork->sw_sync.timeline = sw_sync_timeline_create();
	fence = sw_sync_timeline_create_fence(cork->sw_sync.timeline, 1);

	return fence;
}

static void unplug_sync_fd(struct igt_cork *cork)
{
	sw_sync_timeline_inc(cork->sw_sync.timeline, 1);
	close(cork->sw_sync.timeline);
}

/**
 * igt_cork_plug:
 * @fd: open drm file descriptor
 * @method: method to utilize for corking.
 * @cork: structure that will be filled with the state of the cork bo.
 * Note: this has to match the corking method.
 *
 * This function provides a mechanism to stall submission. It provides two
 * blocking methods:
 *
 * VGEM_BO.
 * Imports a vgem bo with a fence attached to it. This bo can be used as a
 * dependency during submission to stall execution until the fence is signaled.
 *
 * SW_SYNC:
 * Creates a timeline and then a fence on that timeline. The fence can be used
 * as an input fence to a request, the request will be stalled until the fence
 * is signaled.
 *
 * The parameters required to unblock the execution and to cleanup are stored in
 * the provided cork structure.
 *
 * Returns:
 * Handle of the imported BO / Sw sync fence FD.
 */
uint32_t igt_cork_plug(struct igt_cork *cork, int fd)
{
	igt_assert(cork->fd == -1);

	switch (cork->type) {
	case CORK_SYNC_FD:
		return plug_sync_fd(cork);

	case CORK_VGEM_HANDLE:
		return plug_vgem_handle(cork, fd);

	default:
		igt_assert_f(0, "Invalid cork type!\n");
		return 0;
	}
}

/**
 * igt_cork_unplug:
 * @method: method to utilize for corking.
 * @cork: cork state from igt_cork_plug()
 *
 * This function unblocks the execution by signaling the fence attached to the
 * imported bo and does the necessary post-processing.
 *
 * NOTE: the handle returned by igt_cork_plug is not closed during this phase.
 */
void igt_cork_unplug(struct igt_cork *cork)
{
	igt_assert(cork->fd != -1);

	switch (cork->type) {
	case CORK_SYNC_FD:
		unplug_sync_fd(cork);
		break;

	case CORK_VGEM_HANDLE:
		unplug_vgem_handle(cork);
		break;

	default:
		igt_assert_f(0, "Invalid cork type!\n");
	}

	cork->fd = -1; /* Reset cork */
}
