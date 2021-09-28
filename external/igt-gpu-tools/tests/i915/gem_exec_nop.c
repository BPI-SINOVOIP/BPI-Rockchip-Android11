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
 *    Chris Wilson <chris@chris-wilson.co.uk>
 *
 */

#include "igt.h"
#include "igt_device.h"
#include "igt_rand.h"
#include "igt_sysfs.h"

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
#include <sys/poll.h>
#include <sys/time.h>
#include <time.h>
#include "drm.h"

#define LOCAL_I915_EXEC_NO_RELOC (1<<11)
#define LOCAL_I915_EXEC_HANDLE_LUT (1<<12)

#define LOCAL_I915_EXEC_BSD_SHIFT      (13)
#define LOCAL_I915_EXEC_BSD_MASK       (3 << LOCAL_I915_EXEC_BSD_SHIFT)

#define ENGINE_FLAGS  (I915_EXEC_RING_MASK | LOCAL_I915_EXEC_BSD_MASK)

#define MAX_PRIO LOCAL_I915_CONTEXT_MAX_USER_PRIORITY
#define MIN_PRIO LOCAL_I915_CONTEXT_MIN_USER_PRIORITY

#define FORKED 1
#define CHAINED 2
#define CONTEXT 4

static double elapsed(const struct timespec *start, const struct timespec *end)
{
	return ((end->tv_sec - start->tv_sec) +
		(end->tv_nsec - start->tv_nsec)*1e-9);
}

static double nop_on_ring(int fd, uint32_t handle, unsigned ring_id,
			  int timeout, unsigned long *out)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj;
	struct timespec start, now;
	unsigned long count;

	memset(&obj, 0, sizeof(obj));
	obj.handle = handle;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;
	execbuf.flags = ring_id;
	execbuf.flags |= LOCAL_I915_EXEC_HANDLE_LUT;
	execbuf.flags |= LOCAL_I915_EXEC_NO_RELOC;
	if (__gem_execbuf(fd, &execbuf)) {
		execbuf.flags = ring_id;
		gem_execbuf(fd, &execbuf);
	}
	intel_detect_and_clear_missed_interrupts(fd);

	count = 0;
	clock_gettime(CLOCK_MONOTONIC, &start);
	do {
		for (int loop = 0; loop < 1024; loop++)
			gem_execbuf(fd, &execbuf);

		count += 1024;
		clock_gettime(CLOCK_MONOTONIC, &now);
	} while (elapsed(&start, &now) < timeout);
	igt_assert_eq(intel_detect_and_clear_missed_interrupts(fd), 0);

	*out = count;
	return elapsed(&start, &now);
}

static void poll_ring(int fd, unsigned engine, const char *name, int timeout)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	const uint32_t MI_ARB_CHK = 0x5 << 23;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj;
	struct drm_i915_gem_relocation_entry reloc[4], *r;
	uint32_t *bbe[2], *state, *batch;
	struct timespec tv = {};
	unsigned long cycles;
	unsigned flags;
	uint64_t elapsed;

	flags = I915_EXEC_NO_RELOC;
	if (gen == 4 || gen == 5)
		flags |= I915_EXEC_SECURE;

	gem_require_ring(fd, engine);
	igt_require(gem_can_store_dword(fd, engine));

	memset(&obj, 0, sizeof(obj));
	obj.handle = gem_create(fd, 4096);
	obj.relocs_ptr = to_user_pointer(reloc);
	obj.relocation_count = ARRAY_SIZE(reloc);

	r = memset(reloc, 0, sizeof(reloc));
	batch = gem_mmap__wc(fd, obj.handle, 0, 4096, PROT_WRITE);

	for (unsigned int start_offset = 0;
	     start_offset <= 128;
	     start_offset += 128) {
		uint32_t *b = batch + start_offset / sizeof(*batch);

		r->target_handle = obj.handle;
		r->offset = (b - batch + 1) * sizeof(uint32_t);
		r->delta = 4092;
		r->read_domains = I915_GEM_DOMAIN_RENDER;

		*b = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
		if (gen >= 8) {
			*++b = r->delta;
			*++b = 0;
		} else if (gen >= 4) {
			r->offset += sizeof(uint32_t);
			*++b = 0;
			*++b = r->delta;
		} else {
			*b -= 1;
			*++b = r->delta;
		}
		*++b = start_offset != 0;
		r++;

		b = batch + (start_offset + 64) / sizeof(*batch);
		bbe[start_offset != 0] = b;
		*b++ = MI_ARB_CHK;

		r->target_handle = obj.handle;
		r->offset = (b - batch + 1) * sizeof(uint32_t);
		r->read_domains = I915_GEM_DOMAIN_COMMAND;
		r->delta = start_offset + 64;
		if (gen >= 8) {
			*b++ = MI_BATCH_BUFFER_START | 1 << 8 | 1;
			*b++ = r->delta;
			*b++ = 0;
		} else if (gen >= 6) {
			*b++ = MI_BATCH_BUFFER_START | 1 << 8;
			*b++ = r->delta;
		} else {
			*b++ = MI_BATCH_BUFFER_START | 2 << 6;
			if (gen < 4)
				r->delta |= 1;
			*b++ = r->delta;
		}
		r++;
	}
	igt_assert(r == reloc + ARRAY_SIZE(reloc));
	state = batch + 1023;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;
	execbuf.flags = engine | flags;

	cycles = 0;
	do {
		unsigned int idx = ++cycles & 1;

		*bbe[idx] = MI_ARB_CHK;
		execbuf.batch_start_offset =
			(bbe[idx] - batch) * sizeof(*batch) - 64;

		gem_execbuf(fd, &execbuf);

		*bbe[!idx] = MI_BATCH_BUFFER_END;
		__sync_synchronize();

		while (READ_ONCE(*state) != idx)
			;
	} while ((elapsed = igt_nsec_elapsed(&tv)) >> 30 < timeout);
	*bbe[cycles & 1] = MI_BATCH_BUFFER_END;
	gem_sync(fd, obj.handle);

	igt_info("%s completed %ld cycles: %.3f us\n",
		 name, cycles, elapsed*1e-3/cycles);

	munmap(batch, 4096);
	gem_close(fd, obj.handle);
}

static void poll_sequential(int fd, const char *name, int timeout)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	const uint32_t MI_ARB_CHK = 0x5 << 23;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj[2];
	struct drm_i915_gem_relocation_entry reloc[4], *r;
	uint32_t *bbe[2], *state, *batch;
	unsigned engines[16], nengine, engine, flags;
	struct timespec tv = {};
	unsigned long cycles;
	uint64_t elapsed;
	bool cached;

	flags = I915_EXEC_NO_RELOC;
	if (gen == 4 || gen == 5)
		flags |= I915_EXEC_SECURE;

	nengine = 0;
	for_each_physical_engine(fd, engine) {
		if (!gem_can_store_dword(fd, engine))
			continue;

		engines[nengine++] = engine;
	}
	igt_require(nengine);

	memset(obj, 0, sizeof(obj));
	obj[0].handle = gem_create(fd, 4096);
	obj[0].flags = EXEC_OBJECT_WRITE;
	cached = __gem_set_caching(fd, obj[0].handle, 1) == 0;
	obj[1].handle = gem_create(fd, 4096);
	obj[1].relocs_ptr = to_user_pointer(reloc);
	obj[1].relocation_count = ARRAY_SIZE(reloc);

	r = memset(reloc, 0, sizeof(reloc));
	batch = gem_mmap__wc(fd, obj[1].handle, 0, 4096, PROT_WRITE);

	for (unsigned int start_offset = 0;
	     start_offset <= 128;
	     start_offset += 128) {
		uint32_t *b = batch + start_offset / sizeof(*batch);

		r->target_handle = obj[0].handle;
		r->offset = (b - batch + 1) * sizeof(uint32_t);
		r->delta = 0;
		r->read_domains = I915_GEM_DOMAIN_RENDER;
		r->write_domain = I915_GEM_DOMAIN_RENDER;

		*b = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
		if (gen >= 8) {
			*++b = r->delta;
			*++b = 0;
		} else if (gen >= 4) {
			r->offset += sizeof(uint32_t);
			*++b = 0;
			*++b = r->delta;
		} else {
			*b -= 1;
			*++b = r->delta;
		}
		*++b = start_offset != 0;
		r++;

		b = batch + (start_offset + 64) / sizeof(*batch);
		bbe[start_offset != 0] = b;
		*b++ = MI_ARB_CHK;

		r->target_handle = obj[1].handle;
		r->offset = (b - batch + 1) * sizeof(uint32_t);
		r->read_domains = I915_GEM_DOMAIN_COMMAND;
		r->delta = start_offset + 64;
		if (gen >= 8) {
			*b++ = MI_BATCH_BUFFER_START | 1 << 8 | 1;
			*b++ = r->delta;
			*b++ = 0;
		} else if (gen >= 6) {
			*b++ = MI_BATCH_BUFFER_START | 1 << 8;
			*b++ = r->delta;
		} else {
			*b++ = MI_BATCH_BUFFER_START | 2 << 6;
			if (gen < 4)
				r->delta |= 1;
			*b++ = r->delta;
		}
		r++;
	}
	igt_assert(r == reloc + ARRAY_SIZE(reloc));

	if (cached)
		state = gem_mmap__cpu(fd, obj[0].handle, 0, 4096, PROT_READ);
	else
		state = gem_mmap__wc(fd, obj[0].handle, 0, 4096, PROT_READ);

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = ARRAY_SIZE(obj);

	cycles = 0;
	do {
		unsigned int idx = ++cycles & 1;

		*bbe[idx] = MI_ARB_CHK;
		execbuf.batch_start_offset =
			(bbe[idx] - batch) * sizeof(*batch) - 64;

		execbuf.flags = engines[cycles % nengine] | flags;
		gem_execbuf(fd, &execbuf);

		*bbe[!idx] = MI_BATCH_BUFFER_END;
		__sync_synchronize();

		while (READ_ONCE(*state) != idx)
			;
	} while ((elapsed = igt_nsec_elapsed(&tv)) >> 30 < timeout);
	*bbe[cycles & 1] = MI_BATCH_BUFFER_END;
	gem_sync(fd, obj[1].handle);

	igt_info("%s completed %ld cycles: %.3f us\n",
		 name, cycles, elapsed*1e-3/cycles);

	munmap(state, 4096);
	munmap(batch, 4096);
	gem_close(fd, obj[1].handle);
	gem_close(fd, obj[0].handle);
}

static void single(int fd, uint32_t handle,
		   unsigned ring_id, const char *ring_name)
{
	double time;
	unsigned long count;

	gem_require_ring(fd, ring_id);

	time = nop_on_ring(fd, handle, ring_id, 20, &count);
	igt_info("%s: %'lu cycles: %.3fus\n",
		 ring_name, count, time*1e6 / count);
}

static double
stable_nop_on_ring(int fd, uint32_t handle, unsigned int engine,
		   int timeout, int reps)
{
	igt_stats_t s;
	double n;

	igt_assert(reps >= 5);

	igt_stats_init_with_size(&s, reps);
	s.is_float = true;

	while (reps--) {
		unsigned long count;
		double time;

		time = nop_on_ring(fd, handle, engine, timeout, &count);
		igt_stats_push_float(&s, time / count);
	}

	n = igt_stats_get_median(&s);
	igt_stats_fini(&s);

	return n;
}

#define assert_within_epsilon(x, ref, tolerance) \
        igt_assert_f((x) <= (1.0 + tolerance) * ref && \
                     (x) >= (1.0 - tolerance) * ref, \
                     "'%s' != '%s' (%f not within %f%% tolerance of %f)\n",\
                     #x, #ref, x, tolerance * 100.0, ref)

static void headless(int fd, uint32_t handle)
{
	unsigned int nr_connected = 0;
	drmModeConnector *connector;
	drmModeRes *res;
	double n_display, n_headless;

	res = drmModeGetResources(fd);
	igt_require(res);

	/* require at least one connected connector for the test */
	for (int i = 0; i < res->count_connectors; i++) {
		connector = drmModeGetConnectorCurrent(fd, res->connectors[i]);
		if (connector->connection == DRM_MODE_CONNECTED)
			nr_connected++;
		drmModeFreeConnector(connector);
	}
	igt_require(nr_connected > 0);

	/* set graphics mode to prevent blanking */
	kmstest_set_vt_graphics_mode();

	/* benchmark nops */
	n_display = stable_nop_on_ring(fd, handle, I915_EXEC_DEFAULT, 1, 5);
	igt_info("With one display connected: %.2fus\n",
		 n_display * 1e6);

	/* force all connectors off */
	kmstest_unset_all_crtcs(fd, res);

	/* benchmark nops again */
	n_headless = stable_nop_on_ring(fd, handle, I915_EXEC_DEFAULT, 1, 5);
	igt_info("Without a display connected (headless): %.2fus\n",
		 n_headless * 1e6);

	/* check that the two execution speeds are roughly the same */
	assert_within_epsilon(n_headless, n_display, 0.1f);
}

static void parallel(int fd, uint32_t handle, int timeout)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj;
	unsigned engines[16];
	const char *names[16];
	unsigned nengine;
	unsigned engine;
	unsigned long count;
	double time, sum;

	sum = 0;
	nengine = 0;
	for_each_physical_engine(fd, engine) {
		engines[nengine] = engine;
		names[nengine] = e__->name;
		nengine++;

		time = nop_on_ring(fd, handle, engine, 1, &count) / count;
		sum += time;
		igt_debug("%s: %.3fus\n", e__->name, 1e6*time);
	}
	igt_require(nengine);
	igt_info("average (individually): %.3fus\n", sum/nengine*1e6);

	memset(&obj, 0, sizeof(obj));
	obj.handle = handle;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;
	execbuf.flags |= LOCAL_I915_EXEC_HANDLE_LUT;
	execbuf.flags |= LOCAL_I915_EXEC_NO_RELOC;
	if (__gem_execbuf(fd, &execbuf)) {
		execbuf.flags = 0;
		gem_execbuf(fd, &execbuf);
	}
	intel_detect_and_clear_missed_interrupts(fd);

	igt_fork(child, nengine) {
		struct timespec start, now;

		execbuf.flags &= ~ENGINE_FLAGS;
		execbuf.flags |= engines[child];

		count = 0;
		clock_gettime(CLOCK_MONOTONIC, &start);
		do {
			for (int loop = 0; loop < 1024; loop++)
				gem_execbuf(fd, &execbuf);
			count += 1024;
			clock_gettime(CLOCK_MONOTONIC, &now);
		} while (elapsed(&start, &now) < timeout);
		time = elapsed(&start, &now) / count;
		igt_info("%s: %ld cycles, %.3fus\n", names[child], count, 1e6*time);
	}

	igt_waitchildren();
	igt_assert_eq(intel_detect_and_clear_missed_interrupts(fd), 0);

}

static void series(int fd, uint32_t handle, int timeout)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj;
	struct timespec start, now, sync;
	unsigned engines[16];
	unsigned nengine;
	unsigned engine;
	unsigned long count;
	double time, max = 0, min = HUGE_VAL, sum = 0;
	const char *name;

	nengine = 0;
	for_each_physical_engine(fd, engine) {
		time = nop_on_ring(fd, handle, engine, 1, &count) / count;
		if (time > max) {
			name = e__->name;
			max = time;
		}
		if (time < min)
			min = time;
		sum += time;
		engines[nengine++] = engine;
	}
	igt_require(nengine);
	igt_info("Maximum execution latency on %s, %.3fus, min %.3fus, total %.3fus per cycle, average %.3fus\n",
		 name, max*1e6, min*1e6, sum*1e6, sum/nengine*1e6);

	memset(&obj, 0, sizeof(obj));
	obj.handle = handle;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;
	execbuf.flags |= LOCAL_I915_EXEC_HANDLE_LUT;
	execbuf.flags |= LOCAL_I915_EXEC_NO_RELOC;
	if (__gem_execbuf(fd, &execbuf)) {
		execbuf.flags = 0;
		gem_execbuf(fd, &execbuf);
	}
	intel_detect_and_clear_missed_interrupts(fd);

	count = 0;
	clock_gettime(CLOCK_MONOTONIC, &start);
	do {
		for (int loop = 0; loop < 1024; loop++) {
			for (int n = 0; n < nengine; n++) {
				execbuf.flags &= ~ENGINE_FLAGS;
				execbuf.flags |= engines[n];
				gem_execbuf(fd, &execbuf);
			}
		}
		count += nengine * 1024;
		clock_gettime(CLOCK_MONOTONIC, &now);
	} while (elapsed(&start, &now) < timeout); /* Hang detection ~120s */
	gem_sync(fd, handle);
	clock_gettime(CLOCK_MONOTONIC, &sync);
	igt_debug("sync time: %.3fus\n", elapsed(&now, &sync)*1e6);
	igt_assert_eq(intel_detect_and_clear_missed_interrupts(fd), 0);

	time = elapsed(&start, &now) / count;
	igt_info("All (%d engines): %'lu cycles, average %.3fus per cycle [expected %.3fus]\n",
		 nengine, count, 1e6*time, 1e6*((max-min)/nengine+min));

	/* The rate limiting step should be how fast the slowest engine can
	 * execute its queue of requests, as when we wait upon a full ring all
	 * dispatch is frozen. So in general we cannot go faster than the
	 * slowest engine (but as all engines are in lockstep, they should all
	 * be executing in parallel and so the average should be max/nengines),
	 * but we should equally not go any slower.
	 *
	 * However, that depends upon being able to submit fast enough, and
	 * that in turns depends upon debugging turned off and no bottlenecks
	 * within the driver. We cannot assert that we hit ideal conditions
	 * across all engines, so we only look for an outrageous error
	 * condition.
	 */
	igt_assert_f(time < 2*sum,
		     "Average time (%.3fus) exceeds expectation for parallel execution (min %.3fus, max %.3fus; limit set at %.3fus)\n",
		     1e6*time, 1e6*min, 1e6*max, 1e6*sum*2);
}

static void xchg(void *array, unsigned i, unsigned j)
{
	unsigned *u = array;
	unsigned tmp = u[i];
	u[i] = u[j];
	u[j] = tmp;
}

static void sequential(int fd, uint32_t handle, unsigned flags, int timeout)
{
	const int ncpus = flags & FORKED ? sysconf(_SC_NPROCESSORS_ONLN) : 1;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj[2];
	unsigned engines[16];
	unsigned nengine;
	double *results;
	double time, sum;
	unsigned n;

	gem_require_contexts(fd);

	results = mmap(NULL, 4096, PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
	igt_assert(results != MAP_FAILED);

	nengine = 0;
	sum = 0;
	for_each_physical_engine(fd, n) {
		unsigned long count;

		time = nop_on_ring(fd, handle, n, 1, &count) / count;
		sum += time;
		igt_debug("%s: %.3fus\n", e__->name, 1e6*time);

		engines[nengine++] = n;
	}
	igt_require(nengine);
	igt_info("Total (individual) execution latency %.3fus per cycle\n",
		 1e6*sum);

	memset(obj, 0, sizeof(obj));
	obj[0].handle = gem_create(fd, 4096);
	obj[0].flags = EXEC_OBJECT_WRITE;
	obj[1].handle = handle;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = 2;
	execbuf.flags |= LOCAL_I915_EXEC_HANDLE_LUT;
	execbuf.flags |= LOCAL_I915_EXEC_NO_RELOC;
	igt_require(__gem_execbuf(fd, &execbuf) == 0);

	if (flags & CONTEXT) {
		uint32_t id;

		igt_require(__gem_context_create(fd, &id) == 0);
		execbuf.rsvd1 = id;
	}

	for (n = 0; n < nengine; n++) {
		execbuf.flags &= ~ENGINE_FLAGS;
		execbuf.flags |= engines[n];
		igt_require(__gem_execbuf(fd, &execbuf) == 0);
	}

	intel_detect_and_clear_missed_interrupts(fd);

	igt_fork(child, ncpus) {
		struct timespec start, now;
		unsigned long count;

		obj[0].handle = gem_create(fd, 4096);
		gem_execbuf(fd, &execbuf);

		if (flags & CONTEXT)
			execbuf.rsvd1 = gem_context_create(fd);

		hars_petruska_f54_1_random_perturb(child);

		count = 0;
		clock_gettime(CLOCK_MONOTONIC, &start);
		do {
			igt_permute_array(engines, nengine, xchg);
			if (flags & CHAINED) {
				for (n = 0; n < nengine; n++) {
					execbuf.flags &= ~ENGINE_FLAGS;
					execbuf.flags |= engines[n];
					for (int loop = 0; loop < 1024; loop++)
						gem_execbuf(fd, &execbuf);
				}
			} else {
				for (int loop = 0; loop < 1024; loop++) {
					for (n = 0; n < nengine; n++) {
						execbuf.flags &= ~ENGINE_FLAGS;
						execbuf.flags |= engines[n];
						gem_execbuf(fd, &execbuf);
					}
				}
			}
			count += 1024;
			clock_gettime(CLOCK_MONOTONIC, &now);
		} while (elapsed(&start, &now) < timeout); /* Hang detection ~120s */

		gem_sync(fd, obj[0].handle);
		clock_gettime(CLOCK_MONOTONIC, &now);
		results[child] = elapsed(&start, &now) / count;

		if (flags & CONTEXT)
			gem_context_destroy(fd, execbuf.rsvd1);

		gem_close(fd, obj[0].handle);
	}
	igt_waitchildren();
	igt_assert_eq(intel_detect_and_clear_missed_interrupts(fd), 0);

	results[ncpus] = 0;
	for (n = 0; n < ncpus; n++)
		results[ncpus] += results[n];
	results[ncpus] /= ncpus;

	igt_info("Sequential (%d engines, %d processes): average %.3fus per cycle [expected %.3fus]\n",
		 nengine, ncpus, 1e6*results[ncpus], 1e6*sum*ncpus);

	if (flags & CONTEXT)
		gem_context_destroy(fd, execbuf.rsvd1);

	gem_close(fd, obj[0].handle);
	munmap(results, 4096);
}

#define LOCAL_EXEC_FENCE_OUT (1 << 17)
static bool fence_enable_signaling(int fence)
{
	return poll(&(struct pollfd){fence, POLLIN}, 1, 0) == 0;
}

static bool fence_wait(int fence)
{
	return poll(&(struct pollfd){fence, POLLIN}, 1, -1) == 1;
}

static void fence_signal(int fd, uint32_t handle,
			 unsigned ring_id, const char *ring_name,
			 int timeout)
{
#define NFENCES 512
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj;
	struct timespec start, now;
	unsigned engines[16];
	unsigned nengine;
	int *fences, n;
	unsigned long count, signal;

	igt_require(gem_has_exec_fence(fd));

	nengine = 0;
	if (ring_id == ALL_ENGINES) {
		for_each_physical_engine(fd, n)
			engines[nengine++] = n;
	} else {
		gem_require_ring(fd, ring_id);
		engines[nengine++] = ring_id;
	}
	igt_require(nengine);

	fences = malloc(sizeof(*fences) * NFENCES);
	igt_assert(fences);
	memset(fences, -1, sizeof(*fences) * NFENCES);

	memset(&obj, 0, sizeof(obj));
	obj.handle = handle;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;
	execbuf.flags = LOCAL_EXEC_FENCE_OUT;

	n = 0;
	count = 0;
	signal = 0;

	intel_detect_and_clear_missed_interrupts(fd);
	clock_gettime(CLOCK_MONOTONIC, &start);
	do {
		for (int loop = 0; loop < 1024; loop++) {
			for (int e = 0; e < nengine; e++) {
				if (fences[n] != -1) {
					igt_assert(fence_wait(fences[n]));
					close(fences[n]);
				}

				execbuf.flags &= ~ENGINE_FLAGS;
				execbuf.flags |= engines[e];
				gem_execbuf_wr(fd, &execbuf);

				/* Enable signaling by doing a poll() */
				fences[n] = execbuf.rsvd2 >> 32;
				signal += fence_enable_signaling(fences[n]);

				n = (n + 1) % NFENCES;
			}
		}

		count += 1024 * nengine;
		clock_gettime(CLOCK_MONOTONIC, &now);
	} while (elapsed(&start, &now) < timeout);
	igt_assert_eq(intel_detect_and_clear_missed_interrupts(fd), 0);

	for (n = 0; n < NFENCES; n++)
		if (fences[n] != -1)
			close(fences[n]);
	free(fences);

	igt_info("Signal %s: %'lu cycles (%'lu signals): %.3fus\n",
		 ring_name, count, signal, elapsed(&start, &now) * 1e6 / count);
}

static void preempt(int fd, uint32_t handle,
		   unsigned ring_id, const char *ring_name)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj;
	struct timespec start, now;
	unsigned long count;
	uint32_t ctx[2];

	gem_require_ring(fd, ring_id);

	ctx[0] = gem_context_create(fd);
	gem_context_set_priority(fd, ctx[0], MIN_PRIO);

	ctx[1] = gem_context_create(fd);
	gem_context_set_priority(fd, ctx[1], MAX_PRIO);

	memset(&obj, 0, sizeof(obj));
	obj.handle = handle;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;
	execbuf.flags = ring_id;
	execbuf.flags |= LOCAL_I915_EXEC_HANDLE_LUT;
	execbuf.flags |= LOCAL_I915_EXEC_NO_RELOC;
	if (__gem_execbuf(fd, &execbuf)) {
		execbuf.flags = ring_id;
		gem_execbuf(fd, &execbuf);
	}
	execbuf.rsvd1 = ctx[1];
	intel_detect_and_clear_missed_interrupts(fd);

	count = 0;
	clock_gettime(CLOCK_MONOTONIC, &start);
	do {
		igt_spin_t *spin =
			__igt_spin_new(fd,
				       .ctx = ctx[0],
				       .engine = ring_id);

		for (int loop = 0; loop < 1024; loop++)
			gem_execbuf(fd, &execbuf);

		igt_spin_free(fd, spin);

		count += 1024;
		clock_gettime(CLOCK_MONOTONIC, &now);
	} while (elapsed(&start, &now) < 20);
	igt_assert_eq(intel_detect_and_clear_missed_interrupts(fd), 0);

	gem_context_destroy(fd, ctx[1]);
	gem_context_destroy(fd, ctx[0]);

	igt_info("%s: %'lu cycles: %.3fus\n",
		 ring_name, count, elapsed(&start, &now)*1e6 / count);
}

igt_main
{
	const struct intel_execution_engine *e;
	uint32_t handle = 0;
	int device = -1;

	igt_fixture {
		const uint32_t bbe = MI_BATCH_BUFFER_END;

		device = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(device);
		gem_submission_print_method(device);
		gem_scheduler_print_capability(device);

		handle = gem_create(device, 4096);
		gem_write(device, handle, 0, &bbe, sizeof(bbe));

		igt_fork_hang_detector(device);
	}

	igt_subtest("basic-series")
		series(device, handle, 5);

	igt_subtest("basic-parallel")
		parallel(device, handle, 5);

	igt_subtest("basic-sequential")
		sequential(device, handle, 0, 5);

	for (e = intel_execution_engines; e->name; e++) {
		igt_subtest_f("%s", e->name)
			single(device, handle, e->exec_id | e->flags, e->name);
		igt_subtest_f("signal-%s", e->name)
			fence_signal(device, handle, e->exec_id | e->flags, e->name, 5);
	}

	igt_subtest("signal-all")
		fence_signal(device, handle, ALL_ENGINES, "all", 150);

	igt_subtest("series")
		series(device, handle, 150);

	igt_subtest("parallel")
		parallel(device, handle, 150);

	igt_subtest("sequential")
		sequential(device, handle, 0, 150);

	igt_subtest("forked-sequential")
		sequential(device, handle, FORKED, 150);

	igt_subtest("chained-sequential")
		sequential(device, handle, FORKED | CHAINED, 150);

	igt_subtest("context-sequential")
		sequential(device, handle, FORKED | CONTEXT, 150);

	igt_subtest_group {
		igt_fixture {
			gem_require_contexts(device);
			igt_require(gem_scheduler_has_ctx_priority(device));
			igt_require(gem_scheduler_has_preemption(device));
		}

		for (e = intel_execution_engines; e->name; e++) {
			igt_subtest_f("preempt-%s", e->name)
				preempt(device, handle, e->exec_id | e->flags, e->name);
		}
	}

	igt_subtest_group {
		igt_fixture {
			igt_device_set_master(device);
		}

		for (e = intel_execution_engines; e->name; e++) {
			/* Requires master for STORE_DWORD on gen4/5 */
			igt_subtest_f("poll-%s", e->name)
				poll_ring(device,
					  e->exec_id | e->flags, e->name, 20);
		}

		igt_subtest("poll-sequential")
			poll_sequential(device, "Sequential", 20);

		igt_subtest("headless") {
			/* Requires master for changing display modes */
			headless(device, handle);
		}
	}

	igt_fixture {
		igt_stop_hang_detector();
		gem_close(device, handle);
		close(device);
	}
}
