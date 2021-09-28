/*
 * Copyright © 2013 Intel Corporation
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
 * Authors:
 *    Daniel Vetter <daniel.vetter@ffwll.ch>
 *
 */

#include <sys/wait.h>

#include "drmtest.h"

#include "igt_tests_common.h"

static void no_exit_list_only(void)
{
	char prog[] = "igt_list_only";
	char arg[] = "--list-subtests";
	char *fake_argv[] = {prog, arg};
	int fake_argc = ARRAY_SIZE(fake_argv);

	igt_subtest_init(fake_argc, fake_argv);

	igt_subtest("A")
		;
}

static void no_exit(void)
{
	char prog[] = "igt_no_exit";
	char *fake_argv[] = {prog};
	int fake_argc = ARRAY_SIZE(fake_argv);

	igt_subtest_init(fake_argc, fake_argv);

	igt_subtest("A")
		;
}

static int do_fork(void (*test_to_run)(void))
{
	int pid, status;

	switch (pid = fork()) {
	case -1:
		internal_assert(0);
	case 0:
		test_to_run();
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

	ret = do_fork(no_exit);
	internal_assert_wsignaled(ret, SIGABRT);

	ret = do_fork(no_exit_list_only);
	internal_assert_wsignaled(ret, SIGABRT);

	return 0;
}
