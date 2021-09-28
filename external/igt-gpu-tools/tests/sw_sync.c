/*
 * Copyright © 2016 Collabora, Ltd.
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
 *    Robert Foss <robert.foss@collabora.com>
 */

#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "igt.h"
#include "igt_aux.h"
#include "igt_primes.h"

#include "sw_sync.h"


IGT_TEST_DESCRIPTION("Test SW Sync Framework");

typedef struct {
	int timeline;
	uint32_t thread_id;
	_Atomic(uint32_t) *counter;
	sem_t *sem;
} data_t;

static void test_alloc_timeline(void)
{
	int timeline;

	timeline = sw_sync_timeline_create();
	close(timeline);
}

static void test_alloc_fence(void)
{
	int in_fence;
	int timeline;

	timeline = sw_sync_timeline_create();
	in_fence = sw_sync_timeline_create_fence(timeline, 0);

	close(in_fence);
	close(timeline);
}

static void test_alloc_fence_invalid_timeline(void)
{
	igt_assert_f(__sw_sync_timeline_create_fence(-1, 0) < 0,
	    "Did not fail to create fence on invalid timeline\n");
}

static void test_timeline_closed(void)
{
	int fence;
	int timeline;

	timeline = sw_sync_timeline_create();
	fence = sw_sync_timeline_create_fence(timeline, 1);

	close(timeline);
	igt_assert_f(sync_fence_wait(fence, 0) == 0,
		     "Failure waiting on unsignaled fence on closed timeline\n");
	igt_assert_f(sync_fence_status(fence) == -ENOENT,
		     "Failure in marking up an unsignaled fence on closed timeline\n");
}

static void test_timeline_closed_signaled(void)
{
	int fence;
	int timeline;

	timeline = sw_sync_timeline_create();
	fence = sw_sync_timeline_create_fence(timeline, 1);

	sw_sync_timeline_inc(timeline, 1);
	close(timeline);
	igt_assert_f(sync_fence_wait(fence, 0) == 0,
	             "Failure waiting on signaled fence for closed timeline\n");
}

static void test_alloc_merge_fence(void)
{
	int in_fence[2];
	int fence_merge;
	int timeline[2];

	timeline[0] = sw_sync_timeline_create();
	timeline[1] = sw_sync_timeline_create();

	in_fence[0] = sw_sync_timeline_create_fence(timeline[0], 1);
	in_fence[1] = sw_sync_timeline_create_fence(timeline[1], 1);
	fence_merge = sync_fence_merge(in_fence[1], in_fence[0]);

	close(in_fence[0]);
	close(in_fence[1]);
	close(fence_merge);
	close(timeline[0]);
	close(timeline[1]);
}

static void test_sync_busy(void)
{
	int fence;
	int timeline;
	int seqno;

	timeline = sw_sync_timeline_create();
	fence = sw_sync_timeline_create_fence(timeline, 5);

	/* Make sure that fence has not been signaled yet */
	igt_assert_f(sync_fence_wait(fence, 0) == -ETIME,
		     "Fence signaled early (timeline value 0, fence seqno 5)\n");

	/* Advance timeline from 0 -> 1 */
	sw_sync_timeline_inc(timeline, 1);

	/* Make sure that fence has not been signaled yet */
	igt_assert_f(sync_fence_wait(fence, 0) == -ETIME,
		     "Fence signaled early (timeline value 1, fence seqno 5)\n");

	/* Advance timeline from 1 -> 5: signaling the fence (seqno 5)*/
	sw_sync_timeline_inc(timeline, 4);
	igt_assert_f(sync_fence_wait(fence, 0) == 0,
		     "Fence not signaled (timeline value 5, fence seqno 5)\n");

	/* Go even further, and confirm wait still succeeds */
	sw_sync_timeline_inc(timeline, 5);
	igt_assert_f(sync_fence_wait(fence, 0) == 0,
		     "Fence not signaled (timeline value 10, fence seqno 5)\n");

	seqno = 10;
	for_each_prime_number(prime, 100) {
		int fence_prime;
		seqno += prime;

		fence_prime = sw_sync_timeline_create_fence(timeline, seqno);
		sw_sync_timeline_inc(timeline, prime);

		igt_assert_f(sync_fence_wait(fence_prime, 0) == 0,
			     "Fence not signaled during test of prime timeline increments\n");
		close(fence_prime);
	}

	close(fence);
	close(timeline);
}

static void test_sync_busy_fork_unixsocket(void)
{
	int fence;
	int timeline;
	int sv[2];

	igt_require(socketpair(AF_UNIX, SOCK_DGRAM, 0, sv) == 0);

	timeline = sw_sync_timeline_create();
	fence = sw_sync_timeline_create_fence(timeline, 1);

	igt_fork(child, 1) {
		/* Child process */
		int socket = sv[1];
		int socket_timeline;
		struct msghdr msg = {0};
		struct cmsghdr *cmsg;
		unsigned char *data;
		char m_buffer[256];
		char c_buffer[256];
		struct iovec io = { .iov_base = m_buffer, .iov_len = sizeof(m_buffer) };
		close(sv[0]);

		msg.msg_iov = &io;
		msg.msg_iovlen = 1;
		msg.msg_control = c_buffer;
		msg.msg_controllen = sizeof(c_buffer);

		igt_assert(recvmsg(socket, &msg, 0) > 0);

		cmsg = CMSG_FIRSTHDR(&msg);
		data = CMSG_DATA(cmsg);
		socket_timeline = *((int *) data);

		/* Advance timeline from 0 -> 1 */
		sw_sync_timeline_inc(socket_timeline, 1);
	}

	{
		/* Parent process */
		int socket = sv[0];
		struct cmsghdr *cmsg;
		struct iovec io = { .iov_base = (char *)"ABC", .iov_len = 3 };
		struct msghdr msg = { 0 };
		char buf[CMSG_SPACE(sizeof(timeline))];
		memset(buf, '\0', sizeof(buf));
		close(sv[1]);

		msg.msg_iov = &io;
		msg.msg_iovlen = 1;
		msg.msg_control = buf;
		msg.msg_controllen = sizeof(buf);

		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(timeline));

		*((int *) CMSG_DATA(cmsg)) = timeline;
		msg.msg_controllen = cmsg->cmsg_len;

		igt_assert_f(sync_fence_wait(fence, 0) == -ETIME,
			     "Fence signaled (it should not have been signalled yet)\n");

		igt_assert(sendmsg(socket, &msg, 0) > 0);

		igt_assert_f(sync_fence_wait(fence, 2*1000) == 0,
			     "Fence not signaled (timeline value 1 fence seqno 1)\n");
	}

	igt_waitchildren();

	close(fence);
	close(timeline);
}

static void test_sync_busy_fork(void)
{
	int timeline = sw_sync_timeline_create();
	int fence = sw_sync_timeline_create_fence(timeline, 1);

	igt_assert_f(sync_fence_wait(fence, 0) == -ETIME,
		     "Fence signaled (it should not have been signalled yet)\n");

	igt_fork(child, 1) {
		usleep(1*1000*1000);
		/* Advance timeline from 0 -> 1 */
		sw_sync_timeline_inc(timeline, 1);
	}

	igt_assert_f(sync_fence_wait(fence, 2*1000) == 0,
		     "Fence not signaled (timeline value 1 fence seqno 1)\n");

	igt_waitchildren();

	close(fence);
	close(timeline);
}

static void test_sync_merge_invalid(void)
{
	int in_fence;
	int fence_invalid;
	int fence_merge;
	int timeline;
	char tmppath[] = "/tmp/igt-XXXXXX";
	int skip = 0;

	timeline = sw_sync_timeline_create();
	in_fence = sw_sync_timeline_create_fence(timeline, 1);

	fence_invalid = -1;
	fence_merge = sync_fence_merge(in_fence, fence_invalid);
	igt_assert_f(fence_merge < 0, "Verify invalid fd (-1) handling");

	fence_invalid = drm_open_driver(DRIVER_ANY);
	fence_merge = sync_fence_merge(in_fence, fence_invalid);
	igt_assert_f(fence_merge < 0, "Verify invalid fd (device fd) handling");

	fence_invalid = mkstemp(tmppath);
	if (fence_invalid == -1) {
		skip = 1;
		goto out;
	}
	unlink(tmppath);
	fence_invalid = drm_open_driver(DRIVER_ANY);
	fence_merge = sync_fence_merge(in_fence, fence_invalid);
	close(fence_invalid);
	igt_assert_f(fence_merge < 0, "Verify invalid fd (file fd) handling");

out:
	close(in_fence);
	close(fence_merge);
	close(timeline);
	igt_require(skip == 0);
}

static void test_sync_merge(void)
{
	int in_fence[3];
	int fence_merge;
	int timeline;
	int active, signaled;

	timeline = sw_sync_timeline_create();
	in_fence[0] = sw_sync_timeline_create_fence(timeline, 1);
	in_fence[1] = sw_sync_timeline_create_fence(timeline, 2);
	in_fence[2] = sw_sync_timeline_create_fence(timeline, 3);

	fence_merge = sync_fence_merge(in_fence[0], in_fence[1]);
	fence_merge = sync_fence_merge(in_fence[2], fence_merge);

	/* confirm all fences have one active point (even d) */
	active = sync_fence_count_status(in_fence[0],
					    SW_SYNC_FENCE_STATUS_ACTIVE);
	igt_assert_f(active == 1, "in_fence[0] has too many active fences\n");
	active = sync_fence_count_status(in_fence[1],
					    SW_SYNC_FENCE_STATUS_ACTIVE);
	igt_assert_f(active == 1, "in_fence[1] has too many active fences\n");
	active = sync_fence_count_status(in_fence[2],
					    SW_SYNC_FENCE_STATUS_ACTIVE);
	igt_assert_f(active == 1, "in_fence[2] has too many active fences\n");
	active = sync_fence_count_status(fence_merge,
					    SW_SYNC_FENCE_STATUS_ACTIVE);
	igt_assert_f(active == 1, "fence_merge has too many active fences\n");

	/* confirm that fence_merge is not signaled until the max of fence 0,1,2 */
	sw_sync_timeline_inc(timeline, 1);
	signaled = sync_fence_count_status(in_fence[0],
					      SW_SYNC_FENCE_STATUS_SIGNALED);
	active = sync_fence_count_status(fence_merge,
					    SW_SYNC_FENCE_STATUS_ACTIVE);
	igt_assert_f(signaled == 1, "in_fence[0] did not signal\n");
	igt_assert_f(active == 1, "fence_merge signaled too early\n");

	sw_sync_timeline_inc(timeline, 1);
	signaled = sync_fence_count_status(in_fence[1],
					      SW_SYNC_FENCE_STATUS_SIGNALED);
	active = sync_fence_count_status(fence_merge,
					    SW_SYNC_FENCE_STATUS_ACTIVE);
	igt_assert_f(signaled == 1, "in_fence[1] did not signal\n");
	igt_assert_f(active == 1, "fence_merge signaled too early\n");

	sw_sync_timeline_inc(timeline, 1);
	signaled = sync_fence_count_status(in_fence[2],
					      SW_SYNC_FENCE_STATUS_SIGNALED);
	igt_assert_f(signaled == 1, "in_fence[2] did not signal\n");
	signaled = sync_fence_count_status(fence_merge,
					       SW_SYNC_FENCE_STATUS_SIGNALED);
	active = sync_fence_count_status(fence_merge,
					    SW_SYNC_FENCE_STATUS_ACTIVE);
	igt_assert_f(active == 0 && signaled == 1,
		     "fence_merge did not signal\n");

	close(in_fence[0]);
	close(in_fence[1]);
	close(in_fence[2]);
	close(fence_merge);
	close(timeline);
}

static void test_sync_merge_same(void)
{
	int in_fence[2];
	int timeline;
	int signaled;

	timeline = sw_sync_timeline_create();
	in_fence[0] = sw_sync_timeline_create_fence(timeline, 1);
	in_fence[1] = sync_fence_merge(in_fence[0], in_fence[0]);

	signaled = sync_fence_count_status(in_fence[0],
					      SW_SYNC_FENCE_STATUS_SIGNALED);
	igt_assert_f(signaled == 0, "Fence signaled too early\n");

	sw_sync_timeline_inc(timeline, 1);
	signaled = sync_fence_count_status(in_fence[0],
					      SW_SYNC_FENCE_STATUS_SIGNALED);
	igt_assert_f(signaled == 1, "Fence did not signal\n");

	close(in_fence[0]);
	close(in_fence[1]);
	close(timeline);
}

static void test_sync_multi_timeline_wait(void)
{
	int timeline[3];
	int in_fence[3];
	int fence_merge;
	int active, signaled;

	timeline[0] = sw_sync_timeline_create();
	timeline[1] = sw_sync_timeline_create();
	timeline[2] = sw_sync_timeline_create();

	in_fence[0] = sw_sync_timeline_create_fence(timeline[0], 5);
	in_fence[1] = sw_sync_timeline_create_fence(timeline[1], 5);
	in_fence[2] = sw_sync_timeline_create_fence(timeline[2], 5);

	fence_merge = sync_fence_merge(in_fence[0], in_fence[1]);
	fence_merge = sync_fence_merge(in_fence[2], fence_merge);

	/* Confirm fence isn't signaled */
	active = sync_fence_count_status(fence_merge,
					    SW_SYNC_FENCE_STATUS_ACTIVE);
	igt_assert_f(active == 3, "Fence signaled too early\n");

	igt_assert_f(sync_fence_wait(fence_merge, 0) == -ETIME,
		     "Failure waiting on fence until timeout\n");

	sw_sync_timeline_inc(timeline[0], 5);
	active = sync_fence_count_status(fence_merge,
					    SW_SYNC_FENCE_STATUS_ACTIVE);
	signaled = sync_fence_count_status(fence_merge,
					      SW_SYNC_FENCE_STATUS_SIGNALED);
	igt_assert_f(active == 2 && signaled == 1,
		    "Fence did not signal properly\n");

	sw_sync_timeline_inc(timeline[1], 5);
	active = sync_fence_count_status(fence_merge,
					    SW_SYNC_FENCE_STATUS_ACTIVE);
	signaled = sync_fence_count_status(fence_merge,
					      SW_SYNC_FENCE_STATUS_SIGNALED);
	igt_assert_f(active == 1 && signaled == 2,
		    "Fence did not signal properly\n");

	sw_sync_timeline_inc(timeline[2], 5);
	active = sync_fence_count_status(fence_merge,
					    SW_SYNC_FENCE_STATUS_ACTIVE);
	signaled = sync_fence_count_status(fence_merge,
					      SW_SYNC_FENCE_STATUS_SIGNALED);
	igt_assert_f(active == 0 && signaled == 3,
		     "Fence did not signal properly\n");

	/* confirm you can successfully wait */
	igt_assert_f(sync_fence_wait(fence_merge, 100) == 0,
		     "Failure waiting on signaled fence\n");

	close(in_fence[0]);
	close(in_fence[1]);
	close(in_fence[2]);
	close(fence_merge);
	close(timeline[0]);
	close(timeline[1]);
	close(timeline[2]);
}

#define MULTI_CONSUMER_THREADS 8
#define MULTI_CONSUMER_ITERATIONS (1 << 14)
static void * test_sync_multi_consumer_thread(void *arg)
{
	data_t *data = arg;
	int thread_id = data->thread_id;
	int timeline = data->timeline;
	int i;

	for (i = 0; i < MULTI_CONSUMER_ITERATIONS; i++) {
		int next_point = i * MULTI_CONSUMER_THREADS + thread_id;
		int fence = sw_sync_timeline_create_fence(timeline, next_point);

		if (sync_fence_wait(fence, 1000) < 0)
			return (void *) 1;

		if (READ_ONCE(*data->counter) != next_point)
			return (void *) 1;

		sem_post(data->sem);
		close(fence);
	}
	return NULL;
}

static void test_sync_multi_consumer(void)
{

	data_t data_arr[MULTI_CONSUMER_THREADS];
	pthread_t thread_arr[MULTI_CONSUMER_THREADS];
	sem_t sem;
	int timeline;
	_Atomic(uint32_t) counter = 0;
	uintptr_t thread_ret = 0;
	data_t data;
	int i, ret;

	sem_init(&sem, 0, 0);
	timeline = sw_sync_timeline_create();

	data.counter = &counter;
	data.timeline = timeline;
	data.sem = &sem;

	/* Start sync threads. */
	for (i = 0; i < MULTI_CONSUMER_THREADS; i++)
	{
		data_arr[i] = data;
		data_arr[i].thread_id = i;
		ret = pthread_create(&thread_arr[i], NULL,
				     test_sync_multi_consumer_thread,
				     (void *) &(data_arr[i]));
		igt_assert_eq(ret, 0);
	}

	/* Produce 'content'. */
	for (i = 0; i < MULTI_CONSUMER_THREADS * MULTI_CONSUMER_ITERATIONS; i++)
	{
		sem_wait(&sem);

		atomic_fetch_add(&counter, 1);
		sw_sync_timeline_inc(timeline, 1);
	}

	/* Wait for threads to complete. */
	for (i = 0; i < MULTI_CONSUMER_THREADS; i++)
	{
		uintptr_t local_thread_ret;
		pthread_join(thread_arr[i], (void **)&local_thread_ret);
		thread_ret |= local_thread_ret;
	}

	close(timeline);
	sem_destroy(&sem);

	igt_assert_eq(counter,
		      MULTI_CONSUMER_THREADS * MULTI_CONSUMER_ITERATIONS);

	igt_assert_f(thread_ret == 0, "A sync thread reported failure.\n");
}

#define MULTI_CONSUMER_PRODUCER_THREADS 8
#define MULTI_CONSUMER_PRODUCER_ITERATIONS (1 << 14)
static void * test_sync_multi_consumer_producer_thread(void *arg)
{
	data_t *data = arg;
	int thread_id = data->thread_id;
	int timeline = data->timeline;
	int i;

	for (i = 0; i < MULTI_CONSUMER_PRODUCER_ITERATIONS; i++) {
		int next_point = i * MULTI_CONSUMER_PRODUCER_THREADS + thread_id;
		int fence = sw_sync_timeline_create_fence(timeline, next_point);

		if (sync_fence_wait(fence, 1000) < 0)
			return (void *) 1;

		if (atomic_fetch_add(data->counter, 1) != next_point)
			return (void *) 1;

		/* Kick off the next thread. */
		sw_sync_timeline_inc(timeline, 1);

		close(fence);
	}
	return NULL;
}

static void test_sync_multi_consumer_producer(void)
{
	data_t data_arr[MULTI_CONSUMER_PRODUCER_THREADS];
	pthread_t thread_arr[MULTI_CONSUMER_PRODUCER_THREADS];
	int timeline;
	_Atomic(uint32_t) counter = 0;
	uintptr_t thread_ret = 0;
	data_t data;
	int i, ret;

	timeline = sw_sync_timeline_create();

	data.counter = &counter;
	data.timeline = timeline;

	/* Start consumer threads. */
	for (i = 0; i < MULTI_CONSUMER_PRODUCER_THREADS; i++)
	{
		data_arr[i] = data;
		data_arr[i].thread_id = i;
		ret = pthread_create(&thread_arr[i], NULL,
				     test_sync_multi_consumer_producer_thread,
				     (void *) &(data_arr[i]));
		igt_assert_eq(ret, 0);
	}

	/* Wait for threads to complete. */
	for (i = 0; i < MULTI_CONSUMER_PRODUCER_THREADS; i++)
	{
		uintptr_t local_thread_ret;
		pthread_join(thread_arr[i], (void **)&local_thread_ret);
		thread_ret |= local_thread_ret;
	}

	close(timeline);

	igt_assert_eq(counter,
		      MULTI_CONSUMER_PRODUCER_THREADS *
		      MULTI_CONSUMER_PRODUCER_ITERATIONS);

	igt_assert_f(thread_ret == 0, "A sync thread reported failure.\n");
}

static int test_mspc_wait_on_fence(int fence)
{
	int error, active;

	do {
		error = sync_fence_count_status(fence,
						   SW_SYNC_FENCE_STATUS_ERROR);
		igt_assert_f(error == 0, "Error occurred on fence\n");
		active = sync_fence_count_status(fence,
						    SW_SYNC_FENCE_STATUS_ACTIVE);
	} while (active);

	return 0;
}

static struct {
	int iterations;
	int threads;
	int counter;
	int cons_timeline;
	int *prod_timeline;
	pthread_mutex_t lock;
} test_mpsc_data;

static void *mpsc_producer_thread(void *d)
{
	int id = (long)d;
	int fence, i;
	int *prod_timeline = test_mpsc_data.prod_timeline;
	int cons_timeline = test_mpsc_data.cons_timeline;
	int iterations = test_mpsc_data.iterations;

	for (i = 0; i < iterations; i++) {
		fence = sw_sync_timeline_create_fence(cons_timeline, i);

		/* Wait for the consumer to finish. Use alternate
		 * means of waiting on the fence
		 */
		if ((iterations + id) % 8 != 0) {
			igt_assert_f(sync_fence_wait(fence, -1) == 0,
				     "Failure waiting on fence\n");
		} else {
			igt_assert_f(test_mspc_wait_on_fence(fence) == 0,
				     "Failure waiting on fence\n");
		}

		/* Every producer increments the counter, the consumer
		 * checks and erases it
		 */
		pthread_mutex_lock(&test_mpsc_data.lock);
		test_mpsc_data.counter++;
		pthread_mutex_unlock(&test_mpsc_data.lock);

		sw_sync_timeline_inc(prod_timeline[id], 1);
		close(fence);
	}

	return NULL;
}

static int mpsc_consumer_thread(void)
{
	int fence, merged, tmp, it, i;
	int *prod_timeline = test_mpsc_data.prod_timeline;
	int cons_timeline = test_mpsc_data.cons_timeline;
	int iterations = test_mpsc_data.iterations;
	int n = test_mpsc_data.threads;

	for (it = 1; it <= iterations; it++) {
		fence = sw_sync_timeline_create_fence(prod_timeline[0], it);
		for (i = 1; i < n; i++) {
			tmp = sw_sync_timeline_create_fence(prod_timeline[i], it);
			merged = sync_fence_merge(tmp, fence);
			close(tmp);
			close(fence);
			fence = merged;
		}

		/* Make sure we see an increment from every producer thread.
		 * Vary the means by which we wait.
		 */
		if (iterations % 8 != 0) {
			igt_assert_f(sync_fence_wait(fence, -1) == 0,
				    "Producers did not increment as expected\n");
		} else {
			igt_assert_f(test_mspc_wait_on_fence(fence) == 0,
				     "Failure waiting on fence\n");
		}

		igt_assert_f(test_mpsc_data.counter == n * it,
			     "Counter value mismatch\n");

		/* Release the producer threads */
		sw_sync_timeline_inc(cons_timeline, 1);
		close(fence);
	}

	return 0;
}

/* IMPORTANT NOTE: if you see this test failing on your system, it may be
 * due to a shortage of file descriptors. Please ensure your system has
 * a sensible limit for this test to finish correctly.
 */
static void test_sync_multi_producer_single_consumer(void)
{
	int iterations = 1 << 12;
	int n = 5;
	int prod_timeline[n];
	int cons_timeline;
	pthread_t threads[n];
	long i;

	cons_timeline = sw_sync_timeline_create();
	for (i = 0; i < n; i++)
		prod_timeline[i] = sw_sync_timeline_create();

	test_mpsc_data.prod_timeline = prod_timeline;
	test_mpsc_data.cons_timeline = cons_timeline;
	test_mpsc_data.iterations = iterations;
	test_mpsc_data.threads = n;
	test_mpsc_data.counter = 0;
	pthread_mutex_init(&test_mpsc_data.lock, NULL);

	for (i = 0; i < n; i++) {
		pthread_create(&threads[i], NULL, (void * (*)(void *))
			       mpsc_producer_thread,
			       (void *)i);
	}

	mpsc_consumer_thread();

	for (i = 0; i < n; i++)
		pthread_join(threads[i], NULL);
}

static void test_sync_expired_merge(void)
{
	int iterations = 1 << 20;
	int timeline;
	int i;
	int fence_expired, fence_merged;

	timeline = sw_sync_timeline_create();

	sw_sync_timeline_inc(timeline, 100);
	fence_expired = sw_sync_timeline_create_fence(timeline, 1);
	igt_assert_f(sync_fence_wait(fence_expired, 0) == 0,
	             "Failure waiting for expired fence\n");

	fence_merged = sync_fence_merge(fence_expired, fence_expired);
	close(fence_merged);

	for (i = 0; i < iterations; i++) {
		int fence = sync_fence_merge(fence_expired, fence_expired);

		igt_assert_f(sync_fence_wait(fence, -1) == 0,
			     "Failure waiting on fence\n");
		close(fence);
	}

	close(fence_expired);
}

static void test_sync_random_merge(void)
{
	int i, size;
	const int nbr_timeline = 32;
	const int nbr_merge = 1024;
	int fence_map[nbr_timeline];
	int timeline_arr[nbr_timeline];
	int fence, tmpfence, merged;
	int timeline, timeline_offset, sync_pt;

	srand(time(NULL));

	for (i = 0; i < nbr_timeline; i++) {
		timeline_arr[i] = sw_sync_timeline_create();
		fence_map[i] = -1;
	}

	sync_pt = rand();
	fence = sw_sync_timeline_create_fence(timeline_arr[0], sync_pt);

	fence_map[0] = sync_pt;

	/* Randomly create syncpoints out of a fixed set of timelines,
	 * and merge them together.
	 */
	for (i = 0; i < nbr_merge; i++) {
		/* Generate syncpoint. */
		timeline_offset = rand() % nbr_timeline;
		timeline = timeline_arr[timeline_offset];
		sync_pt = rand();

		/* Keep track of the latest sync_pt in each timeline. */
		if (fence_map[timeline_offset] == -1)
			fence_map[timeline_offset] = sync_pt;
		else if (fence_map[timeline_offset] < sync_pt)
			fence_map[timeline_offset] = sync_pt;

		/* Merge. */
		tmpfence = sw_sync_timeline_create_fence(timeline, sync_pt);
		merged = sync_fence_merge(tmpfence, fence);
		close(tmpfence);
		close(fence);
		fence = merged;
	}

	size = 0;
	for (i = 0; i < nbr_timeline; i++)
		if (fence_map[i] != -1)
			size++;

	/* Trigger the merged fence. */
	for (i = 0; i < nbr_timeline; i++) {
		if (fence_map[i] != -1) {
			igt_assert_f(sync_fence_wait(fence, 0) == -ETIME,
				    "Failure waiting on fence until timeout\n");
			/* Increment the timeline to the last sync_pt */
			sw_sync_timeline_inc(timeline_arr[i], fence_map[i]);
		}
	}

	/* Check that the fence is triggered. */
	igt_assert_f(sync_fence_wait(fence, 1) == 0,
		     "Failure triggering fence\n");

	close(fence);
	for (i = 0; i < nbr_timeline; i++)
		close(timeline_arr[i]);
}

igt_main
{
	igt_fixture
		igt_require_sw_sync();

	igt_subtest("alloc_timeline")
		test_alloc_timeline();

	igt_subtest("alloc_fence")
		test_alloc_fence();

	igt_subtest("alloc_fence_invalid_timeline")
		test_alloc_fence_invalid_timeline();

	igt_subtest("timeline_closed")
		test_timeline_closed();

	igt_subtest("timeline_closed_signaled")
		test_timeline_closed_signaled();

	igt_subtest("alloc_merge_fence")
		test_alloc_merge_fence();

	igt_subtest("sync_busy")
		test_sync_busy();

	igt_subtest("sync_busy_fork")
		test_sync_busy_fork();

	igt_subtest("sync_busy_fork_unixsocket")
		test_sync_busy_fork_unixsocket();

	igt_subtest("sync_merge_invalid")
		test_sync_merge_invalid();

	igt_subtest("sync_merge")
		test_sync_merge();

	igt_subtest("sync_merge_same")
		test_sync_merge_same();

	igt_subtest("sync_multi_timeline_wait")
		test_sync_multi_timeline_wait();

	igt_subtest("sync_multi_consumer")
		test_sync_multi_consumer();

	igt_subtest("sync_multi_consumer_producer")
		test_sync_multi_consumer_producer();

	igt_subtest("sync_multi_producer_single_consumer")
		test_sync_multi_producer_single_consumer();

	igt_subtest("sync_expired_merge")
		test_sync_expired_merge();

	igt_subtest("sync_random_merge")
		test_sync_random_merge();
}
