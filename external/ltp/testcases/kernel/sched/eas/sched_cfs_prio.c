/*
 * Copyright (c) 2018 Google, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Some CFS tasks are started with different priorities. The tasks are CPU hogs
 * and affined to the same CPU. Their runtime is checked to see that it
 * corresponds to that which is expected given the task priorities.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>

#include "tst_test.h"
#include "tst_safe_file_ops.h"
#include "tst_safe_pthread.h"

#include "trace_parse.h"
#include "util.h"

#define TRACE_EVENTS "sched_switch"

static int cfs_task_tids[4];
/* If testing a nice value of -1, task_fn's use of nice() must be amended to
 * check for an error properly. */
static int nice_vals[] = { -15, -5, 5, 15 };
/* These come from sched_prio_to_weight in kernel/sched/core.c. */
static int prio_to_weight[] = { 29154, 3121, 335, 36 };

#define TEST_TASK_SECONDS 5
static void *task_fn(void *arg)
{
	int idx = (int *)arg - cfs_task_tids;

	cfs_task_tids[idx] = gettid();

	affine(0);
	if (nice(nice_vals[idx]) != nice_vals[idx]) {
		printf("Error calling nice(%d)\n", nice_vals[idx]);
		return NULL;
	}
	burn(TEST_TASK_SECONDS * USEC_PER_SEC, 0);
	return NULL;
}

#define LOWER_BOUND_PCT 80
#define UPPER_BOUND_PCT 105
#define LOWER_BOUND_US 20000
#define UPPER_BOUND_US 30000

int check_bounds(long long expected_us, long long runtime_us)
{
	int rv = 0;
	long long lower_bound, lower_bound_pct, lower_bound_us;
	long long upper_bound, upper_bound_pct, upper_bound_us;

	lower_bound_pct = (LOWER_BOUND_PCT / (100 * expected_us));
	lower_bound_us = (expected_us - LOWER_BOUND_US);
	if (lower_bound_us > lower_bound_pct)
		lower_bound = lower_bound_us;
	else
		lower_bound = lower_bound_pct;
	upper_bound_pct = (UPPER_BOUND_PCT / (100 * expected_us));
	upper_bound_us = (expected_us + UPPER_BOUND_US);
	if (upper_bound_us > upper_bound_pct)
		upper_bound = upper_bound_us;
	else
		upper_bound = upper_bound_pct;

	if (runtime_us < lower_bound) {
		printf("  lower bound of %lld ms not met\n",
		       lower_bound/1000);
		rv = 1;
	}
	if (runtime_us > upper_bound) {
		printf("  upper bound of %lld ms exceeded\n",
		       upper_bound/1000);
		rv = 1;
	}
	return rv;
}

static int parse_results(void)
{
	int i, j, weight_sum;
	int rv = 0;
	unsigned long long start_ts_us[4] = { 0, 0, 0, 0 };
	long long runtime_us[4] = { 0, 0, 0, 0 };
	long long expected_us[4];

	for (i = 0; i < num_trace_records; i++) {
		struct trace_sched_switch *t = trace[i].event_data;

		if (trace[i].event_type != TRACE_RECORD_SCHED_SWITCH)
			continue;

		for (j = 0; j < 4; j++) {
			if (t->prev_pid == cfs_task_tids[j]) {
				if (!start_ts_us[j]) {
					printf("Trace parse error, start_ts_us "
					       "unset at segment end!\n");
					return -1;
				}
				runtime_us[j] += TS_TO_USEC(trace[i].ts) -
					start_ts_us[j];
				start_ts_us[j] = 0;
			}
			if (t->next_pid == cfs_task_tids[j]) {
				if (start_ts_us[j]) {
					printf("Trace parse error, start_ts_us "
					       "already set at segment "
					       "start!\n");
					return -1;
				}
				start_ts_us[j] = TS_TO_USEC(trace[i].ts);
			}
		}
	}

	/*
	 * Task prio to weight values are defined in the kernel at the end of
	 * kernel/sched/core.c (as of 4.19).
	 * -15: 29154
	 *  -5: 3121
	 *   5: 335
	 *  15: 36
	 *  Sum of weights: 29154 + 3121 + 335 + 36 = 32646
	 *  Expected task runtime % (time with 5 second test):
	 *  29154/32646 = 89.3%, 4465ms
	 *  3121/32646 = 9.56%, 478ms
	 *  335/32646 = 1.02%, 51ms
	 *  36/32646 = 0.11%, 5.5ms
	 */

	weight_sum = 0;
	for (i = 0; i < 4; i++)
		weight_sum += prio_to_weight[i];
	for (i = 0; i < 4; i++) {
		/*
		 * Expected task runtime:
		 * (prio_to_weight[i] / weight_sum) * TEST_TASK_SECONDS
		 */
		expected_us[i] = TEST_TASK_SECONDS * USEC_PER_SEC;
		expected_us[i] *= prio_to_weight[i];
		expected_us[i] /= weight_sum;
	}

	printf("Task runtimes:\n");

	printf("Task a (nice -15): %8lld ms (expected %8lld ms)\n",
	       runtime_us[0] / 1000, expected_us[0] / 1000);
	rv |= check_bounds(expected_us[0], runtime_us[0]);

	printf("Task b (nice -5) : %8lld ms (expected %8lld ms)\n",
	       runtime_us[1] / 1000, expected_us[1] / 1000);
	rv |= check_bounds(expected_us[1], runtime_us[1]);

	printf("Task c (nice 5)  : %8lld ms (expected %8lld ms)\n",
	       runtime_us[2] / 1000, expected_us[2] / 1000);
	rv |= check_bounds(expected_us[2], runtime_us[2]);

	printf("Task d (nice 15) : %8lld ms (expected %8lld ms)\n",
	       runtime_us[3] / 1000, expected_us[3] / 1000);
	rv |= check_bounds(expected_us[3], runtime_us[3]);

	return rv;
}

#define NUM_TASKS 4
static void run(void)
{
	pthread_t tasks[NUM_TASKS];
	int i;

	printf("Running %d CFS tasks concurrently for %d sec\n",
	       NUM_TASKS, TEST_TASK_SECONDS);

	/* configure and enable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	SAFE_FILE_PRINTF(TRACING_DIR "buffer_size_kb", "16384");
	SAFE_FILE_PRINTF(TRACING_DIR "set_event", TRACE_EVENTS);
	SAFE_FILE_PRINTF(TRACING_DIR "trace", "\n");
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "1");

	for (i = 0; i < NUM_TASKS; i++)
		SAFE_PTHREAD_CREATE(&tasks[i], NULL, task_fn,
				    &cfs_task_tids[i]);
	for (i = 0; i < NUM_TASKS; i++)
		SAFE_PTHREAD_JOIN(tasks[i], NULL);

	/* disable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	LOAD_TRACE();

	if (parse_results())
		tst_res(TFAIL, "Task runtimes not within allowed margins "
			"of expected values.\n");
	else
		tst_res(TPASS, "Task runtimes within allowed margins "
			"of expected values.\n");
}

static struct tst_test test = {
	.test_all = run,
	.cleanup = trace_cleanup,
};
