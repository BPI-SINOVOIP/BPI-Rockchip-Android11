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
 */

/*
 * DESCRIPTION: Make sure that IGT framework complains when test try to define
 * conflicting options.
 */

#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#include "drmtest.h"
#include "igt_core.h"
#include "igt_tests_common.h"

static struct option long_options[2];
static const char *short_options;

static int opt_handler(int option, int option_index, void *input)
{

	return 0;
}

static int do_fork(void)
{
	char test_name[] = "test";

	char *argv[] = { test_name };
	int argc = ARRAY_SIZE(argv);

	pid_t pid = fork();
	internal_assert(pid != -1);

	if (pid) {
		int status;
		while (waitpid(pid, &status, 0) == -1 && errno == EINTR)
			;

		return status;
	}


	igt_subtest_init_parse_opts(&argc, argv, short_options, long_options,
				    "", opt_handler, NULL);
	igt_subtest("dummy") {}
	igt_exit();
}

int main(int argc, char **argv)
{
	/* no conflict */
	long_options[0] = (struct option) { "iterations", required_argument, NULL, 'i'};
	short_options = "";
	internal_assert_wexited(do_fork(), 0);

	/* conflict on short option */
	long_options[0] = (struct option) { "iterations", required_argument, NULL, 'i'};
	short_options = "h";
	internal_assert_wsignaled(do_fork(), SIGABRT);

	/* conflict on long option name */
	long_options[0] = (struct option) { "help", required_argument, NULL, 'i'};
	short_options = "";
	internal_assert_wsignaled(do_fork(), SIGABRT);

	/* conflict on long option 'val' representation vs short option */
	long_options[0] = (struct option) { "iterations", required_argument, NULL, 'h'};
	short_options = "";
	internal_assert_wsignaled(do_fork(), SIGABRT);

	/* conflict on long option 'val' representations */
	long_options[0] = (struct option) { "iterations", required_argument, NULL, 500};
	short_options = "";
	internal_assert_wsignaled(do_fork(), SIGABRT);

	return 0;
}
