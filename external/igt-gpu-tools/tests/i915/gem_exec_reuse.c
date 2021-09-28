/*
 * Copyright © 2017 Intel Corporation
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

#include <limits.h>
#include <sys/resource.h>

#include "igt.h"
#include "igt_aux.h"

IGT_TEST_DESCRIPTION("Inspect scaling with large number of reused objects");

struct noop {
	struct drm_i915_gem_exec_object2 *obj;
	uint32_t batch;
	uint32_t *handles;
	unsigned int nhandles;
	unsigned int max_age;
	int fd;
};

static void noop(struct noop *n,
		 unsigned ring, unsigned ctx,
		 unsigned int count, unsigned int offset)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	unsigned int i;

	for (i = 0; i < count; i++)
		n->obj[i].handle = n->handles[(i + offset) & (n->nhandles-1)];
	n->obj[i].handle = n->batch;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(n->obj);
	execbuf.buffer_count = count + 1;
	execbuf.flags = ring | 1 << 12;
	execbuf.rsvd1 = ctx;
	gem_execbuf(n->fd, &execbuf);
}

static uint64_t max_open_files(void)
{
	struct rlimit rlim;

	if (getrlimit(RLIMIT_NOFILE, &rlim))
		rlim.rlim_cur = 64 << 10;

	igt_info("Process limit for file descriptors is %lu\n",
		 (long)rlim.rlim_cur);
	return rlim.rlim_cur;
}

static unsigned int max_nfd(void)
{
	uint64_t vfs = vfs_file_max();
	uint64_t fd = max_open_files();
	uint64_t min = fd < vfs ? fd : vfs;
	if (min > INT_MAX)
		min = INT_MAX;
	return min;
}

igt_main
{
	struct noop no;
	unsigned engines[16];
	unsigned nengine;
	unsigned n;

	igt_fixture {
		uint64_t gtt_size, max;
		uint32_t bbe = MI_BATCH_BUFFER_END;
		unsigned engine;

		igt_allow_unlimited_files();

		no.fd = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(no.fd);

		igt_fork_hang_detector(no.fd);

		gtt_size = (gem_aperture_size(no.fd) / 2) >> 12;
		if (gtt_size > INT_MAX / sizeof(*no.handles))
			gtt_size = INT_MAX / sizeof(*no.handles);

		max = max_nfd() - 16;
		if (max < gtt_size)
			gtt_size = max;

		no.nhandles = 1 << (igt_fls(gtt_size) - 1);
		intel_require_memory(no.nhandles, 4096, CHECK_RAM);

		no.max_age = no.nhandles / 2;

		no.handles = malloc(sizeof(*no.handles) * no.nhandles);
		for (n = 0; n < no.nhandles; n++)
			no.handles[n] = gem_create(no.fd, 4096);

		no.obj = malloc(sizeof(struct drm_i915_gem_exec_object2) * (no.max_age + 1));

		nengine = 0;
		for_each_engine(no.fd, engine)
			if (engine)
				engines[nengine++] = engine;
		igt_require(nengine);

		no.batch = gem_create(no.fd, 4096);
		gem_write(no.fd, no.batch, 0, &bbe, sizeof(bbe));
	}

	igt_subtest_f("single") {
		unsigned int timeout = 5;
		unsigned long age = 0;

		igt_until_timeout(timeout)
			for (n = 0; n < nengine; n++)
				noop(&no, engines[n], 0, 0, age++);
		gem_sync(no.fd, no.batch);
		igt_info("Completed %lu cycles\n", age);
	}

	igt_subtest_f("baggage") {
		unsigned int timeout = 5;
		unsigned long age = 0;

		igt_until_timeout(timeout)
			for (n = 0; n < nengine; n++)
				noop(&no, engines[n], 0,
				     no.max_age, age++);
		gem_sync(no.fd, no.batch);
		igt_info("Completed %lu cycles\n", age);
	}

	igt_subtest_f("contexts") {
		unsigned int timeout = 5;
		unsigned long ctx_age = 0;
		unsigned long obj_age = 0;
		const unsigned int ncontexts = 1024;
		uint32_t contexts[ncontexts];

		gem_require_contexts(no.fd);

		for (n = 0; n < ncontexts; n++)
			contexts[n] = gem_context_create(no.fd);

		igt_until_timeout(timeout) {
			for (n = 0; n < nengine; n++) {
				noop(&no, engines[n],
				     contexts[ctx_age % ncontexts],
				     no.max_age, obj_age);
				obj_age++;
			}
			ctx_age++;
		}
		gem_sync(no.fd, no.batch);
		igt_info("Completed %lu cycles across %lu context switches\n",
			 obj_age, ctx_age);

		for (n = 0; n < ncontexts; n++)
			gem_context_destroy(no.fd, contexts[n]);
	}

	igt_fixture
		igt_stop_hang_detector();
}
