/*
 * Copyright (c) 2018 Google, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Six RT FIFO tasks are created and affined to the same CPU. They execute
 * with a particular pattern of overlapping eligibility to run. The resulting
 * execution pattern is checked to see that the tasks execute as expected given
 * their priorities.
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

static int rt_task_tids[6];

/*
 * Create two of each RT FIFO task at each priority level. Ensure that
 * - higher priority RT tasks preempt lower priority RT tasks
 * - newly woken RT tasks at the same priority level do not preempt currently
 *   running RT tasks
 *
 * Affine all tasks to CPU 0.
 * Have rt_low_fn 1 run first. It wakes up rt_low_fn 2, which should not run
 * until rt_low_fn 1 sleeps/exits.
 * rt_low_fn2 wakes rt_med_fn1. rt_med_fn1 should run immediately, then sleep,
 * allowing rt_low_fn2 to complete.
 * rt_med_fn1 wakes rt_med_fn2, which should not run until rt_med_fn 2
 * sleeps/exits... (etc)
 */
static sem_t sem_high_b;
static sem_t sem_high_a;
static sem_t sem_med_b;
static sem_t sem_med_a;
static sem_t sem_low_b;
static sem_t sem_low_a;

enum {
	RT_LOW_FN_A_TID = 0,
	RT_LOW_FN_B_TID,
	RT_MED_FN_A_TID,
	RT_MED_FN_B_TID,
	RT_HIGH_FN_A_TID,
	RT_HIGH_FN_B_TID,
};

struct expected_event {
	int event_type;
	/*
	 * If sched_wakeup, pid being woken.
	 * If sched_switch, pid being switched to.
	 */
	int event_data;
};
static struct expected_event events[] = {
	/* rt_low_fn_a wakes up rt_low_fn_b:
	 *   sched_wakeup(rt_low_fn_b) */
	{ .event_type = TRACE_RECORD_SCHED_WAKEUP,
		.event_data = RT_LOW_FN_B_TID},
	/* TODO: Expect an event for the exit of rt_low_fn_a. */
	/* 3ms goes by, then rt_low_fn_a exits and rt_low_fn_b starts running */
	{ .event_type = TRACE_RECORD_SCHED_SWITCH,
		.event_data = RT_LOW_FN_B_TID},
	/* rt_low_fn_b wakes rt_med_fn_a which runs immediately */
	{ .event_type = TRACE_RECORD_SCHED_WAKEUP,
		.event_data = RT_MED_FN_A_TID},
	{ .event_type = TRACE_RECORD_SCHED_SWITCH,
		.event_data = RT_MED_FN_A_TID},
	/* rt_med_fn_a sleeps, allowing rt_low_fn_b time to exit */
	{ .event_type = TRACE_RECORD_SCHED_SWITCH,
		.event_data = RT_LOW_FN_B_TID},
	/* TODO: Expect an event for the exit of rt_low_fn_b. */
	{ .event_type = TRACE_RECORD_SCHED_WAKEUP,
		.event_data = RT_MED_FN_A_TID},
	{ .event_type = TRACE_RECORD_SCHED_SWITCH,
		.event_data = RT_MED_FN_A_TID},
	/* rt_med_fn_a wakes rt_med_fn_b */
	{ .event_type = TRACE_RECORD_SCHED_WAKEUP,
		.event_data = RT_MED_FN_B_TID},
	/* 3ms goes by, then rt_med_fn_a exits and rt_med_fn_b starts running */
	/* TODO: Expect an event for the exit of rt_med_fn_a */
	{ .event_type = TRACE_RECORD_SCHED_SWITCH,
		.event_data = RT_MED_FN_B_TID},
	/* rt_med_fn_b wakes up rt_high_fn_a which runs immediately */
	{ .event_type = TRACE_RECORD_SCHED_WAKEUP,
		.event_data = RT_HIGH_FN_A_TID},
	{ .event_type = TRACE_RECORD_SCHED_SWITCH,
		.event_data = RT_HIGH_FN_A_TID},
	/* rt_high_fn_a sleeps, allowing rt_med_fn_b time to exit */
	{ .event_type = TRACE_RECORD_SCHED_SWITCH,
		.event_data = RT_MED_FN_B_TID},
	/* TODO: Expect an event for the exit of rt_med_fn_b */
	{ .event_type = TRACE_RECORD_SCHED_WAKEUP,
		.event_data = RT_HIGH_FN_A_TID},
	{ .event_type = TRACE_RECORD_SCHED_SWITCH,
		.event_data = RT_HIGH_FN_A_TID},
	/* rt_high_fn_a wakes up rt_high_fn_b */
	{ .event_type = TRACE_RECORD_SCHED_WAKEUP,
		.event_data = RT_HIGH_FN_B_TID},
	/* 3ms goes by, then rt_high_fn_a exits and rt_high_fn_b starts running */
	/* TODO: Expect an event for the exit of rt_high_fn_a */
	{ .event_type = TRACE_RECORD_SCHED_SWITCH,
		.event_data = RT_HIGH_FN_B_TID},
};

static void *rt_high_fn_b(void *arg LTP_ATTRIBUTE_UNUSED)
{
	rt_task_tids[RT_HIGH_FN_B_TID] = gettid();
	affine(0);

	/* Wait for rt_high_fn_a to wake us up. */
	sem_wait(&sem_high_b);
	/* Run after rt_high_fn_a exits. */

	return NULL;
}

static void *rt_high_fn_a(void *arg LTP_ATTRIBUTE_UNUSED)
{
	rt_task_tids[RT_HIGH_FN_A_TID] = gettid();
	affine(0);

	/* Wait for rt_med_fn_b to wake us up. */
	sem_wait(&sem_high_a);

	/* Sleep, allowing rt_med_fn_b a chance to exit. */
	usleep(1000);

	/* Wake up rt_high_fn_b. We should continue to run though. */
	sem_post(&sem_high_b);

	/* Busy wait for just a bit. */
	burn(3000, 0);

	return NULL;
}

static void *rt_med_fn_b(void *arg LTP_ATTRIBUTE_UNUSED)
{
	rt_task_tids[RT_MED_FN_B_TID] = gettid();
	affine(0);

	/* Wait for rt_med_fn_a to wake us up. */
	sem_wait(&sem_med_b);
	/* Run after rt_med_fn_a exits. */

	/* This will wake up rt_high_fn_a which will run immediately, preempting
	 * us. */
	sem_post(&sem_high_a);

	return NULL;
}

static void *rt_med_fn_a(void *arg LTP_ATTRIBUTE_UNUSED)
{
	rt_task_tids[RT_MED_FN_A_TID] = gettid();
	affine(0);

	/* Wait for rt_low_fn_b to wake us up. */
	sem_wait(&sem_med_a);

	/* Sleep, allowing rt_low_fn_b a chance to exit. */
	usleep(3000);

	/* Wake up rt_med_fn_b. We should continue to run though. */
	sem_post(&sem_med_b);

	/* Busy wait for just a bit. */
	burn(3000, 0);

	return NULL;
}

static void *rt_low_fn_b(void *arg LTP_ATTRIBUTE_UNUSED)
{
	rt_task_tids[RT_LOW_FN_B_TID] = gettid();
	affine(0);

	/* Wait for rt_low_fn_a to wake us up. */
	sem_wait(&sem_low_b);
	/* Run after rt_low_fn_a exits. */

	/* This will wake up rt_med_fn_a which will run immediately, preempting
	 * us. */
	sem_post(&sem_med_a);

	/* So the previous sem_post isn't actually causing a sched_switch
	 * to med_a immediately - this is running a bit longer and exiting.
	 * Delay here. */
	burn(1000, 0);

	return NULL;
}

/* Put real task tids into the expected events. */
static void fixup_expected_events(void)
{
	int i;
	int size = sizeof(events)/sizeof(struct expected_event);

	for (i = 0; i < size; i++)
		events[i].event_data = rt_task_tids[events[i].event_data];
}

static void *rt_low_fn_a(void *arg LTP_ATTRIBUTE_UNUSED)
{
	rt_task_tids[RT_LOW_FN_A_TID] = gettid();
	affine(0);

	/* Give all other tasks a chance to set their tids and block. */
	usleep(3000);

	fixup_expected_events();

	SAFE_FILE_PRINTF(TRACING_DIR "trace_marker", "TEST START");

	/* Wake up rt_low_fn_b. We should continue to run though. */
	sem_post(&sem_low_b);

	/* Busy wait for just a bit. */
	burn(3000, 0);

	return NULL;
}

/* Returns whether the given tid is a tid of one of the RT tasks in this
 * testcase. */
static int rt_tid(int tid)
{
	int i;
	for (i = 0; i < 6; i++)
		if (rt_task_tids[i] == tid)
			return 1;
	return 0;
}

static int parse_results(void)
{
	int i;
	int test_start = 0;
	int event_idx = 0;
	int events_size = sizeof(events)/sizeof(struct expected_event);

	for (i = 0; i < num_trace_records; i++) {
		if (trace[i].event_type == TRACE_RECORD_TRACING_MARK_WRITE &&
		    !strcmp(trace[i].event_data, "TEST START"))
			test_start = 1;

		if (!test_start)
			continue;

		if (trace[i].event_type != TRACE_RECORD_SCHED_WAKEUP &&
		    trace[i].event_type != TRACE_RECORD_SCHED_SWITCH)
			continue;

		if (trace[i].event_type == TRACE_RECORD_SCHED_SWITCH) {
			struct trace_sched_switch *t = trace[i].event_data;
			if (!rt_tid(t->next_pid))
				continue;
			if (events[event_idx].event_type !=
			    TRACE_RECORD_SCHED_SWITCH ||
			    events[event_idx].event_data !=
			    t->next_pid) {
				printf("Test case failed, expecting event "
				       "index %d type %d for tid %d, "
				       "got sched switch to tid %d\n",
				       event_idx,
				       events[event_idx].event_type,
				       events[event_idx].event_data,
				       t->next_pid);
				return -1;
			}
			event_idx++;
		}

		if (trace[i].event_type == TRACE_RECORD_SCHED_WAKEUP) {
			struct trace_sched_wakeup *t = trace[i].event_data;
			if (!rt_tid(t->pid))
				continue;
			if (events[event_idx].event_type !=
			    TRACE_RECORD_SCHED_WAKEUP ||
			    events[event_idx].event_data !=
			    t->pid) {
				printf("Test case failed, expecting event "
				       "index %d type %d for tid %d, "
				       "got sched wakeup to tid %d\n",
				       event_idx,
				       events[event_idx].event_type,
				       events[event_idx].event_data,
				       t->pid);
				return -1;
			}
			event_idx++;
		}

		if (event_idx == events_size)
			break;
	}

	if (event_idx != events_size) {
		printf("Test case failed, "
		       "did not complete all expected events.\n");
		printf("Next expected event: event type %d for tid %d\n",
		       events[event_idx].event_type,
		       events[event_idx].event_data);
		return -1;
	}

	return 0;
}

static void create_rt_thread(int prio, void *fn, pthread_t *rt_thread)
{
	pthread_attr_t rt_thread_attrs;
	struct sched_param rt_thread_sched_params;

	ERROR_CHECK(pthread_attr_init(&rt_thread_attrs));
	ERROR_CHECK(pthread_attr_setinheritsched(&rt_thread_attrs,
						 PTHREAD_EXPLICIT_SCHED));
	ERROR_CHECK(pthread_attr_setschedpolicy(&rt_thread_attrs,
						SCHED_FIFO));
	rt_thread_sched_params.sched_priority = prio;
	ERROR_CHECK(pthread_attr_setschedparam(&rt_thread_attrs,
					       &rt_thread_sched_params));

	SAFE_PTHREAD_CREATE(rt_thread, &rt_thread_attrs, fn, NULL);
}

static void run(void)
{
	pthread_t rt_low_a, rt_low_b;
	pthread_t rt_med_a, rt_med_b;
	pthread_t rt_high_a, rt_high_b;

	sem_init(&sem_high_b, 0, 0);
	sem_init(&sem_high_a, 0, 0);
	sem_init(&sem_med_b, 0, 0);
	sem_init(&sem_med_a, 0, 0);
	sem_init(&sem_low_b, 0, 0);
	sem_init(&sem_low_a, 0, 0);

	/* configure and enable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	SAFE_FILE_PRINTF(TRACING_DIR "buffer_size_kb", "16384");
	SAFE_FILE_PRINTF(TRACING_DIR "set_event", TRACE_EVENTS);
	SAFE_FILE_PRINTF(TRACING_DIR "trace", "\n");
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "1");

	create_rt_thread(70, rt_low_fn_a, &rt_low_a);
	create_rt_thread(70, rt_low_fn_b, &rt_low_b);
	create_rt_thread(75, rt_med_fn_a, &rt_med_a);
	create_rt_thread(75, rt_med_fn_b, &rt_med_b);
	create_rt_thread(80, rt_high_fn_a, &rt_high_a);
	create_rt_thread(80, rt_high_fn_b, &rt_high_b);

	SAFE_PTHREAD_JOIN(rt_low_a, NULL);
	SAFE_PTHREAD_JOIN(rt_low_b, NULL);
	SAFE_PTHREAD_JOIN(rt_med_a, NULL);
	SAFE_PTHREAD_JOIN(rt_med_b, NULL);
	SAFE_PTHREAD_JOIN(rt_high_a, NULL);
	SAFE_PTHREAD_JOIN(rt_high_b, NULL);

	/* disable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	LOAD_TRACE();

	if (parse_results())
		tst_res(TFAIL, "RT FIFO tasks did not execute in the expected "
			"pattern.\n");
	else
		tst_res(TPASS, "RT FIFO tasks executed in the expected "
			"pattern.\n");
}

static struct tst_test test = {
	.test_all = run,
	.cleanup = trace_cleanup,
};
