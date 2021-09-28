/*
 * Copyright (c) 2018 Google, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * A CFS task is affined to a particular CPU. The task runs as a CPU hog for a
 * while then as a very small task for a while. The latency for the CPU
 * frequency of the CPU to reach max and then min is verified.
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

#define TRACE_EVENTS "sched_process_exit sched_process_fork cpu_frequency"

#define MAX_FREQ_INCREASE_LATENCY_US 70000
#define MAX_FREQ_DECREASE_LATENCY_US 70000

static int test_cpu;

#define BURN_MSEC 500
static void *burn_fn(void *arg LTP_ATTRIBUTE_UNUSED)
{
	int i = 0;
	unsigned int scaling_min_freq, scaling_cur_freq;
	char scaling_freq_file[60];

	affine(test_cpu);

	/*
	 * wait a bit to allow any hacks to boost frequency on migration
	 * to take effect
	 */
	usleep(200);

	sprintf(scaling_freq_file,
		"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq",
		test_cpu);
	SAFE_FILE_SCANF(scaling_freq_file, "%d", &scaling_min_freq);

	sprintf(scaling_freq_file,
		"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq",
		test_cpu);

	/* wait for test_cpu to reach scaling_min_freq */
	while(i++ < 10) {
		usleep(100 * 1000);
		SAFE_FILE_SCANF(scaling_freq_file, "%d",
				&scaling_cur_freq);
		if (scaling_cur_freq == scaling_min_freq)
			break;
	}
	if (i >= 10) {
		printf("Unable to reach scaling_min_freq before test!\n");
		return NULL;
	}

	SAFE_FILE_PRINTF(TRACING_DIR "trace_marker", "affined");
	burn(BURN_MSEC * 1000, 0);
	SAFE_FILE_PRINTF(TRACING_DIR "trace_marker", "small task");
	burn(BURN_MSEC * 1000, 1);

	return NULL;
}

static int parse_results(void)
{
	int i;

	int start_idx;
	int sleep_idx;
	unsigned int max_freq_seen = 0;
	int max_freq_seen_idx;
	unsigned int min_freq_seen = UINT_MAX;
	int min_freq_seen_idx;

	char scaling_freq_file[60];
	unsigned int scaling_max_freq;
	unsigned int scaling_min_freq;

	unsigned int increase_latency_usec;
	unsigned int decrease_latency_usec;

	/* find starting timestamp of test */
	for (i = 0; i < num_trace_records; i++)
		if (trace[i].event_type == TRACE_RECORD_TRACING_MARK_WRITE &&
		    !strcmp(trace[i].event_data, "affined"))
			break;
	if (i == num_trace_records) {
		printf("Did not find start of burn thread in trace!\n");
		return -1;
	}
	start_idx = i;

	/* find timestamp when burn thread sleeps */
	for (; i < num_trace_records; i++)
		if (trace[i].event_type == TRACE_RECORD_TRACING_MARK_WRITE &&
		    !strcmp(trace[i].event_data, "small task"))
				break;
	if (i == num_trace_records) {
		printf("Did not find switch to small task of burn thread in "
		       "trace!\n");
		return -1;
	}
	sleep_idx = i;

	/* find highest CPU frequency bewteen start and sleep timestamp */
	for (i = start_idx; i < sleep_idx; i++)
		if (trace[i].event_type == TRACE_RECORD_CPU_FREQUENCY) {
			struct trace_cpu_frequency *t = trace[i].event_data;
			if (t->cpu == test_cpu && t->state > max_freq_seen) {
				max_freq_seen = t->state;
				max_freq_seen_idx = i;
			}
		}
	if (max_freq_seen == 0) {
		printf("No freq events between start and sleep!\n");
		return -1;
	}

	/* find lowest CPU frequency between sleep timestamp and end */
	for (; i < num_trace_records; i++)
		if (trace[i].event_type == TRACE_RECORD_CPU_FREQUENCY) {
			struct trace_cpu_frequency *t = trace[i].event_data;
			if (t->cpu == test_cpu && t->state < min_freq_seen) {
				min_freq_seen = t->state;
				min_freq_seen_idx = i;
			}
		}
	if (min_freq_seen == UINT_MAX) {
		printf("No freq events between sleep and end!\n");
		return -1;
	}

	/* is highest CPU freq equal or greater than highest reported in
	 * scaling_max_freq? */
	sprintf(scaling_freq_file,
		"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq",
		test_cpu);
	SAFE_FILE_SCANF(scaling_freq_file, "%d", &scaling_max_freq);
	if (max_freq_seen < scaling_max_freq) {
		printf("CPU%d did not reach scaling_max_freq!\n",
		       test_cpu);
		return -1;
	} else {
		printf("CPU%d reached %d MHz during test "
		       "(scaling_max_freq %d MHz).\n", test_cpu,
		       max_freq_seen / 1000, scaling_max_freq / 1000);
	}

	/* is lowest CPU freq equal or less than scaling_min_freq? */
	sprintf(scaling_freq_file,
		"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq",
		test_cpu);
	SAFE_FILE_SCANF(scaling_freq_file, "%d", &scaling_min_freq);
	if (min_freq_seen > scaling_min_freq) {
		printf("CPU%d did not reach scaling_min_freq!\n",
		       test_cpu);
		return -1;
	} else {
		printf("CPU%d reached %d MHz after test "
		       "(scaling_min_freq %d Mhz).\n",
		       test_cpu, min_freq_seen / 1000,
		       scaling_min_freq / 1000);
	}

	/* calculate and check latencies */
	increase_latency_usec = trace[max_freq_seen_idx].ts.sec * USEC_PER_SEC +
		trace[max_freq_seen_idx].ts.usec;
	increase_latency_usec -= trace[start_idx].ts.sec * USEC_PER_SEC +
		trace[start_idx].ts.usec;

	decrease_latency_usec = trace[min_freq_seen_idx].ts.sec * USEC_PER_SEC +
		trace[min_freq_seen_idx].ts.usec;
	decrease_latency_usec -= trace[sleep_idx].ts.sec * USEC_PER_SEC +
		trace[sleep_idx].ts.usec;

	printf("Increase latency: %d usec\n", increase_latency_usec);
	printf("Decrease latency: %d usec\n", decrease_latency_usec);

	return (increase_latency_usec > MAX_FREQ_INCREASE_LATENCY_US ||
		decrease_latency_usec > MAX_FREQ_DECREASE_LATENCY_US);
}

static void run(void)
{
	pthread_t burn_thread;

	tst_res(TINFO, "Max acceptable latency to fmax: %d usec\n",
		MAX_FREQ_INCREASE_LATENCY_US);
	tst_res(TINFO, "Max acceptable latency to fmin: %d usec\n",
		MAX_FREQ_DECREASE_LATENCY_US);

	test_cpu = tst_ncpus() - 1;
	printf("CPU hog will be bound to CPU %d.\n", test_cpu);

	/* configure and enable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	SAFE_FILE_PRINTF(TRACING_DIR "buffer_size_kb", "16384");
	SAFE_FILE_PRINTF(TRACING_DIR "set_event", TRACE_EVENTS);
	SAFE_FILE_PRINTF(TRACING_DIR "trace", "\n");
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "1");

	SAFE_PTHREAD_CREATE(&burn_thread, NULL, burn_fn, NULL);
	SAFE_PTHREAD_JOIN(burn_thread, NULL);

	/* disable tracing */
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	LOAD_TRACE();

	if (parse_results())
		tst_res(TFAIL, "Governor did not meet latency targets.\n");
	else
		tst_res(TPASS, "Governor met latency targets.\n");
}

static struct tst_test test = {
	.test_all = run,
	.cleanup = trace_cleanup,
};
