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

#include <errno.h>
#include <string.h>

#include "ioctl_wrappers.h"
#include "drmtest.h"

#include "i915/gem_context.h"

/**
 * SECTION:gem_context
 * @short_description: Helpers for dealing with contexts
 * @title: GEM Context
 *
 * This helper library contains functions used for handling gem contexts.
 * Conceptually, gem contexts are similar to their CPU counterparts, in that
 * they are a mix of software and hardware features allowing to isolate some
 * aspects of task execution. Initially it was just a matter of maintaining
 * separate state for each context, but more features were added, some
 * improving contexts isolation (per-context address space), some are just
 * software features improving submission model (context priority).
 */

/**
 * gem_has_contexts:
 * @fd: open i915 drm file descriptor
 *
 * Queries whether context creation is supported or not.
 *
 * Returns: Context creation availability.
 */
bool gem_has_contexts(int fd)
{
	uint32_t ctx_id = 0;

	__gem_context_create(fd, &ctx_id);
	if (ctx_id)
		gem_context_destroy(fd, ctx_id);

	return ctx_id;
}

/**
 * gem_require_contexts:
 * @fd: open i915 drm file descriptor
 *
 * This helper will automatically skip the test on platforms where context
 * support is not available.
 */
void gem_require_contexts(int fd)
{
	igt_require(gem_has_contexts(fd));
}

int __gem_context_create(int fd, uint32_t *ctx_id)
{
       struct drm_i915_gem_context_create create;
       int err = 0;

       memset(&create, 0, sizeof(create));
       if (igt_ioctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_CREATE, &create) == 0) {
               *ctx_id = create.ctx_id;
       } else {
	       err = -errno;
	       igt_assume(err != 0);
       }

       errno = 0;
       return err;
}

/**
 * gem_context_create:
 * @fd: open i915 drm file descriptor
 *
 * This wraps the CONTEXT_CREATE ioctl, which is used to allocate a new
 * context. Note that similarly to gem_set_caching() this wrapper skips on
 * kernels and platforms where context support is not available.
 *
 * Returns: The id of the allocated context.
 */
uint32_t gem_context_create(int fd)
{
	uint32_t ctx_id;

	igt_assert_eq(__gem_context_create(fd, &ctx_id), 0);
	igt_assert(ctx_id != 0);

	return ctx_id;
}

int __gem_context_destroy(int fd, uint32_t ctx_id)
{
	struct drm_i915_gem_context_destroy destroy = { ctx_id };
	int err = 0;

	if (igt_ioctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_DESTROY, &destroy)) {
		err = -errno;
		igt_assume(err);
	}

	errno = 0;
	return err;
}

/**
 * gem_context_destroy:
 * @fd: open i915 drm file descriptor
 * @ctx_id: i915 context id
 *
 * This wraps the CONTEXT_DESTROY ioctl, which is used to free a context.
 */
void gem_context_destroy(int fd, uint32_t ctx_id)
{
	igt_assert_eq(__gem_context_destroy(fd, ctx_id), 0);
}

int __gem_context_get_param(int fd, struct drm_i915_gem_context_param *p)
{
	int err = 0;

	if (igt_ioctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM, p)) {
		err = -errno;
		igt_assume(err);
	}

	errno = 0;
	return err;
}

/**
 * gem_context_get_param:
 * @fd: open i915 drm file descriptor
 * @p: i915 context parameter
 *
 * This wraps the CONTEXT_GET_PARAM ioctl, which is used to get a context
 * parameter.
 */
void gem_context_get_param(int fd, struct drm_i915_gem_context_param *p)
{
	igt_assert_eq(__gem_context_get_param(fd, p), 0);
}

int __gem_context_set_param(int fd, struct drm_i915_gem_context_param *p)
{
	int err = 0;

	if (igt_ioctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM, p)) {
		err = -errno;
		igt_assume(err);
	}

	errno = 0;
	return err;
}

/**
 * gem_context_set_param:
 * @fd: open i915 drm file descriptor
 * @p: i915 context parameter
 *
 * This wraps the CONTEXT_SET_PARAM ioctl, which is used to set a context
 * parameter.
 */
void gem_context_set_param(int fd, struct drm_i915_gem_context_param *p)
{
	igt_assert_eq(__gem_context_set_param(fd, p), 0);
}

/**
 * gem_context_require_param:
 * @fd: open i915 drm file descriptor
 * @param: i915 context parameter
 *
 * Feature test macro to query whether context parameter support for @param
 * is available. Automatically skips through igt_require() if not.
 */
void gem_context_require_param(int fd, uint64_t param)
{
	struct drm_i915_gem_context_param p = { .param = param };

	igt_require(__gem_context_get_param(fd, &p) == 0);
}

void gem_context_require_bannable(int fd)
{
	static int has_ban_period = -1;
	static int has_bannable = -1;

	if (has_bannable < 0) {
		struct drm_i915_gem_context_param p;

		p.ctx_id = 0;
		p.param = I915_CONTEXT_PARAM_BANNABLE;
		p.value = 0;
		p.size = 0;

		has_bannable = igt_ioctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM, &p) == 0;
	}

	if (has_ban_period < 0) {
		struct drm_i915_gem_context_param p;

		p.ctx_id = 0;
		p.param = I915_CONTEXT_PARAM_BAN_PERIOD;
		p.value = 0;
		p.size = 0;

		has_ban_period = igt_ioctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM, &p) == 0;
	}

	igt_require(has_ban_period || has_bannable);
}

#define DRM_I915_CONTEXT_PARAM_PRIORITY 0x6

/**
 * __gem_context_set_priority:
 * @fd: open i915 drm file descriptor
 * @ctx_id: i915 context id
 * @prio: desired context priority
 *
 * This function modifies priority property of the context.
 * It is used by the scheduler to decide on the ordering of requests submitted
 * to the hardware.
 *
 * Returns: An integer equal to zero for success and negative for failure
 */
int __gem_context_set_priority(int fd, uint32_t ctx_id, int prio)
{
	struct drm_i915_gem_context_param p = {
		.ctx_id = ctx_id,
		.param = DRM_I915_CONTEXT_PARAM_PRIORITY,
		.value = prio,
	};

	return __gem_context_set_param(fd, &p);
}

/**
 * gem_context_set_priority:
 * @fd: open i915 drm file descriptor
 * @ctx_id: i915 context id
 * @prio: desired context priority
 *
 * Like __gem_context_set_priority(), except we assert on failure.
 */
void gem_context_set_priority(int fd, uint32_t ctx_id, int prio)
{
	igt_assert_eq(__gem_context_set_priority(fd, ctx_id, prio), 0);
}

int
__gem_context_clone(int i915,
		    uint32_t src, unsigned int share,
		    unsigned int flags,
		    uint32_t *out)
{
	struct drm_i915_gem_context_create_ext_clone clone = {
		{ .name = I915_CONTEXT_CREATE_EXT_CLONE },
		.clone_id = src,
		.flags = share,
	};
	struct drm_i915_gem_context_create_ext arg = {
		.flags = flags | I915_CONTEXT_CREATE_FLAGS_USE_EXTENSIONS,
		.extensions = to_user_pointer(&clone),
	};
	int err = 0;

	if (igt_ioctl(i915, DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT, &arg)) {
		err = -errno;
		igt_assume(err);
	}

	*out = arg.ctx_id;

	errno = 0;
	return err;
}

static bool __gem_context_has(int i915, uint32_t share, unsigned int flags)
{
	uint32_t ctx;

	__gem_context_clone(i915, 0, share, flags, &ctx);
	if (ctx)
		gem_context_destroy(i915, ctx);

	errno = 0;
	return ctx;
}

bool gem_contexts_has_shared_gtt(int i915)
{
	return __gem_context_has(i915, I915_CONTEXT_CLONE_VM, 0);
}

bool gem_has_queues(int i915)
{
	return __gem_context_has(i915,
				 I915_CONTEXT_CLONE_VM,
				 I915_CONTEXT_CREATE_FLAGS_SINGLE_TIMELINE);
}

uint32_t gem_context_clone(int i915,
			   uint32_t src, unsigned int share,
			   unsigned int flags)
{
	uint32_t ctx;

	igt_assert_eq(__gem_context_clone(i915, src, share, flags, &ctx), 0);

	return ctx;
}

uint32_t gem_queue_create(int i915)
{
	return gem_context_clone(i915, 0,
				 I915_CONTEXT_CLONE_VM,
				 I915_CONTEXT_CREATE_FLAGS_SINGLE_TIMELINE);
}

bool gem_context_has_engine(int fd, uint32_t ctx, uint64_t engine)
{
	struct drm_i915_gem_exec_object2 exec = {};
	struct drm_i915_gem_execbuffer2 execbuf = {
		.buffers_ptr = to_user_pointer(&exec),
		.buffer_count = 1,
		.flags = engine,
		.rsvd1 = ctx,
	};

	/*
	 * 'engine' value can either store an execbuf engine selector
	 * or a context map index; for the latter case we do not expect
	 * to have any value at bit 13 and 14 (BSD1/2 selector),
	 * therefore, we assume that the following check is safe and it
	 * wouldn't produce any result.
	 */
	if ((engine & ~(3<<13)) == I915_EXEC_BSD) {
		if (engine & (2 << 13) && !gem_has_bsd2(fd))
			return false;
	}

	return __gem_execbuf(fd, &execbuf) == -ENOENT;
}
