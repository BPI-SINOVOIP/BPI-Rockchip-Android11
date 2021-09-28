/*
 * Copyright (c) 2018 Google, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * A DL task runs. Its execution pattern is checked to see that the constraints
 * it has been given (runtime, period, deadline) are satisfied.
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

#define TRACE_EVENTS "sched_switch"

static int dl_task_tid;

/*
 * It is not possible to restrict CPU affinity on SCHED_DEADLINE tasks, so we do
 * not force the CFS and DL tasks to be on the same CPU in this test. CPUsets
 * can be used to do this, perhaps implement that later.
 */

static void *dl_fn(void *arg LTP_ATTRIBUTE_UNUSED)
{
	struct sched_attr attr;
	struct timespec ts;
	uint64_t now_usec, end_usec;

	attr.size = sizeof(attr);
	attr.sched_flags = 0;
	attr.sched_nice = 0;
	attr.sched_priority = 0;

	attr.sched_policy = SCHED_DEADLINE;
	attr.sched_runtime = 5000000;
	attr.sched_period = 20000000;
	attr.sched_deadline = 10000000;

	SAFE_FILE_PRINTF(TRACING_DIR "trace_marker", "DL START");
	ERROR_CHECK(sched_setattr(0, &attr, 0));

	dl_task_tid = gettid();

	if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
		printf("clock_gettime() reported an error\n");
		return NULL;
	}
	now_usec = (ts.tv_sec) * USEC_PER_SEC + (ts.tv_nsec / 1000);
	end_usec = now_usec + 3 * USEC_PER_SEC;
	while (now_usec < end_usec) {
		/* Run for 5ms */
		burn(5000, 0);

		/* Wait until next 20ms boundary. sched_yield() for DL tasks
		 * throttles the task until its next period. */
		sched_yield();
		if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
			printf("clock_gettime() reported an error\n");
			return NULL;
		}
		now_usec = (ts.tv_sec) * USEC_PER_SEC + (ts.tv_nsec / 1000);
	}

	return NULL;
}

static int parse_results(void)
{
	int i;
	unsigned long long next_period_ts_us = 0;
	unsigned long long next_deadline_ts_us = 0;
	unsigned long long start_ts_us = 0;
	unsigned long long period_exec_time_us = 0;
	int periods_parsed = 0;

	for (i = 0; i < num_trace_records; i++) {
		if (trace[i].event_type == TRACE_RECORD_SCHED_SWITCH) {
			struct trace_sched_switch *t = trace[i].event_data;
			if (t->prev_pid == dl_task_tid) {
				unsigned long long end_ts_us;
				if (!start_ts_us) {
					printf("Trace parse error, "
					       "start_ts_us = 0\n");
					return -1;
				}
				if (TS_TO_USEC(trace[i].ts) >
				    next_period_ts_us) {
					printf("Task ran past end of "
					       "period!\n");
					return -1;
				}
				end_ts_us = TS_TO_USEC(trace[i].ts);
				if (end_ts_us > next_deadline_ts_us)
					end_ts_us = next_deadline_ts_us;
				if (start_ts_us > next_deadline_ts_us)
					start_ts_us = next_deadline_ts_us;
				period_exec_time_us +=
					(end_ts_us - start_ts_us);
				start_ts_us = 0;
			}
		}
		if (next_period_ts_us &&
		    TS_TO_USEC(trace[i].ts) > next_period_ts_us) {
			if (start_ts_us) {
				printf("Task was running across period boundary!\n");
				return -1;
			}
			if (period_exec_time_us < 5000) {
				printf("Missed deadline at %llu!\n",
				       next_deadline_ts_us);
				return -1;
			}
			periods_parsed++;
			next_deadline_ts_us += 20000;
			next_period_ts_us += 20000;
		}
		if (trace[i].event_type == TRACE_RECORD_SCHED_SWITCH) {
			struct trace_sched_switch *t = trace[i].event_data;
			if (t->next_pid == dl_task_tid) {

				if (start_ts_us) {
					printf("Trace parse error, "
					       "start_ts_us != 0\n");
					return -1;
				}
				start_ts_us = TS_TO_USEC(trace[i].ts);
				if (!next_period_ts_us) {
					/*
					 * Initialize period and deadline the
					 * first time the DL task runs.
					 */
					next_period_ts_us = start_ts_us + 20000;
					next_deadline_ts_us = start_ts_us +
						10000;
				}
			}
		}
	}
	if (periods_parsed >= 149)
		printf("%d periods parsed successfully.\n", periods_parsed);
	else
		printf("Only %d periods parsed successfully.\n",
		       periods_parsed);
	return 0;
}

static void run(void)
{
	pthread_t dl_thread;
	pthread_attr_t dl_thread_attrs;
	struct sched_param dl_thread_sched_params;

	ERROR_CHECK(pthread_attr_init(&dl_thread_attrs));
	ERROR_CHECK(pthread_attr_setinheritsched(&dl_thread_attrs,
						 PTHREAD_EXPLICIT_SCHED));
	ERROR_CHECK(pthread_attr_setschedpolicy(&dl_thread_attrs,
						SCHED_FIFO));
	dl_thread_sched_params.sched_priority = 80;
	ERROR_CHECK(pthread_attr_setschedparam(&dl_thread_attrs,
					       &dl_thread_sched_params));

	/* configure and enable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	SAFE_FILE_PRINTF(TRACING_DIR "buffer_size_kb", "16384");
	SAFE_FILE_PRINTF(TRACING_DIR "set_event", TRACE_EVENTS);
	SAFE_FILE_PRINTF(TRACING_DIR "trace", "\n");
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "1");

	SAFE_PTHREAD_CREATE(&dl_thread, NULL, dl_fn, NULL);
	SAFE_PTHREAD_JOIN(dl_thread, NULL);

	/* disable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	LOAD_TRACE();

	if (parse_results())
		tst_res(TFAIL, "DL task did not execute as expected.\n");
	else
		tst_res(TPASS, "DL task ran as expected.\n");
}

static struct tst_test test = {
	.test_all = run,
	.cleanup = trace_cleanup,
};
