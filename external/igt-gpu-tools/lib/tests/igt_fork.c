/*
 * Copyright © 2019 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "igt_core.h"
#include "drmtest.h"

#include "igt_tests_common.h"

char test[] = "test";
char *argv_run[] = { test };

static void igt_fork_vs_skip(void)
{
	igt_fork(i, 1) {
		igt_skip("skipping");
	}

	igt_waitchildren();
}

static void igt_fork_vs_assert(void)
{
	igt_fork(i, 1) {
		igt_assert(0);
	}

	igt_waitchildren();
}

static void igt_fork_leak(void)
{
	igt_fork(i, 1) {
		sleep(10);
	}
}

static void plain_fork_leak(void)
{
	int pid;

	switch (pid = fork()) {
	case -1:
		internal_assert(0);
	case 0:
		sleep(1);
	default:
		exit(0);
	}
}

static void igt_fork_timeout_leak(void)
{
	igt_fork(i, 1) {
		sleep(10);
	}

	igt_waitchildren_timeout(1, "library test");
}

static int do_fork(void (*test_to_run)(void))
{
	int pid, status;
	int argc;

	switch (pid = fork()) {
	case -1:
		internal_assert(0);
	case 0:
		argc = ARRAY_SIZE(argv_run);
		igt_simple_init(argc, argv_run);
		test_to_run();
		igt_exit();
	default:
		while (waitpid(pid, &status, 0) == -1 &&
		       errno == EINTR)
			;

		return status;
	}
}


int main(int argc, char **argv)
{
	int ret;

	/* check that igt_assert is forwarded */
	ret = do_fork(igt_fork_vs_assert);
	internal_assert_wexited(ret, IGT_EXIT_FAILURE);

	/* check that igt_skip within a fork blows up */
	ret = do_fork(igt_fork_vs_skip);
	internal_assert_wexited(ret, SIGABRT + 128);

	/* check that failure to clean up fails */
	ret = do_fork(igt_fork_leak);
	internal_assert_wsignaled(ret, SIGABRT);

	/* check that igt_waitchildren_timeout cleans up*/
	ret = do_fork(igt_fork_timeout_leak);
	internal_assert_wexited(ret, SIGKILL + 128);

	/* check that any other process leaks are caught*/
	ret = do_fork(plain_fork_leak);
	internal_assert_wsignaled(ret, SIGABRT);
}
