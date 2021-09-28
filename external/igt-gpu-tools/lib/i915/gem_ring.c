/*
 * Copyright © 2014 Intel Corporation
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

#include "gem_ring.h"

#include <signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "intel_reg.h"
#include "drmtest.h"
#include "ioctl_wrappers.h"
#include "igt_dummyload.h"
#include "igt_gt.h"

static int __execbuf(int fd, struct drm_i915_gem_execbuffer2 *execbuf)
{
	int err;

	err = 0;
	if (ioctl(fd, DRM_IOCTL_I915_GEM_EXECBUFFER2, execbuf))
		err = -errno;

	errno = 0;
	return err;
}

static void alarm_handler(int sig)
{
}

static unsigned int
__gem_measure_ring_inflight(int fd, unsigned int engine, enum measure_ring_flags flags)
{
	struct sigaction old_sa, sa = { .sa_handler = alarm_handler };
	struct drm_i915_gem_exec_object2 obj[2];
	struct drm_i915_gem_execbuffer2 execbuf;
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	unsigned int last[2]= { -1, -1 }, count;
	struct itimerval itv;
	IGT_CORK_HANDLE(cork);

	memset(obj, 0, sizeof(obj));
	obj[1].handle = gem_create(fd, 4096);
	gem_write(fd, obj[1].handle, 0, &bbe, sizeof(bbe));

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj[1]);
	execbuf.buffer_count = 1;
	execbuf.flags = engine;
	gem_execbuf(fd, &execbuf);
	gem_sync(fd, obj[1].handle);

	obj[0].handle = igt_cork_plug(&cork, fd);

	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = 2;

	if (flags & MEASURE_RING_NEW_CTX)
		execbuf.rsvd1 = gem_context_create(fd);

	sigaction(SIGALRM, &sa, &old_sa);
	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 1000;
	itv.it_value.tv_sec = 0;
	itv.it_value.tv_usec = 10000;
	setitimer(ITIMER_REAL, &itv, NULL);

	count = 0;
	do {
		if (__execbuf(fd, &execbuf) == 0) {
			count++;
			continue;
		}

		if (last[1] == count)
			break;

		/* sleep until the next timer interrupt (woken on signal) */
		pause();
		last[1] = last[0];
		last[0] = count;
	} while (1);

	igt_assert_eq(__execbuf(fd, &execbuf), -EINTR);
	igt_assert(count > 1);

	memset(&itv, 0, sizeof(itv));
	setitimer(ITIMER_REAL, &itv, NULL);
	sigaction(SIGALRM, &old_sa, NULL);

	igt_cork_unplug(&cork);
	gem_close(fd, obj[0].handle);
	gem_close(fd, obj[1].handle);

	if (flags & MEASURE_RING_NEW_CTX)
		gem_context_destroy(fd, execbuf.rsvd1);

	gem_quiescent_gpu(fd);

	/* Be conservative in case we must wrap later */
	return count - 1;
}

/**
 * gem_measure_ring_inflight:
 * @fd: open i915 drm file descriptor
 * @engine: execbuf engine flag. Use macro ALL_ENGINES to get the minimum
 *			size across all physical engines.
 * @flags: flags to affect measurement:
 *		- MEASURE_RING_NEW_CTX: use a new context to account for the space
 *		  used by the lrc init.
 *
 * This function calculates the maximum number of batches that can be inserted
 * at the same time in the ring on the selected engine.
 *
 * Returns:
 * Number of batches that fit in the ring
 */
unsigned int
gem_measure_ring_inflight(int fd, unsigned int engine, enum measure_ring_flags flags)
{
	if (engine == ALL_ENGINES) {
		unsigned int global_min = ~0u;

		for_each_physical_engine(fd, engine) {
			unsigned int engine_min = __gem_measure_ring_inflight(fd, engine, flags);

			if (engine_min < global_min)
				global_min = engine_min;
		}

		return global_min;
	}

	return __gem_measure_ring_inflight(fd, engine, flags);
}
