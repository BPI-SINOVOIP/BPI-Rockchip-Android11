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

#include "igt.h"
#include "sw_sync.h"
#include "igt_syncobj.h"
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <signal.h>
#include "drm.h"

IGT_TEST_DESCRIPTION("Tests for the drm sync object wait API");

/* One tenth of a second */
#define SHORT_TIME_NSEC 100000000ull

#define NSECS_PER_SEC 1000000000ull

static uint64_t
gettime_ns(void)
{
	struct timespec current;
	clock_gettime(CLOCK_MONOTONIC, &current);
	return (uint64_t)current.tv_sec * NSECS_PER_SEC + current.tv_nsec;
}

static void
sleep_nsec(uint64_t time_nsec)
{
	struct timespec t;
	t.tv_sec = time_nsec / NSECS_PER_SEC;
	t.tv_nsec = time_nsec % NSECS_PER_SEC;
	igt_assert_eq(nanosleep(&t, NULL), 0);
}

static uint64_t
short_timeout(void)
{
	return gettime_ns() + SHORT_TIME_NSEC;
}

static int
syncobj_attach_sw_sync(int fd, uint32_t handle)
{
	struct drm_syncobj_handle;
	int timeline, fence;

	timeline = sw_sync_timeline_create();
	fence = sw_sync_timeline_create_fence(timeline, 1);
	syncobj_import_sync_file(fd, handle, fence);
	close(fence);

	return timeline;
}

static void
syncobj_trigger(int fd, uint32_t handle)
{
	int timeline = syncobj_attach_sw_sync(fd, handle);
	sw_sync_timeline_inc(timeline, 1);
	close(timeline);
}

static timer_t
set_timer(void (*cb)(union sigval), void *ptr, int i, uint64_t nsec)
{
        timer_t timer;
        struct sigevent sev;
        struct itimerspec its;

        memset(&sev, 0, sizeof(sev));
        sev.sigev_notify = SIGEV_THREAD;
	if (ptr)
		sev.sigev_value.sival_ptr = ptr;
	else
		sev.sigev_value.sival_int = i;
        sev.sigev_notify_function = cb;
        igt_assert(timer_create(CLOCK_MONOTONIC, &sev, &timer) == 0);

        memset(&its, 0, sizeof(its));
        its.it_value.tv_sec = nsec / NSEC_PER_SEC;
        its.it_value.tv_nsec = nsec % NSEC_PER_SEC;
        igt_assert(timer_settime(timer, 0, &its, NULL) == 0);

	return timer;
}

struct fd_handle_pair {
	int fd;
	uint32_t handle;
};

static void
timeline_inc_func(union sigval sigval)
{
	sw_sync_timeline_inc(sigval.sival_int, 1);
}

static void
syncobj_trigger_free_pair_func(union sigval sigval)
{
	struct fd_handle_pair *pair = sigval.sival_ptr;
	syncobj_trigger(pair->fd, pair->handle);
	free(pair);
}

static timer_t
syncobj_trigger_delayed(int fd, uint32_t syncobj, uint64_t nsec)
{
	struct fd_handle_pair *pair = malloc(sizeof(*pair));

	pair->fd = fd;
	pair->handle = syncobj;

	return set_timer(syncobj_trigger_free_pair_func, pair, 0, nsec);
}

static void
test_wait_bad_flags(int fd)
{
	struct local_syncobj_wait wait = { 0 };
	wait.flags = 0xdeadbeef;
	igt_assert_eq(__syncobj_wait(fd, &wait), -EINVAL);
}

static void
test_wait_zero_handles(int fd)
{
	struct local_syncobj_wait wait = { 0 };
	igt_assert_eq(__syncobj_wait(fd, &wait), -EINVAL);
}

static void
test_wait_illegal_handle(int fd)
{
	struct local_syncobj_wait wait = { 0 };
	uint32_t handle = 0;

	wait.count_handles = 1;
	wait.handles = to_user_pointer(&handle);
	igt_assert_eq(__syncobj_wait(fd, &wait), -ENOENT);
}

static void
test_reset_zero_handles(int fd)
{
	struct local_syncobj_array array = { 0 };
	int ret;

	ret = drmIoctl(fd, LOCAL_IOCTL_SYNCOBJ_RESET, &array);
	igt_assert(ret == -1 && errno ==  EINVAL);
}

static void
test_reset_illegal_handle(int fd)
{
	struct local_syncobj_array array = { 0 };
	uint32_t handle = 0;
	int ret;

	array.count_handles = 1;
	array.handles = to_user_pointer(&handle);
	ret = drmIoctl(fd, LOCAL_IOCTL_SYNCOBJ_RESET, &array);
	igt_assert(ret == -1 && errno == ENOENT);
}

static void
test_reset_one_illegal_handle(int fd)
{
	struct local_syncobj_array array = { 0 };
	uint32_t syncobjs[3];
	int ret;

	syncobjs[0] = syncobj_create(fd, LOCAL_SYNCOBJ_CREATE_SIGNALED);
	syncobjs[1] = 0;
	syncobjs[2] = syncobj_create(fd, LOCAL_SYNCOBJ_CREATE_SIGNALED);

	igt_assert_eq(syncobj_wait_err(fd, &syncobjs[0], 1, 0, 0), 0);
	igt_assert_eq(syncobj_wait_err(fd, &syncobjs[2], 1, 0, 0), 0);

	array.count_handles = 3;
	array.handles = to_user_pointer(syncobjs);
	ret = drmIoctl(fd, LOCAL_IOCTL_SYNCOBJ_RESET, &array);
	igt_assert(ret == -1 && errno == ENOENT);

	/* Assert that we didn't actually reset anything */
	igt_assert_eq(syncobj_wait_err(fd, &syncobjs[0], 1, 0, 0), 0);
	igt_assert_eq(syncobj_wait_err(fd, &syncobjs[2], 1, 0, 0), 0);

	syncobj_destroy(fd, syncobjs[0]);
	syncobj_destroy(fd, syncobjs[2]);
}

static void
test_reset_bad_pad(int fd)
{
	struct local_syncobj_array array = { 0 };
	uint32_t handle = 0;
	int ret;

	array.pad = 0xdeadbeef;
	array.count_handles = 1;
	array.handles = to_user_pointer(&handle);
	ret = drmIoctl(fd, LOCAL_IOCTL_SYNCOBJ_RESET, &array);
	igt_assert(ret == -1 && errno == EINVAL);
}

static void
test_signal_zero_handles(int fd)
{
	struct local_syncobj_array array = { 0 };
	int ret;

	ret = drmIoctl(fd, LOCAL_IOCTL_SYNCOBJ_SIGNAL, &array);
	igt_assert(ret == -1 && errno == EINVAL);
}

static void
test_signal_illegal_handle(int fd)
{
	struct local_syncobj_array array = { 0 };
	uint32_t handle = 0;
	int ret;

	array.count_handles = 1;
	array.handles = to_user_pointer(&handle);
	ret = drmIoctl(fd, LOCAL_IOCTL_SYNCOBJ_SIGNAL, &array);
	igt_assert(ret == -1 && errno == ENOENT);
}

static void
test_signal_one_illegal_handle(int fd)
{
	struct local_syncobj_array array = { 0 };
	uint32_t syncobjs[3];
	int ret;

	syncobjs[0] = syncobj_create(fd, 0);
	syncobjs[1] = 0;
	syncobjs[2] = syncobj_create(fd, 0);

	igt_assert_eq(syncobj_wait_err(fd, &syncobjs[0], 1, 0, 0), -EINVAL);
	igt_assert_eq(syncobj_wait_err(fd, &syncobjs[2], 1, 0, 0), -EINVAL);

	array.count_handles = 3;
	array.handles = to_user_pointer(syncobjs);
	ret = drmIoctl(fd, LOCAL_IOCTL_SYNCOBJ_SIGNAL, &array);
	igt_assert(ret == -1 && errno == ENOENT);

	/* Assert that we didn't actually reset anything */
	igt_assert_eq(syncobj_wait_err(fd, &syncobjs[0], 1, 0, 0), -EINVAL);
	igt_assert_eq(syncobj_wait_err(fd, &syncobjs[2], 1, 0, 0), -EINVAL);

	syncobj_destroy(fd, syncobjs[0]);
	syncobj_destroy(fd, syncobjs[2]);
}

static void
test_signal_bad_pad(int fd)
{
	struct local_syncobj_array array = { 0 };
	uint32_t handle = 0;
	int ret;

	array.pad = 0xdeadbeef;
	array.count_handles = 1;
	array.handles = to_user_pointer(&handle);
	ret = drmIoctl(fd, LOCAL_IOCTL_SYNCOBJ_SIGNAL, &array);
	igt_assert(ret == -1 && errno ==  EINVAL);
}

#define WAIT_FOR_SUBMIT		(1 << 0)
#define WAIT_ALL		(1 << 1)
#define WAIT_UNSUBMITTED	(1 << 2)
#define WAIT_SUBMITTED		(1 << 3)
#define WAIT_SIGNALED		(1 << 4)
#define WAIT_FLAGS_MAX		(1 << 5) - 1

static uint32_t
flags_for_test_flags(uint32_t test_flags)
{
	uint32_t flags = 0;

	if (test_flags & WAIT_FOR_SUBMIT)
		flags |= LOCAL_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT;

	if (test_flags & WAIT_ALL)
		flags |= LOCAL_SYNCOBJ_WAIT_FLAGS_WAIT_ALL;

	return flags;
}

static void
test_single_wait(int fd, uint32_t test_flags, int expect)
{
	uint32_t syncobj = syncobj_create(fd, 0);
	uint32_t flags = flags_for_test_flags(test_flags);
	int timeline = -1;

	if (test_flags & (WAIT_SUBMITTED | WAIT_SIGNALED))
		timeline = syncobj_attach_sw_sync(fd, syncobj);

	if (test_flags & WAIT_SIGNALED)
		sw_sync_timeline_inc(timeline, 1);

	igt_assert_eq(syncobj_wait_err(fd, &syncobj, 1, 0, flags), expect);

	igt_assert_eq(syncobj_wait_err(fd, &syncobj, 1, short_timeout(),
				       flags), expect);

	if (expect != -ETIME) {
		igt_assert_eq(syncobj_wait_err(fd, &syncobj, 1, UINT64_MAX,
					       flags), expect);
	}

	syncobj_destroy(fd, syncobj);
	if (timeline != -1)
		close(timeline);
}

static void
test_wait_delayed_signal(int fd, uint32_t test_flags)
{
	uint32_t syncobj = syncobj_create(fd, 0);
	uint32_t flags = flags_for_test_flags(test_flags);
	int timeline = -1;
	timer_t timer;

	if (test_flags & WAIT_FOR_SUBMIT) {
		timer = syncobj_trigger_delayed(fd, syncobj, SHORT_TIME_NSEC);
	} else {
		timeline = syncobj_attach_sw_sync(fd, syncobj);
		timer = set_timer(timeline_inc_func, NULL,
				  timeline, SHORT_TIME_NSEC);
	}

	igt_assert(syncobj_wait(fd, &syncobj, 1,
				gettime_ns() + SHORT_TIME_NSEC * 2,
				flags, NULL));

	timer_delete(timer);

	if (timeline != -1)
		close(timeline);

	syncobj_destroy(fd, syncobj);
}

static void
test_reset_unsignaled(int fd)
{
	uint32_t syncobj = syncobj_create(fd, 0);

	igt_assert_eq(syncobj_wait_err(fd, &syncobj, 1, 0, 0), -EINVAL);

	syncobj_reset(fd, &syncobj, 1);

	igt_assert_eq(syncobj_wait_err(fd, &syncobj, 1, 0, 0), -EINVAL);

	syncobj_destroy(fd, syncobj);
}

static void
test_reset_signaled(int fd)
{
	uint32_t syncobj = syncobj_create(fd, 0);

	syncobj_trigger(fd, syncobj);

	igt_assert_eq(syncobj_wait_err(fd, &syncobj, 1, 0, 0), 0);

	syncobj_reset(fd, &syncobj, 1);

	igt_assert_eq(syncobj_wait_err(fd, &syncobj, 1, 0, 0), -EINVAL);

	syncobj_destroy(fd, syncobj);
}

static void
test_reset_multiple_signaled(int fd)
{
	uint32_t syncobjs[3];
	int i;

	for (i = 0; i < 3; i++) {
		syncobjs[i] = syncobj_create(fd, 0);
		syncobj_trigger(fd, syncobjs[i]);
	}

	igt_assert_eq(syncobj_wait_err(fd, syncobjs, 3, 0, 0), 0);

	syncobj_reset(fd, syncobjs, 3);

	for (i = 0; i < 3; i++) {
		igt_assert_eq(syncobj_wait_err(fd, &syncobjs[i], 1,
					       0, 0), -EINVAL);
		syncobj_destroy(fd, syncobjs[i]);
	}
}

static void
reset_and_trigger_func(union sigval sigval)
{
	struct fd_handle_pair *pair = sigval.sival_ptr;
	syncobj_reset(pair->fd, &pair->handle, 1);
	syncobj_trigger(pair->fd, pair->handle);
}

static void
test_reset_during_wait_for_submit(int fd)
{
	uint32_t syncobj = syncobj_create(fd, 0);
	uint32_t flags = LOCAL_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT;
	struct fd_handle_pair pair;
	timer_t timer;

	pair.fd = fd;
	pair.handle = syncobj;
	timer = set_timer(reset_and_trigger_func, &pair, 0, SHORT_TIME_NSEC);

	/* A reset should be a no-op even if we're in the middle of a wait */
	igt_assert(syncobj_wait(fd, &syncobj, 1,
				gettime_ns() + SHORT_TIME_NSEC * 2,
				flags, NULL));

	timer_delete(timer);

	syncobj_destroy(fd, syncobj);
}

static void
test_signal(int fd)
{
	uint32_t syncobj = syncobj_create(fd, 0);
	uint32_t flags = LOCAL_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT;

	igt_assert_eq(syncobj_wait_err(fd, &syncobj, 1, 0, 0), -EINVAL);
	igt_assert_eq(syncobj_wait_err(fd, &syncobj, 1, 0, flags), -ETIME);

	syncobj_signal(fd, &syncobj, 1);

	igt_assert(syncobj_wait(fd, &syncobj, 1, 0, 0, NULL));
	igt_assert(syncobj_wait(fd, &syncobj, 1, 0, flags, NULL));

	syncobj_destroy(fd, syncobj);
}

static void
test_multi_wait(int fd, uint32_t test_flags, int expect)
{
	uint32_t syncobjs[3];
	uint32_t tflag, flags;
	int i, fidx, timeline;

	syncobjs[0] = syncobj_create(fd, 0);
	syncobjs[1] = syncobj_create(fd, 0);
	syncobjs[2] = syncobj_create(fd, 0);

	flags = flags_for_test_flags(test_flags);
	test_flags &= ~(WAIT_ALL | WAIT_FOR_SUBMIT);

	for (i = 0; i < 3; i++) {
		fidx = ffs(test_flags) - 1;
		tflag = (1 << fidx);

		if (test_flags & ~tflag)
			test_flags &= ~tflag;

		if (tflag & (WAIT_SUBMITTED | WAIT_SIGNALED))
			timeline = syncobj_attach_sw_sync(fd, syncobjs[i]);
		if (tflag & WAIT_SIGNALED)
			sw_sync_timeline_inc(timeline, 1);
	}

	igt_assert_eq(syncobj_wait_err(fd, syncobjs, 3, 0, flags), expect);

	igt_assert_eq(syncobj_wait_err(fd, syncobjs, 3, short_timeout(),
				       flags), expect);

	if (expect != -ETIME) {
		igt_assert_eq(syncobj_wait_err(fd, syncobjs, 3, UINT64_MAX,
					       flags), expect);
	}

	syncobj_destroy(fd, syncobjs[0]);
	syncobj_destroy(fd, syncobjs[1]);
	syncobj_destroy(fd, syncobjs[2]);
}

struct wait_thread_data {
	int fd;
	struct local_syncobj_wait wait;
};

static void *
wait_thread_func(void *data)
{
	struct wait_thread_data *wait = data;
	igt_assert_eq(__syncobj_wait(wait->fd, &wait->wait), 0);
	return NULL;
}

static void
test_wait_snapshot(int fd, uint32_t test_flags)
{
	struct wait_thread_data wait = { 0 };
	uint32_t syncobjs[2];
	int timelines[3] = { -1, -1, -1 };
	pthread_t thread;

	syncobjs[0] = syncobj_create(fd, 0);
	syncobjs[1] = syncobj_create(fd, 0);

	if (!(test_flags & WAIT_FOR_SUBMIT)) {
		timelines[0] = syncobj_attach_sw_sync(fd, syncobjs[0]);
		timelines[1] = syncobj_attach_sw_sync(fd, syncobjs[1]);
	}

	wait.fd = fd;
	wait.wait.handles = to_user_pointer(syncobjs);
	wait.wait.count_handles = 2;
	wait.wait.timeout_nsec = short_timeout();
	wait.wait.flags = flags_for_test_flags(test_flags);

	igt_assert_eq(pthread_create(&thread, NULL, wait_thread_func, &wait), 0);

	sleep_nsec(SHORT_TIME_NSEC / 5);

	/* Try to fake the kernel out by triggering or partially triggering
	 * the first fence.
	 */
	if (test_flags & WAIT_ALL) {
		/* If it's WAIT_ALL, actually trigger it */
		if (timelines[0] == -1)
			syncobj_trigger(fd, syncobjs[0]);
		else
			sw_sync_timeline_inc(timelines[0], 1);
	} else if (test_flags & WAIT_FOR_SUBMIT) {
		timelines[0] = syncobj_attach_sw_sync(fd, syncobjs[0]);
	}

	sleep_nsec(SHORT_TIME_NSEC / 5);

	/* Then reset it */
	syncobj_reset(fd, &syncobjs[0], 1);

	sleep_nsec(SHORT_TIME_NSEC / 5);

	/* Then "submit" it in a way that will never trigger.  This way, if
	 * the kernel picks up on the new fence (it shouldn't), we'll get a
	 * timeout.
	 */
	timelines[2] = syncobj_attach_sw_sync(fd, syncobjs[0]);

	sleep_nsec(SHORT_TIME_NSEC / 5);

	/* Now trigger the second fence to complete the wait */

	if (timelines[1] == -1)
		syncobj_trigger(fd, syncobjs[1]);
	else
		sw_sync_timeline_inc(timelines[1], 1);

	pthread_join(thread, NULL);

	if (!(test_flags & WAIT_ALL))
		igt_assert_eq(wait.wait.first_signaled, 1);

	close(timelines[0]);
	close(timelines[1]);
	close(timelines[2]);
	syncobj_destroy(fd, syncobjs[0]);
	syncobj_destroy(fd, syncobjs[1]);
}

/* The numbers 0-7, each repeated 5x and shuffled. */
static const unsigned shuffled_0_7_x4[] = {
	2, 0, 6, 1, 1, 4, 5, 2, 0, 7, 1, 7, 6, 3, 4, 5,
	0, 2, 7, 3, 5, 4, 0, 6, 7, 3, 2, 5, 6, 1, 4, 3,
};

enum syncobj_stage {
	STAGE_UNSUBMITTED,
	STAGE_SUBMITTED,
	STAGE_SIGNALED,
	STAGE_RESET,
	STAGE_RESUBMITTED,
};

static void
test_wait_complex(int fd, uint32_t test_flags)
{
	struct wait_thread_data wait = { 0 };
	uint32_t syncobjs[8];
	enum syncobj_stage stage[8];
	int i, j, timelines[8];
	uint32_t first_signaled = -1, num_signaled = 0;
	pthread_t thread;

	for (i = 0; i < 8; i++) {
		stage[i] = STAGE_UNSUBMITTED;
		syncobjs[i] = syncobj_create(fd, 0);
	}

	if (test_flags & WAIT_FOR_SUBMIT) {
		for (i = 0; i < 8; i++)
			timelines[i] = -1;
	} else {
		for (i = 0; i < 8; i++)
			timelines[i] = syncobj_attach_sw_sync(fd, syncobjs[i]);
	}

	wait.fd = fd;
	wait.wait.handles = to_user_pointer(syncobjs);
	wait.wait.count_handles = 2;
	wait.wait.timeout_nsec = gettime_ns() + NSECS_PER_SEC;
	wait.wait.flags = flags_for_test_flags(test_flags);

	igt_assert_eq(pthread_create(&thread, NULL, wait_thread_func, &wait), 0);

	sleep_nsec(NSECS_PER_SEC / 50);

	num_signaled = 0;
	for (j = 0; j < ARRAY_SIZE(shuffled_0_7_x4); j++) {
		i = shuffled_0_7_x4[j];
		igt_assert_lt(i, ARRAY_SIZE(syncobjs));

		switch (stage[i]++) {
		case STAGE_UNSUBMITTED:
			/* We need to submit attach a fence */
			if (!(test_flags & WAIT_FOR_SUBMIT)) {
				/* We had to attach one up-front */
				igt_assert_neq(timelines[i], -1);
				break;
			}
			timelines[i] = syncobj_attach_sw_sync(fd, syncobjs[i]);
			break;

		case STAGE_SUBMITTED:
			/* We have a fence, trigger it */
			igt_assert_neq(timelines[i], -1);
			sw_sync_timeline_inc(timelines[i], 1);
			close(timelines[i]);
			timelines[i] = -1;
			if (num_signaled == 0)
				first_signaled = i;
			num_signaled++;
			break;

		case STAGE_SIGNALED:
			/* We're already signaled, reset */
			syncobj_reset(fd, &syncobjs[i], 1);
			break;

		case STAGE_RESET:
			/* We're reset, submit and don't signal */
			timelines[i] = syncobj_attach_sw_sync(fd, syncobjs[i]);
			break;

		case STAGE_RESUBMITTED:
			igt_assert(!"Should not reach this stage");
			break;
		}

		if (test_flags & WAIT_ALL) {
			if (num_signaled == ARRAY_SIZE(syncobjs))
				break;
		} else {
			if (num_signaled > 0)
				break;
		}

		sleep_nsec(NSECS_PER_SEC / 100);
	}

	pthread_join(thread, NULL);

	if (test_flags & WAIT_ALL) {
		igt_assert_eq(num_signaled, ARRAY_SIZE(syncobjs));
	} else {
		igt_assert_eq(num_signaled, 1);
		igt_assert_eq(wait.wait.first_signaled, first_signaled);
	}

	for (i = 0; i < 8; i++) {
		close(timelines[i]);
		syncobj_destroy(fd, syncobjs[i]);
	}
}

static void
test_wait_interrupted(int fd, uint32_t test_flags)
{
	struct local_syncobj_wait wait = { 0 };
	uint32_t syncobj = syncobj_create(fd, 0);
	int timeline;

	wait.handles = to_user_pointer(&syncobj);
	wait.count_handles = 1;
	wait.flags = flags_for_test_flags(test_flags);

	if (test_flags & WAIT_FOR_SUBMIT) {
		wait.timeout_nsec = short_timeout();
		igt_while_interruptible(true)
			igt_assert_eq(__syncobj_wait(fd, &wait), -ETIME);
	}

	timeline = syncobj_attach_sw_sync(fd, syncobj);

	wait.timeout_nsec = short_timeout();
	igt_while_interruptible(true)
		igt_assert_eq(__syncobj_wait(fd, &wait), -ETIME);

	syncobj_destroy(fd, syncobj);
	close(timeline);
}

static bool
has_syncobj_wait(int fd)
{
	struct local_syncobj_wait wait = { 0 };
	uint32_t handle = 0;
	uint64_t value;
	int ret;

	if (drmGetCap(fd, DRM_CAP_SYNCOBJ, &value))
		return false;
	if (!value)
		return false;

	/* Try waiting for zero sync objects should fail with EINVAL */
	wait.count_handles = 1;
	wait.handles = to_user_pointer(&handle);
	ret = drmIoctl(fd, LOCAL_IOCTL_SYNCOBJ_WAIT, &wait);
	return ret == -1 && errno == ENOENT;
}

igt_main
{
	int fd = -1;

	igt_fixture {
		fd = drm_open_driver(DRIVER_ANY);
		igt_require(has_syncobj_wait(fd));
		igt_require_sw_sync();
	}

	igt_subtest("invalid-wait-bad-flags")
		test_wait_bad_flags(fd);

	igt_subtest("invalid-wait-zero-handles")
		test_wait_zero_handles(fd);

	igt_subtest("invalid-wait-illegal-handle")
		test_wait_illegal_handle(fd);

	igt_subtest("invalid-reset-zero-handles")
		test_reset_zero_handles(fd);

	igt_subtest("invalid-reset-illegal-handle")
		test_reset_illegal_handle(fd);

	igt_subtest("invalid-reset-one-illegal-handle")
		test_reset_one_illegal_handle(fd);

	igt_subtest("invalid-reset-bad-pad")
		test_reset_bad_pad(fd);

	igt_subtest("invalid-signal-zero-handles")
		test_signal_zero_handles(fd);

	igt_subtest("invalid-signal-illegal-handle")
		test_signal_illegal_handle(fd);

	igt_subtest("invalid-signal-one-illegal-handle")
		test_signal_one_illegal_handle(fd);

	igt_subtest("invalid-signal-bad-pad")
		test_signal_bad_pad(fd);

	for (unsigned flags = 0; flags < WAIT_FLAGS_MAX; flags++) {
		int err;

		/* Only one wait mode for single-wait tests */
		if (__builtin_popcount(flags & (WAIT_UNSUBMITTED |
						WAIT_SUBMITTED |
						WAIT_SIGNALED)) != 1)
			continue;

		if ((flags & WAIT_UNSUBMITTED) && !(flags & WAIT_FOR_SUBMIT))
			err = -EINVAL;
		else if (!(flags & WAIT_SIGNALED))
			err = -ETIME;
		else
			err = 0;

		igt_subtest_f("%ssingle-wait%s%s%s%s%s",
			      err == -EINVAL ? "invalid-" : "",
			      (flags & WAIT_ALL) ? "-all" : "",
			      (flags & WAIT_FOR_SUBMIT) ? "-for-submit" : "",
			      (flags & WAIT_UNSUBMITTED) ? "-unsubmitted" : "",
			      (flags & WAIT_SUBMITTED) ? "-submitted" : "",
			      (flags & WAIT_SIGNALED) ? "-signaled" : "")
			test_single_wait(fd, flags, err);
	}

	igt_subtest("wait-delayed-signal")
		test_wait_delayed_signal(fd, 0);

	igt_subtest("wait-for-submit-delayed-submit")
		test_wait_delayed_signal(fd, WAIT_FOR_SUBMIT);

	igt_subtest("wait-all-delayed-signal")
		test_wait_delayed_signal(fd, WAIT_ALL);

	igt_subtest("wait-all-for-submit-delayed-submit")
		test_wait_delayed_signal(fd, WAIT_ALL | WAIT_FOR_SUBMIT);

	igt_subtest("reset-unsignaled")
		test_reset_unsignaled(fd);

	igt_subtest("reset-signaled")
		test_reset_signaled(fd);

	igt_subtest("reset-multiple-signaled")
		test_reset_multiple_signaled(fd);

	igt_subtest("reset-during-wait-for-submit")
		test_reset_during_wait_for_submit(fd);

	igt_subtest("signal")
		test_signal(fd);

	for (unsigned flags = 0; flags < WAIT_FLAGS_MAX; flags++) {
		int err;

		/* At least one wait mode for multi-wait tests */
		if (!(flags & (WAIT_UNSUBMITTED |
			       WAIT_SUBMITTED |
			       WAIT_SIGNALED)))
			continue;

		err = 0;
		if ((flags & WAIT_UNSUBMITTED) && !(flags & WAIT_FOR_SUBMIT)) {
			err = -EINVAL;
		} else if (flags & WAIT_ALL) {
			if (flags & (WAIT_UNSUBMITTED | WAIT_SUBMITTED))
				err = -ETIME;
		} else {
			if (!(flags & WAIT_SIGNALED))
				err = -ETIME;
		}

		igt_subtest_f("%smulti-wait%s%s%s%s%s",
			      err == -EINVAL ? "invalid-" : "",
			      (flags & WAIT_ALL) ? "-all" : "",
			      (flags & WAIT_FOR_SUBMIT) ? "-for-submit" : "",
			      (flags & WAIT_UNSUBMITTED) ? "-unsubmitted" : "",
			      (flags & WAIT_SUBMITTED) ? "-submitted" : "",
			      (flags & WAIT_SIGNALED) ? "-signaled" : "")
			test_multi_wait(fd, flags, err);
	}

	igt_subtest("wait-any-snapshot")
		test_wait_snapshot(fd, 0);

	igt_subtest("wait-all-snapshot")
		test_wait_snapshot(fd, WAIT_ALL);

	igt_subtest("wait-for-submit-snapshot")
		test_wait_snapshot(fd, WAIT_FOR_SUBMIT);

	igt_subtest("wait-all-for-submit-snapshot")
		test_wait_snapshot(fd, WAIT_ALL | WAIT_FOR_SUBMIT);

	igt_subtest("wait-any-complex")
		test_wait_complex(fd, 0);

	igt_subtest("wait-all-complex")
		test_wait_complex(fd, WAIT_ALL);

	igt_subtest("wait-for-submit-complex")
		test_wait_complex(fd, WAIT_FOR_SUBMIT);

	igt_subtest("wait-all-for-submit-complex")
		test_wait_complex(fd, WAIT_ALL | WAIT_FOR_SUBMIT);

	igt_subtest("wait-any-interrupted")
		test_wait_interrupted(fd, 0);

	igt_subtest("wait-all-interrupted")
		test_wait_interrupted(fd, WAIT_ALL);
}
