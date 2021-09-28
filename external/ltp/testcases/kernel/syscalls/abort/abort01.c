// SPDX-License-Identifier: GPL-2.0-or-later

/*
 *   Copyright (c) International Business Machines  Corp., 2002
 *   01/02/2003	Port to LTP	avenkat@us.ibm.com
 *   11/11/2002: Ported to LTP Suite by Ananda
 *   06/30/2001	Port to Linux	nsharoff@us.ibm.com
 *
 * ALGORITHM
 *	Fork child.  Have child abort, check return status.
 *
 * RESTRICTIONS
 *      The ulimit for core file size must be greater than 0.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>

#include "tst_test.h"

static void do_child(void)
{
	abort();
	tst_res(TFAIL, "Abort returned");
	exit(0);
}

void verify_abort(void)
{
	int status, kidpid;
	int sig, core;

	kidpid = SAFE_FORK();
	if (kidpid == 0)
		do_child();

	SAFE_WAIT(&status);

	if (!WIFSIGNALED(status)) {
		tst_res(TFAIL, "Child %s, expected SIGIOT",
			tst_strstatus(status));
		return;
	}

	core = WCOREDUMP(status);
	sig = WTERMSIG(status);

	if (core == 0)
		tst_res(TFAIL, "abort() failed to dump core");
	else
		tst_res(TPASS, "abort() dumped core");

	if (sig == SIGIOT)
		tst_res(TPASS, "abort() raised SIGIOT");
	else
		tst_res(TFAIL, "abort() raised %s", tst_strsig(sig));
}

#define MIN_RLIMIT_CORE (1024 * 1024)

static void setup(void)
{
	struct rlimit rlim;

	/* make sure we get core dumps */
	SAFE_GETRLIMIT(RLIMIT_CORE, &rlim);
	if (rlim.rlim_cur < MIN_RLIMIT_CORE) {
		rlim.rlim_cur = MIN_RLIMIT_CORE;
		SAFE_SETRLIMIT(RLIMIT_CORE, &rlim);
	}
}

static struct tst_test test = {
	.needs_tmpdir = 1,
	.forks_child = 1,
	.setup = setup,
	.test_all = verify_abort,
};
