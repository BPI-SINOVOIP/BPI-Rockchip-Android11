// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2019 Linaro Limited. All rights reserved.
 * Author: Rafael David Tinoco <rafael.tinoco@linaro.org>
 */

/*
 * clock_adjtime() syscall might have as execution path:
 *
 *   1) a regular POSIX clock (only REALTIME clock implements adjtime())
 *      - will behave exactly like adjtimex() system call.
 *      - only one being tested here.
 *
 *   2) a dynamic POSIX clock (which ops are implemented by PTP clocks)
 *      - will trigger the PTP clock driver function "adjtime()"
 *      - different implementations from one PTP clock to another
 *      - might return EOPNOTSUPP (like ptp_kvm_caps, for example)
 *      - no entry point for clock_adjtime(), missing "CLOCK_PTP" model
 *
 * so it is sane to check possible adjustments:
 *
 *    - ADJ_OFFSET     - usec or nsec, kernel adjusts time gradually by offset
 *                       (-512000 < offset < 512000)
 *    - ADJ_FREQUENCY  - system clock frequency offset
 *    - ADJ_MAXERROR   - maximum error (usec)
 *    - ADJ_ESTERROR   - estimated time error in us
 *    - ADJ_STATUS     - clock command/status of ntp implementation
 *    - ADJ_TIMECONST  - PLL stiffness (jitter dependent) + poll int for PLL
 *    - ADJ_TICK       - us between clock ticks
 *                       (>= 900000/HZ, <= 1100000/HZ)
 *
 * and also the standalone ones (using .offset variable):
 *
 *    - ADJ_OFFSET_SINGLESHOT - behave like adjtime()
 *    - ADJ_OFFSET_SS_READ - ret remaining time for completion after SINGLESHOT
 *
 * For ADJ_STATUS, consider the following flags:
 *
 *      rw  STA_PLL - enable phase-locked loop updates (ADJ_OFFSET)
 *      rw  STA_PPSFREQ - enable PPS (pulse-per-second) freq discipline
 *      rw  STA_PPSTIME - enable PPS time discipline
 *      rw  STA_FLL - select freq-locked loop mode.
 *      rw  STA_INS - ins leap sec after the last sec of UTC day (all days)
 *      rw  STA_DEL - del leap sec at last sec of UTC day (all days)
 *      rw  STA_UNSYNC - clock unsynced
 *      rw  STA_FREQHOLD - hold freq. ADJ_OFFSET made w/out auto small adjs
 *      ro  STA_PPSSIGNAL - valid PPS (pulse-per-second) signal is present
 *      ro  STA_PPSJITTER - PPS signal jitter exceeded.
 *      ro  STA_PPSWANDER - PPS signal wander exceeded.
 *      ro  STA_PPSERROR - PPS signal calibration error.
 *      ro  STA_CLOKERR - clock HW fault.
 *      ro  STA_NANO - 0 = us, 1 = ns (set = ADJ_NANO, cl = ADJ_MICRO)
 *      rw  STA_MODE - mode: 0 = phased locked loop. 1 = freq locked loop
 *      ro  STA_CLK - clock source. unused.
 */

#include "clock_adjtime.h"

static long hz;
static struct timex saved, ttxc;
static int supported;

struct test_case {
	unsigned int modes;
	long highlimit;
	long *ptr;
	long delta;
};

struct test_case tc[] = {
	{
	 .modes = ADJ_OFFSET_SINGLESHOT,
	},
	{
	 .modes = ADJ_OFFSET_SS_READ,
	},
	{
	 .modes = ADJ_ALL,
	},
	{
	 .modes = ADJ_OFFSET,
	 .highlimit = 500000,
	 .ptr = &ttxc.offset,
	 .delta = 10000,
	},
	{
	 .modes = ADJ_FREQUENCY,
	 .ptr = &ttxc.freq,
	 .delta = 100,
	},
	{
	 .modes = ADJ_MAXERROR,
	 .ptr = &ttxc.maxerror,
	 .delta = 100,
	},
	{
	 .modes = ADJ_ESTERROR,
	 .ptr = &ttxc.esterror,
	 .delta = 100,
	},
	{
	 .modes = ADJ_TIMECONST,
	 .ptr = &ttxc.constant,
	 .delta = 1,
	},
	{
	 .modes = ADJ_TICK,
	 .highlimit = 1100000,
	 .ptr = &ttxc.tick,
	 .delta = 1000,
	},
};

static void verify_clock_adjtime(unsigned int i)
{
	long ptroff, *ptr = NULL;
	struct timex verify;

	memset(&ttxc, 0, sizeof(struct timex));
	memset(&verify, 0, sizeof(struct timex));

	SAFE_CLOCK_ADJTIME(CLOCK_REALTIME, &ttxc);
	timex_show("GET", ttxc);

	ttxc.modes = tc[i].modes;

	if (tc[i].ptr && tc[i].delta) {

		*tc[i].ptr += tc[i].delta;

		/* fix limits, if existent, so no errors occur */

		if (tc[i].highlimit) {
			if (*tc[i].ptr >= tc[i].highlimit)
				*tc[i].ptr = tc[i].highlimit;
		}
	}

	SAFE_CLOCK_ADJTIME(CLOCK_REALTIME, &ttxc);
	timex_show("SET", ttxc);

	if (tc[i].ptr) {

		/* adjtimex field being tested so we can verify later */

		ptroff = (long) tc[i].ptr - (long) &ttxc;
		ptr = (void *) &verify + ptroff;
	}

	TEST(sys_clock_adjtime(CLOCK_REALTIME, &verify));
	timex_show("VERIFY", verify);

	if (tc[i].ptr && *tc[i].ptr != *ptr) {
		tst_res(TFAIL, "clock_adjtime(): could not set value (mode=%x)",
				tc[i].modes);
	}

	if (TST_RET < 0) {
		tst_res(TFAIL | TTERRNO, "clock_adjtime(): mode=%x, returned "
				"error", tc[i].modes);
	}

	tst_res(TPASS, "clock_adjtime(): success (mode=%x)", tc[i].modes);
}

static void setup(void)
{
	size_t i;
	int rval;

	rval = SAFE_CLOCK_ADJTIME(CLOCK_REALTIME, &saved);
	supported = 1;

	if (rval != TIME_OK && rval != TIME_ERROR) {
		timex_show("SAVE_STATUS", saved);
		tst_brk(TBROK | TTERRNO, "clock has on-going leap changes, "
				"returned: %i", rval);
	}

	hz = SAFE_SYSCONF(_SC_CLK_TCK);

	for (i = 0; i < ARRAY_SIZE(tc); i++) {

		/* fix high and low limits by dividing it per HZ value */

		if (tc[i].modes == ADJ_TICK)
			tc[i].highlimit /= hz;

		/* fix usec as being test default resolution */

		if (saved.modes & ADJ_NANO) {
			if (tc[i].modes == ADJ_OFFSET) {
				tc[i].highlimit *= 1000;
				tc[i].delta *= 1000;
			}
		}
	}
}

static void cleanup(void)
{
	if (supported == 0)
		return;

	saved.modes = ADJ_ALL;

	/* restore clock resolution based on original status flag */

	if (saved.status & STA_NANO)
		saved.modes |= ADJ_NANO;
	else
		saved.modes |= ADJ_MICRO;

	/* restore original clock flags */

	SAFE_CLOCK_ADJTIME(CLOCK_REALTIME, &saved);
}

static struct tst_test test = {
	.test = verify_clock_adjtime,
	.setup = setup,
	.cleanup = cleanup,
	.tcnt = ARRAY_SIZE(tc),
	.needs_root = 1,
	.restore_wallclock = 1,
};
