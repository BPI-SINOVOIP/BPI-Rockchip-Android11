/*
 * Copyright (c) 2018 Google, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This test looks for a high number of wakeups from the schedutil governor
 * threads.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <time.h>

#include "tst_test.h"
#include "tst_safe_file_ops.h"
#include "tst_safe_pthread.h"

#include "trace_parse.h"

#define TRACE_EVENTS "sched_switch"

#define MAX_WAKEUPS 100

#define SLEEP_SEC 10
static void run(void)
{
	int i;
	int num_sugov_wakeups = 0;

	tst_res(TINFO, "Observing sugov wakeups over %d sec, "
		"%d wakeups allowed\n", SLEEP_SEC, MAX_WAKEUPS);

	/* configure and enable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	SAFE_FILE_PRINTF(TRACING_DIR "buffer_size_kb", "16384");
	SAFE_FILE_PRINTF(TRACING_DIR "set_event", TRACE_EVENTS);
	SAFE_FILE_PRINTF(TRACING_DIR "trace", "\n");
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "1");

	sleep(SLEEP_SEC);

	/* disable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	LOAD_TRACE();

	for (i = 0; i < num_trace_records; i++) {
		if (trace[i].event_type == TRACE_RECORD_SCHED_SWITCH) {
			struct trace_sched_switch *t = trace[i].event_data;
			if (!strncmp("sugov:", t->next_comm, 6))
				num_sugov_wakeups++;
		}
	}
	printf("%d sugov wakeups occurred.\n", num_sugov_wakeups);
	if (num_sugov_wakeups > MAX_WAKEUPS)
		tst_res(TFAIL, "Too many wakeups from the schedutil "
			"governor.\n");
	else
		tst_res(TPASS, "Wakeups from schedutil governor were below "
			"threshold.\n");
}

static struct tst_test test = {
	.test_all = run,
	.cleanup = trace_cleanup,
};
