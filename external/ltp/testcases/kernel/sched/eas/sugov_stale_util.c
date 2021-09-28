/*
 * Copyright (c) 2018 Google, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This test attempts to verify that the schedutil governor does not take into
 * account stale utilization from an idle CPU when calculating the frequency for
 * a shared policy.
 *
 * This test is not yet complete and may never be. The CPU in question may
 * receive spurious updates which push the stale deadline out, causing the test
 * to fail.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <semaphore.h>
#include <stdlib.h>

#include "tst_test.h"
#include "tst_safe_file_ops.h"
#include "tst_safe_pthread.h"

#include "trace_parse.h"
#include "util.h"

#define TRACE_EVENTS "sugov_next_freq sugov_util_update"

#define MAX_TEST_CPUS 32
static int policy_cpus[MAX_TEST_CPUS];
static int policy_num_cpus = 0;

static int test_cpu;
static sem_t sem;

/* sugov currently waits 1.125 * TICK_NSEC, which with HZ=300, is
 * ~3.75ms for PELT
 * On WALT, 1.125 * sched_ravg_window (20ms) is 22.5ms */
#define MAX_STALE_USEC 22500
/* The event task may not wake up right away due to timer slack. */
#define SLACK_USEC 10000

static void *event_fn(void *arg LTP_ATTRIBUTE_UNUSED)
{
	/*
	 * FIXME: Proper logic to identify a multi-CPU policy and select two
	 * CPUS from it is required here.
	 */
	affine(test_cpu - 1);

	sem_wait(&sem);

	SAFE_FILE_PRINTF(TRACING_DIR "trace_marker",
			 "event task sleep");
	usleep(MAX_STALE_USEC);
	SAFE_FILE_PRINTF(TRACING_DIR "trace_marker",
			 "event task wake");
	/*
	 * Waking up should be sufficient to get the cpufreq policy to
	 * re-evaluate.
	 */
	return NULL;
}

#define BURN_MSEC 500
static void *burn_fn(void *arg LTP_ATTRIBUTE_UNUSED)
{
	affine(test_cpu);

	/*
	 * wait a bit to allow any hacks to boost frequency on migration
	 * to take effect
	 */
	usleep(200);

	/* Busy loop for BURN_MSEC to get the task demand to maximum. */
	burn(BURN_MSEC * 1000, 0);

	/*
	 * Sleep. The next sugov update after TICK_NSEC should not include
	 * this task's contribution.
	 */
	SAFE_FILE_PRINTF(TRACING_DIR "trace_marker", "sleeping");

	/*
	 * Wake up task on another CPU in the same policy which will sleep
	 * for stale_ns, then wake up briefly to trigger a recalculation of the
	 * cpufreq policy.
	 */
	sem_post(&sem);
	sleep(2);

	return NULL;
}

static int cpu_in_policy(int cpu)
{
	int i;
	for (i = 0; i < policy_num_cpus; i++)
		if (cpu == policy_cpus[i])
			return 1;
	return 0;
}

static int parse_results(void)
{
	int i, sleep_idx;
	int max_util_seen = 0;
	unsigned int stale_usec;

	/* Verify that utilization reached 1024 before sleep. */
	for (i = 0; i < num_trace_records; i++) {
		if (trace[i].event_type == TRACE_RECORD_SUGOV_UTIL_UPDATE) {
			struct trace_sugov_util_update *t =
				trace[i].event_data;
			if (t->cpu == test_cpu && t->util > max_util_seen)
				max_util_seen = t->util;
		}
		if (trace[i].event_type == TRACE_RECORD_TRACING_MARK_WRITE &&
		    !strcmp(trace[i].event_data, "sleeping"))
			break;
	}
	printf("Max util seen from CPU hog: %d\n", max_util_seen);
	if (max_util_seen < 1000) {
		printf("Trace parse error, utilization of CPU hog did "
		       "not reach 1000.\n");
		return -1;
	}
	sleep_idx = i;
//	print_trace_record(&trace[i]);
	for (; i < num_trace_records; i++)
		if (trace[i].event_type == TRACE_RECORD_SUGOV_NEXT_FREQ) {
			struct trace_sugov_next_freq *t =
				trace[i].event_data;
			/* We should only see some minor utilization. */
			if (cpu_in_policy(t->cpu) && t->util < 200)
				break;
		}
	if (i == num_trace_records) {
		printf("Trace parse error, util never went stale!\n");
		return -1;
	}
//	print_trace_record(&trace[i]);
	stale_usec = TS_TO_USEC(trace[i].ts) - TS_TO_USEC(trace[sleep_idx].ts);

	printf("Stale vote shown to be cleared in %d usec.\n", stale_usec);
	return (stale_usec > (MAX_STALE_USEC + SLACK_USEC));
}

#define POLICY_CPUS_BUFSIZE 1024
static void get_policy_cpus(void)
{
	int i=0, len, policy_cpus_fd;
	char policy_cpus_fname[128];;
	char *buf;

	sprintf(policy_cpus_fname,
		"/sys/devices/system/cpu/cpu%d/cpufreq/related_cpus",
		test_cpu);
	buf = SAFE_MALLOC(POLICY_CPUS_BUFSIZE);

	policy_cpus_fd = open(policy_cpus_fname, O_RDONLY);
	if (policy_cpus_fd < 0) {
		printf("Failed to open policy cpus (errno %d)\n",
		       errno);
		return;
	}

	len = read(policy_cpus_fd, buf, POLICY_CPUS_BUFSIZE -1);
	/* At least one digit is expected. */
	if (len < 2) {
		printf("Read of policy cpus returned %d (errno %d)\n",
		       len, errno);
		return;
	}
	close(policy_cpus_fd);
	/* buf now has a list of CPUs, parse it */
	while(buf[i] >= '0' && buf[i] <= '9') {
		int j = i;
		while (buf[j] >= '0' && buf[j] <= '9')
			j++;
		buf[j] = 0;
		policy_cpus[policy_num_cpus++] = atoi(&buf[i]);
		i = j + 1;
	}
	printf("Testing on CPU %d, all CPUs in that policy:\n",
	       test_cpu);
	for (int i = 0; i < policy_num_cpus; i++)
		printf(" %d", policy_cpus[i]);
	printf("\n");
	free(buf);
}

static void run(void)
{
	pthread_t burn_thread, event_thread;

	test_cpu = tst_ncpus() - 1;
	printf("CPU hog will be bound to CPU %d.\n", test_cpu);
	get_policy_cpus();

	sem_init(&sem, 0, 0);

	/* configure and enable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	SAFE_FILE_PRINTF(TRACING_DIR "buffer_size_kb", "16384");
	SAFE_FILE_PRINTF(TRACING_DIR "set_event", TRACE_EVENTS);
	SAFE_FILE_PRINTF(TRACING_DIR "trace", "\n");
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "1");

	SAFE_PTHREAD_CREATE(&burn_thread, NULL, burn_fn, NULL);
	SAFE_PTHREAD_CREATE(&event_thread, NULL, event_fn, NULL);

	SAFE_PTHREAD_JOIN(burn_thread, NULL);
	SAFE_PTHREAD_JOIN(event_thread, NULL);

	/* disable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	LOAD_TRACE();

	if (parse_results())
		tst_res(TFAIL, "Stale utilization not cleared within expected "
			"time (%d usec).\n", MAX_STALE_USEC + SLACK_USEC);
	else
		tst_res(TPASS, "Stale utilization cleared within expected "
			"time.\n");
}

static struct tst_test test = {
	.test_all = run,
	.cleanup = trace_cleanup,
};
