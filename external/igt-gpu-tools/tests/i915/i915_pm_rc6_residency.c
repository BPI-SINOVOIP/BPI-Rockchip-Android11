/*
 * Copyright © 2012 Intel Corporation
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
 *
 * Authors:
 *    Ben Widawsky <ben@bwidawsk.net>
 *
 */

#include "igt.h"
#include "igt_sysfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>


#define SLEEP_DURATION 3 /* in seconds */

#define RC6_ENABLED	1
#define RC6P_ENABLED	2
#define RC6PP_ENABLED	4

static int sysfs;

struct residencies {
	int rc6;
	int media_rc6;
	int rc6p;
	int rc6pp;
	int duration;
};

static unsigned long get_rc6_enabled_mask(void)
{
	unsigned long enabled;

	enabled = 0;
	igt_sysfs_scanf(sysfs, "power/rc6_enable", "%lu", &enabled);
	return enabled;
}

static bool has_rc6_residency(const char *name)
{
	unsigned long residency;
	char path[128];

	sprintf(path, "power/%s_residency_ms", name);
	return igt_sysfs_scanf(sysfs, path, "%lu", &residency) == 1;
}

static unsigned long read_rc6_residency(const char *name)
{
	unsigned long residency;
	char path[128];

	residency = 0;
	sprintf(path, "power/%s_residency_ms", name);
	igt_assert(igt_sysfs_scanf(sysfs, path, "%lu", &residency) == 1);
	return residency;
}

static void residency_accuracy(unsigned int diff,
			       unsigned int duration,
			       const char *name_of_rc6_residency)
{
	double ratio;

	ratio = (double)diff / duration;

	igt_info("Residency in %s or deeper state: %u ms (sleep duration %u ms) (%.1f%% of expected duration)\n",
		 name_of_rc6_residency, diff, duration, 100*ratio);
	igt_assert_f(ratio > 0.9 && ratio < 1.05,
		     "Sysfs RC6 residency counter is inaccurate.\n");
}

static unsigned long gettime_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void read_residencies(int devid, unsigned int mask,
			     struct residencies *res)
{
	res->duration = gettime_ms();

	if (mask & RC6_ENABLED)
		res->rc6 = read_rc6_residency("rc6");

	if ((mask & RC6_ENABLED) &&
	    (IS_VALLEYVIEW(devid) || IS_CHERRYVIEW(devid)))
		res->media_rc6 = read_rc6_residency("media_rc6");

	if (mask & RC6P_ENABLED)
		res->rc6p = read_rc6_residency("rc6p");

	if (mask & RC6PP_ENABLED)
		res->rc6pp = read_rc6_residency("rc6pp");

	res->duration += (gettime_ms() - res->duration) / 2;
}

static void measure_residencies(int devid, unsigned int mask,
				struct residencies *res)
{
	struct residencies start = { };
	struct residencies end = { };
	int retry;

	/*
	 * Retry in case of counter wrap-around. We simply re-run the
	 * measurement, since the valid counter range is different on
	 * different platforms and so fixing it up would be non-trivial.
	 */
	read_residencies(devid, mask, &end);
	igt_debug("time=%d: rc6=(%d, %d), rc6p=%d, rc6pp=%d\n",
		  end.duration, end.rc6, end.media_rc6, end.rc6p, end.rc6pp);
	for (retry = 0; retry < 2; retry++) {
		start = end;
		sleep(SLEEP_DURATION);
		read_residencies(devid, mask, &end);

		igt_debug("time=%d: rc6=(%d, %d), rc6p=%d, rc6pp=%d\n",
			  end.duration,
			  end.rc6, end.media_rc6, end.rc6p, end.rc6pp);

		if (end.rc6 >= start.rc6 &&
		    end.media_rc6 >= start.media_rc6 &&
		    end.rc6p >= start.rc6p &&
		    end.rc6pp >= start.rc6pp)
			break;
	}
	igt_assert_f(retry < 2, "residency values are not consistent\n");

	res->rc6 = end.rc6 - start.rc6;
	res->rc6p = end.rc6p - start.rc6p;
	res->rc6pp = end.rc6pp - start.rc6pp;
	res->media_rc6 = end.media_rc6 - start.media_rc6;
	res->duration = end.duration - start.duration;

	/*
	 * For the purposes of this test case we want a given residency value
	 * to include the time spent in the corresponding RC state _and_ also
	 * the time spent in any enabled deeper states. So for example if any
	 * of RC6P or RC6PP is enabled we want the time spent in these states
	 * to be also included in the RC6 residency value. The kernel reported
	 * residency values are exclusive, so add up things here.
	 */
	res->rc6p += res->rc6pp;
	res->rc6 += res->rc6p;
}

static bool wait_for_rc6(void)
{
	struct timespec tv = {};
	unsigned long start, now;

	/* First wait for roughly an RC6 Evaluation Interval */
	usleep(160 * 1000);

	/* Then poll for RC6 to start ticking */
	now = read_rc6_residency("rc6");
	do {
		start = now;
		usleep(5000);
		now = read_rc6_residency("rc6");
		if (now - start > 1)
			return true;
	} while (!igt_seconds_elapsed(&tv));

	return false;
}

igt_main
{
	unsigned int rc6_enabled = 0;
	unsigned int devid = 0;

	igt_skip_on_simulation();

	/* Use drm_open_driver to verify device existence */
	igt_fixture {
		int fd;

		fd = drm_open_driver(DRIVER_INTEL);
		devid = intel_get_drm_devid(fd);
		sysfs = igt_sysfs_open(fd);

		igt_require(has_rc6_residency("rc6"));

		/* Make sure rc6 counters are running */
		igt_drop_caches_set(fd, DROP_IDLE);
		igt_require(wait_for_rc6());

		close(fd);

		rc6_enabled = get_rc6_enabled_mask();
		igt_require(rc6_enabled & RC6_ENABLED);
	}

	igt_subtest("rc6-accuracy") {
		struct residencies res;

		measure_residencies(devid, rc6_enabled, &res);
		residency_accuracy(res.rc6, res.duration, "rc6");
	}

	igt_subtest("media-rc6-accuracy") {
		struct residencies res;

		igt_require(IS_VALLEYVIEW(devid) || IS_CHERRYVIEW(devid));

		measure_residencies(devid, rc6_enabled, &res);
		residency_accuracy(res.media_rc6, res.duration, "media_rc6");
	}
}
