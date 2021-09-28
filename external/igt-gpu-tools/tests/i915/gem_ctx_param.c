/*
 * Copyright © 2015 Intel Corporation
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
 *    Daniel Vetter <daniel.vetter@ffwll.ch>
 */

#include <fcntl.h>
#include <limits.h>

#include "igt.h"
#include "i915/gem_vm.h"

IGT_TEST_DESCRIPTION("Basic test for context set/get param input validation.");

#define NEW_CTX	BIT(0)
#define USER BIT(1)

static void set_priority(int i915)
{
	static const int64_t test_values[] = {
		/* Test space too big, pick significant values */
		INT_MIN,

		I915_CONTEXT_MIN_USER_PRIORITY - 1,
		I915_CONTEXT_MIN_USER_PRIORITY,
		I915_CONTEXT_MIN_USER_PRIORITY + 1,

		I915_CONTEXT_DEFAULT_PRIORITY - 1,
		I915_CONTEXT_DEFAULT_PRIORITY,
		I915_CONTEXT_DEFAULT_PRIORITY + 1,

		I915_CONTEXT_MAX_USER_PRIORITY - 1,
		I915_CONTEXT_MAX_USER_PRIORITY,
		I915_CONTEXT_MAX_USER_PRIORITY + 1,

		INT_MAX
	};
	unsigned int size;
	int64_t *values;

	igt_require(getuid() == 0);

	size = ARRAY_SIZE(test_values);
	values = malloc(sizeof(test_values) * 8);
	igt_assert(values);

	for (unsigned i = 0; i < size; i++) {
		values[i + 0*size] = test_values[i];
		values[i + 1*size] = test_values[i] | (uint64_t)1 << 32;
		values[i + 2*size] = test_values[i] | (uint64_t)rand() << 32;
		values[i + 3*size] = test_values[i] ^ rand();
		values[i + 4*size] = rand() % (I915_CONTEXT_MAX_USER_PRIORITY - I915_CONTEXT_MIN_USER_PRIORITY) + I915_CONTEXT_MIN_USER_PRIORITY;
		values[i + 5*size] = rand();
		values[i + 6*size] = rand() | (uint64_t)rand() << 32;
		values[i + 7*size] = (uint64_t)test_values[i] << 32;
	}
	size *= 8;

	igt_permute_array(values, size, igt_exchange_int64);

	igt_fork(flags, NEW_CTX | USER) {
		int fd = gem_reopen_driver(i915);
		struct drm_i915_gem_context_param arg = {
			.param = I915_CONTEXT_PARAM_PRIORITY,
			.ctx_id = flags & NEW_CTX ? gem_context_create(fd) : 0,
		};
		int64_t old_prio;

		if (flags & USER) {
			igt_debug("Dropping root privilege\n");
			igt_drop_root();
		}

		gem_context_get_param(fd, &arg);
		old_prio = arg.value;

		for (unsigned i = 0; i < size; i++) {
			int64_t prio = values[i];
			int expected = 0;
			int err;

			arg.value = prio;

			if (flags & USER &&
			    prio > I915_CONTEXT_DEFAULT_PRIORITY)
				expected = -EPERM;

			if (prio < I915_CONTEXT_MIN_USER_PRIORITY ||
			    prio > I915_CONTEXT_MAX_USER_PRIORITY)
				expected = -EINVAL;

			err =__gem_context_set_param(fd, &arg);
			igt_assert_f(err == expected,
				     "Priority requested %" PRId64 " with flags %x, expected result %d, returned %d\n",
				     prio, flags, expected, err);

			gem_context_get_param(fd, &arg);
			if (!err)
				old_prio = prio;
			igt_assert_eq(arg.value, old_prio);
		}

		arg.value = 0;
		gem_context_set_param(fd, &arg);

		if (flags & NEW_CTX)
			gem_context_destroy(fd, arg.ctx_id);
	}

	igt_waitchildren();
	free(values);
}

static uint32_t __batch_create(int i915, uint32_t offset)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	uint32_t handle;

	handle = gem_create(i915, ALIGN(offset + 4, 4096));
	gem_write(i915, handle, offset, &bbe, sizeof(bbe));

	return handle;
}

static uint32_t batch_create(int i915)
{
	return __batch_create(i915, 0);
}

static void test_vm(int i915)
{
	const uint64_t nonzero_offset = 48 << 20;
	struct drm_i915_gem_exec_object2 batch = {
		.handle = batch_create(i915),
	};
	struct drm_i915_gem_execbuffer2 eb = {
		.buffers_ptr = to_user_pointer(&batch),
		.buffer_count = 1,
	};
	struct drm_i915_gem_context_param arg = {
		.param = I915_CONTEXT_PARAM_VM,
	};
	uint32_t parent, child;

	/*
	 * Proving 2 contexts share the same GTT is quite tricky as we have no
	 * means of directly comparing them (each handle returned to userspace
	 * is unique). What we do instead is rely on a quirk of execbuf that
	 * it does not try to move an VMA without good reason, and so that
	 * having used an object in one context, it will have the same address
	 * in the next context that shared the VM.
	 */

	arg.value = -1ull;
	igt_require(__gem_context_set_param(i915, &arg) == -ENOENT);

	parent = gem_context_create(i915);
	child = gem_context_create(i915);

	/* Using implicit soft-pinning */
	eb.rsvd1 = parent;
	batch.offset = nonzero_offset;
	gem_execbuf(i915, &eb);
	igt_assert_eq_u64(batch.offset, nonzero_offset);

	eb.rsvd1 = child;
	batch.offset = 0;
	gem_execbuf(i915, &eb);
	igt_assert_eq_u64(batch.offset, 0);

	eb.rsvd1 = parent;
	gem_execbuf(i915, &eb);
	igt_assert_eq_u64(batch.offset, nonzero_offset);

	arg.ctx_id = parent;
	gem_context_get_param(i915, &arg);
	gem_context_set_param(i915, &arg);

	/* Still the same VM, so expect the old VMA again */
	batch.offset = 0;
	gem_execbuf(i915, &eb);
	igt_assert_eq_u64(batch.offset, nonzero_offset);

	arg.ctx_id = child;
	gem_context_set_param(i915, &arg);

	eb.rsvd1 = child;
	batch.offset = 0;
	gem_execbuf(i915, &eb);
	igt_assert_eq_u64(batch.offset, nonzero_offset);

	gem_context_destroy(i915, child);
	gem_context_destroy(i915, parent);

	/* both contexts destroyed, but we still keep hold of the vm */
	child = gem_context_create(i915);

	arg.ctx_id = child;
	gem_context_set_param(i915, &arg);

	eb.rsvd1 = child;
	batch.offset = 0;
	gem_execbuf(i915, &eb);
	igt_assert_eq_u64(batch.offset, nonzero_offset);

	gem_context_destroy(i915, child);
	gem_vm_destroy(i915, arg.value);

	gem_sync(i915, batch.handle);
	gem_close(i915, batch.handle);
}

igt_main
{
	struct drm_i915_gem_context_param arg;
	int fd;
	uint32_t ctx;

	memset(&arg, 0, sizeof(arg));

	igt_fixture {
		fd = drm_open_driver_render(DRIVER_INTEL);

		gem_require_contexts(fd);
		ctx = gem_context_create(fd);

		arg.param = I915_CONTEXT_PARAM_BAN_PERIOD;

		/* XXX start to enforce ban period returning -EINVAL when
		 * transition has been done */
		if (__gem_context_get_param(fd, &arg) == -EINVAL)
			arg.param = I915_CONTEXT_PARAM_BANNABLE;
	}

	igt_subtest("basic") {
		arg.ctx_id = ctx;
		gem_context_get_param(fd, &arg);
		gem_context_set_param(fd, &arg);
	}

	igt_subtest("basic-default") {
		arg.ctx_id = 0;
		gem_context_get_param(fd, &arg);
		gem_context_set_param(fd, &arg);
	}

	igt_subtest("invalid-ctx-get") {
		arg.ctx_id = 2;
		igt_assert_eq(__gem_context_get_param(fd, &arg), -ENOENT);
	}

	igt_subtest("invalid-ctx-set") {
		arg.ctx_id = ctx;
		gem_context_get_param(fd, &arg);
		arg.ctx_id = 2;
		igt_assert_eq(__gem_context_set_param(fd, &arg), -ENOENT);
	}

	igt_subtest("invalid-size-get") {
		arg.ctx_id = ctx;
		arg.size = 8;
		gem_context_get_param(fd, &arg);
		igt_assert(arg.size == 0);
	}

	igt_subtest("invalid-size-set") {
		arg.ctx_id = ctx;
		gem_context_get_param(fd, &arg);
		arg.size = 8;
		igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);
		arg.size = 0;
	}

	igt_subtest("non-root-set") {
		igt_fork(child, 1) {
			igt_drop_root();

			arg.ctx_id = ctx;
			gem_context_get_param(fd, &arg);
			arg.value--;
			igt_assert_eq(__gem_context_set_param(fd, &arg), -EPERM);
		}

		igt_waitchildren();
	}

	igt_subtest("root-set") {
		arg.ctx_id = ctx;
		gem_context_get_param(fd, &arg);
		arg.value--;
		gem_context_set_param(fd, &arg);
	}

	arg.param = I915_CONTEXT_PARAM_NO_ZEROMAP;

	igt_subtest("non-root-set-no-zeromap") {
		igt_fork(child, 1) {
			igt_drop_root();

			arg.ctx_id = ctx;
			gem_context_get_param(fd, &arg);
			arg.value--;
			gem_context_set_param(fd, &arg);
		}

		igt_waitchildren();
	}

	igt_subtest("root-set-no-zeromap-enabled") {
		arg.ctx_id = ctx;
		gem_context_get_param(fd, &arg);
		arg.value = 1;
		gem_context_set_param(fd, &arg);
	}

	igt_subtest("root-set-no-zeromap-disabled") {
		arg.ctx_id = ctx;
		gem_context_get_param(fd, &arg);
		arg.value = 0;
		gem_context_set_param(fd, &arg);
	}

	igt_subtest("vm")
		test_vm(fd);

	arg.param = I915_CONTEXT_PARAM_PRIORITY;

	igt_subtest("set-priority-not-supported") {
		igt_require(!gem_scheduler_has_ctx_priority(fd));

		arg.ctx_id = ctx;
		arg.size = 0;

		igt_assert_eq(__gem_context_set_param(fd, &arg), -ENODEV);
	}

	igt_subtest_group {
		igt_fixture {
			igt_require(gem_scheduler_has_ctx_priority(fd));
		}

		igt_subtest("get-priority-new-ctx") {
			struct drm_i915_gem_context_param local_arg = arg;
			uint32_t local_ctx = gem_context_create(fd);

			local_arg.ctx_id = local_ctx;

			gem_context_get_param(fd, &local_arg);
			igt_assert_eq(local_arg.value, I915_CONTEXT_DEFAULT_PRIORITY);

			gem_context_destroy(fd, local_ctx);
		}

		igt_subtest("set-priority-invalid-size") {
			struct drm_i915_gem_context_param local_arg = arg;
			local_arg.ctx_id = ctx;
			local_arg.value = 0;
			local_arg.size = ~0;

			igt_assert_eq(__gem_context_set_param(fd, &local_arg), -EINVAL);
		}

		igt_subtest("set-priority-range")
			set_priority(fd);
	}

	/* I915_CONTEXT_PARAM_SSEU tests are located in gem_ctx_sseu.c */

	arg.param = -1; /* Should be safely unused for a while */

	igt_subtest("invalid-param-get") {
		arg.ctx_id = ctx;
		igt_assert_eq(__gem_context_get_param(fd, &arg), -EINVAL);
	}

	igt_subtest("invalid-param-set") {
		arg.ctx_id = ctx;
		igt_assert_eq(__gem_context_set_param(fd, &arg), -EINVAL);
	}

	igt_fixture
		close(fd);
}
