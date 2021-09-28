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
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>

#include "drm.h"
#include "ioctl_wrappers.h"
#include "drmtest.h"
#include "intel_chipset.h"
#include "intel_reg.h"
#include "igt_stats.h"
#include "i915/gem_mman.h"

#define LOCAL_I915_EXEC_NO_RELOC (1<<11)
#define LOCAL_I915_EXEC_HANDLE_LUT (1<<12)

#define LOCAL_I915_EXEC_BSD_SHIFT      (13)
#define LOCAL_I915_EXEC_BSD_MASK       (3 << LOCAL_I915_EXEC_BSD_SHIFT)

#define ENGINE_FLAGS  (I915_EXEC_RING_MASK | LOCAL_I915_EXEC_BSD_MASK)

#define WRITE 0x1
#define IDLE 0x2
#define DMABUF 0x4
#define WAIT 0x8
#define SYNC 0x10
#define SYNCOBJ 0x20

#define LOCAL_I915_EXEC_FENCE_ARRAY (1 << 19)
struct local_gem_exec_fence {
	uint32_t handle;
	uint32_t flags;
#define LOCAL_EXEC_FENCE_WAIT (1 << 0)
#define LOCAL_EXEC_FENCE_SIGNAL (1 << 1)
};

static void gem_busy(int fd, uint32_t handle)
{
	struct drm_i915_gem_busy busy = { .handle = handle };
	ioctl(fd, DRM_IOCTL_I915_GEM_BUSY, &busy);
}

static void gem_wait__busy(int fd, uint32_t handle)
{
	struct drm_i915_gem_wait wait = { .bo_handle = handle };
	ioctl(fd, DRM_IOCTL_I915_GEM_WAIT, &wait);
}

static double elapsed(const struct timespec *start,
		      const struct timespec *end)
{
	return 1e9*(end->tv_sec - start->tv_sec) +
		(end->tv_nsec - start->tv_nsec);
}

struct sync_merge_data {
	char    name[32];
	__s32   fd2;
	__s32   fence;
	__u32   flags;
	__u32   pad;
};

#define SYNC_IOC_MAGIC         '>'
#define SYNC_IOC_MERGE         _IOWR(SYNC_IOC_MAGIC, 3, struct sync_merge_data)

static int sync_merge(int fd1, int fd2)
{
	struct sync_merge_data data;

	if (fd1 == -1)
		return dup(fd2);

	if (fd2 == -1)
		return dup(fd1);

	memset(&data, 0, sizeof(data));
	data.fd2 = fd2;
	strcpy(data.name, "i965");

	if (ioctl(fd1, SYNC_IOC_MERGE, &data))
		return -errno;

	return data.fence;
}

static uint32_t __syncobj_create(int fd)
{
	struct local_syncobj_create {
		uint32_t handle, flags;
	} arg;
#define LOCAL_IOCTL_SYNCOBJ_CREATE        DRM_IOWR(0xBF, struct local_syncobj_create)

	memset(&arg, 0, sizeof(arg));
	ioctl(fd, LOCAL_IOCTL_SYNCOBJ_CREATE, &arg);

	return arg.handle;
}

static uint32_t syncobj_create(int fd)
{
	uint32_t ret;

	igt_assert_neq((ret = __syncobj_create(fd)), 0);

	return ret;
}

#define LOCAL_SYNCOBJ_WAIT_FLAGS_WAIT_ALL (1 << 0)
#define LOCAL_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT (1 << 1)
struct local_syncobj_wait {
       __u64 handles;
       /* absolute timeout */
       __s64 timeout_nsec;
       __u32 count_handles;
       __u32 flags;
       __u32 first_signaled; /* only valid when not waiting all */
       __u32 pad;
};
#define LOCAL_IOCTL_SYNCOBJ_WAIT	DRM_IOWR(0xC3, struct local_syncobj_wait)
static int __syncobj_wait(int fd, struct local_syncobj_wait *args)
{
	int err = 0;
	if (drmIoctl(fd, LOCAL_IOCTL_SYNCOBJ_WAIT, args))
		err = -errno;
	return err;
}

static int loop(unsigned ring, int reps, int ncpus, unsigned flags)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj[2];
	struct drm_i915_gem_relocation_entry reloc[2];
	struct local_gem_exec_fence syncobj;
	unsigned engines[16];
	unsigned nengine;
	uint32_t *batch;
	double *shared;
	int fd, i, gen;
	int dmabuf;

	shared = mmap(0, 4096, PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);

	fd = drm_open_driver(DRIVER_INTEL);
	gen = intel_gen(intel_get_drm_devid(fd));

	memset(obj, 0, sizeof(obj));
	obj[0].handle = gem_create(fd, 4096);
	if (flags & WRITE)
		obj[0].flags = EXEC_OBJECT_WRITE;
	obj[1].handle = gem_create(fd, 4096);
	if (gem_mmap__has_wc(fd))
		batch = gem_mmap__wc(fd, obj[1].handle, 0, 4096, PROT_WRITE);
	else
		batch = gem_mmap__gtt(fd, obj[1].handle, 4096, PROT_WRITE);
	gem_set_domain(fd, obj[1].handle,
			I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	batch[0] = MI_BATCH_BUFFER_END;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = 2;
	execbuf.flags |= LOCAL_I915_EXEC_HANDLE_LUT;
	execbuf.flags |= LOCAL_I915_EXEC_NO_RELOC;
	if (__gem_execbuf(fd, &execbuf)) {
		execbuf.flags = 0;
		if (__gem_execbuf(fd, &execbuf))
			return 77;
	}

	if (flags & SYNCOBJ) {
		syncobj.handle = syncobj_create(fd);
		syncobj.flags = LOCAL_EXEC_FENCE_SIGNAL;

		execbuf.cliprects_ptr = to_user_pointer(&syncobj);
		execbuf.num_cliprects = 1;
		execbuf.flags |= LOCAL_I915_EXEC_FENCE_ARRAY;
	}

	if (ring == -1) {
		nengine = 0;
		for (ring = 1; ring < 16; ring++) {
			execbuf.flags &= ~ENGINE_FLAGS;
			execbuf.flags |= ring;
			if (__gem_execbuf(fd, &execbuf) == 0)
				engines[nengine++] = ring;
		}
	} else {
		nengine = 1;
		engines[0] = ring;
	}

	obj[1].relocs_ptr = to_user_pointer(reloc);
	obj[1].relocation_count = 2;

	if (flags & DMABUF)
		dmabuf = prime_handle_to_fd(fd, obj[0].handle);

	gem_set_domain(fd, obj[1].handle,
			I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);

	reloc[0].target_handle = obj[1].handle; /* recurse */
	reloc[0].presumed_offset = obj[1].offset;
	reloc[0].offset = sizeof(uint32_t);
	reloc[0].delta = 0;
	if (gen < 4)
		reloc[0].delta = 1;
	reloc[0].read_domains = I915_GEM_DOMAIN_COMMAND;
	reloc[0].write_domain = 0;

	reloc[1].target_handle = obj[0].handle;
	reloc[1].presumed_offset = obj[0].offset;
	reloc[1].offset = 1024;
	reloc[1].delta = 0;
	reloc[1].read_domains = I915_GEM_DOMAIN_RENDER;
	reloc[1].write_domain = 0;
	if (flags & WRITE)
		reloc[1].write_domain = I915_GEM_DOMAIN_RENDER;

	while (reps--) {
		int fence = -1;
		memset(shared, 0, 4096);

		gem_set_domain(fd, obj[1].handle,
			       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
		sleep(1); /* wait for the hw to go back to sleep */
		batch[i = 0] = MI_BATCH_BUFFER_START;
		if (gen >= 8) {
			batch[i] |= 1 << 8 | 1;
			batch[++i] = obj[1].offset;
			batch[++i] = obj[1].offset >> 32;
		} else if (gen >= 6) {
			batch[i] |= 1 << 8;
			batch[++i] = obj[1].offset;
		} else {
			batch[i] |= 2 << 6;
			batch[++i] = obj[1].offset;
			if (gen < 4)
				batch[i] |= 1;
		}

		if ((flags & IDLE) == 0) {
			for (int n = 0; n < nengine; n++) {
				execbuf.flags &= ~(3 << 16);
				if (flags & SYNC)
					execbuf.flags |= 1 << 17;
				execbuf.flags &= ~ENGINE_FLAGS;
				execbuf.flags |= engines[n];
				gem_execbuf_wr(fd, &execbuf);
				if (execbuf.flags & (1 << 17))
					fence = sync_merge(fence, execbuf.rsvd2 >> 32);
			}
		}

		igt_fork(child, ncpus) {
			struct timespec start, end;
			unsigned count = 0;

			clock_gettime(CLOCK_MONOTONIC, &start);
			do {
				if (flags & DMABUF) {
					struct pollfd pfd = { .fd = dmabuf, .events = POLLOUT };
					for (int inner = 0; inner < 1024; inner++)
						poll(&pfd, 1, 0);
				} else if (flags & SYNCOBJ) {
					struct local_syncobj_wait arg = {
						.handles = to_user_pointer(&syncobj.handle),
						.count_handles = 1,
					};

					for (int inner = 0; inner < 1024; inner++)
						__syncobj_wait(fd, &arg);
				} else if (flags & SYNC) {
					struct pollfd pfd = { .fd = fence, .events = POLLOUT };
					for (int inner = 0; inner < 1024; inner++)
						poll(&pfd, 1, 0);
				} else if (flags & WAIT) {
					for (int inner = 0; inner < 1024; inner++)
						gem_wait__busy(fd, obj[0].handle);
				} else {
					for (int inner = 0; inner < 1024; inner++)
						gem_busy(fd, obj[0].handle);
				}

				clock_gettime(CLOCK_MONOTONIC, &end);
				count += 1024;
			} while (elapsed(&start, &end) < 2e9);

			clock_gettime(CLOCK_MONOTONIC, &end);
			shared[child] = elapsed(&start, &end) / count;
		}
		igt_waitchildren();

		batch[0] = MI_BATCH_BUFFER_END;
		if (fence != -1)
			close(fence);

		for (int child = 0; child < ncpus; child++)
			shared[ncpus] += shared[child];
		printf("%7.3f\n", shared[ncpus] / ncpus);
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

	while ((c = getopt (argc, argv, "e:r:dfsSwWI")) != -1) {
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

		case 'd':
			flags |= DMABUF;
			break;

		case 'w':
			flags |= WAIT;
			break;

		case 's':
			flags |= SYNC;
			break;

		case 'S':
			flags |= SYNCOBJ;
			break;

		case 'W':
			flags |= WRITE;
			break;

		case 'I':
			flags |= IDLE;
			break;
		default:
			break;
		}
	}

	return loop(ring, reps, ncpus, flags);
}
