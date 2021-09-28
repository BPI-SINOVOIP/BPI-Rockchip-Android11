/*
 * Copyright (c) 2018 Google, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * A single big task executes. Task placement and upmigration latency is
 * verified.
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

static int big_task_tid;

#define MAX_UPMIGRATE_LATENCY_US 100000
#define MIN_CORRECT_CLUSTER_PCT 90
#define BURN_SEC 3
static void *task_fn(void *arg LTP_ATTRIBUTE_UNUSED)
{
	big_task_tid = gettid();

	printf("Big task executing for %ds...\n", BURN_SEC);
	burn(BURN_SEC * USEC_PER_SEC, 0);

	return NULL;
}

static int parse_results(void)
{
	int i, pct;
	unsigned long long exec_start_us = 0;
	unsigned long long correct_us = 0;
	unsigned long long total_us = 0;
	unsigned long long start_ts_usec = 0;
	unsigned long long upmigration_ts_usec = 0;
	unsigned long long upmigration_latency_usec = 0;
	cpu_set_t cpuset;

	if (find_cpus_with_capacity(1, &cpuset)) {
		printf("Failed to find the CPUs in the big cluster.\n");
		return -1;
	}

	for (i = 0; i < num_trace_records; i++) {
		unsigned long long segment_us;
		struct trace_sched_switch *t = trace[i].event_data;

		if (trace[i].event_type != TRACE_RECORD_SCHED_SWITCH)
			continue;
		if (t->next_pid == big_task_tid) {
			/* Start of task execution segment. */
			if (exec_start_us) {
				printf("Trace parse fail: double exec start\n");
				return -1;
			}
			exec_start_us = TS_TO_USEC(trace[i].ts);
			if (!start_ts_usec)
				start_ts_usec = exec_start_us;
			if (!upmigration_ts_usec &&
			    CPU_ISSET(trace[i].cpu, &cpuset))
				upmigration_ts_usec = exec_start_us;
			continue;
		}
		if (t->prev_pid != big_task_tid)
			continue;
		/* End of task execution segment. */
		segment_us = TS_TO_USEC(trace[i].ts);
		segment_us -= exec_start_us;
		exec_start_us = 0;
		if (CPU_ISSET(trace[i].cpu, &cpuset))
			correct_us += segment_us;
		total_us += segment_us;
	}

	pct = (correct_us * 100) / total_us;
	printf("Total time task scheduled: %lld usec\n"
	       "Time scheduled on a big CPU: %lld usec (%d%%)\n",
	       total_us, correct_us, pct);

	if (!upmigration_ts_usec) {
		printf("Task never upmigrated!\n");
		return -1;
	}

	upmigration_latency_usec = upmigration_ts_usec - start_ts_usec;
	printf("Upmigration latency: %lld usec\n", upmigration_latency_usec);

	return (pct < MIN_CORRECT_CLUSTER_PCT ||
		upmigration_latency_usec > MAX_UPMIGRATE_LATENCY_US);
}

static void run(void)
{
	pthread_t task_thread;

	tst_res(TINFO, "Minimum correct cluster time percentage: %d%%",
		MIN_CORRECT_CLUSTER_PCT);
	tst_res(TINFO, "Maximum upmigration latency: %d usec",
		MAX_UPMIGRATE_LATENCY_US);

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
		tst_res(TFAIL, "Task placement/migration latency did not meet "
			"requirements.");
	else
		tst_res(TPASS, "Task placement/migration latency met "
			"requirements.");
}

static struct tst_test test = {
	.test_all = run,
	.cleanup = trace_cleanup,
};
