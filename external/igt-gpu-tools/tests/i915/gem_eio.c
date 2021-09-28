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
 */

/*
 * Testcase: Test that only specific ioctl report a wedged GPU.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <time.h>

#include <drm.h>

#include "igt.h"
#include "igt_device.h"
#include "igt_stats.h"
#include "igt_sysfs.h"
#include "sw_sync.h"
#include "i915/gem_ring.h"

IGT_TEST_DESCRIPTION("Test that specific ioctls report a wedged GPU (EIO).");

static bool i915_reset_control(bool enable)
{
	const char *path = "/sys/module/i915/parameters/reset";
	int fd, ret;

	igt_debug("%s GPU reset\n", enable ? "Enabling" : "Disabling");

	fd = open(path, O_RDWR);
	igt_require(fd >= 0);

	ret = write(fd, &"01"[enable], 1) == 1;
	close(fd);

	return ret;
}

static void trigger_reset(int fd)
{
	struct timespec ts = { };

	igt_nsec_elapsed(&ts);

	igt_kmsg(KMSG_DEBUG "Forcing GPU reset\n");
	igt_force_gpu_reset(fd);

	/* And just check the gpu is indeed running again */
	igt_kmsg(KMSG_DEBUG "Checking that the GPU recovered\n");
	gem_test_engine(fd, ALL_ENGINES);
	igt_drop_caches_set(fd, DROP_ACTIVE);

	/* We expect forced reset and health check to be quick. */
	igt_assert(igt_seconds_elapsed(&ts) < 2);
}

static void manual_hang(int drm_fd)
{
	int dir = igt_debugfs_dir(drm_fd);

	igt_sysfs_set(dir, "i915_wedged", "-1");

	close(dir);
}

static void wedge_gpu(int fd)
{
	/* First idle the GPU then disable GPU resets before injecting a hang */
	gem_quiescent_gpu(fd);

	igt_require(i915_reset_control(false));
	manual_hang(fd);
	igt_assert(i915_reset_control(true));
}

static int __gem_throttle(int fd)
{
	int err = 0;
	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_THROTTLE, NULL))
		err = -errno;
	return err;
}

static void test_throttle(int fd)
{
	wedge_gpu(fd);

	igt_assert_eq(__gem_throttle(fd), -EIO);

	trigger_reset(fd);
}

static void test_context_create(int fd)
{
	uint32_t ctx;

	gem_require_contexts(fd);

	wedge_gpu(fd);

	igt_assert_eq(__gem_context_create(fd, &ctx), -EIO);

	trigger_reset(fd);
}

static void test_execbuf(int fd)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 exec;
	uint32_t tmp[] = { MI_BATCH_BUFFER_END };

	memset(&exec, 0, sizeof(exec));
	memset(&execbuf, 0, sizeof(execbuf));

	exec.handle = gem_create(fd, 4096);
	gem_write(fd, exec.handle, 0, tmp, sizeof(tmp));

	execbuf.buffers_ptr = to_user_pointer(&exec);
	execbuf.buffer_count = 1;

	wedge_gpu(fd);

	igt_assert_eq(__gem_execbuf(fd, &execbuf), -EIO);
	gem_close(fd, exec.handle);

	trigger_reset(fd);
}

static int __gem_wait(int fd, uint32_t handle, int64_t timeout)
{
	struct drm_i915_gem_wait wait = {
		.bo_handle = handle,
		.timeout_ns = timeout,
	};
	int err;

	err = 0;
	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_WAIT, &wait))
		err = -errno;

	errno = 0;
	return err;
}

static igt_spin_t * __spin_poll(int fd, uint32_t ctx, unsigned long flags)
{
	struct igt_spin_factory opts = {
		.ctx = ctx,
		.engine = flags,
		.flags = IGT_SPIN_FAST | IGT_SPIN_FENCE_OUT,
	};

	if (gem_can_store_dword(fd, opts.engine))
		opts.flags |= IGT_SPIN_POLL_RUN;

	return __igt_spin_factory(fd, &opts);
}

static void __spin_wait(int fd, igt_spin_t *spin)
{
	if (igt_spin_has_poll(spin)) {
		igt_spin_busywait_until_started(spin);
	} else {
		igt_debug("__spin_wait - usleep mode\n");
		usleep(500e3); /* Better than nothing! */
	}
}

static igt_spin_t * spin_sync(int fd, uint32_t ctx, unsigned long flags)
{
	igt_spin_t *spin = __spin_poll(fd, ctx, flags);

	__spin_wait(fd, spin);

	return spin;
}

struct hang_ctx {
	int debugfs;
	struct timespec delay;
	struct timespec *ts;
	timer_t timer;
};

static void hang_handler(union sigval arg)
{
	struct hang_ctx *ctx = arg.sival_ptr;

	igt_debug("hang delay = %.2fus\n",
		  igt_nsec_elapsed(&ctx->delay) / 1000.0);

	igt_nsec_elapsed(ctx->ts);
	igt_assert(igt_sysfs_set(ctx->debugfs, "i915_wedged", "-1"));

	igt_assert_eq(timer_delete(ctx->timer), 0);
	close(ctx->debugfs);
	free(ctx);
}

static void hang_after(int fd, unsigned int us, struct timespec *ts)
{
	struct sigevent sev = {
		.sigev_notify = SIGEV_THREAD,
		.sigev_notify_function = hang_handler
	};
	struct itimerspec its = {
		.it_value.tv_sec = us / USEC_PER_SEC,
		.it_value.tv_nsec = us % USEC_PER_SEC * 1000,
	};
	struct hang_ctx *ctx;

	ctx = calloc(1, sizeof(*ctx));
	igt_assert(ctx);

	ctx->debugfs = igt_debugfs_dir(fd);
	igt_assert_fd(ctx->debugfs);

	sev.sigev_value.sival_ptr = ctx;

	igt_assert_eq(timer_create(CLOCK_MONOTONIC, &sev, &ctx->timer), 0);

	ctx->ts = ts;
	igt_nsec_elapsed(&ctx->delay);

	igt_assert_eq(timer_settime(ctx->timer, 0, &its, NULL), 0);
}

static void check_wait(int fd, uint32_t bo, unsigned int wait, igt_stats_t *st)
{
	struct timespec ts = {};

	if (wait) {
		hang_after(fd, wait, &ts);
	} else {
		igt_nsec_elapsed(&ts);
		manual_hang(fd);
	}

	gem_sync(fd, bo);

	if (st)
		igt_stats_push(st, igt_nsec_elapsed(&ts));
}

static void check_wait_elapsed(const char *prefix, int fd, igt_stats_t *st)
{
	double med, max, limit;

	igt_info("%s: completed %d resets, wakeups took %.3f+-%.3fms (min:%.3fms, median:%.3fms, max:%.3fms)\n",
		 prefix, st->n_values,
		 igt_stats_get_mean(st)*1e-6,
		 igt_stats_get_std_deviation(st)*1e-6,
		 igt_stats_get_min(st)*1e-6,
		 igt_stats_get_median(st)*1e-6,
		 igt_stats_get_max(st)*1e-6);

	if (st->n_values < 9)
		return; /* too few for stable median */

	/*
	 * Older platforms need to reset the display (incl. modeset to off,
	 * modeset back on) around resets, so may take a lot longer.
	 */
	limit = 250e6;
	if (intel_gen(intel_get_drm_devid(fd)) < 5)
		limit += 300e6; /* guestimate for 2x worstcase modeset */

	med = igt_stats_get_median(st);
	max = igt_stats_get_max(st);
	igt_assert_f(med < limit && max < 5 * limit,
		     "Wake up following reset+wedge took %.3f+-%.3fms (min:%.3fms, median:%.3fms, max:%.3fms); limit set to %.0fms on average and %.0fms maximum\n",
		     igt_stats_get_mean(st)*1e-6,
		     igt_stats_get_std_deviation(st)*1e-6,
		     igt_stats_get_min(st)*1e-6,
		     igt_stats_get_median(st)*1e-6,
		     igt_stats_get_max(st)*1e-6,
		     limit*1e-6, limit*5e-6);
}

static void __test_banned(int fd)
{
	struct drm_i915_gem_exec_object2 obj = {
		.handle = gem_create(fd, 4096),
	};
	struct drm_i915_gem_execbuffer2 execbuf = {
		.buffers_ptr = to_user_pointer(&obj),
		.buffer_count = 1,
	};
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	unsigned long count = 0;

	gem_write(fd, obj.handle, 0, &bbe, sizeof(bbe));

	gem_quiescent_gpu(fd);
	igt_require(i915_reset_control(true));

	igt_until_timeout(5) {
		igt_spin_t *hang;

		if (__gem_execbuf(fd, &execbuf) == -EIO) {
			uint32_t ctx = 0;

			igt_info("Banned after causing %lu hangs\n", count);
			igt_assert(count > 1);

			/* Only this context, not the file, should be banned */
			igt_assert_neq(__gem_context_create(fd, &ctx), -EIO);
			if (ctx) { /* remember the contextless! */
				/* And check it actually works! */
				execbuf.rsvd1 = ctx;
				gem_execbuf(fd, &execbuf);

				gem_context_destroy(fd, ctx);
			}
			return;
		}

		/* Trigger a reset, making sure we are detected as guilty */
		hang = spin_sync(fd, 0, 0);
		trigger_reset(fd);
		igt_spin_free(fd, hang);

		count++;
	}

	igt_assert_f(false,
		     "Ran for 5s, %lu hangs without being banned\n",
		     count);
}

static void test_banned(int fd)
{
	fd = gem_reopen_driver(fd);
	__test_banned(fd);
	close(fd);
}

#define TEST_WEDGE (1)

static void test_wait(int fd, unsigned int flags, unsigned int wait)
{
	igt_spin_t *hang;

	fd = gem_reopen_driver(fd);
	igt_require_gem(fd);

	/*
	 * If the request we wait on completes due to a hang (even for
	 * that request), the user expects the return value to 0 (success).
	 */

	if (flags & TEST_WEDGE)
		igt_require(i915_reset_control(false));
	else
		igt_require(i915_reset_control(true));

	hang = spin_sync(fd, 0, I915_EXEC_DEFAULT);

	check_wait(fd, hang->handle, wait, NULL);

	igt_spin_free(fd, hang);

	igt_require(i915_reset_control(true));

	trigger_reset(fd);
	close(fd);
}

static void test_suspend(int fd, int state)
{
	fd = gem_reopen_driver(fd);
	igt_require_gem(fd);

	/* Do a suspend first so that we don't skip inside the test */
	igt_system_suspend_autoresume(state, SUSPEND_TEST_DEVICES);

	/* Check we can suspend when the driver is already wedged */
	igt_require(i915_reset_control(false));
	manual_hang(fd);

	igt_system_suspend_autoresume(state, SUSPEND_TEST_DEVICES);

	igt_require(i915_reset_control(true));
	trigger_reset(fd);
	close(fd);
}

static void test_inflight(int fd, unsigned int wait)
{
	int parent_fd = fd;
	unsigned int engine;
	int fence[64]; /* mostly conservative estimate of ring size */
	int max;

	igt_require_gem(fd);
	igt_require(gem_has_exec_fence(fd));

	max = gem_measure_ring_inflight(fd, -1, 0);
	igt_require(max > 1);
	max = min(max - 1, ARRAY_SIZE(fence));

	for_each_engine(parent_fd, engine) {
		const uint32_t bbe = MI_BATCH_BUFFER_END;
		struct drm_i915_gem_exec_object2 obj[2];
		struct drm_i915_gem_execbuffer2 execbuf;
		igt_spin_t *hang;

		fd = gem_reopen_driver(parent_fd);
		igt_require_gem(fd);

		memset(obj, 0, sizeof(obj));
		obj[0].flags = EXEC_OBJECT_WRITE;
		obj[1].handle = gem_create(fd, 4096);
		gem_write(fd, obj[1].handle, 0, &bbe, sizeof(bbe));

		gem_quiescent_gpu(fd);
		igt_debug("Starting %s on engine '%s'\n", __func__, e__->name);
		igt_require(i915_reset_control(false));

		hang = spin_sync(fd, 0, engine);
		obj[0].handle = hang->handle;

		memset(&execbuf, 0, sizeof(execbuf));
		execbuf.buffers_ptr = to_user_pointer(obj);
		execbuf.buffer_count = 2;
		execbuf.flags = engine | I915_EXEC_FENCE_OUT;

		for (unsigned int n = 0; n < max; n++) {
			gem_execbuf_wr(fd, &execbuf);
			fence[n] = execbuf.rsvd2 >> 32;
			igt_assert(fence[n] != -1);
		}

		check_wait(fd, obj[1].handle, wait, NULL);

		for (unsigned int n = 0; n < max; n++) {
			igt_assert_eq(sync_fence_status(fence[n]), -EIO);
			close(fence[n]);
		}

		igt_spin_free(fd, hang);
		igt_assert(i915_reset_control(true));
		trigger_reset(fd);

		gem_close(fd, obj[1].handle);
		close(fd);
	}
}

static void test_inflight_suspend(int fd)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj[2];
	uint32_t bbe = MI_BATCH_BUFFER_END;
	int fence[64]; /* mostly conservative estimate of ring size */
	igt_spin_t *hang;
	int max;

	max = gem_measure_ring_inflight(fd, -1, 0);
	igt_require(max > 1);
	max = min(max - 1, ARRAY_SIZE(fence));

	fd = gem_reopen_driver(fd);
	igt_require_gem(fd);
	igt_require(gem_has_exec_fence(fd));
	igt_require(i915_reset_control(false));

	memset(obj, 0, sizeof(obj));
	obj[0].flags = EXEC_OBJECT_WRITE;
	obj[1].handle = gem_create(fd, 4096);
	gem_write(fd, obj[1].handle, 0, &bbe, sizeof(bbe));

	hang = spin_sync(fd, 0, 0);
	obj[0].handle = hang->handle;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = 2;
	execbuf.flags = I915_EXEC_FENCE_OUT;

	for (unsigned int n = 0; n < max; n++) {
		gem_execbuf_wr(fd, &execbuf);
		fence[n] = execbuf.rsvd2 >> 32;
		igt_assert(fence[n] != -1);
	}

	igt_set_autoresume_delay(30);
	igt_system_suspend_autoresume(SUSPEND_STATE_MEM, SUSPEND_TEST_NONE);

	check_wait(fd, obj[1].handle, 10, NULL);

	for (unsigned int n = 0; n < max; n++) {
		igt_assert_eq(sync_fence_status(fence[n]), -EIO);
		close(fence[n]);
	}

	igt_spin_free(fd, hang);
	igt_assert(i915_reset_control(true));
	trigger_reset(fd);
	close(fd);
}

static uint32_t context_create_safe(int i915)
{
	struct drm_i915_gem_context_param param;

	memset(&param, 0, sizeof(param));

	param.ctx_id = gem_context_create(i915);
	param.param = I915_CONTEXT_PARAM_BANNABLE;
	gem_context_set_param(i915, &param);

	param.param = I915_CONTEXT_PARAM_NO_ERROR_CAPTURE;
	param.value = 1;
	gem_context_set_param(i915, &param);

	return param.ctx_id;
}

static void test_inflight_contexts(int fd, unsigned int wait)
{
	int parent_fd = fd;
	unsigned int engine;

	igt_require_gem(fd);
	igt_require(gem_has_exec_fence(fd));
	gem_require_contexts(fd);

	for_each_engine(parent_fd, engine) {
		const uint32_t bbe = MI_BATCH_BUFFER_END;
		struct drm_i915_gem_exec_object2 obj[2];
		struct drm_i915_gem_execbuffer2 execbuf;
		unsigned int count;
		igt_spin_t *hang;
		uint32_t ctx[64];
		int fence[64];

		fd = gem_reopen_driver(parent_fd);
		igt_require_gem(fd);

		for (unsigned int n = 0; n < ARRAY_SIZE(ctx); n++)
			ctx[n] = context_create_safe(fd);

		gem_quiescent_gpu(fd);

		igt_debug("Starting %s on engine '%s'\n", __func__, e__->name);
		igt_require(i915_reset_control(false));

		memset(obj, 0, sizeof(obj));
		obj[0].flags = EXEC_OBJECT_WRITE;
		obj[1].handle = gem_create(fd, 4096);
		gem_write(fd, obj[1].handle, 0, &bbe, sizeof(bbe));

		hang = spin_sync(fd, 0, engine);
		obj[0].handle = hang->handle;

		memset(&execbuf, 0, sizeof(execbuf));
		execbuf.buffers_ptr = to_user_pointer(obj);
		execbuf.buffer_count = 2;
		execbuf.flags = engine | I915_EXEC_FENCE_OUT;

		count = 0;
		for (unsigned int n = 0; n < ARRAY_SIZE(fence); n++) {
			execbuf.rsvd1 = ctx[n];
			if (__gem_execbuf_wr(fd, &execbuf))
				break; /* small shared ring */
			fence[n] = execbuf.rsvd2 >> 32;
			igt_assert(fence[n] != -1);
			count++;
		}

		check_wait(fd, obj[1].handle, wait, NULL);

		for (unsigned int n = 0; n < count; n++) {
			igt_assert_eq(sync_fence_status(fence[n]), -EIO);
			close(fence[n]);
		}

		igt_spin_free(fd, hang);
		gem_close(fd, obj[1].handle);
		igt_assert(i915_reset_control(true));
		trigger_reset(fd);

		for (unsigned int n = 0; n < ARRAY_SIZE(ctx); n++)
			gem_context_destroy(fd, ctx[n]);

		close(fd);
	}
}

static void test_inflight_external(int fd)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj;
	igt_spin_t *hang;
	uint32_t fence;
	IGT_CORK_FENCE(cork);

	igt_require_sw_sync();
	igt_require(gem_has_exec_fence(fd));

	fd = gem_reopen_driver(fd);
	igt_require_gem(fd);

	fence = igt_cork_plug(&cork, fd);

	igt_require(i915_reset_control(false));
	hang = __spin_poll(fd, 0, 0);

	memset(&obj, 0, sizeof(obj));
	obj.handle = gem_create(fd, 4096);
	gem_write(fd, obj.handle, 0, &bbe, sizeof(bbe));

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;
	execbuf.flags = I915_EXEC_FENCE_IN | I915_EXEC_FENCE_OUT;
	execbuf.rsvd2 = (uint32_t)fence;

	gem_execbuf_wr(fd, &execbuf);
	close(fence);

	fence = execbuf.rsvd2 >> 32;
	igt_assert(fence != -1);

	__spin_wait(fd, hang);
	manual_hang(fd);

	gem_sync(fd, hang->handle); /* wedged, with an unready batch */
	igt_assert(!gem_bo_busy(fd, hang->handle));
	igt_assert(gem_bo_busy(fd, obj.handle));
	igt_cork_unplug(&cork); /* only now submit our batches */

	igt_assert_eq(__gem_wait(fd, obj.handle, -1), 0);
	igt_assert_eq(sync_fence_status(fence), -EIO);
	close(fence);

	igt_spin_free(fd, hang);
	igt_assert(i915_reset_control(true));
	trigger_reset(fd);
	close(fd);
}

static void test_inflight_internal(int fd, unsigned int wait)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj[2];
	uint32_t bbe = MI_BATCH_BUFFER_END;
	unsigned engine, nfence = 0;
	int fences[16];
	igt_spin_t *hang;

	igt_require(gem_has_exec_fence(fd));

	fd = gem_reopen_driver(fd);
	igt_require_gem(fd);

	igt_require(i915_reset_control(false));
	hang = spin_sync(fd, 0, 0);

	memset(obj, 0, sizeof(obj));
	obj[0].handle = hang->handle;
	obj[0].flags = EXEC_OBJECT_WRITE;
	obj[1].handle = gem_create(fd, 4096);
	gem_write(fd, obj[1].handle, 0, &bbe, sizeof(bbe));

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = 2;
	for_each_engine(fd, engine) {
		execbuf.flags = engine | I915_EXEC_FENCE_OUT;

		gem_execbuf_wr(fd, &execbuf);

		fences[nfence] = execbuf.rsvd2 >> 32;
		igt_assert(fences[nfence] != -1);
		nfence++;
	}

	check_wait(fd, obj[1].handle, wait, NULL);

	while (nfence--) {
		igt_assert_eq(sync_fence_status(fences[nfence]), -EIO);
		close(fences[nfence]);
	}

	igt_spin_free(fd, hang);
	igt_assert(i915_reset_control(true));
	trigger_reset(fd);
	close(fd);
}

static void reset_stress(int fd, uint32_t ctx0,
			 const char *name, unsigned int engine,
			 unsigned int flags)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 obj = {
		.handle = gem_create(fd, 4096)
	};
	struct drm_i915_gem_execbuffer2 execbuf = {
		.buffers_ptr = to_user_pointer(&obj),
		.buffer_count = 1,
		.flags = engine,
	};
	igt_stats_t stats;
	int max;

	max = gem_measure_ring_inflight(fd, engine, 0);
	max = max / 2 - 1; /* assume !execlists and a shared ring */
	igt_require(max > 0);

	gem_write(fd, obj.handle, 0, &bbe, sizeof(bbe));

	igt_stats_init(&stats);
	igt_until_timeout(5) {
		uint32_t ctx = context_create_safe(fd);
		igt_spin_t *hang;
		unsigned int i;

		gem_quiescent_gpu(fd);

		igt_require(i915_reset_control(flags & TEST_WEDGE ?
					       false : true));

		/*
		 * Start executing a spin batch with some queued batches
		 * against a different context after it.
		 */
		hang = spin_sync(fd, ctx0, engine);

		execbuf.rsvd1 = ctx;
		for (i = 0; i < max; i++)
			gem_execbuf(fd, &execbuf);

		execbuf.rsvd1 = ctx0;
		for (i = 0; i < max; i++)
			gem_execbuf(fd, &execbuf);

		/* Wedge after a small delay. */
		check_wait(fd, obj.handle, 100e3, &stats);
		igt_assert_eq(sync_fence_status(hang->out_fence), -EIO);

		/* Unwedge by forcing a reset. */
		igt_assert(i915_reset_control(true));
		trigger_reset(fd);

		gem_quiescent_gpu(fd);

		/*
		 * Verify that we are able to submit work after unwedging from
		 * both contexts.
		 */
		execbuf.rsvd1 = ctx;
		for (i = 0; i < max; i++)
			gem_execbuf(fd, &execbuf);

		execbuf.rsvd1 = ctx0;
		for (i = 0; i < max; i++)
			gem_execbuf(fd, &execbuf);

		gem_sync(fd, obj.handle);
		igt_spin_free(fd, hang);
		gem_context_destroy(fd, ctx);
	}
	check_wait_elapsed(name, fd, &stats);
	igt_stats_fini(&stats);

	gem_close(fd, obj.handle);
}

/*
 * Verify that we can submit and execute work after unwedging the GPU.
 */
static void test_reset_stress(int fd, unsigned int flags)
{
	uint32_t ctx0 = context_create_safe(fd);
	unsigned int engine;

	for_each_engine(fd, engine)
		reset_stress(fd, ctx0, e__->name, engine, flags);

	gem_context_destroy(fd, ctx0);
}

static int fd = -1;

static void
exit_handler(int sig)
{
	i915_reset_control(true);
	igt_force_gpu_reset(fd);
}

igt_main
{
	igt_skip_on_simulation();

	igt_fixture {
		fd = drm_open_driver(DRIVER_INTEL);
		igt_device_drop_master(fd);

		gem_submission_print_method(fd);
		igt_require_gem(fd);

		igt_allow_hang(fd, 0, 0);

		igt_require(i915_reset_control(true));
		igt_force_gpu_reset(fd);
		igt_install_exit_handler(exit_handler);
	}

	igt_subtest("throttle")
		test_throttle(fd);

	igt_subtest("context-create")
		test_context_create(fd);

	igt_subtest("execbuf")
		test_execbuf(fd);

	igt_subtest("banned")
		test_banned(fd);

	igt_subtest("suspend")
		test_suspend(fd, SUSPEND_STATE_MEM);

	igt_subtest("hibernate")
		test_suspend(fd, SUSPEND_STATE_DISK);

	igt_subtest("in-flight-external")
		test_inflight_external(fd);

	igt_subtest("in-flight-suspend")
		test_inflight_suspend(fd);

	igt_subtest_group {
		igt_fixture {
			igt_require(gem_has_contexts(fd));
		}

		igt_subtest("reset-stress")
			test_reset_stress(fd, 0);

		igt_subtest("unwedge-stress")
			test_reset_stress(fd, TEST_WEDGE);
	}

	igt_subtest_group {
		const struct {
			unsigned int wait;
			const char *name;
		} waits[] = {
			{ .wait = 0, .name = "immediate" },
			{ .wait = 1, .name = "1us" },
			{ .wait = 10000, .name = "10ms" },
		};
		unsigned int i;

		for (i = 0; i < sizeof(waits) / sizeof(waits[0]); i++) {
			igt_subtest_f("wait-%s", waits[i].name)
				test_wait(fd, 0, waits[i].wait);

			igt_subtest_f("wait-wedge-%s", waits[i].name)
				test_wait(fd, TEST_WEDGE, waits[i].wait);

			igt_subtest_f("in-flight-%s", waits[i].name)
				test_inflight(fd, waits[i].wait);

			igt_subtest_f("in-flight-contexts-%s", waits[i].name)
				test_inflight_contexts(fd, waits[i].wait);

			igt_subtest_f("in-flight-internal-%s", waits[i].name) {
				igt_skip_on(gem_has_semaphores(fd));
				test_inflight_internal(fd, waits[i].wait);
			}
		}
	}
}
