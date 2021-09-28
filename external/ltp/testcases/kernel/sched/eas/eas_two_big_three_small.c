/*
 * Copyright (c) 2018 Google, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Two big and three small tasks execute. Task placement is verified.
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

static int task_tids[5];

#define MAX_INCORRECT_CLUSTER_PCT 10
#define BURN_SEC 3
static void *task_fn(void *arg)
{
	int id = (int *)arg - task_tids;

	task_tids[id] = gettid();

	/* Tasks 0-2 are small, 3-4 are big. */
	burn(BURN_SEC * USEC_PER_SEC, id < 3 ? 1 : 0);

	return NULL;
}

static int parse_results(void)
{
	int i, j, pct, rv = 0;
	unsigned long long exec_start_us[5] = { 0, 0, 0, 0, 0 };
	unsigned long long incorrect_us[5] = { 0, 0, 0, 0, 0 };
	unsigned long long total_us[5] = { 0, 0, 0, 0, 0 };
	cpu_set_t cpuset;

	if (find_cpus_with_capacity(0, &cpuset)) {
		printf("Failed to find the CPUs in the little cluster.\n");
		return -1;
	}

	for (i = 0; i < num_trace_records; i++) {
		struct trace_sched_switch *t = trace[i].event_data;
		unsigned long long segment_us;

		if (trace[i].event_type != TRACE_RECORD_SCHED_SWITCH)
			continue;

		/* Is this the start of an execution segment? */
		for (j = 0; j < 5; j++) {
			if (t->next_pid != task_tids[j])
				continue;
			/* Start of execution segment for task j */
			if (exec_start_us[j]) {
				printf("Trace parse fail: double exec start\n");
				return -1;
			}
			exec_start_us[j] = TS_TO_USEC(trace[i].ts);
		}

		/* Is this the end of an execution segment? */
		for (j = 0; j < 5; j++) {
			if (t->prev_pid != task_tids[j])
				continue;
			/* End of execution segment for task j */
			segment_us = TS_TO_USEC(trace[i].ts);
			segment_us -= exec_start_us[j];
			exec_start_us[j] = 0;
			if (CPU_ISSET(trace[i].cpu, &cpuset) && j > 2)
				incorrect_us[j] += segment_us;
			if (!CPU_ISSET(trace[i].cpu, &cpuset) && j < 3)
				incorrect_us[j] += segment_us;
			total_us[j] += segment_us;

		}
	}

	for (i = 0; i < 3; i++) {
		pct = (incorrect_us[i] * 100) / total_us[i];
		rv |= (pct > MAX_INCORRECT_CLUSTER_PCT);
		printf("Total time little task scheduled: %lld Time scheduled "
		       "on big CPU: %lld (%d%%)\n", total_us[i],
		       incorrect_us[i], pct);
	}
	for (i = 3; i < 5; i++) {
		pct = (incorrect_us[i] * 100) / total_us[i];
		rv |= (pct > MAX_INCORRECT_CLUSTER_PCT);
		printf("Total time big task scheduled: %lld Time scheduled on "
		       "little CPU: %lld (%d%%)\n", total_us[i],
		       incorrect_us[i], pct);
	}

	return rv;
}

#define NUM_TASKS 5
static void run(void)
{
	int i;
	pthread_t tasks[NUM_TASKS];

	tst_res(TINFO, "Maximum incorrect cluster time percentage: %d%%",
		MAX_INCORRECT_CLUSTER_PCT);

	printf("Tasks running for %d sec\n", BURN_SEC);

	/* configure and enable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	SAFE_FILE_PRINTF(TRACING_DIR "buffer_size_kb", "16384");
	SAFE_FILE_PRINTF(TRACING_DIR "set_event", TRACE_EVENTS);
	SAFE_FILE_PRINTF(TRACING_DIR "trace", "\n");
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "1");

	for (i = 0; i < NUM_TASKS; i++)
		SAFE_PTHREAD_CREATE(&tasks[i], NULL, task_fn, &task_tids[i]);
	for (i = 0; i < NUM_TASKS; i++)
		SAFE_PTHREAD_JOIN(tasks[i], NULL);

	/* disable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	LOAD_TRACE();

	if (parse_results())
		tst_res(TFAIL, "Task placement goals were not met.\n");
	else
		tst_res(TPASS, "Task placement goals were met.\n");
}

static struct tst_test test = {
	.test_all = run,
	.cleanup = trace_cleanup,
};
