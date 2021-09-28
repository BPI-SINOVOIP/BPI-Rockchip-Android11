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
 */

#include <limits.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "drmtest.h"
#include "igt_aux.h"
#include "igt_core.h"
#include "igt_gt.h"
#include "igt_sysfs.h"
#include "igt_debugfs.h"
#include "ioctl_wrappers.h"
#include "intel_reg.h"
#include "intel_chipset.h"
#include "igt_dummyload.h"
#include "i915/gem_engine_topology.h"

/**
 * SECTION:igt_gt
 * @short_description: GT support library
 * @title: GT
 * @include: igt.h
 *
 * This library provides various auxiliary helper functions to handle general
 * interactions with the GT like forcewake handling, injecting hangs or stopping
 * engines.
 */

static bool has_gpu_reset(int fd)
{
	static int once = -1;
	if (once < 0) {
		struct drm_i915_getparam gp;
		int val = 0;

		memset(&gp, 0, sizeof(gp));
		gp.param = 35; /* HAS_GPU_RESET */
		gp.value = &val;

		if (ioctl(fd, DRM_IOCTL_I915_GETPARAM, &gp))
			once = intel_gen(intel_get_drm_devid(fd)) >= 5;
		else
			once = val > 0;
	}
	return once;
}

static void eat_error_state(int dev)
{
	int dir;

	dir = igt_sysfs_open(dev);
	if (dir < 0)
		return;

	/* Any write to the error state clears it */
	igt_sysfs_set(dir, "error", "");
	close(dir);
}

/**
 * igt_require_hang_ring:
 * @fd: open i915 drm file descriptor
 * @ring: execbuf ring flag
 *
 * Convenience helper to check whether advanced hang injection is supported by
 * the kernel. Uses igt_skip to automatically skip the test/subtest if this
 * isn't the case.
 *
 * Note that we can't simply just call this from igt_hang_ring since some
 * tests want to exercise gpu wedging behavior. For which we intentionally
 * disable gpu reset support, but still want to inject a hang, see for example
 * tests/gem_eio.c Instead, we expect that the first invocation of
 * igt_require_hand_ring be from a vanilla context and use the has_gpu_reset()
 * determined then for all later instances. This allows us the convenience
 * of double checking when injecting hangs, whilst pushing the complexity
 * to the tests that are deliberating trying to break the box.
 *
 * This function is also controlled by the environment variables:
 *
 * IGT_HANG (boolean) - if false, skip all tests that try to inject a hang.
 * Default: true
 *
 * IGT_HANG_WITHOUT_RESET (boolean) - if true, allow the hang even if the
 * kernel does not support GPU recovery. The machine will be wedged afterwards
 * (and so require a reboot between testing), but it does allow limited testing
 * to be done under hang injection.
 * Default: false
 */
void igt_require_hang_ring(int fd, int ring)
{
	if (!igt_check_boolean_env_var("IGT_HANG", true))
		igt_skip("hang injection disabled by user");

	gem_require_ring(fd, ring);
	gem_context_require_bannable(fd);
	if (!igt_check_boolean_env_var("IGT_HANG_WITHOUT_RESET", false))
		igt_require(has_gpu_reset(fd));
}

static unsigned context_get_ban(int fd, unsigned ctx)
{
	struct drm_i915_gem_context_param param = {
		.ctx_id = ctx,
		.param = I915_CONTEXT_PARAM_BANNABLE,
	};

	if (__gem_context_get_param(fd, &param) == -EINVAL) {
		igt_assert(param.value == 0);
		param.param = I915_CONTEXT_PARAM_BAN_PERIOD;
		gem_context_get_param(fd, &param);
	}

	return param.value;
}

static void context_set_ban(int fd, unsigned ctx, unsigned ban)
{
	struct drm_i915_gem_context_param param = {
		.ctx_id = ctx,
		.param = I915_CONTEXT_PARAM_BANNABLE,
		.value = ban,
	};

	if(__gem_context_set_param(fd, &param) == -EINVAL) {
		igt_assert(param.value == ban);
		param.param = I915_CONTEXT_PARAM_BAN_PERIOD;
		gem_context_set_param(fd, &param);
	}
}

igt_hang_t igt_allow_hang(int fd, unsigned ctx, unsigned flags)
{
	struct drm_i915_gem_context_param param = {
		.ctx_id = ctx,
	};
	unsigned ban;

	/*
	 * If the driver is already wedged, we don't expect it to be able
	 * to recover from reset and for it to remain wedged. It's hard to
	 * say even if we do hang/reset making the test suspect.
	 */
	igt_require_gem(fd);

	if (!igt_check_boolean_env_var("IGT_HANG", true))
		igt_skip("hang injection disabled by user");
	gem_context_require_bannable(fd);
	if (!igt_check_boolean_env_var("IGT_HANG_WITHOUT_RESET", false))
		igt_require(has_gpu_reset(fd));

	igt_require(igt_sysfs_set_parameter
		    (fd, "reset", "%d", INT_MAX /* any reset method */));

	if ((flags & HANG_ALLOW_CAPTURE) == 0) {
		param.param = I915_CONTEXT_PARAM_NO_ERROR_CAPTURE;
		param.value = 1;
		/* Older kernels may not have NO_ERROR_CAPTURE, in which case
		 * we just eat the error state in post-hang (and hope we eat
		 * the right one).
		 */
		__gem_context_set_param(fd, &param);
	}

	ban = context_get_ban(fd, ctx);
	if ((flags & HANG_ALLOW_BAN) == 0)
		context_set_ban(fd, ctx, 0);

	return (struct igt_hang){ 0, ctx, ban, flags };
}

void igt_disallow_hang(int fd, igt_hang_t arg)
{
	context_set_ban(fd, arg.ctx, arg.ban);

	if ((arg.flags & HANG_ALLOW_CAPTURE) == 0) {
		struct drm_i915_gem_context_param param = {
			.ctx_id = arg.ctx,
			.param = I915_CONTEXT_PARAM_NO_ERROR_CAPTURE,
			.value = 0,
		};
		__gem_context_set_param(fd, &param);

		eat_error_state(fd);
	}
}

/**
 * has_ctx_exec:
 * @fd: open i915 drm file descriptor
 * @ring: execbuf ring flag
 * @ctx: Context to be checked
 *
 * This helper function checks if non default context submission is allowed
 * on a ring.
 *
 * Returns:
 * True if allowed
 *
 */
static bool has_ctx_exec(int fd, unsigned ring, uint32_t ctx)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 exec;

	/* silly ABI, the kernel thinks everyone who has BSD also has BSD2 */
	if ((ring & ~(3<<13)) == I915_EXEC_BSD) {
		if (ring & (3 << 13) && !gem_has_bsd2(fd))
			return false;
	}

	memset(&exec, 0, sizeof(exec));
	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&exec);
	execbuf.buffer_count = 1;
	execbuf.flags = ring;
	execbuf.rsvd1 = ctx;
	/*
	 * If context submission is not allowed, this will return EINVAL
	 * Otherwise, this will return ENOENT on account of no gem obj
	 * being submitted
	 */
	return __gem_execbuf(fd, &execbuf) == -ENOENT;
}

/**
 * igt_hang_ring_ctx:
 * @fd: open i915 drm file descriptor
 * @ctx: the contxt specifier
 * @ring: execbuf ring flag
 * @flags: set of flags to control execution
 * @offset: The resultant gtt offset of the exec obj
 *
 * This helper function injects a hanging batch associated with @ctx into @ring.
 * It returns a #igt_hang_t structure which must be passed to
 * igt_post_hang_ring() for hang post-processing (after the gpu hang
 * interaction has been tested.
 *
 * Returns:
 * Structure with helper internal state for igt_post_hang_ring().
 */
igt_hang_t igt_hang_ctx(int fd, uint32_t ctx, int ring, unsigned flags)
{
	struct drm_i915_gem_context_param param;
	igt_spin_t *spin;
	unsigned ban;

	igt_require_hang_ring(fd, ring);

	/* check if non-default ctx submission is allowed */
	igt_require(ctx == 0 || has_ctx_exec(fd, ring, ctx));

	param.ctx_id = ctx;
	param.size = 0;

	if ((flags & HANG_ALLOW_CAPTURE) == 0) {
		param.param = I915_CONTEXT_PARAM_NO_ERROR_CAPTURE;
		param.value = 1;
		/* Older kernels may not have NO_ERROR_CAPTURE, in which case
		 * we just eat the error state in post-hang (and hope we eat
		 * the right one).
		 */
		__gem_context_set_param(fd, &param);
	}

	ban = context_get_ban(fd, ctx);
	if ((flags & HANG_ALLOW_BAN) == 0)
		context_set_ban(fd, ctx, 0);

	spin = __igt_spin_new(fd,
			      .ctx = ctx,
			      .engine = ring,
			      .flags = IGT_SPIN_NO_PREEMPTION);

	return (igt_hang_t){ spin, ctx, ban, flags };
}

/**
 * igt_hang_ring:
 * @fd: open i915 drm file descriptor
 * @ring: execbuf ring flag
 *
 * This helper function injects a hanging batch into @ring. It returns a
 * #igt_hang_t structure which must be passed to igt_post_hang_ring() for
 * hang post-processing (after the gpu hang interaction has been tested.
 *
 * Returns:
 * Structure with helper internal state for igt_post_hang_ring().
 */
igt_hang_t igt_hang_ring(int fd, int ring)
{
	return igt_hang_ctx(fd, 0, ring, 0);
}

/**
 * igt_post_hang_ring:
 * @fd: open i915 drm file descriptor
 * @arg: hang state from igt_hang_ring()
 *
 * This function does the necessary post-processing after a gpu hang injected
 * with igt_hang_ring().
 */
void igt_post_hang_ring(int fd, igt_hang_t arg)
{
	if (!arg.spin)
		return;

	gem_sync(fd, arg.spin->handle); /* Wait until it hangs */
	igt_spin_free(fd, arg.spin);

	context_set_ban(fd, arg.ctx, arg.ban);

	if ((arg.flags & HANG_ALLOW_CAPTURE) == 0) {
		struct drm_i915_gem_context_param param = {
			.ctx_id = arg.ctx,
			.param = I915_CONTEXT_PARAM_NO_ERROR_CAPTURE,
			.value = 0,
		};
		__gem_context_set_param(fd, &param);

		eat_error_state(fd);
	}
}

/**
 * igt_force_gpu_reset:
 *
 * forces a gpu reset using the i915_wedged debugfs interface. To be used to
 * recover from situations where the hangcheck didn't trigger and/or the gpu is
 * stuck, either because the test manually disabled gpu resets or because the
 * test hit an hangcheck bug
 */
void igt_force_gpu_reset(int drm_fd)
{
	int dir, wedged;

	igt_debug("Triggering GPU reset\n");

	dir = igt_debugfs_dir(drm_fd);

	igt_sysfs_set(dir, "i915_wedged", "-1");
	igt_sysfs_scanf(dir, "i915_wedged", "%d", &wedged);

	close(dir);
	errno = 0;

	igt_assert(!wedged);
}

/* GPU abusers */
static struct igt_helper_process hang_helper;
static void __attribute__((noreturn))
hang_helper_process(pid_t pid, int fd)
{
	while (1) {
		if (kill(pid, 0)) /* Parent has died, so must we. */
			exit(0);

		igt_post_hang_ring(fd,
				   igt_hang_ring(fd, I915_EXEC_DEFAULT));

		sleep(1);
	}
}

/**
 * igt_fork_hang_helper:
 *
 * Fork a child process using #igt_fork_helper to hang the default engine
 * of the GPU at regular intervals.
 *
 * This is useful to exercise slow running code (such as aperture placement)
 * which needs to be robust against a GPU reset.
 *
 * This function automatically skips when test requirements aren't met using
 * igt_skip().
 */
void igt_fork_hang_helper(void)
{
	int fd, gen;

	fd = drm_open_driver(DRIVER_INTEL);

	gen = intel_gen(intel_get_drm_devid(fd));
	igt_skip_on(gen < 5);

	igt_fork_helper(&hang_helper)
		hang_helper_process(getppid(), fd);

	close(fd);
}

/**
 * igt_stop_hang_helper:
 *
 * Stops the child process spawned with igt_fork_hang_helper().
 *
 * In tests with subtests this function can be called outside of failure
 * catching code blocks like #igt_fixture or #igt_subtest.
 */
void igt_stop_hang_helper(void)
{
	if (hang_helper.running)
		igt_stop_helper(&hang_helper);
}

/**
 * igt_open_forcewake_handle:
 *
 * This functions opens the debugfs forcewake file and so prevents the GT from
 * suspending. The reference is automatically dropped when the is closed.
 *
 * Returns:
 * The file descriptor of the forcewake handle or -1 if that didn't work out.
 */
int igt_open_forcewake_handle(int fd)
{
	if (getenv("IGT_NO_FORCEWAKE"))
		return -1;
	return igt_debugfs_open(fd, "i915_forcewake_user", O_WRONLY);
}

#if defined(__x86_64__) || defined(__i386__)
static unsigned int clflush_size;

int igt_setup_clflush(void)
{
	FILE *file;
	char *line = NULL;
	size_t size = 0;
	int first_stanza = 1;
	int has_clflush = 0;

	if (clflush_size)
		return 1;

	file = fopen("/proc/cpuinfo", "r");
	if (file == NULL)
		return 0;

	while (getline(&line, &size, file) != -1) {
		if (strncmp(line, "processor", 9) == 0) {
			if (!first_stanza)
				break;
			first_stanza = 0;
		}

		if (strncmp(line, "flags", 5) == 0) {
			if (strstr(line, "clflush"))
				has_clflush = 1;
		}

		if (strncmp(line, "clflush size", 12) == 0) {
			char *colon = strchr(line, ':');
			if (colon)
				clflush_size = atoi(colon + 1);
		}
	}
	free(line);
	fclose(file);

	return has_clflush && clflush_size;
}

__attribute__((target("sse2")))
void igt_clflush_range(void *addr, int size)
{
	char *p, *end;

	end = (char *)addr + size;
	p = (char *)((uintptr_t)addr & ~((uintptr_t)clflush_size - 1));

	__builtin_ia32_mfence();
	for (; p < end; p += clflush_size)
		__builtin_ia32_clflush(p);
	__builtin_ia32_clflush(end - 1); /* magic serialisation for byt+ */
	__builtin_ia32_mfence();
}
#else
int igt_setup_clflush(void)
{
	/* requires mfence + clflush, both SSE2 instructions */
	return 0;
}

void igt_clflush_range(void *addr, int size)
{
	fprintf(stderr, "igt_clflush_range() unsupported\n");
}
#endif

/**
 * intel_detect_and_clear_missed_irq:
 * @fd: open i915 drm file descriptor, used to quiesce the gpu
 *
 * This functions idles the GPU and then queries whether there has
 * been a missed interrupt reported by the driver. Afterwards it
 * clears the missed interrupt flag, in order to disable the timer
 * fallback for the next test.
 */
unsigned intel_detect_and_clear_missed_interrupts(int fd)
{
	unsigned missed;
	int dir;

	gem_quiescent_gpu(fd);

	dir = igt_debugfs_dir(fd);

	missed = 0;
	igt_sysfs_scanf(dir, "i915_ring_missed_irq", "%x", &missed);
	if (missed)
		igt_sysfs_set(dir, "i915_ring_missed_irq", "0");

	close(dir);

	errno = 0;
	return missed;
}

const struct intel_execution_engine intel_execution_engines[] = {
	{ "default", NULL, 0, 0 },
	{ "render", "rcs0", I915_EXEC_RENDER, 0 },
	{ "bsd", "vcs0", I915_EXEC_BSD, 0 },
	{ "bsd1", "vcs0", I915_EXEC_BSD, 1<<13 /*I915_EXEC_BSD_RING1*/ },
	{ "bsd2", "vcs1", I915_EXEC_BSD, 2<<13 /*I915_EXEC_BSD_RING2*/ },
	{ "blt", "bcs0", I915_EXEC_BLT, 0 },
	{ "vebox", "vecs0", I915_EXEC_VEBOX, 0 },
	{ NULL, 0, 0 }
};

bool gem_class_can_store_dword(int fd, int class)
{
	uint16_t devid = intel_get_drm_devid(fd);
	const struct intel_device_info *info = intel_get_device_info(devid);
	const int gen = ffs(info->gen);

	if (gen <= 2) /* requires physical addresses */
		return false;

	if (gen == 3 && (info->is_grantsdale || info->is_alviso))
		return false; /* only supports physical addresses */

	if (gen == 6 && class == I915_ENGINE_CLASS_VIDEO)
		return false;

	if (info->is_broadwater)
		return false; /* Not sure yet... */

	return true;
}

bool gem_can_store_dword(int fd, unsigned int engine)
{
	return gem_class_can_store_dword(fd,
				gem_execbuf_flags_to_engine_class(engine));
}

const struct intel_execution_engine2 intel_execution_engines2[] = {
	{ "rcs0", I915_ENGINE_CLASS_RENDER, 0, I915_EXEC_RENDER },
	{ "bcs0", I915_ENGINE_CLASS_COPY, 0, I915_EXEC_BLT },
	{ "vcs0", I915_ENGINE_CLASS_VIDEO, 0, I915_EXEC_BSD | I915_EXEC_BSD_RING1 },
	{ "vcs1", I915_ENGINE_CLASS_VIDEO, 1, I915_EXEC_BSD | I915_EXEC_BSD_RING2 },
	{ "vcs2", I915_ENGINE_CLASS_VIDEO, 2, -1 },
	{ "vecs0", I915_ENGINE_CLASS_VIDEO_ENHANCE, 0, I915_EXEC_VEBOX },
	{ }
};

int gem_execbuf_flags_to_engine_class(unsigned int flags)
{
	switch (flags & 0x3f) {
	case I915_EXEC_DEFAULT:
	case I915_EXEC_RENDER:
		return I915_ENGINE_CLASS_RENDER;
	case I915_EXEC_BLT:
		return I915_ENGINE_CLASS_COPY;
	case I915_EXEC_BSD:
		return I915_ENGINE_CLASS_VIDEO;
	case I915_EXEC_VEBOX:
		return I915_ENGINE_CLASS_VIDEO_ENHANCE;
	default:
		igt_assert(0);
	}
}

bool gem_ring_is_physical_engine(int fd, unsigned ring)
{
	if (ring == I915_EXEC_DEFAULT)
		return false;

	/* BSD uses an extra flag to chose between aliasing modes */
	if ((ring & 63) == I915_EXEC_BSD) {
		bool explicit_bsd = ring & (3 << 13);
		bool has_bsd2 = gem_has_bsd2(fd);
		return explicit_bsd ? has_bsd2 : !has_bsd2;
	}

	return true;
}

bool gem_ring_has_physical_engine(int fd, unsigned ring)
{
	if (!gem_ring_is_physical_engine(fd, ring))
		return false;

	return gem_has_ring(fd, ring);
}
