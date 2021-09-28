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

#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>

#include "drmtest.h"
#include "igt_tests_common.h"

IGT_TEST_DESCRIPTION("the top level description");
static void fake_main(int argc, char **argv) {
	igt_subtest_init(argc, argv);

	igt_describe("Basic A");
	igt_subtest("A")
		;

	igt_fixture
		printf("should not be executed!\n");

	igt_describe("Group with B, C & D");
	igt_subtest_group {
		igt_describe("Basic B");
		igt_subtest("B")
			;

		if (!igt_only_list_subtests())
			printf("should not be executed!\n");

		igt_describe("Group with C & D");
		igt_subtest_group {
			igt_describe("Basic C");
			igt_subtest("C")
				printf("should not be executed!\n");

			// NO DOC
			igt_subtest("D")
				;
		}
	}

	// NO DOC
	igt_subtest_group {
		// NO DOC
		igt_subtest("E")
			;
	}

	// NO DOC
	igt_subtest("F")
		;

	igt_describe("this description should be so long that it wraps itself nicely in the terminal "
		     "this description should be so long that it wraps itself nicely in the terminal "
		     "this description should be so long that it wraps itself nicely in the terminal "
		     "this description should be so long that it wraps itself nicely in the terminal "
		     "this description should be so long that it wraps itself nicely in the terminal "
		     "this description should be so long that it wraps itself nicely in the terminal");
	igt_subtest("G")
		;

	igt_describe("verylongwordthatshoudlbeprintedeventhoughitspastthewrppinglimit"
		     "verylongwordthatshoudlbeprintedeventhoughitspastthewrappinglimit "
		     "verylongwordthatshoudlbeprintedeventhoughitspastthewrappinglimit"
		     "verylongwordthatshoudlbeprintedeventhoughitspastthewrappinglimit");
	igt_subtest("F")
		;

	igt_exit();
}

static const char DESCRIBE_ALL_OUTPUT[] = \
	"the top level description\n"
	"\n"
	"SUB A ../lib/tests/igt_describe.c:36:\n"
	"  Basic A\n"
	"\n"
	"SUB B ../lib/tests/igt_describe.c:45:\n"
	"  Group with B, C & D\n"
	"\n"
	"  Basic B\n"
	"\n"
	"SUB C ../lib/tests/igt_describe.c:54:\n"
	"  Group with B, C & D\n"
	"\n"
	"  Group with C & D\n"
	"\n"
	"  Basic C\n"
	"\n"
	"SUB D ../lib/tests/igt_describe.c:58:\n"
	"  Group with B, C & D\n"
	"\n"
	"  Group with C & D\n"
	"\n"
	"SUB E ../lib/tests/igt_describe.c:66:\n"
	"  NO DOCUMENTATION!\n"
	"\n"
	"SUB F ../lib/tests/igt_describe.c:71:\n"
	"  NO DOCUMENTATION!\n"
	"\n"
	"SUB G ../lib/tests/igt_describe.c:80:\n"
	"  this description should be so long that it wraps itself nicely in the terminal this\n"
	"  description should be so long that it wraps itself nicely in the terminal this description\n"
	"  should be so long that it wraps itself nicely in the terminal this description should be so\n"
	"  long that it wraps itself nicely in the terminal this description should be so long that it\n"
	"  wraps itself nicely in the terminal this description should be so long that it wraps itself\n"
	"  nicely in the terminal\n"
	"\n"
	"SUB F ../lib/tests/igt_describe.c:87:\n"
	"  verylongwordthatshoudlbeprintedeventhoughitspastthewrppinglimitverylongwordthatshoudlbeprintedeventhoughitspastthewrappinglimit\n"
	"  verylongwordthatshoudlbeprintedeventhoughitspastthewrappinglimitverylongwordthatshoudlbeprintedeventhoughitspastthewrappinglimit\n\n";

static const char JUST_C_OUTPUT[] = \
	"the top level description\n"
	"\n"
	"SUB C ../lib/tests/igt_describe.c:54:\n"
	"  Group with B, C & D\n"
	"\n"
	"  Group with C & D\n"
	"\n"
	"  Basic C\n"
	"\n";

static void assert_pipe_empty(int fd)
{
	char buf[5];
	internal_assert(0 == read(fd, buf, sizeof(buf)));
}

static void read_whole_pipe(int fd, char *buf, size_t buflen)
{
	ssize_t readlen;
	off_t offset;

	offset = 0;
	while ((readlen = read(fd, buf+offset, buflen-offset))) {
		if (readlen == -1) {
			if (errno == EINTR) {
				continue;
			} else {
				printf("read failed with %s\n", strerror(errno));
				exit(1);
			}
		}
		internal_assert(readlen != -1);
		offset += readlen;
	}
}

static pid_t do_fork(int argc, char **argv, int *out, int *err)
{
	int outfd[2], errfd[2];
	pid_t pid;

	internal_assert(pipe(outfd) != -1);
	internal_assert(pipe(errfd) != -1);

	pid = fork();
	internal_assert(pid != -1);

	if (pid == 0) {
		while (dup2(outfd[1], STDOUT_FILENO) == -1 && errno == EINTR) {}
		while (dup2(errfd[1], STDERR_FILENO) == -1 && errno == EINTR) {}

		close(outfd[0]);
		close(outfd[1]);
		close(errfd[0]);
		close(errfd[1]);

		fake_main(argc, argv);

		exit(-1);
	} else {
		/* close the writing ends */
		close(outfd[1]);
		close(errfd[1]);

		*out = outfd[0];
		*err = errfd[0];

		return pid;
	}
}

static int _wait(pid_t pid, int *status) {
	int ret;

	do {
		ret = waitpid(pid, status, 0);
	} while (ret == -1 && errno == EINTR);

	return ret;
}

int main(int argc, char **argv)
{
	char prog[] = "igt_describe";
	int status;
	int outfd, errfd;

	/* describe all subtest */ {
		static char out[4096];
		char arg[] = "--describe";
		char *fake_argv[] = {prog, arg};
		int fake_argc = ARRAY_SIZE(fake_argv);

		pid_t pid = do_fork(fake_argc, fake_argv, &outfd, &errfd);

		read_whole_pipe(outfd, out, sizeof(out));
		assert_pipe_empty(errfd);

		internal_assert(_wait(pid, &status) != -1);
		internal_assert(WIFEXITED(status));
		internal_assert(WEXITSTATUS(status) == IGT_EXIT_SUCCESS);
		internal_assert(0 == strcmp(DESCRIBE_ALL_OUTPUT, out));

		close(outfd);
		close(errfd);
	}

	/* describe C using a pattern */ {
		static char out[4096];
		char arg[] = "--describe=C";
		char *fake_argv[] = {prog, arg};
		int fake_argc = ARRAY_SIZE(fake_argv);

		pid_t pid = do_fork(fake_argc, fake_argv, &outfd, &errfd);

		read_whole_pipe(outfd, out, sizeof(out));
		assert_pipe_empty(errfd);

		internal_assert(_wait(pid, &status) != -1);
		internal_assert(WIFEXITED(status));
		internal_assert(WEXITSTATUS(status) == IGT_EXIT_SUCCESS);
		internal_assert(0 == strcmp(JUST_C_OUTPUT, out));

		close(outfd);
		close(errfd);
	}

	/* fail describing with a bad pattern */ {
		static char err[4096];
		char arg[] = "--describe=Z";
		char *fake_argv[] = {prog, arg};
		int fake_argc = ARRAY_SIZE(fake_argv);

		pid_t pid = do_fork(fake_argc, fake_argv, &outfd, &errfd);

		read_whole_pipe(errfd, err, sizeof(err));

		internal_assert(_wait(pid, &status) != -1);
		internal_assert(WIFEXITED(status));
		internal_assert(WEXITSTATUS(status) == IGT_EXIT_INVALID);
		internal_assert(strstr(err, "Unknown subtest: Z"));

		close(outfd);
		close(errfd);
	}

	return 0;
}
