/*
 * Copyright (c) 2018 Google, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * A DL task and a CFS task are created and affined to the same CPU. The CFS
 * task is a CPU hog. The latency to switch to the DL task (which should preempt
 * the CFS task immediately) is checked.
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

#include "lapi/syscalls.h"
#include "lapi/sched.h"
#include "trace_parse.h"
#include "util.h"

#define TRACE_EVENTS "sched_wakeup sched_switch"

#define MAX_EXEC_LATENCY_US 100

static int dl_task_tid;
static sem_t sem;

/*
 * It is not possible to restrict CPU affinity on SCHED_DEADLINE tasks, so we do
 * not force the CFS and DL tasks to be on the same CPU in this test. CPUsets
 * can be used to do this, perhaps implement that later.
 */

static void *dl_fn(void *arg LTP_ATTRIBUTE_UNUSED)
{
	struct sched_attr attr;

	attr.size = sizeof(attr);
	attr.sched_flags = 0;
	attr.sched_nice = 0;
	attr.sched_priority = 0;

	attr.sched_policy = SCHED_DEADLINE;
	attr.sched_runtime = 10000000;
	attr.sched_period = 30000000;
	attr.sched_deadline = 30000000;

	ERROR_CHECK(sched_setattr(0, &attr, 0));

	dl_task_tid = gettid();
	sem_wait(&sem);
	return NULL;
}

static void *cfs_fn(void *arg LTP_ATTRIBUTE_UNUSED)
{
	usleep(5000);
	SAFE_FILE_PRINTF(TRACING_DIR "trace_marker", "WAKING");
	sem_post(&sem);

	burn(USEC_PER_SEC, 0);
	return NULL;
}

static int parse_results(void)
{
	int i;
	unsigned long long dl_wakeup_ts_us = 0;
	unsigned long long dl_exec_ts_us = 0;
	unsigned long dl_exec_latency_us;

	for (i = 0; i < num_trace_records; i++) {
		if (trace[i].event_type == TRACE_RECORD_SCHED_WAKEUP) {
			struct trace_sched_wakeup *t = trace[i].event_data;
			if (t->pid != dl_task_tid)
				continue;
			dl_wakeup_ts_us = TS_TO_USEC(trace[i].ts);
			continue;
		}
		if (dl_wakeup_ts_us &&
		    trace[i].event_type == TRACE_RECORD_SCHED_SWITCH) {
			struct trace_sched_switch *t = trace[i].event_data;
			if (t->next_pid != dl_task_tid)
				continue;
			if (!dl_wakeup_ts_us) {
				printf("DL task woke without being woken!\n");
				return -1;
			}
			dl_exec_ts_us = TS_TO_USEC(trace[i].ts);
			break;
		}
	}
	if (!dl_wakeup_ts_us || !dl_exec_ts_us) {
		printf("DL task either wasn't woken or didn't wake up.\n");
		return -1;
	}
	dl_exec_latency_us = dl_exec_ts_us - dl_wakeup_ts_us;
	printf("DL exec latency: %ld usec\n", dl_exec_latency_us);
	return (dl_exec_latency_us > MAX_EXEC_LATENCY_US);
}

static void run(void)
{
	pthread_t cfs_thread;
	pthread_t dl_thread;
	pthread_attr_t cfs_thread_attrs;
	pthread_attr_t dl_thread_attrs;
	struct sched_param cfs_thread_sched_params;
	struct sched_param dl_thread_sched_params;

	ERROR_CHECK(pthread_attr_init(&cfs_thread_attrs));
	ERROR_CHECK(pthread_attr_setinheritsched(&cfs_thread_attrs,
						 PTHREAD_EXPLICIT_SCHED));
	ERROR_CHECK(pthread_attr_setschedpolicy(&cfs_thread_attrs,
						SCHED_OTHER));
	cfs_thread_sched_params.sched_priority = 0;
	ERROR_CHECK(pthread_attr_setschedparam(&cfs_thread_attrs,
					       &cfs_thread_sched_params));

	ERROR_CHECK(pthread_attr_init(&dl_thread_attrs));
	ERROR_CHECK(pthread_attr_setinheritsched(&dl_thread_attrs,
						 PTHREAD_EXPLICIT_SCHED));
	ERROR_CHECK(pthread_attr_setschedpolicy(&dl_thread_attrs,
						SCHED_FIFO));
	dl_thread_sched_params.sched_priority = 80;
	ERROR_CHECK(pthread_attr_setschedparam(&dl_thread_attrs,
					       &dl_thread_sched_params));

	sem_init(&sem, 0, 0);

	/* configure and enable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	SAFE_FILE_PRINTF(TRACING_DIR "buffer_size_kb", "16384");
	SAFE_FILE_PRINTF(TRACING_DIR "set_event", TRACE_EVENTS);
	SAFE_FILE_PRINTF(TRACING_DIR "trace", "\n");
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "1");

	SAFE_PTHREAD_CREATE(&cfs_thread, &cfs_thread_attrs, cfs_fn, NULL);
	SAFE_PTHREAD_CREATE(&dl_thread, &dl_thread_attrs, dl_fn, NULL);
	SAFE_PTHREAD_JOIN(cfs_thread, NULL);
	SAFE_PTHREAD_JOIN(dl_thread, NULL);

	/* disable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	LOAD_TRACE();

	if (parse_results())
		tst_res(TFAIL, "DL task did not execute within expected "
			"latency of %d usec.\n", MAX_EXEC_LATENCY_US);
	else
		tst_res(TPASS, "DL task executed within expected latency "
			"of %d usec.\n", MAX_EXEC_LATENCY_US);
}

static struct tst_test test = {
	.test_all = run,
	.cleanup = trace_cleanup,
};
