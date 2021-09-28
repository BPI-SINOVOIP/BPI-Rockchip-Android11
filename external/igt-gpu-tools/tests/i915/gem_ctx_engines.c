/*
 * Copyright © 2018 Intel Corporation
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

#include <drm.h>

#include "i915/gem_context.h"
#include "sw_sync.h"

#define engine_class(e, n) ((e)->engines[(n)].engine_class)
#define engine_instance(e, n) ((e)->engines[(n)].engine_instance)

static bool has_context_engines(int i915)
{
	struct drm_i915_gem_context_param param = {
		.ctx_id = 0,
		.param = I915_CONTEXT_PARAM_ENGINES,
	};
	return __gem_context_set_param(i915, &param) == 0;
}

static void invalid_engines(int i915)
{
	struct i915_context_param_engines stack = {}, *engines;
	struct drm_i915_gem_context_param param = {
		.ctx_id = gem_context_create(i915),
		.param = I915_CONTEXT_PARAM_ENGINES,
		.value = to_user_pointer(&stack),
	};
	uint32_t handle;
	void *ptr;

	param.size = 0;
	igt_assert_eq(__gem_context_set_param(i915, &param), 0);

	param.size = 1;
	igt_assert_eq(__gem_context_set_param(i915, &param), -EINVAL);

	param.size = sizeof(stack) - 1;
	igt_assert_eq(__gem_context_set_param(i915, &param), -EINVAL);

	param.size = sizeof(stack) + 1;
	igt_assert_eq(__gem_context_set_param(i915, &param), -EINVAL);

	param.size = 0;
	igt_assert_eq(__gem_context_set_param(i915, &param), 0);

	/* Create a single page surrounded by inaccessible nothingness */
	ptr = mmap(NULL, 3 * 4096, PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	igt_assert(ptr != MAP_FAILED);

	munmap(ptr, 4096);
	engines = ptr + 4096;
	munmap(ptr + 2 *4096, 4096);

	param.size = sizeof(*engines) + sizeof(*engines->engines);
	param.value = to_user_pointer(engines);

	engines->engines[0].engine_class = -1;
	igt_assert_eq(__gem_context_set_param(i915, &param), -ENOENT);

	mprotect(engines, 4096, PROT_READ);
	igt_assert_eq(__gem_context_set_param(i915, &param), -ENOENT);

	mprotect(engines, 4096, PROT_WRITE);
	engines->engines[0].engine_class = 0;
	if (__gem_context_set_param(i915, &param)) /* XXX needs RCS */
		goto out;

	engines->extensions = to_user_pointer(ptr);
	igt_assert_eq(__gem_context_set_param(i915, &param), -EFAULT);

	engines->extensions = 0;
	igt_assert_eq(__gem_context_set_param(i915, &param), 0);

	param.value = to_user_pointer(engines - 1);
	igt_assert_eq(__gem_context_set_param(i915, &param), -EFAULT);

	param.value = to_user_pointer(engines) - 1;
	igt_assert_eq(__gem_context_set_param(i915, &param), -EFAULT);

	param.value = to_user_pointer(engines) - param.size +  1;
	igt_assert_eq(__gem_context_set_param(i915, &param), -EFAULT);

	param.value = to_user_pointer(engines) + 4096;
	igt_assert_eq(__gem_context_set_param(i915, &param), -EFAULT);

	param.value = to_user_pointer(engines) - param.size + 4096;
	igt_assert_eq(__gem_context_set_param(i915, &param), 0);

	param.value = to_user_pointer(engines) - param.size + 4096 + 1;
	igt_assert_eq(__gem_context_set_param(i915, &param), -EFAULT);

	param.value = to_user_pointer(engines) + 4096;
	igt_assert_eq(__gem_context_set_param(i915, &param), -EFAULT);

	param.value = to_user_pointer(engines) + 4096 - 1;
	igt_assert_eq(__gem_context_set_param(i915, &param), -EFAULT);

	param.value = to_user_pointer(engines) - 1;
	igt_assert_eq(__gem_context_set_param(i915, &param), -EFAULT);

	param.value = to_user_pointer(engines - 1);
	igt_assert_eq(__gem_context_set_param(i915, &param), -EFAULT);

	param.value = to_user_pointer(engines - 1) + 4096;
	igt_assert_eq(__gem_context_set_param(i915, &param), -EFAULT);

	param.value = to_user_pointer(engines - 1) + 4096 - sizeof(*engines->engines) / 2;
	igt_assert_eq(__gem_context_set_param(i915, &param), -EFAULT);

	handle = gem_create(i915, 4096 * 3);
	ptr = gem_mmap__gtt(i915, handle, 4096 * 3, PROT_READ);
	gem_close(i915, handle);

	munmap(ptr, 4096);
	munmap(ptr + 8192, 4096);

	param.value = to_user_pointer(ptr + 4096);
	igt_assert_eq(__gem_context_set_param(i915, &param), 0);

	param.value = to_user_pointer(ptr);
	igt_assert_eq(__gem_context_set_param(i915, &param), -EFAULT);

	param.value = to_user_pointer(ptr) + 4095;
	igt_assert_eq(__gem_context_set_param(i915, &param), -EFAULT);

	param.value = to_user_pointer(ptr) + 8192;
	igt_assert_eq(__gem_context_set_param(i915, &param), -EFAULT);

	param.value = to_user_pointer(ptr) + 12287;
	igt_assert_eq(__gem_context_set_param(i915, &param), -EFAULT);

	munmap(ptr + 4096, 4096);

out:
	munmap(engines, 4096);
	gem_context_destroy(i915, param.ctx_id);
}

static void idempotent(int i915)
{
	I915_DEFINE_CONTEXT_PARAM_ENGINES(engines , I915_EXEC_RING_MASK + 1);
	I915_DEFINE_CONTEXT_PARAM_ENGINES(expected , I915_EXEC_RING_MASK + 1);
	struct drm_i915_gem_context_param p = {
		.ctx_id = gem_context_create(i915),
		.param = I915_CONTEXT_PARAM_ENGINES,
		.value = to_user_pointer(&engines),
		.size = sizeof(engines),
	};
	const size_t base = sizeof(struct i915_context_param_engines);
	const struct intel_execution_engine2 *e;
	int idx;

	/* What goes in, must come out. And what comes out, must go in */

	gem_context_get_param(i915, &p);
	igt_assert_eq(p.size, 0); /* atm default is to use legacy ring mask */

	idx = 0;
	memset(&engines, 0, sizeof(engines));
	__for_each_physical_engine(i915, e) {
		engines.engines[idx].engine_class = e->class;
		engines.engines[idx].engine_instance = e->instance;
		idx++;
	}
	idx *= sizeof(*engines.engines);
	p.size = base + idx;
	gem_context_set_param(i915, &p);

	memcpy(&expected, &engines, sizeof(expected));

	gem_context_get_param(i915, &p);
	igt_assert_eq(p.size, base + idx);
	igt_assert(!memcmp(&expected, &engines, idx));

	p.size = base;
	gem_context_set_param(i915, &p);
	gem_context_get_param(i915, &p);
	igt_assert_eq(p.size, base);

	/* and it should not have overwritten the previous contents */
	igt_assert(!memcmp(&expected, &engines, idx));

	memset(&engines, 0, sizeof(engines));
	engines.engines[0].engine_class = I915_ENGINE_CLASS_INVALID;
	engines.engines[0].engine_instance = I915_ENGINE_CLASS_INVALID_NONE;
	idx = sizeof(*engines.engines);
	p.size = base + idx;
	gem_context_set_param(i915, &p);

	memcpy(&expected, &engines, sizeof(expected));

	gem_context_get_param(i915, &p);
	igt_assert_eq(p.size, base + idx);
	igt_assert(!memcmp(&expected, &engines, idx));

	memset(&engines, 0, sizeof(engines));
	p.size = sizeof(engines);
	gem_context_set_param(i915, &p);

	memcpy(&expected, &engines, sizeof(expected));

	gem_context_get_param(i915, &p);
	igt_assert_eq(p.size, sizeof(engines));
	igt_assert(!memcmp(&expected, &engines, idx));

	gem_context_destroy(i915, p.ctx_id);
}

static void execute_one(int i915)
{
	I915_DEFINE_CONTEXT_PARAM_ENGINES(engines , I915_EXEC_RING_MASK + 1);
	struct drm_i915_gem_context_param param = {
		.ctx_id = gem_context_create(i915),
		.param = I915_CONTEXT_PARAM_ENGINES,
		.value = to_user_pointer(&engines),
		/* .size to be filled in later */
	};
	struct drm_i915_gem_exec_object2 obj = {
		.handle = gem_create(i915, 4096),
	};
	struct drm_i915_gem_execbuffer2 execbuf = {
		.buffers_ptr = to_user_pointer(&obj),
		.buffer_count = 1,
		.rsvd1 = param.ctx_id,
	};
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	const struct intel_execution_engine2 *e;

	gem_write(i915, obj.handle, 0, &bbe, sizeof(bbe));

	/* Unadulterated I915_EXEC_DEFAULT should work */
	execbuf.flags = 0;
	gem_execbuf(i915, &execbuf);
	gem_sync(i915, obj.handle);

	__for_each_physical_engine(i915, e) {
		struct drm_i915_gem_busy busy = { .handle = obj.handle };

		for (int i = -1; i <= I915_EXEC_RING_MASK; i++) {
			igt_spin_t *spin;

			memset(&engines, 0, sizeof(engines));
			engine_class(&engines, 0) = e->class;
			engine_instance(&engines, 0) = e->instance;
			param.size = offsetof(typeof(engines), engines[1]);
			gem_context_set_param(i915, &param);

			spin = igt_spin_new(i915,
					    .ctx = param.ctx_id,
					    .engine = 0,
					    .flags = (IGT_SPIN_NO_PREEMPTION |
						      IGT_SPIN_POLL_RUN));

			igt_debug("Testing with map of %d engines\n", i + 1);
			memset(&engines.engines, -1, sizeof(engines.engines));
			if (i != -1) {
				engine_class(&engines, i) = e->class;
				engine_instance(&engines, i) = e->instance;
			}
			param.size = sizeof(uint64_t) + (i + 1) * sizeof(uint32_t);
			gem_context_set_param(i915, &param);

			igt_spin_busywait_until_started(spin);
			for (int j = 0; j <= I915_EXEC_RING_MASK; j++) {
				int expected = j == i ? 0 : -EINVAL;

				execbuf.flags = j;
				igt_assert_f(__gem_execbuf(i915, &execbuf) == expected,
					     "Failed to report the %s engine for slot %d (valid at %d)\n",
					     j == i ? "valid" : "invalid", j, i);
			}

			do_ioctl(i915, DRM_IOCTL_I915_GEM_BUSY, &busy);
			igt_assert_eq(busy.busy, i != -1 ? 1 << (e->class + 16) : 0);

			igt_spin_free(i915, spin);

			gem_sync(i915, obj.handle);
			do_ioctl(i915, DRM_IOCTL_I915_GEM_BUSY, &busy);
			igt_assert_eq(busy.busy, 0);
		}
	}

	/* Restore the defaults and check I915_EXEC_DEFAULT works again. */
	param.size = 0;
	gem_context_set_param(i915, &param);
	execbuf.flags = 0;
	gem_execbuf(i915, &execbuf);

	gem_close(i915, obj.handle);
	gem_context_destroy(i915, param.ctx_id);
}

static void execute_oneforall(int i915)
{
	I915_DEFINE_CONTEXT_PARAM_ENGINES(engines , I915_EXEC_RING_MASK + 1);
	struct drm_i915_gem_context_param param = {
		.ctx_id = gem_context_create(i915),
		.param = I915_CONTEXT_PARAM_ENGINES,
		.value = to_user_pointer(&engines),
		.size = sizeof(engines),
	};
	const struct intel_execution_engine2 *e;

	__for_each_physical_engine(i915, e) {
		memset(&engines, 0, sizeof(engines));
		for (int i = 0; i <= I915_EXEC_RING_MASK; i++) {
			engine_class(&engines, i) = e->class;
			engine_instance(&engines, i) = e->instance;
		}
		gem_context_set_param(i915, &param);

		for (int i = 0; i <= I915_EXEC_RING_MASK; i++) {
			struct drm_i915_gem_busy busy = {};
			igt_spin_t *spin;

			spin = __igt_spin_new(i915,
					      .ctx = param.ctx_id,
					      .engine = i);

			busy.handle = spin->handle;
			do_ioctl(i915, DRM_IOCTL_I915_GEM_BUSY, &busy);
			igt_assert_eq(busy.busy, 1 << (e->class + 16));

			igt_spin_free(i915, spin);
		}
	}

	gem_context_destroy(i915, param.ctx_id);
}

static void execute_allforone(int i915)
{
	I915_DEFINE_CONTEXT_PARAM_ENGINES(engines , I915_EXEC_RING_MASK + 1);
	struct drm_i915_gem_context_param param = {
		.ctx_id = gem_context_create(i915),
		.param = I915_CONTEXT_PARAM_ENGINES,
		.value = to_user_pointer(&engines),
	};
	const struct intel_execution_engine2 *e;
	int i;

	i = 0;
	memset(&engines, 0, sizeof(engines));
	__for_each_physical_engine(i915, e) {
		engine_class(&engines, i) = e->class;
		engine_instance(&engines, i) = e->instance;
		i++;
	}
	param.size = sizeof(uint64_t) + i * sizeof(uint32_t);
	gem_context_set_param(i915, &param);

	i = 0;
	__for_each_physical_engine(i915, e) {
		struct drm_i915_gem_busy busy = {};
		igt_spin_t *spin;

		spin = __igt_spin_new(i915,
				      .ctx = param.ctx_id,
				      .engine = i++);

		busy.handle = spin->handle;
		do_ioctl(i915, DRM_IOCTL_I915_GEM_BUSY, &busy);
		igt_assert_eq(busy.busy, 1 << (e->class + 16));

		igt_spin_free(i915, spin);
	}

	gem_context_destroy(i915, param.ctx_id);
}

static uint32_t read_result(int timeline, uint32_t *map, int idx)
{
	sw_sync_timeline_inc(timeline, 1);
	while (!READ_ONCE(map[idx]))
		;
	return map[idx];
}

static void independent(int i915)
{
#define RCS_TIMESTAMP (0x2000 + 0x358)
	const int gen = intel_gen(intel_get_drm_devid(i915));
	const int has_64bit_reloc = gen >= 8;
	I915_DEFINE_CONTEXT_PARAM_ENGINES(engines , I915_EXEC_RING_MASK + 1);
	struct drm_i915_gem_context_param param = {
		.ctx_id = gem_context_create(i915),
		.param = I915_CONTEXT_PARAM_ENGINES,
		.value = to_user_pointer(&engines),
		.size = sizeof(engines),
	};
	struct drm_i915_gem_exec_object2 results = { .handle = gem_create(i915, 4096) };
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	int timeline = sw_sync_timeline_create();
	uint32_t last, *map;

	igt_require(gen >= 6); /* No per-engine TIMESTAMP on older gen */
	igt_require(gem_scheduler_enabled(i915));

	{
		struct drm_i915_gem_execbuffer2 execbuf = {
			.buffers_ptr = to_user_pointer(&results),
			.buffer_count = 1,
			.rsvd1 = param.ctx_id,
		};
		gem_write(i915, results.handle, 0, &bbe, sizeof(bbe));
		gem_execbuf(i915, &execbuf);
		results.flags = EXEC_OBJECT_PINNED;
	}

	memset(&engines, 0, sizeof(engines)); /* All rcs0 */
	gem_context_set_param(i915, &param);

	gem_set_caching(i915, results.handle, I915_CACHING_CACHED);
	map = gem_mmap__cpu(i915, results.handle, 0, 4096, PROT_READ);
	gem_set_domain(i915, results.handle,
		       I915_GEM_DOMAIN_CPU, I915_GEM_DOMAIN_CPU);
	memset(map, 0, 4096);

	for (int i = 0; i < I915_EXEC_RING_MASK + 1; i++) {
		struct drm_i915_gem_exec_object2 obj[2] = {
			results, /* write hazard lies! */
			{ .handle = gem_create(i915, 4096) },
		};
		struct drm_i915_gem_execbuffer2 execbuf = {
			.buffers_ptr = to_user_pointer(obj),
			.buffer_count = 2,
			.rsvd1 = param.ctx_id,
			.rsvd2 = sw_sync_timeline_create_fence(timeline, i + 1),
			.flags = (I915_EXEC_RING_MASK - i) | I915_EXEC_FENCE_IN,
		};
		uint64_t offset = results.offset + 4 * i;
		uint32_t *cs;
		int j = 0;

		cs = gem_mmap__cpu(i915, obj[1].handle, 0, 4096, PROT_WRITE);

		cs[j] = 0x24 << 23 | 1; /* SRM */
		if (has_64bit_reloc)
			cs[j]++;
		j++;
		cs[j++] = RCS_TIMESTAMP;
		cs[j++] = offset;
		if (has_64bit_reloc)
			cs[j++] = offset >> 32;
		cs[j++] = MI_BATCH_BUFFER_END;

		munmap(cs, 4096);

		gem_execbuf(i915, &execbuf);
		gem_close(i915, obj[1].handle);
		close(execbuf.rsvd2);
	}

	last = read_result(timeline, map, 0);
	for (int i = 1; i < I915_EXEC_RING_MASK + 1; i++) {
		uint32_t t = read_result(timeline, map, i);
		igt_assert_f(t - last > 0,
			     "Engine instance [%d] executed too late, previous timestamp %08x, now %08x\n",
			     i, last, t);
		last = t;
	}
	munmap(map, 4096);

	close(timeline);
	gem_sync(i915, results.handle);
	gem_close(i915, results.handle);

	gem_context_destroy(i915, param.ctx_id);
}

igt_main
{
	int i915 = -1;

	igt_fixture {
		i915 = drm_open_driver_render(DRIVER_INTEL);
		igt_require_gem(i915);

		gem_require_contexts(i915);
		igt_require(has_context_engines(i915));

		igt_fork_hang_detector(i915);
	}

	igt_subtest("invalid-engines")
		invalid_engines(i915);

	igt_subtest("idempotent")
		idempotent(i915);

	igt_subtest("execute-one")
		execute_one(i915);

	igt_subtest("execute-oneforall")
		execute_oneforall(i915);

	igt_subtest("execute-allforone")
		execute_allforone(i915);

	igt_subtest("independent")
		independent(i915);

	igt_fixture
		igt_stop_hang_detector();
}
