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
#include <assert.h>

#include "drm.h"
#include "ioctl_wrappers.h"
#include "drmtest.h"
#include "intel_io.h"
#include "igt_stats.h"

enum {
	ADD_BO = 0,
	DEL_BO,
	ADD_CTX,
	DEL_CTX,
	EXEC,
	WAIT,
};

struct trace_add_bo {
	uint32_t handle;
	uint64_t size;
} __attribute__((packed));

struct trace_del_bo {
	uint32_t handle;
} __attribute__((packed));

struct trace_add_ctx {
	uint32_t handle;
} __attribute__((packed));

struct trace_del_ctx {
	uint32_t handle;
} __attribute__((packed));

struct trace_exec {
	uint32_t object_count;
	uint64_t flags;
	uint32_t context;
}__attribute__((packed));

struct trace_exec_object {
	uint32_t handle;
	uint32_t relocation_count;
	uint64_t alignment;
	uint64_t offset;
	uint64_t flags;
	uint64_t rsvd1;
	uint64_t rsvd2;
}__attribute__((packed));

struct trace_wait {
	uint32_t handle;
} __attribute__((packed));

static uint32_t hars_petruska_f54_1_random(void)
{
	static uint32_t state = 0x12345678;

#define rol(x,k) ((x << k) | (x >> (32-k)))
	return state = (state ^ rol (state, 5) ^ rol (state, 24)) + 0x37798849;
#undef rol
}

static double elapsed(const struct timespec *start, const struct timespec *end)
{
	return 1e3*(end->tv_sec - start->tv_sec) + 1e-6*(end->tv_nsec - start->tv_nsec);
}

static uint32_t __gem_context_create_local(int fd)
{
	struct drm_i915_gem_context_create arg = {};
	drmIoctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_CREATE, &arg);
	return arg.ctx_id;
}

static double replay(const char *filename, long nop, long range)
{
	struct timespec t_start, t_end;
	struct drm_i915_gem_execbuffer2 eb = {};
	const struct trace_version {
		uint32_t magic;
		uint32_t version;
	} *tv;
	const uint32_t bbe = 0xa << 23;
	struct drm_i915_gem_exec_object2 *exec_objects = NULL;
	uint32_t *bo, *ctx;
	int num_bo, num_ctx;
	int max_objects = 0;
	struct stat st;
	uint8_t *ptr, *end;
	int fd;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return -1;

	if (fstat(fd, &st) < 0) {
		close(fd);
		return -1;
	}

	ptr = mmap(0, st.st_size, PROT_WRITE, MAP_PRIVATE, fd, 0);
	close(fd);

	if (ptr == MAP_FAILED)
		return -1;

	madvise(ptr, st.st_size, MADV_SEQUENTIAL);
	end = ptr + st.st_size;

	tv = (struct trace_version *)ptr;
	if (tv->magic != 0xdeadbeef) {
		fprintf(stderr, "%s: invalid magic\n", filename);
		return -1;
	}
	if (tv->version != 1) {
		fprintf(stderr, "%s: unhandled version %d\n",
			filename, tv->version);
		return -1;
	}
	ptr = (void *)(tv + 1);

	ctx = calloc(1024, sizeof(*ctx));
	num_ctx = 1024;

	bo = calloc(4096, sizeof(*bo));
	num_bo = 4096;

	fd = drm_open_driver(DRIVER_INTEL);
	if (nop > 0) {
		bo[0] = gem_create(fd, nop + range);
		gem_write(fd, bo[0], nop + range - sizeof(bbe),
			  &bbe, sizeof(bbe));
		range *= 2;
		range -= 64;
	} else {
		bo[0] = gem_create(fd, 4096);
		gem_write(fd, bo[0], 0, &bbe, sizeof(bbe));
	}

	clock_gettime(CLOCK_MONOTONIC, &t_start);
	do switch (*ptr++) {
	case ADD_BO:
		{
			struct trace_add_bo *t = (void *)ptr;
			ptr = (void *)(t + 1);

			if (t->handle >= num_bo) {
				int new_bo = ALIGN(t->handle, 4096);
				bo = realloc(bo, sizeof(*bo)*new_bo);
				memset(bo + num_bo, 0, sizeof(*bo)*(new_bo - num_bo));
				num_bo = new_bo;
			}

			bo[t->handle] = gem_create(fd, t->size);
			break;
		}
	case DEL_BO:
		{
			struct trace_del_bo *t = (void *)ptr;
			ptr = (void *)(t + 1);

			assert(t->handle && t->handle < num_bo && bo[t->handle]);
			gem_close(fd, bo[t->handle]);
			bo[t->handle] = 0;
			break;
		}
	case ADD_CTX:
		{
			struct trace_add_ctx *t = (void *)ptr;
			ptr = (void *)(t + 1);

			if (t->handle >= num_ctx) {
				int new_ctx = ALIGN(t->handle, 1024);
				ctx = realloc(ctx, sizeof(*ctx)*new_ctx);
				memset(ctx + num_ctx, 0, sizeof(*ctx)*(new_ctx - num_ctx));
				num_ctx = new_ctx;
			}

			ctx[t->handle] = __gem_context_create_local(fd);
			break;
		}
	case DEL_CTX:
		{
			struct trace_del_ctx *t = (void *)ptr;
			ptr = (void *)(t + 1);

			assert(t->handle < num_ctx && ctx[t->handle]);
			gem_context_destroy(fd, ctx[t->handle]);
			ctx[t->handle] = 0;
			break;
		}
	case EXEC:
		{
			struct trace_exec *t = (void *)ptr;
			ptr = (void *)(t + 1);

			eb.buffer_count = t->object_count;
			eb.flags = t->flags;
			eb.rsvd1 = ctx[t->context];

			if (eb.buffer_count >= max_objects) {
				free(exec_objects);

				max_objects = ALIGN(eb.buffer_count + 1, 4096);

				exec_objects = malloc(max_objects*sizeof(*exec_objects));
				eb.buffers_ptr = (uintptr_t)exec_objects;
			}

			for (uint32_t i = 0; i < eb.buffer_count; i++) {
				struct trace_exec_object *to = (void *)ptr;
				ptr = (void *)(to + 1);

				exec_objects[i].handle = bo[to->handle];
				exec_objects[i].alignment = to->alignment;
				exec_objects[i].offset = to->offset;
				exec_objects[i].flags = to->flags;
				exec_objects[i].rsvd1 = to->rsvd1;
				exec_objects[i].rsvd2 = to->rsvd2;

				exec_objects[i].relocation_count = to->relocation_count;
				exec_objects[i].relocs_ptr = (uintptr_t)ptr;

				if (!(eb.flags & I915_EXEC_HANDLE_LUT)) {
					struct drm_i915_gem_relocation_entry *relocs =
						(struct drm_i915_gem_relocation_entry *)ptr;
					for (uint32_t j = 0; j < to->relocation_count; j++)
						relocs[j].target_handle = bo[relocs[j].target_handle];
				}

				ptr += sizeof(struct drm_i915_gem_relocation_entry) * to->relocation_count;
			}

			((struct drm_i915_gem_exec_object2 *)
			 memset(&exec_objects[eb.buffer_count++], 0,
				sizeof(*exec_objects)))->handle = bo[0];

			if (nop > 0) {
				eb.batch_start_offset = hars_petruska_f54_1_random();
				eb.batch_start_offset =
					((uint64_t)eb.batch_start_offset * range) >> 32;
				eb.batch_start_offset = ALIGN(eb.batch_start_offset, 64);
			}
			gem_execbuf(fd, &eb);
			break;
		}

	case WAIT:
		{
			struct trace_wait *t = (void *)ptr;
			ptr = (void *)(t + 1);

			assert(t->handle && t->handle < num_bo && bo[t->handle]);
			gem_wait(fd, bo[t->handle], NULL);
			break;
		}

	default:
		fprintf(stderr, "Unknown cmd: %x\n", *ptr);
		return -1;
	} while (ptr < end);
	clock_gettime(CLOCK_MONOTONIC, &t_end);

	return elapsed(&t_start, &t_end);
}

static long calibrate_nop(int usecs)
{
	const uint32_t bbe = 0xa << 23;
	int fd = drm_open_driver(DRIVER_INTEL);
	struct drm_i915_gem_exec_object2 obj = {};
	struct drm_i915_gem_execbuffer2 eb = { .buffer_count = 1, .buffers_ptr = (uintptr_t)&obj};
	unsigned long size, last_size;

	size = 256*1024;
	do {
		struct timespec t_start, t_end;

		obj.handle = gem_create(fd, size);
		gem_write(fd, obj.handle, size - sizeof(bbe), &bbe, sizeof(bbe));
		gem_execbuf(fd, &eb);
		gem_sync(fd, obj.handle);

		clock_gettime(CLOCK_MONOTONIC, &t_start);
		for (int loop = 0; loop < 9; loop++)
			gem_execbuf(fd, &eb);
		gem_sync(fd, obj.handle);
		clock_gettime(CLOCK_MONOTONIC, &t_end);

		gem_close(fd, obj.handle);

		last_size = size;
		size = 9e-3*usecs / elapsed(&t_start, &t_end) * size;
		size = ALIGN(size, 4096);
	} while (size != last_size);

	close(fd);
	return size;
}

static int measure_nop(long nop)
{
	const uint32_t bbe = 0xa << 23;
	int fd = drm_open_driver(DRIVER_INTEL);
	struct drm_i915_gem_exec_object2 obj = {};
	struct drm_i915_gem_execbuffer2 eb = { .buffer_count = 1, .buffers_ptr = (uintptr_t)&obj};
	struct timespec t_start, t_end;

	obj.handle = gem_create(fd, nop);
	gem_write(fd, obj.handle, nop - sizeof(bbe), &bbe, sizeof(bbe));
	gem_execbuf(fd, &eb);
	gem_sync(fd, obj.handle);

	clock_gettime(CLOCK_MONOTONIC, &t_start);
	for (int loop = 0; loop < 9; loop++)
		gem_execbuf(fd, &eb);
	gem_sync(fd, obj.handle);
	clock_gettime(CLOCK_MONOTONIC, &t_end);

	gem_close(fd, obj.handle);

	close(fd);
	return 1e3*elapsed(&t_start, &t_end) / 9;
}

int main(int argc, char **argv)
{
	int delay = 1000;
	double *results;
	long nop = 0;
	long range = 0;
	int i, c;

	results = mmap(NULL, ALIGN(argc*sizeof(double), 4096),
		       PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);

	while ((c = getopt(argc, argv, "d:n:r:")) != -1) {
		switch (c) {
		case 'd':
			delay = atoi(optarg);
			break;
		case 'n':
			nop = strtol(optarg, NULL, 0);
			if (nop > 0)
				nop = ALIGN(nop, 4096);
			break;
		case 'r':
			range = strtol(optarg, NULL, 0);
			if (range > 0)
				range = ALIGN(range, 4096);
			break;
		default:
			break;
		}
	}

	if (!nop)
		nop = calibrate_nop(delay);
	if (!range)
		range = nop / 2;
	if (nop > 0) {
		delay = measure_nop(nop);
		printf("Using %lu nop batch for ~%dus delay, range %lu [%dus]\n",
		       nop, delay,
		       range, (int)(delay * range / nop));
	}

	igt_fork(child, argc-optind)
		results[child] = replay(argv[child + optind], nop, range);
	igt_waitchildren();

	for (i = 0; i < argc - optind; i++) {
		double t = results[i];
		if (t < 0)
			printf("%s: failed\n", argv[optind + i]);
		else
			printf("%s: %.3f\n", argv[optind + i], t);
	}

	return 0;
}
