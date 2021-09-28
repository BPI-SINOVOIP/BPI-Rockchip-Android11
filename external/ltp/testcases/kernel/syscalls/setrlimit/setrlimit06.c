// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2019 FUJITSU LIMITED. All rights reserved.
 * Author: Xiao Yang <yangx.jy@cn.fujitsu.com>
 */

/*
 * Description:
 * Set CPU time limit for a process and check its behavior
 * after reaching CPU time limit.
 * 1) Process got SIGXCPU after reaching soft limit of CPU time.
 * 2) Process got SIGKILL after reaching hard limit of CPU time.
 *
 * Note:
 * This is also a regression test for the following kernel bug:
 * 'c3bca5d450b62 ("posix-cpu-timers: Ensure set_process_cpu_timer is always evaluated")'
 */

#define _GNU_SOURCE
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "tst_test.h"

static int *end;

static void sighandler(int sig)
{
	*end = sig;
}

static void setup(void)
{
	SAFE_SIGNAL(SIGXCPU, sighandler);

	end = SAFE_MMAP(NULL, sizeof(int), PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_ANONYMOUS, -1, 0);
}

static void cleanup(void)
{
	if (end)
		SAFE_MUNMAP(end, sizeof(int));
}

static void verify_setrlimit(void)
{
	int status;
	pid_t pid;

	*end = 0;

	pid = SAFE_FORK();
	if (!pid) {
		struct rlimit rlim = {
			.rlim_cur = 2,
			.rlim_max = 3,
		};

		TEST(setrlimit(RLIMIT_CPU, &rlim));
		if (TST_RET == -1) {
			tst_res(TFAIL | TTERRNO,
				"setrlimit(RLIMIT_CPU) failed");
			exit(1);
		}

		alarm(10);

		while (1);
	}

	SAFE_WAITPID(pid, &status, 0);

	if (WIFEXITED(status) && WEXITSTATUS(status) == 1)
		return;

	if (WIFSIGNALED(status)) {
		if (WTERMSIG(status) == SIGKILL && *end == SIGXCPU) {
			tst_res(TPASS,
				"Got SIGXCPU then SIGKILL after reaching both limit");
			return;
		}

		if (WTERMSIG(status) == SIGKILL && !*end) {
			tst_res(TFAIL,
				"Got only SIGKILL after reaching both limit");
			return;
		}

		if (WTERMSIG(status) == SIGALRM && *end == SIGXCPU) {
			tst_res(TFAIL,
				"Got only SIGXCPU after reaching both limit");
			return;
		}

		if (WTERMSIG(status) == SIGALRM && !*end) {
			tst_res(TFAIL,
				"Got no signal after reaching both limit");
			return;
		}
	}

	tst_res(TBROK, "Child %s", tst_strstatus(status));
}

static struct tst_test test = {
	.test_all = verify_setrlimit,
	.setup = setup,
	.cleanup = cleanup,
	.forks_child = 1,
	.tags = (const struct tst_tag[]) {
		{"linux-git", "c3bca5d450b62"},
		{}
	}
};
