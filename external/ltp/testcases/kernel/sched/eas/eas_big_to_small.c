/*
 * Copyright (c) 2018 Google, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Task starts out as a CPU hog and then becomes small.
 * Task placement and downmigration latency are verified.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>

#include "tst_test.h"
#include "tst_safe_file_ops.h"
#include "tst_safe_pthread.h"

#include "trace_parse.h"
#include "util.h"

#define TRACE_EVENTS "sched_switch"

static int task_tid;

#define MAX_DOWNMIGRATE_LATENCY_US 100000
#define MAX_INCORRECT_CLUSTER_PCT 10
#define BURN_SEC 3
static void *task_fn(void *arg LTP_ATTRIBUTE_UNUSED)
{
	task_tid = gettid();

	printf("Big task executing for %ds...\n", BURN_SEC);
	burn(BURN_SEC * USEC_PER_SEC, 0);

	printf("Changing to small task...\n");
	SAFE_FILE_PRINTF(TRACING_DIR "trace_marker", "SMALL TASK");
	burn(BURN_SEC * USEC_PER_SEC, 1);

	return NULL;
}

static int parse_results(void)
{
	int i, pct, rv = 0;
	unsigned long long exec_start_us = 0;
	unsigned long long too_big_cpu_us = 0;
	unsigned long long too_small_cpu_us = 0;
	unsigned long long big_task_us = 0;
	unsigned long long small_task_us = 0;
	unsigned long long smalltask_ts_usec = 0;
	unsigned long long downmigrate_ts_usec = 0;
	unsigned long long downmigrate_latency_usec = 0;
	cpu_set_t cpuset;

	if (find_cpus_with_capacity(0, &cpuset)) {
		printf("Failed to find the CPUs in the little cluster.\n");
		return -1;
	}

	for (i = 0; i < num_trace_records; i++) {
		unsigned long long segment_us;
		struct trace_sched_switch *t = trace[i].event_data;

		if (trace[i].event_type == TRACE_RECORD_TRACING_MARK_WRITE &&
			   !strcmp(trace[i].event_data, "SMALL TASK")) {
			smalltask_ts_usec = TS_TO_USEC(trace[i].ts);
			continue;
		}

		if (trace[i].event_type != TRACE_RECORD_SCHED_SWITCH)
			continue;

		if (t->next_pid == task_tid) {
			/* Start of task execution segment. */
			if (exec_start_us) {
				printf("Trace parse fail: double exec start\n");
				return -1;
			}
			exec_start_us = TS_TO_USEC(trace[i].ts);
			if (smalltask_ts_usec && !downmigrate_ts_usec &&
			    CPU_ISSET(trace[i].cpu, &cpuset))
				downmigrate_ts_usec = exec_start_us;
			continue;
		}
		if (t->prev_pid != task_tid)
			continue;
		/* End of task execution segment. */
		segment_us = TS_TO_USEC(trace[i].ts);
		segment_us -= exec_start_us;
		exec_start_us = 0;
		if (CPU_ISSET(trace[i].cpu, &cpuset)) {
			/* Task is running on little CPUs. */
			if (!smalltask_ts_usec)
				too_small_cpu_us += segment_us;
		} else {
			/* Task is running on big CPUs. */
			if (smalltask_ts_usec) {
				/*
				 * Downmigration is accounted separately, so
				 * only record mis-scheduled time here if it
				 * happened after downmigration.
				 */
				if (downmigrate_ts_usec)
					too_big_cpu_us += segment_us;
			}
		}
		if (smalltask_ts_usec)
			small_task_us += segment_us;
		else
			big_task_us += segment_us;
	}

	pct = (too_small_cpu_us * 100) / big_task_us;
	rv |= (pct > MAX_INCORRECT_CLUSTER_PCT);
	printf("Time incorrectly scheduled on small when task was big: "
	       "%lld usec (%d%% of big task CPU time)\n", too_small_cpu_us,
	       pct);
	pct = (too_big_cpu_us * 100) / small_task_us;
	rv |= (pct > MAX_INCORRECT_CLUSTER_PCT);
	printf("Time incorrectly scheduled on big when task was small, after "
	       "downmigration: %lld usec (%d%% of small task CPU time)\n",
	       too_big_cpu_us, pct);

	if (downmigrate_ts_usec) {
		downmigrate_latency_usec = downmigrate_ts_usec -
			smalltask_ts_usec;
		printf("Downmigration latency: %lld usec\n",
		       downmigrate_latency_usec);
	} else {
		printf("Task never downmigrated!\n");
		downmigrate_latency_usec = UINT_MAX;
	}

	return (rv || downmigrate_latency_usec > MAX_DOWNMIGRATE_LATENCY_US);
}

static void run(void)
{
	pthread_t task_thread;

	tst_res(TINFO, "Maximum incorrect cluster time percentage: %d%%",
		MAX_INCORRECT_CLUSTER_PCT);
	tst_res(TINFO, "Maximum downmigration latency: %d usec",
		MAX_DOWNMIGRATE_LATENCY_US);

	/* configure and enable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	SAFE_FILE_PRINTF(TRACING_DIR "buffer_size_kb", "16384");
	SAFE_FILE_PRINTF(TRACING_DIR "set_event", TRACE_EVENTS);
	SAFE_FILE_PRINTF(TRACING_DIR "trace", "\n");
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "1");

	SAFE_PTHREAD_CREATE(&task_thread, NULL, task_fn, NULL);
	SAFE_PTHREAD_JOIN(task_thread, NULL);

	/* disable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	LOAD_TRACE();

	if (parse_results())
		tst_res(TFAIL, "Task placement/migration latency goals "
			"not met.\n");
	else
		tst_res(TPASS, "Task placement/migration latency goals "
			"met.\n");
}

static struct tst_test test = {
	.test_all = run,
	.cleanup = trace_cleanup,
};
