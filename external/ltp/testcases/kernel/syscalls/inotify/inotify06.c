// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2015 SUSE Linux.  All Rights Reserved.
 * Author: Jan Kara <jack@suse.cz>
 *
 * DESCRIPTION
 * Test for inotify mark destruction race.
 *
 * Kernels prior to 4.2 have a race when inode is being deleted while
 * inotify group watching that inode is being torn down. When the race is
 * hit, the kernel crashes or loops.
 *
 * The problem has been fixed by commit:
 *  8f2f3eb59dff "fsnotify: fix oops in fsnotify_clear_marks_by_group_flags()".
 */

#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/syscall.h>

#include "tst_test.h"
#include "inotify.h"

#if defined(HAVE_SYS_INOTIFY_H)
#include <sys/inotify.h>

/* Number of test loops to run the test for */
#define TEARDOWNS 400

/* Number of files to test (must be > 1) */
#define FILES 5

char names[FILES][PATH_MAX];

static void setup(void)
{
	int i;

	for (i = 0; i < FILES; i++)
		sprintf(names[i], "fname_%d", i);
}

static void verify_inotify(void)
{
	int inotify_fd, fd;
	pid_t pid;
	int i, tests;

	pid = SAFE_FORK();
	if (pid == 0) {
		while (1) {
			for (i = 0; i < FILES; i++) {
				fd = SAFE_OPEN(names[i], O_CREAT | O_RDWR, 0600);
				SAFE_CLOSE(fd);
			}
			for (i = 0; i < FILES; i++)
				SAFE_UNLINK(names[i]);
		}
	}

	for (tests = 0; tests < TEARDOWNS; tests++) {
		inotify_fd = SAFE_MYINOTIFY_INIT1(O_NONBLOCK);

		for (i = 0; i < FILES; i++) {
			/*
			 * Both failure and success are fine since
			 * files are being deleted in parallel - this
			 * is what provokes the race we want to test
			 * for...
			 */
			myinotify_add_watch(inotify_fd, names[i], IN_MODIFY);
		}
		SAFE_CLOSE(inotify_fd);
	}
	/* We survived for given time - test succeeded */
	tst_res(TPASS, "kernel survived inotify beating");

	/* Kill the child creating / deleting files and wait for it */
	SAFE_KILL(pid, SIGKILL);
	SAFE_WAIT(NULL);
}

static struct tst_test test = {
	.timeout = 600,
	.needs_tmpdir = 1,
	.forks_child = 1,
	.setup = setup,
	.test_all = verify_inotify,
};

#else
	TST_TEST_TCONF("system doesn't have required inotify support");
#endif
