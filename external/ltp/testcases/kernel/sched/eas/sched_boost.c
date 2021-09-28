/*
 * Copyright (c) 2018 Google, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * A test schedboost cgroup is created and a task is put inside it. The
 * utilization of that task is monitored and verified while changing the boost
 * of the test schedboost cgroup to different values.
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
#include "tst_safe_macros.h"

#include "trace_parse.h"
#include "util.h"

#define TRACE_EVENTS "sugov_util_update sched_switch"

static sem_t test_sem;
static sem_t result_sem;
static int test_cpu;
static int task_pid;
static unsigned int test_index = 0;
static int test_boost[] = { 0, 25, 50, 75, 100 };
#define NUM_TESTS (sizeof(test_boost)/sizeof(int))
static int test_utils[NUM_TESTS];
#define STUNE_TEST_PATH "/dev/stune/test"
#define STUNE_ROOT_TASKS "/dev/stune/tasks"

#define BUSY_USEC 1000
#define SLEEP_USEC 19000
/* Run each test for one second. */
#define TEST_LENGTH_USEC USEC_PER_SEC

static void do_work(void)
{
	struct timespec ts;
	unsigned long long now_usec, end_usec;

	/* Calculate overall end time. */
	if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
		printf("clock_gettime() reported an error\n");
		return;
	}
	now_usec = ts.tv_sec * USEC_PER_SEC + ts.tv_nsec / 1000;
	end_usec = now_usec + TEST_LENGTH_USEC/2;

	while (now_usec < end_usec) {
		burn(BUSY_USEC, 0);
		usleep(SLEEP_USEC);
		if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
			printf("clock_gettime() reported an error\n");
			return;
		}
		now_usec = ts.tv_sec * USEC_PER_SEC + ts.tv_nsec / 1000;
	}
}

static void *test_fn(void *arg LTP_ATTRIBUTE_UNUSED)
{
	unsigned int tests_done = 0;

	affine(test_cpu);

	task_pid = gettid();
	SAFE_FILE_PRINTF(STUNE_TEST_PATH "/tasks", "%d", task_pid);
	while(tests_done < NUM_TESTS) {
		sem_wait(&test_sem);
		// give time for utilization to track real task usage
		do_work();
		// start measuring
		SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "1");
		do_work();
		sem_post(&result_sem);
		tests_done++;
	}
	SAFE_FILE_PRINTF(STUNE_ROOT_TASKS, "%d", task_pid);
	return NULL;
}

static int parse_results(void)
{
	int i;
	int max_util_seen = 0;

	for (i = 0; i < num_trace_records; i++)
		if (trace[i].event_type == TRACE_RECORD_SUGOV_UTIL_UPDATE) {
			struct trace_sugov_util_update *t =
				trace[i].event_data;
			if (t->cpu == test_cpu && t->util > max_util_seen)
				max_util_seen = t->util;
		}

	test_utils[test_index] = max_util_seen;
	printf("Max util seen for boost %d: %d\n", test_boost[test_index],
	       max_util_seen);
	return 0;
}

static void run_test(void)
{
	SAFE_FILE_PRINTF(STUNE_TEST_PATH "/schedtune.boost",
			 "%d", test_boost[test_index]);
	SAFE_FILE_PRINTF(TRACING_DIR "trace", "\n");
	sem_post(&test_sem);
	sem_wait(&result_sem);
	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	LOAD_TRACE();
	parse_results();
	test_index++;
}

#define TEST_MARGIN 50
static void check_results(void)
{
	unsigned int i;
	for (i = 0; i < NUM_TESTS; i++) {
		int target_util = test_boost[i] * 10;
		if (test_utils[i] < target_util - TEST_MARGIN ||
		    test_utils[i] > target_util + TEST_MARGIN)
			tst_res(TFAIL, "Test %i (boost %d) failed with "
				"util %d (allowed %d - %d).\n",
				i, test_boost[i], test_utils[i],
				target_util - TEST_MARGIN,
				target_util + TEST_MARGIN);
		else
			tst_res(TPASS, "Test %i (boost %d) passed with "
				"util %d (allowed %d - %d).\n",
				i, test_boost[i], test_utils[i],
				target_util - TEST_MARGIN,
				target_util + TEST_MARGIN);
	}
}

static void run(void)
{
	pthread_t test_thread;

	sem_init(&test_sem, 0, 0);
	sem_init(&result_sem, 0, 0);

	if (access("/dev/stune", R_OK))
		tst_brk(TCONF, "schedtune not detected");

	test_cpu = tst_ncpus() - 1;
	printf("Running %ld tests for %d sec\n", NUM_TESTS,
	       TEST_LENGTH_USEC / USEC_PER_SEC);
	printf("CPU hog will be bound to CPU %d.\n", test_cpu);

	/*
	 * If this fails due to ENOSPC the definition of BOOSTGROUPS_COUNT in
	 * kernel/sched/tune.c needs to be increased.
	 */
	SAFE_MKDIR(STUNE_TEST_PATH, 0777);
	if (access(STUNE_TEST_PATH "/schedtune.colocate", W_OK) != -1) {
		SAFE_FILE_PRINTF(STUNE_TEST_PATH "/schedtune.colocate",
				 "0");
	}
	if (access(STUNE_TEST_PATH "/schedtune.prefer_idle", W_OK) != -1) {
		SAFE_FILE_PRINTF(STUNE_TEST_PATH "/schedtune.prefer_idle",
				 "0");
	}
	if (access(STUNE_TEST_PATH "/schedtune.sched_boost_enabled", W_OK) !=
	    -1) {
		SAFE_FILE_PRINTF(STUNE_TEST_PATH "/schedtune.sched_boost_enabled",
				 "1");
	}
	if (access(STUNE_TEST_PATH "/schedtune.sched_boost_no_override",
		   W_OK) != -1) {
		SAFE_FILE_PRINTF(STUNE_TEST_PATH "/schedtune.sched_boost_no_override",
				 "0");
	}

	SAFE_PTHREAD_CREATE(&test_thread, NULL, test_fn, NULL);

	SAFE_FILE_PRINTF(TRACING_DIR "tracing_on", "0");
	SAFE_FILE_PRINTF(TRACING_DIR "buffer_size_kb", "16384");
	SAFE_FILE_PRINTF(TRACING_DIR "set_event", TRACE_EVENTS);

	while (test_index < NUM_TESTS)
		run_test();

	SAFE_PTHREAD_JOIN(test_thread, NULL);
	SAFE_RMDIR(STUNE_TEST_PATH);
	check_results();
}

static struct tst_test test = {
	.test_all = run,
	.cleanup = trace_cleanup,
};
