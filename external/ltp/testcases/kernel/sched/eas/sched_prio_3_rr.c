/*
 * Copyright (c) 2018 Google, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Three RT RR tasks are created and affined to the same CPU. They execute as
 * CPU hogs. Their runtime is checked to see that they share the CPU as
 * expected.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <time.h>

#include "tst_test.h"
#include "tst_safe_file_ops.h"
#include "tst_safe_pthread.h"

#include "trace_parse.h"
#include "util.h"

#define TRACE_EVENTS "sched_wakeup sched_switch sched_process_exit"

#define EXEC_MIN_PCT 33
#define EXEC_MAX_PCT 34

static sem_t sem;

static int rt_a_tid;
static int rt_b_tid;
static int rt_c_tid;

#define BUSY_WAIT_USECS 10000000
static void *rt_b_fn(void *arg LTP_ATTRIBUTE_UNUSED)
{
	rt_b_tid = gettid();
	affine(0);
	sem_wait(&sem);
	burn(BUSY_WAIT_USECS, 0);
	return NULL;
}

static void *rt_c_fn(void *arg LTP_ATTRIBUTE_UNUSED)
{
	rt_c_tid = gettid();
	affine(0);
	sem_wait(&sem);
	burn(BUSY_WAIT_USECS, 0);
	return NULL;
}

static void *rt_a_fn(void *arg LTP_ATTRIBUTE_UNUSED)
{
	rt_a_tid = gettid();
	affine(0);
	/* Give all other tasks a chance to affine and block. */
	usleep(3000);
	SAFE_FILE_PRINTF(TRACING_DIR "trace_marker", "TEST START");
	sem_post(&sem);
	sem_post(&sem);
	burn(BUSY_WAIT_USECS, 0);
	return NULL;
}

static int parse_results(void)
{
	int i, pct, rv;
	int test_start = 0;
	unsigned long long exec_start_us = 0;
	unsigned long a_exec_us = 0;
	unsigned long b_exec_us = 0;
	unsigned long c_exec_us = 0;
	unsigned long total;

	for (i = 0; i < num_trace_records; i++) {
		struct trace_sched_switch *t = trace[i].event_data;
		unsigned long long segment_us;

		if (trace[i].event_type == TRACE_RECORD_TRACING_MARK_WRITE &&
		    !strcmp(trace[i].event_data, "TEST START")) {
			/* We need to include this segment in A's exec time. */
			exec_start_us = TS_TO_USEC(trace[i].ts);
			test_start = 1;
		}

		if (!test_start)
			continue;

		if (trace[i].event_type != TRACE_RECORD_SCHED_SWITCH)
			continue;

		segment_us = TS_TO_USEC(trace[i].ts) - exec_start_us;
		if (t->prev_pid == rt_a_tid)
			a_exec_us += segment_us;
		else if (t->prev_pid == rt_b_tid)
			b_exec_us += segment_us;
		else if (t->prev_pid == rt_c_tid)
			c_exec_us += segment_us;
		if (t->next_pid == rt_a_tid ||
		    t->next_pid == rt_b_tid ||
		    t->next_pid == rt_c_tid)
			exec_start_us = TS_TO_USEC(trace[i].ts);
	}

	rv = 0;
	total = a_exec_us + b_exec_us + c_exec_us;
	pct = (a_exec_us * 100) / total;
	rv |= (pct < EXEC_MIN_PCT || pct > EXEC_MAX_PCT);
	printf("a exec time: %ld usec (%d%%)\n", a_exec_us, pct);
	pct = (b_exec_us * 100) / total;
	rv |= (pct < EXEC_MIN_PCT || pct > EXEC_MAX_PCT);
	printf("b exec time: %ld usec (%d%%)\n", b_exec_us, pct);
	pct = (c_exec_us * 100) / total;
	rv |= (pct < EXEC_MIN_PCT || pct > EXEC_MAX_PCT);
	printf("c exec time: %ld usec (%d%%)\n", c_exec_us, pct);

	return rv;
}

static void create_rt_thread(int prio, void *fn, pthread_t *rt_thread)
{
	pthread_attr_t rt_thread_attrs;
	struct sched_param rt_thread_sched_params;

	ERROR_CHECK(pthread_attr_init(&rt_thread_attrs));
	ERROR_CHECK(pthread_attr_setinheritsched(&rt_thread_attrs,
						 PTHREAD_EXPLICIT_SCHED));
	ERROR_CHECK(pthread_attr_setschedpolicy(&rt_thread_attrs,
						SCHED_RR));
	rt_thread_sched_params.sched_priority = prio;
	ERROR_CHECK(pthread_attr_setschedparam(&rt_thread_attrs,
					       &rt_thread_sched_params));

	SAFE_PTHREAD_CREATE(rt_thread, &rt_thread_attrs, fn, NULL);
}

#define NUM_TASKS 3
static void run(void)
{
	pthread_t rt_a, rt_b, rt_c;

	sem_init(&sem, 0, 0);

	printf("Running %d RT RR tasks for 10 seconds...\n", NUM_TASKS);

	/* configure and enable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	SAFE_FILE_PRINTF(TRACING_DIR "buffer_size_kb", "16384");
	SAFE_FILE_PRINTF(TRACING_DIR "set_event", TRACE_EVENTS);
	SAFE_FILE_PRINTF(TRACING_DIR "trace", "\n");
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "1");

	create_rt_thread(70, rt_a_fn, &rt_a);
	create_rt_thread(70, rt_b_fn, &rt_b);
	create_rt_thread(70, rt_c_fn, &rt_c);

	SAFE_PTHREAD_JOIN(rt_a, NULL);
	SAFE_PTHREAD_JOIN(rt_b, NULL);
	SAFE_PTHREAD_JOIN(rt_c, NULL);

	/* disable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	LOAD_TRACE();

	if (parse_results())
		tst_res(TFAIL, "RT RR tasks did not receive the expected CPU "
			"time (all between %d-%d %% CPU).\n", EXEC_MIN_PCT,
			EXEC_MAX_PCT);
	else
		tst_res(TPASS, "RT RR tasks received the expected CPU time.\n");
}

static struct tst_test test = {
	.test_all = run,
	.cleanup = trace_cleanup,
};
