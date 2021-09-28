// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Wipro Technologies Ltd, 2003.  All Rights Reserved.
 *
 * Author:	Aniruddha Marathe <aniruddha.marathe@wipro.com>
 *
 * Ported to new library:
 * 07/2019      Christian Amann <camann@suse.com>
 */
/*
 * Basic test for timer_delete(2)
 *
 *	Creates a timer for each available clock and then tries
 *	to delete them again.
 */

#include <errno.h>
#include <time.h>
#include "tst_test.h"
#include "lapi/common_timers.h"

static void run(void)
{
	unsigned int i;
	kernel_timer_t timer_id;

	for (i = 0; i < CLOCKS_DEFINED; ++i) {
		clock_t clock = clock_list[i];

		if (clock == CLOCK_PROCESS_CPUTIME_ID ||
			clock == CLOCK_THREAD_CPUTIME_ID) {
			if (!have_cputime_timers())
				continue;
		}

		tst_res(TINFO, "Testing %s", get_clock_str(clock));

		TEST(tst_syscall(__NR_timer_create, clock, NULL, &timer_id));
		if (TST_RET != 0) {
			if (possibly_unsupported(clock) &&
				(TST_ERR == EINVAL || TST_ERR == ENOTSUP)) {
				tst_res(TCONF | TTERRNO, "%s unsupported",
					get_clock_str(clock));
			} else {
				tst_res(TFAIL | TTERRNO,
					"Aborting test - timer_create(%s) failed",
					get_clock_str(clock));
			}
			continue;
		}

		TEST(tst_syscall(__NR_timer_delete, timer_id));
		if (TST_RET == 0)
			tst_res(TPASS, "Timer deleted successfully!");
		else
			tst_res(TFAIL | TTERRNO, "Timer deletion failed!");
	}
}

static struct tst_test test = {
	.test_all = run,
	.needs_root = 1,
};
