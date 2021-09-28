// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) Wipro Technologies Ltd, 2002.  All Rights Reserved.
 * Author: Saji Kumar.V.R <saji.kumar@wipro.com>
 *
 * Tests whether we can use capset() to modify the capabilities of a thread
 * other than itself. Now, most linux distributions with kernel supporting
 * VFS capabilities, this should be never permitted.
 */
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "tst_test.h"
#include "lapi/syscalls.h"
#include <linux/capability.h>

static struct __user_cap_header_struct *header;
static struct __user_cap_data_struct *data;
static pid_t child_pid;

static void verify_capset(void)
{
	child_pid = SAFE_FORK();
	if (!child_pid)
		pause();

	header->pid = child_pid;

	TEST(tst_syscall(__NR_capset, header, data));
	if (TST_RET == 0) {
		tst_res(TFAIL, "capset succeed unexpectedly");
		return;
	}

	if (TST_ERR == EPERM)
		tst_res(TPASS, "capset can't modify other process capabilities");
	else
		tst_res(TFAIL | TTERRNO, "capset expected EPERM, bug got");

	SAFE_KILL(child_pid, SIGTERM);
	SAFE_WAIT(NULL);
}

static void setup(void)
{
	header->version = 0x20080522;
	TEST(tst_syscall(__NR_capget, header, data));
	if (TST_RET == -1)
		tst_brk(TBROK | TTERRNO, "capget data failed");
}

static struct tst_test test = {
	.setup = setup,
	.test_all = verify_capset,
	.forks_child = 1,
	.bufs = (struct tst_buffers []) {
		{&header, .size = sizeof(*header)},
		{&data, .size = 2 * sizeof(*data)},
		{},
	}
};
