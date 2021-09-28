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
 *
 * Authors:
 *    Chris Wilson <chris@chris-wilson.co.uk>
 *
 */

/*
 * Testcase: boundary testing of read(drm_fd)
 */

#include "igt.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <pthread.h>
#include "drm.h"

IGT_TEST_DESCRIPTION("Call read(drm) and see if it behaves.");

static void sighandler(int sig)
{
}

static void assert_empty(int fd)
{
	struct pollfd pfd = {fd, POLLIN};
	do_or_die(poll(&pfd, 1, 0));
}

static void generate_event(int fd, enum pipe pipe)
{
	igt_assert(kmstest_get_vblank(fd, pipe, DRM_VBLANK_EVENT));
}

static void wait_for_event(int fd)
{
	struct pollfd pfd = {fd, POLLIN};
	igt_assert(poll(&pfd, 1, -1) == 1);
}

static int setup(int in, int nonblock)
{
	int fd;
	int ret = -1;

	alarm(0);

	fd = dup(in);
	if (fd != -1)
		ret = fcntl(fd, F_GETFL);
	if (ret != -1) {
		if (nonblock)
			ret |= O_NONBLOCK;
		else
			ret &= ~O_NONBLOCK;
		ret = fcntl(fd, F_SETFL, ret);
	}
	igt_require(ret != -1);

	assert_empty(fd);
	return fd;
}

static void teardown(int fd)
{
	alarm(0);
	assert_empty(fd);
	close(fd);
	errno = 0;
}

static void test_invalid_buffer(int in)
{
	int fd = setup(in, 0);

	alarm(1);

	igt_assert_eq(read(fd, (void *)-1, 4096), -1);
	igt_assert_eq(errno, EFAULT);

	teardown(fd);
}

static void test_fault_buffer(int in, enum pipe pipe)
{
	int fd = setup(in, 0);
	struct drm_mode_map_dumb arg;
	char *buf;

	memset(&arg, 0, sizeof(arg));
	arg.handle = kmstest_dumb_create(fd, 32, 32, 32, NULL, NULL);

	do_ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &arg);

	buf = mmap(0, 4096, PROT_WRITE, MAP_SHARED, fd, arg.offset);
	igt_assert(buf != MAP_FAILED);

	generate_event(fd, pipe);

	alarm(1);

	igt_assert(read(fd, buf, 4096) > 0);

	munmap(buf, 4096);
	teardown(fd);
}

static void test_empty(int in, int nonblock, int expected)
{
	char buffer[1024];
	int fd = setup(in, nonblock);

	alarm(1);
	igt_assert_eq(read(fd, buffer, sizeof(buffer)), -1);
	igt_assert_eq(errno, expected);

	teardown(fd);
}

static void test_short_buffer(int in, int nonblock, enum pipe pipe)
{
	char buffer[1024]; /* events are typically 32 bytes */
	int fd = setup(in, nonblock);

	generate_event(fd, pipe);
	generate_event(fd, pipe);

	wait_for_event(fd);

	alarm(3);

	igt_assert_eq(read(fd, buffer, 4), 0);
	igt_assert(read(fd, buffer, 40) > 0);
	igt_assert(read(fd, buffer, 40) > 0);

	teardown(fd);
}

struct short_buffer_wakeup {
	pthread_mutex_t mutex;
	pthread_cond_t send, recv;
	int counter;
	int done;
	int fd;
};

static void *thread_short_buffer_wakeup(void *arg)
{
	struct short_buffer_wakeup *w = arg;
	char buffer; /* events are typically 32 bytes */

	while (!w->done) {
		/* Short read, does not consume the event. */
		igt_assert_eq(read(w->fd, &buffer, sizeof(buffer)), 0);

		pthread_mutex_lock(&w->mutex);
		if (!--w->counter)
			pthread_cond_signal(&w->send);
		pthread_cond_wait(&w->recv, &w->mutex);
		pthread_mutex_unlock(&w->mutex);
	}

	return NULL;
}

static void test_short_buffer_wakeup(int in, enum pipe pipe)
{
	const int nt = sysconf(_SC_NPROCESSORS_ONLN) + 1;
	struct short_buffer_wakeup w = {
		.fd = setup(in, 0),
	};
	pthread_t t[nt];
	char buffer[1024]; /* events are typically 32 bytes */

	pthread_mutex_init(&w.mutex, NULL);
	pthread_cond_init(&w.send, NULL);
	pthread_cond_init(&w.recv, NULL);

	for (int n = 0; n < nt; n++)
		pthread_create(&t[n], NULL, thread_short_buffer_wakeup, &w);

	igt_until_timeout(30) {
		struct timespec tv;
		int err = 0;

		pthread_mutex_lock(&w.mutex);
		w.counter = nt;
		pthread_cond_broadcast(&w.recv);
		pthread_mutex_unlock(&w.mutex);

		/* Give each thread a chance to sleep in drm_read() */
		pthread_yield();

		/* One event should wake all threads as none consume */
		generate_event(w.fd, pipe);

		clock_gettime(CLOCK_REALTIME, &tv);
		tv.tv_sec += 5; /* Let's be very generous to the scheduler */

		pthread_mutex_lock(&w.mutex);
		while (w.counter && !err)
			err = pthread_cond_timedwait(&w.send, &w.mutex, &tv);
		pthread_mutex_unlock(&w.mutex);

		igt_assert_f(err == 0,
			     "Timed out waiting for drm_read() to wakeup on an event\n");

		/* No thread should consume the event */
		igt_assert(read(w.fd, buffer, sizeof(buffer)) > 0);
	}
	pthread_mutex_lock(&w.mutex);
	w.done = true;
	pthread_cond_broadcast(&w.recv);
	pthread_mutex_unlock(&w.mutex);

	for (int n = 0; n < nt; n++)
		pthread_join(t[n], NULL);

	close(w.fd);
}

igt_main
{
	int fd;
	igt_display_t display;
	struct igt_fb fb;
	enum pipe pipe;

	signal(SIGALRM, sighandler);
	siginterrupt(SIGALRM, 1);

	igt_fixture {
		igt_output_t *output;

		fd = drm_open_driver_master(DRIVER_ANY);
		kmstest_set_vt_graphics_mode();

		igt_display_require(&display, fd);
		igt_display_require_output(&display);

		for_each_pipe_with_valid_output(&display, pipe, output) {
			drmModeModeInfo *mode = igt_output_get_mode(output);

			igt_create_pattern_fb(fd, mode->hdisplay, mode->vdisplay,
					      DRM_FORMAT_XRGB8888,
					      LOCAL_DRM_FORMAT_MOD_NONE, &fb);

			igt_output_set_pipe(output, pipe);
			igt_plane_set_fb(igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY), &fb);
			break;
		}

		igt_display_commit2(&display, display.is_atomic ? COMMIT_ATOMIC : COMMIT_LEGACY);
		igt_require(kmstest_get_vblank(fd, pipe, 0));
	}

	igt_subtest("invalid-buffer")
		test_invalid_buffer(fd);

	igt_subtest("fault-buffer")
		test_fault_buffer(fd, pipe);

	igt_subtest("empty-block")
		test_empty(fd, 0, EINTR);

	igt_subtest("empty-nonblock")
		test_empty(fd, 1, EAGAIN);

	igt_subtest("short-buffer-block")
		test_short_buffer(fd, 0, pipe);

	igt_subtest("short-buffer-nonblock")
		test_short_buffer(fd, 1, pipe);

	igt_subtest("short-buffer-wakeup")
		test_short_buffer_wakeup(fd, pipe);
}
