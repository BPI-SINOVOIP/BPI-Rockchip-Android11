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
#include "igt_dummyload.h"
#include "i915/gem_vm.h"

static int vm_create_ioctl(int i915, struct drm_i915_gem_vm_control *ctl)
{
	int err = 0;
	if (igt_ioctl(i915, DRM_IOCTL_I915_GEM_VM_CREATE, ctl)) {
		err = -errno;
		igt_assume(err);
	}
	errno = 0;
	return err;
}

static int vm_destroy_ioctl(int i915, struct drm_i915_gem_vm_control *ctl)
{
	int err = 0;
	if (igt_ioctl(i915, DRM_IOCTL_I915_GEM_VM_DESTROY, ctl)) {
		err = -errno;
		igt_assume(err);
	}
	errno = 0;
	return err;
}

static int ctx_create_ioctl(int i915,
			    struct drm_i915_gem_context_create_ext *arg)
{
	int err = 0;
	if (igt_ioctl(i915, DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT, arg)) {
		err = -errno;
		igt_assume(err);
	}
	errno = 0;
	return err;
}

static bool has_vm(int i915)
{
	struct drm_i915_gem_vm_control ctl = {};
	int err;

	err = vm_create_ioctl(i915, &ctl);
	switch (err) {
	case -EINVAL: /* unknown ioctl */
	case -ENODEV: /* !full-ppgtt */
		return false;

	case 0:
		gem_vm_destroy(i915, ctl.vm_id);
		return true;

	default:
		igt_fail_on_f(err, "Unknown response from VM_CREATE\n");
		return false;
	}
}

static void invalid_create(int i915)
{
	struct drm_i915_gem_vm_control ctl = {};
	struct i915_user_extension ext = { .name = -1 };

	igt_assert_eq(vm_create_ioctl(i915, &ctl), 0);
	gem_vm_destroy(i915, ctl.vm_id);

	ctl.vm_id = 0xdeadbeef;
	igt_assert_eq(vm_create_ioctl(i915, &ctl), 0);
	gem_vm_destroy(i915, ctl.vm_id);
	ctl.vm_id = 0;

	ctl.flags = -1;
	igt_assert_eq(vm_create_ioctl(i915, &ctl), -EINVAL);
	ctl.flags = 0;

	ctl.extensions = -1;
	igt_assert_eq(vm_create_ioctl(i915, &ctl), -EFAULT);
	ctl.extensions = to_user_pointer(&ext);
	igt_assert_eq(vm_create_ioctl(i915, &ctl), -EINVAL);
	ctl.extensions = 0;
}

static void invalid_destroy(int i915)
{
	struct drm_i915_gem_vm_control ctl = {};

	igt_assert_eq(vm_destroy_ioctl(i915, &ctl), -ENOENT);

	igt_assert_eq(vm_create_ioctl(i915, &ctl), 0);
	igt_assert_eq(vm_destroy_ioctl(i915, &ctl), 0);
	igt_assert_eq(vm_destroy_ioctl(i915, &ctl), -ENOENT);

	igt_assert_eq(vm_create_ioctl(i915, &ctl), 0);
	ctl.vm_id = ctl.vm_id + 1; /* assumes no one else allocated */
	igt_assert_eq(vm_destroy_ioctl(i915, &ctl), -ENOENT);
	ctl.vm_id = ctl.vm_id - 1;
	igt_assert_eq(vm_destroy_ioctl(i915, &ctl), 0);

	igt_assert_eq(vm_create_ioctl(i915, &ctl), 0);
	ctl.flags = -1;
	igt_assert_eq(vm_destroy_ioctl(i915, &ctl), -EINVAL);
	ctl.flags = 0;
	igt_assert_eq(vm_destroy_ioctl(i915, &ctl), 0);

	igt_assert_eq(vm_create_ioctl(i915, &ctl), 0);
	ctl.extensions = -1;
	igt_assert_eq(vm_destroy_ioctl(i915, &ctl), -EINVAL);
	ctl.extensions = 0;
	igt_assert_eq(vm_destroy_ioctl(i915, &ctl), 0);
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
}

static void create_ext(int i915)
{
	struct drm_i915_gem_context_create_ext_setparam ext = {
		{ .name = I915_CONTEXT_CREATE_EXT_SETPARAM },
		{ .param = I915_CONTEXT_PARAM_VM }
	};
	struct drm_i915_gem_context_create_ext create = {
		.flags = I915_CONTEXT_CREATE_FLAGS_USE_EXTENSIONS
	};
	uint32_t ctx[2];

	igt_require(ctx_create_ioctl(i915, &create) == 0);
	gem_context_destroy(i915, create.ctx_id);

	create.extensions = to_user_pointer(&ext);

	ext.param.value = gem_vm_create(i915);

	igt_assert_eq(ctx_create_ioctl(i915, &create), 0);
	ctx[0] = create.ctx_id;

	igt_assert_eq(ctx_create_ioctl(i915, &create), 0);
	ctx[1] = create.ctx_id;

	gem_vm_destroy(i915, ext.param.value);

	check_same_vm(i915, ctx[0], ctx[1]);

	gem_context_destroy(i915, ctx[1]);
	gem_context_destroy(i915, ctx[0]);
}

static void execbuf(int i915)
{
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

	/* First verify that we try to use "softpinning" by default */
	batch.offset = 48 << 20;
	gem_execbuf(i915, &eb);
	igt_assert_eq_u64(batch.offset, 48 << 20);

	arg.value = gem_vm_create(i915);
	gem_context_set_param(i915, &arg);
	gem_execbuf(i915, &eb);
	igt_assert_eq_u64(batch.offset, 48 << 20);
	gem_vm_destroy(i915, arg.value);

	arg.value = gem_vm_create(i915);
	gem_context_set_param(i915, &arg);
	batch.offset = 0;
	gem_execbuf(i915, &eb);
	igt_assert_eq_u64(batch.offset, 0);
	gem_vm_destroy(i915, arg.value);

	gem_sync(i915, batch.handle);
	gem_close(i915, batch.handle);
}

static void
write_to_address(int fd, uint32_t ctx, uint64_t addr, uint32_t value)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	struct drm_i915_gem_exec_object2 batch = {
		.handle = gem_create(fd, 4096)
	};
	struct drm_i915_gem_execbuffer2 eb = {
		.buffers_ptr = to_user_pointer(&batch),
		.buffer_count = 1,
		.rsvd1 = ctx,
	};
	uint32_t cs[16];
	int i;

	i = 0;
	cs[i] = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
	if (gen >= 8) {
		cs[++i] = addr;
		cs[++i] = addr >> 32;
	} else if (gen >= 4) {
		cs[++i] = 0;
		cs[++i] = addr;
	} else {
		cs[i]--;
		cs[++i] = addr;
	}
	cs[++i] = value;
	cs[++i] = MI_BATCH_BUFFER_END;
	gem_write(fd, batch.handle, 0, cs, sizeof(cs));

	gem_execbuf(fd, &eb);
	igt_assert(batch.offset != addr);

	gem_sync(fd, batch.handle);
	gem_close(fd, batch.handle);
}

static void isolation(int i915)
{
	struct drm_i915_gem_exec_object2 obj[2] = {
		{
			.handle = gem_create(i915, 4096),
			.offset = 1 << 20
		},
		{ .handle = batch_create(i915), }
	};
	struct drm_i915_gem_execbuffer2 eb = {
		.buffers_ptr = to_user_pointer(obj),
		.buffer_count = 2,
	};
	struct drm_i915_gem_context_param arg = {
		.param = I915_CONTEXT_PARAM_VM,
	};
	int other = gem_reopen_driver(i915);
	uint32_t ctx[2], vm[2], result;
	int loops = 4096;

	/* An vm_id on one fd is not the same on another fd */
	igt_assert_neq(i915, other);

	ctx[0] = gem_context_create(i915);
	ctx[1] = gem_context_create(other);

	vm[0] = gem_vm_create(i915);
	do {
		vm[1] = gem_vm_create(other);
	} while (vm[1] != vm[0] && loops-- > 0);
	igt_assert(loops);

	arg.ctx_id = ctx[0];
	arg.value = vm[0];
	gem_context_set_param(i915, &arg);

	arg.ctx_id = ctx[1];
	arg.value = vm[1];
	gem_context_set_param(other, &arg);

	eb.rsvd1 = ctx[0];
	gem_execbuf(i915, &eb); /* bind object into vm[0] */

	/* Verify the trick with the assumed target address works */
	write_to_address(i915, ctx[0], obj[0].offset, 1);
	gem_read(i915, obj[0].handle, 0, &result, sizeof(result));
	igt_assert_eq(result, 1);

	/* Now check that we can't write to vm[0] from second fd/vm */
	write_to_address(other, ctx[1], obj[0].offset, 2);
	gem_read(i915, obj[0].handle, 0, &result, sizeof(result));
	igt_assert_eq(result, 1);

	close(other);

	gem_close(i915, obj[1].handle);
	gem_close(i915, obj[0].handle);

	gem_context_destroy(i915, ctx[0]);
	gem_vm_destroy(i915, vm[0]);
}

static void async_destroy(int i915)
{
	struct drm_i915_gem_context_param arg = {
		.ctx_id = gem_context_create(i915),
		.value = gem_vm_create(i915),
		.param = I915_CONTEXT_PARAM_VM,
	};
	igt_spin_t *spin[2];

	spin[0] = igt_spin_new(i915,
			       .ctx = arg.ctx_id,
			       .flags = IGT_SPIN_POLL_RUN);
	igt_spin_busywait_until_started(spin[0]);

	gem_context_set_param(i915, &arg);
	spin[1] = __igt_spin_new(i915, .ctx = arg.ctx_id);

	igt_spin_end(spin[0]);
	gem_sync(i915, spin[0]->handle);

	gem_vm_destroy(i915, arg.value);
	gem_context_destroy(i915, arg.ctx_id);

	igt_spin_end(spin[1]);
	gem_sync(i915, spin[1]->handle);

	for (int i = 0; i < ARRAY_SIZE(spin); i++)
		igt_spin_free(i915, spin[i]);
}

igt_main
{
	int i915 = -1;

	igt_fixture {
		i915 = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(i915);
		igt_require(has_vm(i915));
	}

	igt_subtest("invalid-create")
		invalid_create(i915);

	igt_subtest("invalid-destroy")
		invalid_destroy(i915);

	igt_subtest_group {
		igt_fixture {
			gem_context_require_param(i915, I915_CONTEXT_PARAM_VM);
		}

		igt_subtest("execbuf")
			execbuf(i915);

		igt_subtest("isolation")
			isolation(i915);

		igt_subtest("create-ext")
			create_ext(i915);

		igt_subtest("async-destroy")
			async_destroy(i915);
	}

	igt_fixture {
		close(i915);
	}
}
