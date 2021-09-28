// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) International Business Machines Corp., 2001
 * Ported to LTP: Wayne Boyer
 * Copyright (c) 2016 Cyril Hrubis <chrubis@suse.cz>
 */

/*
 *  Verify that root can provide a negative value to nice()
 *  and hence root can decrease the nice value of the process
 *  using nice() system call
 */
#include <unistd.h>
#include <errno.h>
#include <sys/resource.h>
#include "tst_test.h"

#define	NICEINC		-12

static void verify_nice(void)
{
	int new_nice;
	int orig_nice;

	orig_nice = SAFE_GETPRIORITY(PRIO_PROCESS, 0);

	TEST(nice(NICEINC));

	if (TST_RET != (orig_nice + NICEINC)) {
		tst_res(TFAIL | TTERRNO, "nice(%d) returned %li, expected %i",
			NICEINC, TST_RET, orig_nice + NICEINC);
		return;
	}

	if (TST_ERR) {
		tst_res(TFAIL | TTERRNO, "nice(%d) failed", NICEINC);
		return;
	}

	new_nice = SAFE_GETPRIORITY(PRIO_PROCESS, 0);

	if (new_nice != (orig_nice + NICEINC)) {
		tst_res(TFAIL, "Process priority %i, expected %i",
		        new_nice, orig_nice + NICEINC);
		return;
	}

	tst_res(TPASS, "nice(%d) passed", NICEINC);

	TEST(nice(-NICEINC));
	if (TST_ERR)
		tst_brk(TBROK | TERRNO, "nice(-NICEINC) failed");
}

static struct tst_test test = {
	.test_all = verify_nice,
	.needs_root = 1,
};
