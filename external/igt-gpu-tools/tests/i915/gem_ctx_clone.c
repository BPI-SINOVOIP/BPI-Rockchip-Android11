/*
 * Copyright © 2019 Intel Corporation
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
#include "igt_gt.h"
#include "i915/gem_vm.h"
#include "i915_drm.h"

static int ctx_create_ioctl(int i915, struct drm_i915_gem_context_create_ext *arg)
{
	int err;

	err = 0;
	if (igt_ioctl(i915, DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT, arg)) {
		err = -errno;
		igt_assume(err);
	}

	errno = 0;
	return err;
}

static bool has_ctx_clone(int i915)
{
	struct drm_i915_gem_context_create_ext_clone ext = {
		{ .name = I915_CONTEXT_CREATE_EXT_CLONE },
		.clone_id = -1,
	};
	struct drm_i915_gem_context_create_ext create = {
		.flags = I915_CONTEXT_CREATE_FLAGS_USE_EXTENSIONS,
		.extensions = to_user_pointer(&ext),
	};
	return ctx_create_ioctl(i915, &create) == -ENOENT;
}

static void invalid_clone(int i915)
{
	struct drm_i915_gem_context_create_ext_clone ext = {
		{ .name = I915_CONTEXT_CREATE_EXT_CLONE },
	};
	struct drm_i915_gem_context_create_ext create = {
		.flags = I915_CONTEXT_CREATE_FLAGS_USE_EXTENSIONS,
		.extensions = to_user_pointer(&ext),
	};

	igt_assert_eq(ctx_create_ioctl(i915, &create), 0);
	gem_context_destroy(i915, create.ctx_id);

	ext.flags = -1; /* Hopefully we won't run out of flags */
	igt_assert_eq(ctx_create_ioctl(i915, &create), -EINVAL);
	ext.flags = 0;

	ext.base.next_extension = -1;
	igt_assert_eq(ctx_create_ioctl(i915, &create), -EFAULT);
	ext.base.next_extension = to_user_pointer(&ext);
	igt_assert_eq(ctx_create_ioctl(i915, &create), -E2BIG);
	ext.base.next_extension = 0;

	ext.clone_id = -1;
	igt_assert_eq(ctx_create_ioctl(i915, &create), -ENOENT);
	ext.clone_id = 0;
}

static void clone_flags(int i915)
{
	struct drm_i915_gem_context_create_ext_setparam set = {
		{ .name = I915_CONTEXT_CREATE_EXT_SETPARAM },
		{ .param = I915_CONTEXT_PARAM_RECOVERABLE },
	};
	struct drm_i915_gem_context_create_ext_clone ext = {
		{ .name = I915_CONTEXT_CREATE_EXT_CLONE },
		.flags = I915_CONTEXT_CLONE_FLAGS,
	};
	struct drm_i915_gem_context_create_ext create = {
		.flags = I915_CONTEXT_CREATE_FLAGS_USE_EXTENSIONS,
		.extensions = to_user_pointer(&ext),
	};
	int expected;

	set.param.value = 1; /* default is recoverable */
	igt_require(__gem_context_set_param(i915, &set.param) == 0);

	for (int pass = 0; pass < 2; pass++) { /* cloning default, then child */
		igt_debug("Cloning %d\n", ext.clone_id);
		igt_assert_eq(ctx_create_ioctl(i915, &create), 0);

		set.param.ctx_id = ext.clone_id;
		gem_context_get_param(i915, &set.param);
		expected = set.param.value;

		set.param.ctx_id = create.ctx_id;
		gem_context_get_param(i915, &set.param);

		igt_assert_eq_u64(set.param.param,
				  I915_CONTEXT_PARAM_RECOVERABLE);
		igt_assert_eq((int)set.param.value, expected);

		gem_context_destroy(i915, create.ctx_id);

		expected = set.param.value = 0;
		set.param.ctx_id = ext.clone_id;
		gem_context_set_param(i915, &set.param);

		igt_assert_eq(ctx_create_ioctl(i915, &create), 0);

		set.param.ctx_id = create.ctx_id;
		gem_context_get_param(i915, &set.param);

		igt_assert_eq_u64(set.param.param,
				  I915_CONTEXT_PARAM_RECOVERABLE);
		igt_assert_eq((int)set.param.value, expected);

		gem_context_destroy(i915, create.ctx_id);

		/* clone but then reset priority to default... */
		set.param.ctx_id = 0;
		set.param.value = 1;
		ext.base.next_extension = to_user_pointer(&set);
		igt_assert_eq(ctx_create_ioctl(i915, &create), 0);
		ext.base.next_extension = 0;

		/* then new context should have updated priority... */
		set.param.ctx_id = create.ctx_id;
		gem_context_get_param(i915, &set.param);
		igt_assert_eq_u64(set.param.value, 1);

		/* but original context should have default priority */
		set.param.ctx_id = ext.clone_id;
		gem_context_get_param(i915, &set.param);
		igt_assert_eq_u64(set.param.value, 0);

		gem_context_destroy(i915, create.ctx_id);
		ext.clone_id = gem_context_create(i915);
	}

	gem_context_destroy(i915, ext.clone_id);
}

static void clone_engines(int i915)
{
	struct drm_i915_gem_context_create_ext_setparam set = {
		{ .name = I915_CONTEXT_CREATE_EXT_SETPARAM },
		{ .param = I915_CONTEXT_PARAM_ENGINES },
	};
	struct drm_i915_gem_context_create_ext_clone ext = {
		{ .name = I915_CONTEXT_CREATE_EXT_CLONE },
		.flags = I915_CONTEXT_CLONE_ENGINES,
	};
	struct drm_i915_gem_context_create_ext create = {
		.flags = I915_CONTEXT_CREATE_FLAGS_USE_EXTENSIONS,
		.extensions = to_user_pointer(&ext),
	};
	I915_DEFINE_CONTEXT_PARAM_ENGINES(expected, 64);
	I915_DEFINE_CONTEXT_PARAM_ENGINES(engines, 64);
	uint64_t ex_size;

	memset(&expected, 0, sizeof(expected));
	memset(&engines, 0, sizeof(engines));

	igt_require(__gem_context_set_param(i915, &set.param) == 0);

	for (int pass = 0; pass < 2; pass++) { /* cloning default, then child */
		igt_debug("Cloning %d\n", ext.clone_id);
		igt_assert_eq(ctx_create_ioctl(i915, &create), 0);

		/* Check that we cloned the engine map */
		set.param.ctx_id = ext.clone_id;
		set.param.size = sizeof(expected);
		set.param.value = to_user_pointer(&expected);
		gem_context_get_param(i915, &set.param);
		ex_size = set.param.size;

		set.param.ctx_id = create.ctx_id;
		set.param.size = sizeof(engines);
		set.param.value = to_user_pointer(&engines);
		gem_context_get_param(i915, &set.param);

		igt_assert_eq_u64(set.param.param, I915_CONTEXT_PARAM_ENGINES);
		igt_assert_eq_u64(set.param.size, ex_size);
		igt_assert(!memcmp(&engines, &expected, ex_size));

		gem_context_destroy(i915, create.ctx_id);

		/* Check that the clone will replace an earlier set */
		expected.engines[0].engine_class =
			I915_ENGINE_CLASS_INVALID;
		expected.engines[0].engine_instance =
			I915_ENGINE_CLASS_INVALID_NONE;
		ex_size = (sizeof(struct i915_context_param_engines) +
			   sizeof(expected.engines[0]));

		set.param.ctx_id = ext.clone_id;
		set.param.size = ex_size;
		set.param.value = to_user_pointer(&expected);
		gem_context_set_param(i915, &set.param);

		igt_assert_eq(ctx_create_ioctl(i915, &create), 0);

		set.param.ctx_id = create.ctx_id;
		set.param.size = sizeof(engines);
		set.param.value = to_user_pointer(&engines);
		gem_context_get_param(i915, &set.param);

		igt_assert_eq_u64(set.param.size, ex_size);
		igt_assert(!memcmp(&engines, &expected, ex_size));

		gem_context_destroy(i915, create.ctx_id);

		/* clone but then reset engines to default */
		set.param.ctx_id = 0;
		set.param.size = 0;
		set.param.value = 0;
		ext.base.next_extension = to_user_pointer(&set);

		igt_assert_eq(ctx_create_ioctl(i915, &create), 0);
		ext.base.next_extension = 0;

		set.param.ctx_id = create.ctx_id;
		set.param.size = sizeof(engines);
		set.param.value = to_user_pointer(&engines);
		gem_context_get_param(i915, &set.param);
		igt_assert_eq_u64(set.param.size, 0);

		gem_context_destroy(i915, create.ctx_id);

		/* And check we ignore the flag */
		ext.flags = 0;
		igt_assert_eq(ctx_create_ioctl(i915, &create), 0);
		ext.flags = I915_CONTEXT_CLONE_ENGINES;

		set.param.ctx_id = create.ctx_id;
		set.param.size = sizeof(engines);
		set.param.value = to_user_pointer(&engines);
		gem_context_get_param(i915, &set.param);
		igt_assert_eq_u64(set.param.size, 0);

		ext.clone_id = gem_context_create(i915);
	}

	gem_context_destroy(i915, ext.clone_id);
}

static void clone_scheduler(int i915)
{
	struct drm_i915_gem_context_create_ext_setparam set = {
		{ .name = I915_CONTEXT_CREATE_EXT_SETPARAM },
		{ .param = I915_CONTEXT_PARAM_PRIORITY },
	};
	struct drm_i915_gem_context_create_ext_clone ext = {
		{ .name = I915_CONTEXT_CREATE_EXT_CLONE },
		.flags = I915_CONTEXT_CLONE_SCHEDATTR,
	};
	struct drm_i915_gem_context_create_ext create = {
		.flags = I915_CONTEXT_CREATE_FLAGS_USE_EXTENSIONS,
		.extensions = to_user_pointer(&ext),
	};
	int expected;

	igt_require(__gem_context_set_param(i915, &set.param) == 0);

	for (int pass = 0; pass < 2; pass++) { /* cloning default, then child */
		igt_debug("Cloning %d\n", ext.clone_id);
		igt_assert_eq(ctx_create_ioctl(i915, &create), 0);

		set.param.ctx_id = ext.clone_id;
		gem_context_get_param(i915, &set.param);
		expected = set.param.value;

		set.param.ctx_id = create.ctx_id;
		gem_context_get_param(i915, &set.param);

		igt_assert_eq_u64(set.param.param, I915_CONTEXT_PARAM_PRIORITY);
		igt_assert_eq((int)set.param.value, expected);

		gem_context_destroy(i915, create.ctx_id);

		expected = set.param.value = 1;
		set.param.ctx_id = ext.clone_id;
		gem_context_set_param(i915, &set.param);

		igt_assert_eq(ctx_create_ioctl(i915, &create), 0);

		set.param.ctx_id = create.ctx_id;
		gem_context_get_param(i915, &set.param);

		igt_assert_eq_u64(set.param.param, I915_CONTEXT_PARAM_PRIORITY);
		igt_assert_eq((int)set.param.value, expected);

		gem_context_destroy(i915, create.ctx_id);

		/* clone but then reset priority to default */
		set.param.ctx_id = 0;
		set.param.value = 0;
		ext.base.next_extension = to_user_pointer(&set);
		igt_assert_eq(ctx_create_ioctl(i915, &create), 0);
		ext.base.next_extension = 0;

		set.param.ctx_id = create.ctx_id;
		gem_context_get_param(i915, &set.param);
		igt_assert_eq_u64(set.param.value, 0);

		set.param.ctx_id = ext.clone_id;
		gem_context_get_param(i915, &set.param);
		igt_assert_eq_u64(set.param.value, 1);

		gem_context_destroy(i915, create.ctx_id);
		ext.clone_id = gem_context_create(i915);
	}

	gem_context_destroy(i915, ext.clone_id);
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

static void check_same_vm(int i915, uint32_t ctx_a, uint32_t ctx_b)
{
	struct drm_i915_gem_exec_object2 batch = {
		.handle = batch_create(i915),
	};
	struct drm_i915_gem_execbuffer2 eb = {
		.buffers_ptr = to_user_pointer(&batch),
		.buffer_count = 1,
	};

	/* First verify that we try to use "softpinning" by default */
	batch.offset = 48 << 20;
	eb.rsvd1 = ctx_a;
	gem_execbuf(i915, &eb);
	igt_assert_eq_u64(batch.offset, 48 << 20);

	/* An already active VMA will try to keep its offset */
	batch.offset = 0;
	eb.rsvd1 = ctx_b;
	gem_execbuf(i915, &eb);
	igt_assert_eq_u64(batch.offset, 48 << 20);

	gem_sync(i915, batch.handle);
	gem_close(i915, batch.handle);

	gem_quiescent_gpu(i915); /* evict the vma */
}

static void clone_vm(int i915)
{
	struct drm_i915_gem_context_param set = {
		.param = I915_CONTEXT_PARAM_VM,
	};
	struct drm_i915_gem_context_create_ext_clone ext = {
		{ .name = I915_CONTEXT_CREATE_EXT_CLONE },
		.flags = I915_CONTEXT_CLONE_VM,
	};
	struct drm_i915_gem_context_create_ext create = {
		.flags = I915_CONTEXT_CREATE_FLAGS_USE_EXTENSIONS,
		.extensions = to_user_pointer(&ext),
	};
	uint32_t vm_id[2];

	igt_require(__gem_context_set_param(i915, &set) == -ENOENT);

	/* Scrub the VM for our tests */
	i915 = gem_reopen_driver(i915);

	set.ctx_id = gem_context_create(i915);
	gem_context_get_param(i915, &set);
	vm_id[0] = set.value;
	gem_context_destroy(i915, set.ctx_id);

	vm_id[1] = gem_vm_create(i915);

	for (int pass = 0; pass < 2; pass++) { /* cloning default, then child */
		igt_debug("Cloning %d\n", ext.clone_id);

		igt_assert_eq(ctx_create_ioctl(i915, &create), 0);
		check_same_vm(i915, ext.clone_id, create.ctx_id);
		gem_context_destroy(i915, create.ctx_id);

		set.value = vm_id[pass];
		set.ctx_id = ext.clone_id;
		gem_context_set_param(i915, &set);

		igt_assert_eq(ctx_create_ioctl(i915, &create), 0);
		check_same_vm(i915, ext.clone_id, create.ctx_id);
		gem_context_destroy(i915, create.ctx_id);

		ext.clone_id = gem_context_create(i915);
	}

	gem_context_destroy(i915, ext.clone_id);

	for (int i = 0; i < ARRAY_SIZE(vm_id); i++)
		gem_vm_destroy(i915, vm_id[i]);

	close(i915);
}

igt_main
{
	int i915 = -1;

	igt_fixture {
		i915 = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(i915);
		gem_require_contexts(i915);

		igt_require(has_ctx_clone(i915));
		igt_fork_hang_detector(i915);
	}

	igt_subtest("invalid")
		invalid_clone(i915);

	igt_subtest("engines")
		clone_engines(i915);

	igt_subtest("flags")
		clone_flags(i915);

	igt_subtest("scheduler")
		clone_scheduler(i915);

	igt_subtest("vm")
		clone_vm(i915);

	igt_fixture {
		igt_stop_hang_detector();
		close(i915);
	}
}
