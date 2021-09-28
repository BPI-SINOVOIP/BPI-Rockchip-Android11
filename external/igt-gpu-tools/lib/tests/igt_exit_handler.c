/*
 * Copyright © 2016 Intel Corporation
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
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "igt_core.h"
#include "drmtest.h"

#include "igt_tests_common.h"

int test;
int pipes[2];

static void exit_handler1(int sig)
{
	internal_assert(test == 1);
	test++;
}

static void exit_handler2(int sig)
{
	char tmp = 1;

	/* ensure exit handlers are called in reverse */
	internal_assert(test == 0);
	test++;

	/* we need to get a side effect to the parent to make sure exit handlers
	 * actually run. */
	internal_assert(write(pipes[1], &tmp, 1) == 1);
}

enum test_type {
	SUC,
	NORMAL,
	FAIL,
	SKIP,
	SIG
};

static int testfunc(enum test_type test_type)
{
	char prog[] = "igt_no_exit";
	char *fake_argv[] = {prog};
	int fake_argc = ARRAY_SIZE(fake_argv);
	pid_t pid;
	int status;
	char tmp = 0;

	internal_assert(pipe2(pipes, O_NONBLOCK) == 0);

	pid = fork();

	if (pid == 0) {
		igt_subtest_init(fake_argc, fake_argv);

		igt_fixture {
			/* register twice, should only be called once */
			igt_install_exit_handler(exit_handler1);
			igt_install_exit_handler(exit_handler1);

			igt_install_exit_handler(exit_handler2);
		}

		igt_subtest("subtest") {
			switch (test_type) {
			case SUC:
				igt_success();
			case FAIL:
				igt_fail(1);
			case SKIP:
				igt_skip("skip");
			case NORMAL:
				break;
			case SIG:
				raise(SIGTERM);
			}
		}

		igt_exit();
	}

	internal_assert(waitpid(pid, &status, 0) != -1);

	internal_assert(read(pipes[0], &tmp, 1) == 1);
	internal_assert(tmp == 1);

	return status;
}

int main(int argc, char **argv)
{
	int status;

	internal_assert(testfunc(SUC) == 0);

	internal_assert(testfunc(NORMAL) == 0);

	status = testfunc(FAIL);
	internal_assert_wexited(status, 1);

	status = testfunc(SKIP);
	internal_assert_wexited(status, IGT_EXIT_SKIP);

	status = testfunc(SIG);
	internal_assert_wsignaled(status, SIGTERM);
}
