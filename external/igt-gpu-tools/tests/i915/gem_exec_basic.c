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

IGT_TEST_DESCRIPTION("Basic sanity check of execbuf-ioctl rings.");

static uint32_t batch_create(int fd)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	uint32_t handle;

	handle = gem_create(fd, 4096);
	gem_write(fd, handle, 0, &bbe, sizeof(bbe));

	return handle;
}

static void batch_fini(int fd, uint32_t handle)
{
	gem_sync(fd, handle); /* catch any GPU hang */
	gem_close(fd, handle);
}

static void noop(int fd, uint64_t flags)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 exec;

	gem_require_ring(fd, flags);

	memset(&exec, 0, sizeof(exec));

	exec.handle = batch_create(fd);

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&exec);
	execbuf.buffer_count = 1;
	execbuf.flags = flags;
	gem_execbuf(fd, &execbuf);

	batch_fini(fd, exec.handle);
}

static void readonly(int fd, uint64_t flags)
{
	struct drm_i915_gem_execbuffer2 *execbuf;
	struct drm_i915_gem_exec_object2 exec;

	gem_require_ring(fd, flags);

	memset(&exec, 0, sizeof(exec));
	exec.handle = batch_create(fd);

	execbuf = mmap(NULL, 4096, PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	igt_assert(execbuf != NULL);

	execbuf->buffers_ptr = to_user_pointer(&exec);
	execbuf->buffer_count = 1;
	execbuf->flags = flags;
	igt_assert(mprotect(execbuf, 4096, PROT_READ) == 0);

	gem_execbuf(fd, execbuf);

	munmap(execbuf, 4096);

	batch_fini(fd, exec.handle);
}

static void gtt(int fd, uint64_t flags)
{
	struct drm_i915_gem_execbuffer2 *execbuf;
	struct drm_i915_gem_exec_object2 *exec;
	uint32_t handle;

	gem_require_ring(fd, flags);

	handle = gem_create(fd, 4096);

	gem_set_domain(fd, handle, I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	execbuf = gem_mmap__gtt(fd, handle, 4096, PROT_WRITE);
	exec = (struct drm_i915_gem_exec_object2 *)(execbuf + 1);
	gem_close(fd, handle);

	exec->handle = batch_create(fd);

	execbuf->buffers_ptr = to_user_pointer(exec);
	execbuf->buffer_count = 1;
	execbuf->flags = flags;

	gem_execbuf(fd, execbuf);

	batch_fini(fd, exec->handle);
	munmap(execbuf, 4096);
}

static void all(int i915)
{
	const struct intel_execution_engine2 *e;

	__for_each_physical_engine(i915, e)
		noop(i915, e->flags);
}

static void readonly_all(int i915)
{
	const struct intel_execution_engine2 *e;

	__for_each_physical_engine(i915, e)
		readonly(i915, e->flags);
}

static void gtt_all(int i915)
{
	const struct intel_execution_engine2 *e;

	__for_each_physical_engine(i915, e)
		gtt(i915, e->flags);
}

igt_main
{
	const struct intel_execution_engine2 *e;
	int fd = -1;

	igt_fixture {
		fd = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(fd);

		igt_fork_hang_detector(fd);
	}

	igt_subtest("basic-all")
		all(fd);

	igt_subtest("readonly-all")
		readonly_all(fd);

	igt_subtest("gtt-all")
		gtt_all(fd);

	__for_each_physical_engine(fd, e) {
		igt_subtest_f("basic-%s", e->name)
			noop(fd, e->flags);
		igt_subtest_f("readonly-%s", e->name)
			readonly(fd, e->flags);
		igt_subtest_f("gtt-%s", e->name)
			gtt(fd, e->flags);
	}

	igt_fixture {
		igt_stop_hang_detector();
		close(fd);
	}
}
