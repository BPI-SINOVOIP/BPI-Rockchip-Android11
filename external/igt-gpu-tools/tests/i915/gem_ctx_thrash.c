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
 *
 */

#include "igt.h"
#include "igt_sysfs.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/resource.h>

IGT_TEST_DESCRIPTION("Fill the Gobal GTT with context objects and VMs\n");

#define NUM_THREADS (2*sysconf(_SC_NPROCESSORS_ONLN))

static void xchg_int(void *array, unsigned i, unsigned j)
{
	int *A = array;
	igt_swap(A[i], A[j]);
}

static unsigned context_size(int fd)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));

	switch (gen) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7: return 18 << 12;
	case 8: return 20 << 12;
	case 9: return 22 << 12;
	default: return 32 << 12;
	}
}

static unsigned get_num_contexts(int fd, int num_engines)
{
	uint64_t ggtt_size;
	unsigned size;
	unsigned count;

	/* Compute the number of contexts we can allocate to fill the GGTT */
	ggtt_size = gem_global_aperture_size(fd);

	size = context_size(fd);
	if (gem_has_execlists(fd)) {
		size += 4 << 12; /* ringbuffer as well */
		if (num_engines) /* one per engine with execlists */
			size *= num_engines;
	}

	count = 3 * (ggtt_size / size) / 2;
	igt_info("Creating %lld contexts (assuming of size %lld%s)\n",
		 (long long)count, (long long)size,
		 gem_has_execlists(fd) ? " with execlists" : "");

	intel_require_memory(count, size, CHECK_RAM | CHECK_SWAP);
	return count;
}

static void single(const char *name, bool all_engines)
{
	struct drm_i915_gem_exec_object2 *obj;
	struct drm_i915_gem_relocation_entry *reloc;
	unsigned int engines[16], num_engines, num_ctx;
	uint32_t *ctx, *map, scratch, size;
	int fd, gen;
#define MAX_LOOP 16

	fd = drm_open_driver(DRIVER_INTEL);
	igt_require_gem(fd);
	gem_require_contexts(fd);

	gen = intel_gen(intel_get_drm_devid(fd));

	num_engines = 0;
	if (all_engines) {
		unsigned engine;

		for_each_physical_engine(fd, engine) {
			if (!gem_can_store_dword(fd, engine))
				continue;

			engines[num_engines++] = engine;
			if (num_engines == ARRAY_SIZE(engines))
				break;
		}
	} else {
		igt_require(gem_can_store_dword(fd, 0));
		engines[num_engines++] = 0;
	}
	igt_require(num_engines);

	num_ctx = get_num_contexts(fd, num_engines);

	size = ALIGN(num_ctx * sizeof(uint32_t), 4096);
	scratch = gem_create(fd, size);
	gem_set_caching(fd, scratch, I915_CACHING_CACHED);
	obj = calloc(num_ctx, 3 * sizeof(*obj));
	reloc = calloc(num_ctx, 2 * sizeof(*reloc));

	ctx = malloc(num_ctx * sizeof(uint32_t));
	igt_assert(ctx);
	for (unsigned n = 0; n < num_ctx; n++) {
		ctx[n] = gem_context_create(fd);

		obj[3*n + 0].handle = gem_create(fd, 4096);
		reloc[2*n + 0].target_handle = obj[3*n + 0].handle;
		reloc[2*n + 0].presumed_offset = 0;
		reloc[2*n + 0].offset = 4000;
		reloc[2*n + 0].delta = 0;
		reloc[2*n + 0].read_domains = I915_GEM_DOMAIN_RENDER;
		reloc[2*n + 0].write_domain = I915_GEM_DOMAIN_RENDER;

		obj[3*n + 1].handle = scratch;
		reloc[2*n + 1].target_handle = scratch;
		reloc[2*n + 1].presumed_offset = 0;
		reloc[2*n + 1].offset = sizeof(uint32_t);
		reloc[2*n + 1].delta = n * sizeof(uint32_t);
		reloc[2*n + 1].read_domains = I915_GEM_DOMAIN_RENDER;
		reloc[2*n + 1].write_domain = 0; /* lies! */
		if (gen >= 4 && gen < 8)
			reloc[2*n + 1].offset += sizeof(uint32_t);

		obj[3*n + 2].relocs_ptr = to_user_pointer(&reloc[2*n]);
		obj[3*n + 2].relocation_count = 2;
	}

	map = gem_mmap__cpu(fd, scratch, 0, size, PROT_WRITE);
	for (unsigned int loop = 1; loop <= MAX_LOOP; loop <<= 1) {
		const unsigned int count = loop * num_ctx;
		uint32_t *all;

		all = malloc(count * sizeof(uint32_t));
		for (unsigned int n = 0; n < count; n++)
			all[n] = ctx[n % num_ctx];
		igt_permute_array(all, count, xchg_int);

		for (unsigned int n = 0; n < count; n++) {
			const unsigned int r = n % num_ctx;
			struct drm_i915_gem_execbuffer2 execbuf = {
				.buffers_ptr = to_user_pointer(&obj[3*r]),
				.buffer_count = 3,
				.flags = engines[n % num_engines],
				.rsvd1 = all[n],
			};
			uint64_t offset =
				reloc[2*r + 1].presumed_offset +
				reloc[2*r + 1].delta;
			uint32_t handle = gem_create(fd, 4096);
			uint32_t buf[16];
			int i;

			buf[i = 0] = MI_STORE_DWORD_IMM;
			if (gen >= 8) {
				buf[++i] = offset;
				buf[++i] = offset >> 32;
			} else if (gen >= 4) {
				if (gen < 6)
					buf[i] |= 1 << 22;
				buf[++i] = 0;
				buf[++i] = offset;
			} else {
				buf[i]--;
				buf[++i] = offset;
			}
			buf[++i] = all[n];
			buf[++i] = MI_BATCH_BUFFER_END;
			gem_write(fd, handle, 0, buf, sizeof(buf));
			obj[3*r + 2].handle = handle;

			gem_execbuf(fd, &execbuf);
			gem_close(fd, handle);
		}

		/*
		 * Note we lied about the write-domain when writing from the
		 * GPU (in order to avoid inter-ring synchronisation), so now
		 * we have to force the synchronisation here.
		 */
		gem_set_domain(fd, scratch,
			       I915_GEM_DOMAIN_CPU, I915_GEM_DOMAIN_CPU);
		for (unsigned int n = count - num_ctx; n < count; n++)
			igt_assert_eq(map[n % num_ctx], all[n]);
		free(all);
	}
	munmap(map, size);

	free(ctx);
	close(fd);
}

static void processes(void)
{
	unsigned engines[16], engine;
	int num_engines;
	struct rlimit rlim;
	unsigned num_ctx;
	uint32_t name;
	int fd, *fds;

	fd = drm_open_driver(DRIVER_INTEL);

	num_engines = 0;
	for_each_physical_engine(fd, engine) {
		engines[num_engines++] = engine;
		if (num_engines == ARRAY_SIZE(engines))
			break;
	}

	num_ctx = get_num_contexts(fd, num_engines);

	/* tweak rlimits to allow us to create this many files */
	igt_assert(getrlimit(RLIMIT_NOFILE, &rlim) == 0);
	if (rlim.rlim_cur < ALIGN(num_ctx + 1024, 1024)) {
		rlim.rlim_cur = ALIGN(num_ctx + 1024, 1024);
		if (rlim.rlim_cur > rlim.rlim_max)
			rlim.rlim_max = rlim.rlim_cur;
		igt_require(setrlimit(RLIMIT_NOFILE, &rlim) == 0);
	}

	fds = malloc(num_ctx * sizeof(int));
	igt_assert(fds);
	for (unsigned n = 0; n < num_ctx; n++) {
		fds[n] = drm_open_driver(DRIVER_INTEL);
		if (fds[n] == -1) {
			int err = errno;
			for (unsigned i = n; i--; )
				close(fds[i]);
			free(fds);
			errno = err;
			igt_assert_f(0, "failed to create context %lld/%lld\n", (long long)n, (long long)num_ctx);
		}
	}

	if (1) {
		uint32_t bbe = MI_BATCH_BUFFER_END;
		name = gem_create(fd, 4096);
		gem_write(fd, name, 0, &bbe, sizeof(bbe));
		name = gem_flink(fd, name);
	}

	igt_fork(child, NUM_THREADS) {
		struct drm_i915_gem_execbuffer2 execbuf;
		struct drm_i915_gem_exec_object2 obj;

		memset(&obj, 0, sizeof(obj));
		memset(&execbuf, 0, sizeof(execbuf));
		execbuf.buffers_ptr = to_user_pointer(&obj);
		execbuf.buffer_count = 1;

		igt_permute_array(fds, num_ctx, xchg_int);
		for (unsigned n = 0; n < num_ctx; n++) {
			obj.handle = gem_open(fds[n], name);
			execbuf.flags = engines[n % num_engines];
			gem_execbuf(fds[n], &execbuf);
			gem_close(fds[n], obj.handle);
		}
	}
	igt_waitchildren();

	for (unsigned n = 0; n < num_ctx; n++)
		close(fds[n]);
	free(fds);
	close(fd);
}

struct thread {
	int fd;
	uint32_t *all_ctx;
	unsigned num_ctx;
	uint32_t batch;
};

static void *thread(void *data)
{
	struct thread *t = data;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj;
	uint32_t *ctx;

	memset(&obj, 0, sizeof(obj));
	obj.handle = t->batch;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;

	ctx = malloc(t->num_ctx * sizeof(uint32_t));
	igt_assert(ctx);
	memcpy(ctx, t->all_ctx, t->num_ctx * sizeof(uint32_t));

	igt_until_timeout(150) {
		igt_permute_array(ctx, t->num_ctx, xchg_int);
		for (unsigned n = 0; n < t->num_ctx; n++) {
			execbuf.rsvd1 = ctx[n];
			gem_execbuf(t->fd, &execbuf);
		}
	}

	free(ctx);

	return NULL;
}

static void threads(void)
{
	uint32_t bbe = MI_BATCH_BUFFER_END;
	pthread_t threads[NUM_THREADS];
	struct thread data;

	data.fd = drm_open_driver_render(DRIVER_INTEL);
	igt_require_gem(data.fd);

	gem_require_contexts(data.fd);

	data.num_ctx = get_num_contexts(data.fd, false);
	data.all_ctx = malloc(data.num_ctx * sizeof(uint32_t));
	igt_assert(data.all_ctx);
	for (unsigned n = 0; n < data.num_ctx; n++)
		data.all_ctx[n] = gem_context_create(data.fd);
	data.batch = gem_create(data.fd, 4096);
	gem_write(data.fd, data.batch, 0, &bbe, sizeof(bbe));

	for (int n = 0; n < NUM_THREADS; n++)
		pthread_create(&threads[n], NULL, thread, &data);

	for (int n = 0; n < NUM_THREADS; n++)
		pthread_join(threads[n], NULL);

	close(data.fd);
}

igt_main
{
	igt_skip_on_simulation();

	igt_subtest("single")
		single("single", false);
	igt_subtest("engines")
		single("engines", true);

	igt_subtest("processes")
		processes();

	igt_subtest("threads")
		threads();
}
