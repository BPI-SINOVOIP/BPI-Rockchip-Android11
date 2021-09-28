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
#include <time.h>

#include "drm.h"
#include "ioctl_wrappers.h"
#include "drmtest.h"
#include "intel_io.h"
#include "intel_reg.h"
#include "igt_stats.h"

#define LOCAL_I915_EXEC_NO_RELOC (1<<11)
#define LOCAL_I915_EXEC_HANDLE_LUT (1<<12)

#define LOCAL_I915_EXEC_BSD_SHIFT      (13)
#define LOCAL_I915_EXEC_BSD_MASK       (3 << LOCAL_I915_EXEC_BSD_SHIFT)

#define ENGINE_FLAGS  (I915_EXEC_RING_MASK | LOCAL_I915_EXEC_BSD_MASK)

#define SYNC 0x1
#define WRITE 0x2
#define READ_ALL 0x4

static double elapsed(const struct timespec *start,
		      const struct timespec *end)
{
	return (end->tv_sec - start->tv_sec) + 1e-9*(end->tv_nsec - start->tv_nsec);
}

static uint32_t batch(int fd)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	uint32_t handle = gem_create(fd, 4096);
	gem_write(fd, handle, 0, &bbe, sizeof(bbe));
	return handle;
}

static int loop(unsigned ring, int reps, int ncpus, unsigned flags)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj[2];
	unsigned all_engines[16];
	unsigned all_nengine;
	unsigned engines[16];
	unsigned nengine;
	double *shared;
	int fd;

	shared = mmap(0, 4096, PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);

	fd = drm_open_driver(DRIVER_INTEL);

	memset(obj, 0, sizeof(obj));
	obj[0].handle = gem_create(fd, 4096);
	if (flags & WRITE)
		obj[0].flags = EXEC_OBJECT_WRITE;
	obj[1].handle = batch(fd);

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = (uintptr_t)obj;
	execbuf.buffer_count = 2;
	execbuf.flags |= LOCAL_I915_EXEC_HANDLE_LUT;
	execbuf.flags |= LOCAL_I915_EXEC_NO_RELOC;
	if (__gem_execbuf(fd, &execbuf)) {
		execbuf.flags = 0;
		if (__gem_execbuf(fd, &execbuf))
			return 77;
	}

	if (flags & WRITE && !(execbuf.flags & LOCAL_I915_EXEC_HANDLE_LUT))
		return 77;

	all_nengine = 0;
	for (unsigned r = 1; r < 16; r++) {
		execbuf.flags &= ~ENGINE_FLAGS;
		execbuf.flags |= r;
		if (__gem_execbuf(fd, &execbuf) == 0)
			all_engines[all_nengine++] = r;
	}

	if (ring == -1) {
		nengine = all_nengine;
		memcpy(engines, all_engines, all_nengine*sizeof(engines[0]));
	} else {
		nengine = 1;
		engines[0] = ring;
	}

	while (reps--) {
		memset(shared, 0, 4096);

		gem_set_domain(fd, obj[1].handle, I915_GEM_DOMAIN_GTT, 0);
		sleep(1); /* wait for the hw to go back to sleep */

		igt_fork(child, ncpus) {
			struct timespec start, end;
			unsigned count = 0;

			obj[0].handle = gem_create(fd, 4096);
			obj[1].handle = batch(fd);

			clock_gettime(CLOCK_MONOTONIC, &start);
			do {
				for (int inner = 0; inner < 1024; inner++) {
					if (flags & READ_ALL) {
						obj[0].flags = 0;
						for (int n = 0; n < all_nengine; n++) {
							execbuf.flags &= ~ENGINE_FLAGS;
							execbuf.flags |= all_engines[n];
							gem_execbuf(fd, &execbuf);
						}
						if (flags & WRITE)
							obj[0].flags = EXEC_OBJECT_WRITE;
					}
					execbuf.flags &= ~ENGINE_FLAGS;
					execbuf.flags |= engines[count++ % nengine];
					gem_execbuf(fd, &execbuf);
					if (flags & SYNC)
						gem_sync(fd, obj[1].handle);
				}

				clock_gettime(CLOCK_MONOTONIC, &end);
			} while (elapsed(&start, &end) < 2.);

			gem_sync(fd, obj[1].handle);
			clock_gettime(CLOCK_MONOTONIC, &end);
			shared[child] = 1e6*elapsed(&start, &end) / count;

			gem_close(fd, obj[1].handle);
			gem_close(fd, obj[0].handle);
		}
		igt_waitchildren();

		for (int child = 0; child < ncpus; child++)
			shared[ncpus] += shared[child];
		printf("%7.3f\n", shared[ncpus] / ncpus);

		obj[0].flags = 0;
		for (int n = 0; n < nengine; n++) {
			execbuf.flags &= ~ENGINE_FLAGS;
			execbuf.flags |= engines[n];
			gem_execbuf(fd, &execbuf);
		}
		if (flags & WRITE)
			obj[0].flags = EXEC_OBJECT_WRITE;
	}
	return 0;
}

int main(int argc, char **argv)
{
	unsigned ring = I915_EXEC_RENDER;
	unsigned flags = 0;
	int reps = 1;
	int ncpus = 1;
	int c;

	while ((c = getopt (argc, argv, "e:r:sf")) != -1) {
		switch (c) {
		case 'e':
			if (strcmp(optarg, "rcs") == 0)
				ring = I915_EXEC_RENDER;
			else if (strcmp(optarg, "vcs") == 0)
				ring = I915_EXEC_BSD;
			else if (strcmp(optarg, "bcs") == 0)
				ring = I915_EXEC_BLT;
			else if (strcmp(optarg, "vecs") == 0)
				ring = I915_EXEC_VEBOX;
			else if (strcmp(optarg, "all") == 0)
				ring = -1;
			else
				ring = atoi(optarg);
			break;

		case 'r':
			reps = atoi(optarg);
			if (reps < 1)
				reps = 1;
			break;

		case 'f':
			ncpus = sysconf(_SC_NPROCESSORS_ONLN);
			break;

		case 's':
			flags |= SYNC;
			break;

		case 'W':
			flags |= WRITE;
			break;

		case 'A':
			flags |= READ_ALL;
			break;

		default:
			break;
		}
	}

	return loop(ring, reps, ncpus, flags);
}
