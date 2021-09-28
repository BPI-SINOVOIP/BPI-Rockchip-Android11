// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2019 FUJITSU LIMITED. All rights reserved.
 * Author: Yang Xu <xuyang2018.jy@cn.fujitsu.com>
 *
 * Test PR_GET_NAME and PR_SET_NAME of prctl(2).
 * 1)Set the name of the calling thread, the name can be up to 16 bytes
 *   long, including the terminating null byte. If exceeds 16 bytes, the
 *   string is silently truncated.
 * 2)Return the name of the calling thread, the buffer should allow space
 *   for up to 16 bytes, the returned string will be null-terminated.
 * 3)Check /proc/self/task/[tid]/comm and /proc/self/comm name whether
 *   matches the thread name.
 */

#include <sys/prctl.h>
#include <string.h>
#include <stdio.h>
#include "lapi/syscalls.h"
#include "lapi/prctl.h"
#include "tst_test.h"

static struct tcase {
	char setname[20];
	char expname[20];
} tcases[] = {
	{"prctl05_test", "prctl05_test"},
	{"prctl05_test_xxxxx", "prctl05_test_xx"}
};

static void check_proc_comm(char *path, char *name)
{
	char comm_buf[20];

	SAFE_FILE_SCANF(path, "%s", comm_buf);
	if (strcmp(name, comm_buf))
		tst_res(TFAIL,
			"%s has %s, expected %s", path, comm_buf, name);
	else
		tst_res(TPASS, "%s sets to %s", path, comm_buf);
}

static void verify_prctl(unsigned int n)
{
	char buf[20];
	char comm_path[40];
	pid_t tid;
	struct tcase *tc = &tcases[n];

	TEST(prctl(PR_SET_NAME, tc->setname));
	if (TST_RET == -1) {
		tst_res(TFAIL | TTERRNO, "prctl(PR_SET_NAME) failed");
		return;
	}
	tst_res(TPASS, "prctl(PR_SET_NAME, '%s') succeeded", tc->setname);

	TEST(prctl(PR_GET_NAME, buf));
	if (TST_RET == -1) {
		tst_res(TFAIL | TTERRNO, "prctl(PR_GET_NAME) failed");
		return;
	}

	if (strncmp(tc->expname, buf, sizeof(buf))) {
		tst_res(TFAIL,
		        "prctl(PR_GET_NAME) failed, expected %s, got %s",
		        tc->expname, buf);
		return;
	}
	tst_res(TPASS, "prctl(PR_GET_NAME) succeeded, thread name is %s", buf);

	tid = tst_syscall(__NR_gettid);

	sprintf(comm_path, "/proc/self/task/%d/comm", tid);
	check_proc_comm(comm_path, tc->expname);

	check_proc_comm("/proc/self/comm", tc->expname);
}

static struct tst_test test = {
	.test = verify_prctl,
	.tcnt = ARRAY_SIZE(tcases),
};
