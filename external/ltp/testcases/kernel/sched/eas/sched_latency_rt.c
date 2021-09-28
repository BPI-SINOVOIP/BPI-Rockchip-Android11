/*
 * Copyright (c) 2018 Google, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * A CFS task and an RT task are created and affined to the same CPU. The CFS
 * task is a CPU hog. The latency for the RT task to execute after it has been
 * woken is measured and verified.
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

#define TRACE_EVENTS "sched_wakeup sched_switch"

#define MAX_EXEC_LATENCY_US 100

static int rt_task_tid;
static sem_t sem;

static void *rt_fn(void *arg LTP_ATTRIBUTE_UNUSED)
{
	rt_task_tid = gettid();
	affine(0);
	sem_wait(&sem);
	return NULL;
}

#define BURN_MSEC 1000
static void *cfs_fn(void *arg LTP_ATTRIBUTE_UNUSED)
{
	affine(0);

	usleep(5000);
	SAFE_FILE_PRINTF(TRACING_DIR "trace_marker", "WAKING");
	sem_post(&sem);

	burn(USEC_PER_SEC, 0);
	return NULL;
}

static int parse_results(void)
{
	int i;
	unsigned long long rt_wakeup_ts_us = 0;
	unsigned long long rt_exec_ts_us = 0;
	unsigned long rt_exec_latency_us;

	for (i = 0; i < num_trace_records; i++) {
		if (trace[i].event_type == TRACE_RECORD_SCHED_WAKEUP) {
			struct trace_sched_wakeup *t = trace[i].event_data;
			if (t->pid != rt_task_tid)
				continue;
			rt_wakeup_ts_us = TS_TO_USEC(trace[i].ts);
			continue;
		}
		if (rt_wakeup_ts_us &&
		    trace[i].event_type == TRACE_RECORD_SCHED_SWITCH) {
			struct trace_sched_switch *t = trace[i].event_data;
			if (t->next_pid != rt_task_tid)
				continue;
			if (!rt_wakeup_ts_us) {
				printf("RT task woke without being woken!\n");
				return -1;
			}
			rt_exec_ts_us = TS_TO_USEC(trace[i].ts);
			break;
		}
	}
	if (!rt_wakeup_ts_us || !rt_exec_ts_us) {
		printf("RT task either wasn't woken or didn't wake up.\n");
		return -1;
	}
	rt_exec_latency_us = rt_exec_ts_us - rt_wakeup_ts_us;
	printf("RT exec latency: %ld usec\n", rt_exec_latency_us);
	return (rt_exec_latency_us > MAX_EXEC_LATENCY_US);
}

static void run(void)
{
	pthread_t cfs_thread;
	pthread_t rt_thread;
	pthread_attr_t cfs_thread_attrs;
	pthread_attr_t rt_thread_attrs;
	struct sched_param cfs_thread_sched_params;
	struct sched_param rt_thread_sched_params;

	ERROR_CHECK(pthread_attr_init(&cfs_thread_attrs));
	ERROR_CHECK(pthread_attr_setinheritsched(&cfs_thread_attrs,
						 PTHREAD_EXPLICIT_SCHED));
	ERROR_CHECK(pthread_attr_setschedpolicy(&cfs_thread_attrs,
						SCHED_OTHER));
	cfs_thread_sched_params.sched_priority = 0;
	ERROR_CHECK(pthread_attr_setschedparam(&cfs_thread_attrs,
					       &cfs_thread_sched_params));

	ERROR_CHECK(pthread_attr_init(&rt_thread_attrs));
	ERROR_CHECK(pthread_attr_setinheritsched(&rt_thread_attrs,
						 PTHREAD_EXPLICIT_SCHED));
	ERROR_CHECK(pthread_attr_setschedpolicy(&rt_thread_attrs,
						SCHED_FIFO));
	rt_thread_sched_params.sched_priority = 80;
	ERROR_CHECK(pthread_attr_setschedparam(&rt_thread_attrs,
					       &rt_thread_sched_params));

	sem_init(&sem, 0, 0);

	/* configure and enable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	SAFE_FILE_PRINTF(TRACING_DIR "buffer_size_kb", "16384");
	SAFE_FILE_PRINTF(TRACING_DIR "set_event", TRACE_EVENTS);
	SAFE_FILE_PRINTF(TRACING_DIR "trace", "\n");
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "1");

	SAFE_PTHREAD_CREATE(&cfs_thread, &cfs_thread_attrs, cfs_fn, NULL);
	SAFE_PTHREAD_CREATE(&rt_thread, &rt_thread_attrs, rt_fn, NULL);
	SAFE_PTHREAD_JOIN(cfs_thread, NULL);
	SAFE_PTHREAD_JOIN(rt_thread, NULL);

	/* disable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	LOAD_TRACE();

	if (parse_results())
		tst_res(TFAIL, "RT task did not execute within required "
			"latency of %d usec.\n", MAX_EXEC_LATENCY_US);
	else
		tst_res(TPASS, "RT task executed within required latency "
			"of %d usec..\n", MAX_EXEC_LATENCY_US);
}

static struct tst_test test = {
	.test_all = run,
	.cleanup = trace_cleanup,
};
