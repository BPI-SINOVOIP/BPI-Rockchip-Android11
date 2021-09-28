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
 *    Jeff McGee <jeff.mcgee@intel.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/wait.h>

#include "igt.h"
#include "igt_dummyload.h"
#include "igt_sysfs.h"

IGT_TEST_DESCRIPTION("Render P-States tests - verify GPU frequency changes");

static int drm_fd;

enum {
	CUR,
	MIN,
	MAX,
	RP0,
	RP1,
	RPn,
	BOOST,
	NUMFREQ
};

static int origfreqs[NUMFREQ];

struct sysfs_file {
	const char *name;
	const char *mode;
	FILE *filp;
} sysfs_files[] = {
	{ "cur", "r", NULL },
	{ "min", "rb+", NULL },
	{ "max", "rb+", NULL },
	{ "RP0", "r", NULL },
	{ "RP1", "r", NULL },
	{ "RPn", "r", NULL },
	{ "boost", "rb+", NULL },
	{ NULL, NULL, NULL }
};

static int readval(FILE *filp)
{
	int val;
	int scanned;

	rewind(filp);
	scanned = fscanf(filp, "%d", &val);
	igt_assert_eq(scanned, 1);

	return val;
}

static void read_freqs(int *freqs)
{
	int i;

	for (i = 0; i < NUMFREQ; i++)
		freqs[i] = readval(sysfs_files[i].filp);
}

static void nsleep(unsigned long ns)
{
	struct timespec ts;
	int ret;

	ts.tv_sec = 0;
	ts.tv_nsec = ns;
	do {
		struct timespec rem;

		ret = nanosleep(&ts, &rem);
		igt_assert(ret == 0 || errno == EINTR);
		ts = rem;
	} while (ret && errno == EINTR);
}

static void wait_freq_settle(void)
{
	int timeout = 10;

	while (1) {
		int freqs[NUMFREQ];

		read_freqs(freqs);
		if (freqs[CUR] >= freqs[MIN] && freqs[CUR] <= freqs[MAX])
			break;
		nsleep(1000000);
		if (!timeout--)
			break;
	}
}

static int do_writeval(FILE *filp, int val, int lerrno, bool readback_check)
{
	int ret, orig;

	orig = readval(filp);
	rewind(filp);
	ret = fprintf(filp, "%d", val);

	if (lerrno) {
		/* Expecting specific error */
		igt_assert(ret == EOF && errno == lerrno);
		if (readback_check)
			igt_assert_eq(readval(filp), orig);
	} else {
		/* Expecting no error */
		igt_assert_lt(0, ret);
		wait_freq_settle();
		if (readback_check)
			igt_assert_eq(readval(filp), val);
	}

	return ret;
}
#define writeval(filp, val) do_writeval(filp, val, 0, true)
#define writeval_inval(filp, val) do_writeval(filp, val, EINVAL, true)
#define writeval_nocheck(filp, val) do_writeval(filp, val, 0, false)

static void check_freq_constraints(const int *freqs)
{
	igt_assert_lte(freqs[MIN], freqs[MAX]);
	igt_assert_lte(freqs[CUR], freqs[MAX]);
	igt_assert_lte(freqs[RPn], freqs[CUR]);
	igt_assert_lte(freqs[RPn], freqs[MIN]);
	igt_assert_lte(freqs[MAX], freqs[RP0]);
	igt_assert_lte(freqs[RP1], freqs[RP0]);
	igt_assert_lte(freqs[RPn], freqs[RP1]);
	igt_assert_neq(freqs[RP0], 0);
	igt_assert_neq(freqs[RP1], 0);
}

static void dump(const int *freqs)
{
	int i;

	igt_debug("gt freq (MHz):");
	for (i = 0; i < NUMFREQ; i++)
		igt_debug("  %s=%d", sysfs_files[i].name, freqs[i]);

	igt_debug("\n");
}

enum load {
	LOW = 0,
	HIGH
};

static struct load_helper {
	int link;
	enum load load;
	bool exit;
	bool signal;
	struct igt_helper_process igt_proc;
} lh;

static void load_helper_signal_handler(int sig)
{
	if (sig == SIGUSR2) {
		lh.load = !lh.load;
		lh.signal = true;
		igt_debug("Switching background load to %s\n", lh.load ? "high" : "low");
	} else
		lh.exit = true;
}

static void load_helper_sync(void)
{
	bool dummy;

	igt_assert_eq(read(lh.link, &dummy, sizeof(dummy)), sizeof(dummy));
}

#define LOAD_HELPER_PAUSE_USEC 500
#define LOAD_HELPER_BO_SIZE (16*1024*1024)
static void load_helper_set_load(enum load load)
{
	igt_assert(lh.igt_proc.running);

	if (lh.load == load)
		return;

	lh.load = load;
	kill(lh.igt_proc.pid, SIGUSR2);

	/* wait for load-helper to switch */
	load_helper_sync();
}

static void load_helper_run(enum load load)
{
	int link[2];

	/*
	 * FIXME fork helpers won't get cleaned up when started from within a
	 * subtest, so handle the case where it sticks around a bit too long.
	 */
	if (lh.igt_proc.running) {
		load_helper_set_load(load);
		return;
	}

	igt_require_gem(drm_fd);

	lh.exit = false;
	lh.load = load;
	lh.signal = true;

	pipe(link);
	lh.link = link[1];

	igt_fork_helper(&lh.igt_proc) {
		igt_spin_t *spin[2] = {};
		bool prev_load;
		uint32_t handle;

		signal(SIGUSR1, load_helper_signal_handler);
		signal(SIGUSR2, load_helper_signal_handler);

		igt_debug("Applying %s load...\n", lh.load ? "high" : "low");

		prev_load = lh.load == HIGH;
		spin[0] = __igt_spin_new(drm_fd);
		if (prev_load)
			spin[1] = __igt_spin_new(drm_fd);
		prev_load = !prev_load; /* send the initial signal */
		while (!lh.exit) {
			bool high_load;

			handle = spin[0]->handle;
			igt_spin_end(spin[0]);
			while (gem_bo_busy(drm_fd, handle))
				usleep(100);

			igt_spin_free(drm_fd, spin[0]);
			usleep(100);

			high_load = lh.load == HIGH;
			if (!high_load && spin[1]) {
				igt_spin_free(drm_fd, spin[1]);
				spin[1] = NULL;
			} else {
				spin[0] = spin[1];
			}
			spin[high_load] = __igt_spin_new(drm_fd);

			if (lh.signal && high_load != prev_load) {
				write(lh.link, &lh.signal, sizeof(lh.signal));
				lh.signal = false;
			}
			prev_load = high_load;
		}

		handle = spin[0]->handle;
		igt_spin_end(spin[0]);

		if (spin[1]) {
			handle = spin[1]->handle;
			igt_spin_end(spin[1]);
		}

		/* Wait for completion without boosting */
		usleep(1000);
		while (gem_bo_busy(drm_fd, handle))
			usleep(1000);

		/*
		 * Idle/boost logic is tied with request retirement.
		 * Speed up detection of idle state and ensure deboost
		 * after removing load.
		 */
		igt_drop_caches_set(drm_fd, DROP_RETIRE);

		igt_spin_free(drm_fd, spin[1]);
		igt_spin_free(drm_fd, spin[0]);
	}

	close(lh.link);
	lh.link = link[0];

	/* wait for our helper to complete its first round */
	load_helper_sync();
}

static void load_helper_stop(void)
{
	kill(lh.igt_proc.pid, SIGUSR1);
	igt_assert(igt_wait_helper(&lh.igt_proc) == 0);
}

static void do_load_gpu(void)
{
	load_helper_run(LOW);
	nsleep(10000000);
	load_helper_stop();
}

/* Return a frequency rounded by HW to the nearest supported value */
static int get_hw_rounded_freq(int target)
{
	int freqs[NUMFREQ];
	int old_freq;
	int idx;
	int ret;

	read_freqs(freqs);

	if (freqs[MIN] > target)
		idx = MIN;
	else
		idx = MAX;

	old_freq = freqs[idx];
	writeval_nocheck(sysfs_files[idx].filp, target);
	read_freqs(freqs);
	ret = freqs[idx];
	writeval_nocheck(sysfs_files[idx].filp, old_freq);

	return ret;
}

/*
 * Modify softlimit MIN and MAX freqs to valid and invalid levels. Depending
 * on subtest run different check after each modification.
 */
static void min_max_config(void (*check)(void), bool load_gpu)
{
	int fmid = (origfreqs[RPn] + origfreqs[RP0]) / 2;

	/*
	 * hw (and so kernel) rounds to the nearest value supported by
	 * the given platform.
	 */
	fmid = get_hw_rounded_freq(fmid);

	igt_debug("\nCheck original min and max...\n");
	if (load_gpu)
		do_load_gpu();
	check();

	igt_debug("\nSet min=RPn and max=RP0...\n");
	writeval(sysfs_files[MIN].filp, origfreqs[RPn]);
	writeval(sysfs_files[MAX].filp, origfreqs[RP0]);
	if (load_gpu)
		do_load_gpu();
	check();

	igt_debug("\nIncrease min to midpoint...\n");
	writeval(sysfs_files[MIN].filp, fmid);
	if (load_gpu)
		do_load_gpu();
	check();

	igt_debug("\nIncrease min to RP0...\n");
	writeval(sysfs_files[MIN].filp, origfreqs[RP0]);
	if (load_gpu)
		do_load_gpu();
	check();

	igt_debug("\nIncrease min above RP0 (invalid)...\n");
	writeval_inval(sysfs_files[MIN].filp, origfreqs[RP0] + 1000);
	check();

	igt_debug("\nDecrease max to RPn (invalid)...\n");
	writeval_inval(sysfs_files[MAX].filp, origfreqs[RPn]);
	check();

	igt_debug("\nDecrease min to midpoint...\n");
	writeval(sysfs_files[MIN].filp, fmid);
	if (load_gpu)
		do_load_gpu();
	check();

	igt_debug("\nDecrease min to RPn...\n");
	writeval(sysfs_files[MIN].filp, origfreqs[RPn]);
	if (load_gpu)
		do_load_gpu();
	check();

	igt_debug("\nDecrease min below RPn (invalid)...\n");
	writeval_inval(sysfs_files[MIN].filp, 0);
	check();

	igt_debug("\nDecrease max to midpoint...\n");
	writeval(sysfs_files[MAX].filp, fmid);
	check();

	igt_debug("\nDecrease max to RPn...\n");
	writeval(sysfs_files[MAX].filp, origfreqs[RPn]);
	check();

	igt_debug("\nDecrease max below RPn (invalid)...\n");
	writeval_inval(sysfs_files[MAX].filp, 0);
	check();

	igt_debug("\nIncrease min to RP0 (invalid)...\n");
	writeval_inval(sysfs_files[MIN].filp, origfreqs[RP0]);
	check();

	igt_debug("\nIncrease max to midpoint...\n");
	writeval(sysfs_files[MAX].filp, fmid);
	check();

	igt_debug("\nIncrease max to RP0...\n");
	writeval(sysfs_files[MAX].filp, origfreqs[RP0]);
	check();

	igt_debug("\nIncrease max above RP0 (invalid)...\n");
	writeval_inval(sysfs_files[MAX].filp, origfreqs[RP0] + 1000);
	check();

	writeval(sysfs_files[MIN].filp, origfreqs[MIN]);
	writeval(sysfs_files[MAX].filp, origfreqs[MAX]);
}

static void basic_check(void)
{
	int freqs[NUMFREQ];

	read_freqs(freqs);
	dump(freqs);
	check_freq_constraints(freqs);
}

#define IDLE_WAIT_TIMESTEP_MSEC 250
#define IDLE_WAIT_TIMEOUT_MSEC 2500
static void idle_check(void)
{
	int freqs[NUMFREQ];
	int wait = 0;

	/* Monitor frequencies until cur settles down to min, which should
	 * happen within the allotted time */
	do {
		read_freqs(freqs);
		dump(freqs);
		check_freq_constraints(freqs);
		if (freqs[CUR] == freqs[RPn])
			break;
		usleep(1000 * IDLE_WAIT_TIMESTEP_MSEC);
		wait += IDLE_WAIT_TIMESTEP_MSEC;
	} while (wait < IDLE_WAIT_TIMEOUT_MSEC);

	igt_debugfs_dump(drm_fd, "i915_rps_boost_info");
	igt_assert_eq(freqs[CUR], freqs[RPn]);
	igt_debug("Required %d msec to reach cur=idle\n", wait);
}

#define LOADED_WAIT_TIMESTEP_MSEC 100
#define LOADED_WAIT_TIMEOUT_MSEC 3000
static void loaded_check(void)
{
	int freqs[NUMFREQ];
	int wait = 0;

	/* Monitor frequencies until cur increases to max, which should
	 * happen within the allotted time */
	do {
		read_freqs(freqs);
		dump(freqs);
		check_freq_constraints(freqs);
		if (freqs[CUR] >= freqs[MAX])
			break;
		usleep(1000 * LOADED_WAIT_TIMESTEP_MSEC);
		wait += LOADED_WAIT_TIMESTEP_MSEC;
	} while (wait < LOADED_WAIT_TIMEOUT_MSEC);

	igt_debugfs_dump(drm_fd, "i915_rps_boost_info");
	igt_assert_lte(freqs[MAX], freqs[CUR]);
	igt_debug("Required %d msec to reach cur=max\n", wait);
}

#define STABILIZE_WAIT_TIMESTEP_MSEC 250
#define STABILIZE_WAIT_TIMEOUT_MSEC 15000
static void stabilize_check(int *out)
{
	int freqs[NUMFREQ];
	int wait = 0;

	/* Monitor frequencies until HW will stabilize cur frequency.
	 * It should happen within allotted time */
	read_freqs(freqs);
	dump(freqs);
	usleep(1000 * STABILIZE_WAIT_TIMESTEP_MSEC);
	do {
		read_freqs(out);
		dump(out);

		if (memcmp(freqs, out, sizeof(freqs)) == 0)
			break;

		memcpy(freqs, out, sizeof(freqs));
		wait += STABILIZE_WAIT_TIMESTEP_MSEC;
	} while (wait < STABILIZE_WAIT_TIMEOUT_MSEC);

	igt_debugfs_dump(drm_fd, "i915_rps_boost_info");
	igt_debug("Waited %d msec to stabilize cur\n", wait);
}

static void resubmit_batch(int fd, uint32_t handle, int count)
{
	struct drm_i915_gem_exec_object2 obj = {
		.handle = handle
	};
	struct drm_i915_gem_execbuffer2 eb = {
		.buffer_count = 1,
		.buffers_ptr = to_user_pointer(&obj),
	};
	while (count--)
		gem_execbuf(fd, &eb);
}

static void boost_freq(int fd, int *boost_freqs)
{
	int64_t timeout = 1;
	igt_spin_t *load;

	load = igt_spin_new(fd);
	resubmit_batch(fd, load->handle, 16);

	/* Waiting will grant us a boost to maximum */
	gem_wait(fd, load->handle, &timeout);

	read_freqs(boost_freqs);
	dump(boost_freqs);

	/* Avoid downlocking till boost request is pending */
	igt_spin_end(load);
	gem_sync(fd, load->handle);
	igt_spin_free(fd, load);
}

static void waitboost(int fd, bool reset)
{
	int pre_freqs[NUMFREQ];
	int boost_freqs[NUMFREQ];
	int post_freqs[NUMFREQ];
	int fmid = (origfreqs[RPn] + origfreqs[RP0]) / 2;
	fmid = get_hw_rounded_freq(fmid);

	load_helper_run(LOW);

	igt_debug("Apply low load...\n");
	sleep(1);
	stabilize_check(pre_freqs);

	if (reset) {
		igt_debug("Reset gpu...\n");
		igt_force_gpu_reset(fd);
		sleep(1);
	}

	/* Set max freq to less than boost freq */
	writeval(sysfs_files[MAX].filp, fmid);

	/* When we wait upon the GPU, we want to temporarily boost it
	 * to maximum.
	 */
	boost_freq(fd, boost_freqs);

	/* Set max freq to original softmax */
	writeval(sysfs_files[MAX].filp, origfreqs[MAX]);

	igt_debug("Apply low load again...\n");
	sleep(1);
	stabilize_check(post_freqs);

	igt_debug("Removing load...\n");
	load_helper_stop();
	idle_check();

	igt_assert_lt(pre_freqs[CUR], pre_freqs[MAX]);
	igt_assert_eq(boost_freqs[CUR], boost_freqs[BOOST]);
	igt_assert_lt(post_freqs[CUR], post_freqs[MAX]);
}

static void pm_rps_exit_handler(int sig)
{
	if (origfreqs[MIN] > readval(sysfs_files[MAX].filp)) {
		writeval(sysfs_files[MAX].filp, origfreqs[MAX]);
		writeval(sysfs_files[MIN].filp, origfreqs[MIN]);
	} else {
		writeval(sysfs_files[MIN].filp, origfreqs[MIN]);
		writeval(sysfs_files[MAX].filp, origfreqs[MAX]);
	}

	if (lh.igt_proc.running)
		load_helper_stop();

	close(drm_fd);
}

igt_main
{
	igt_skip_on_simulation();

	igt_fixture {
		struct sysfs_file *sysfs_file = sysfs_files;
		char sysfs_path[80];
		int ret;

		/* Use drm_open_driver to verify device existence */
		drm_fd = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(drm_fd);
		igt_require(gem_can_store_dword(drm_fd, 0));
		igt_assert(igt_sysfs_path(drm_fd, sysfs_path,
					  sizeof(sysfs_path)));

		do {
			int val = -1;
			char *path;

			ret = asprintf(&path, "%s/gt_%s_freq_mhz",
				       sysfs_path, sysfs_file->name);
			igt_assert(ret != -1);
			sysfs_file->filp = fopen(path, sysfs_file->mode);
			igt_require(sysfs_file->filp);
			setbuf(sysfs_file->filp, NULL);

			val = readval(sysfs_file->filp);
			igt_assert(val >= 0);
			sysfs_file++;
		} while (sysfs_file->name != NULL);

		read_freqs(origfreqs);

		igt_install_exit_handler(pm_rps_exit_handler);
	}

	igt_subtest("basic-api")
		min_max_config(basic_check, false);

	/* Verify the constraints, check if we can reach idle */
	igt_subtest("min-max-config-idle")
		min_max_config(idle_check, true);

	/* Verify the constraints with high load, check if we can reach max */
	igt_subtest("min-max-config-loaded") {
		load_helper_run(HIGH);
		min_max_config(loaded_check, false);
		load_helper_stop();
	}

	/* Checks if we achieve boost using gem_wait */
	igt_subtest("waitboost")
		waitboost(drm_fd, false);

	/* Test boost frequency after GPU reset */
	igt_subtest("reset") {
		igt_hang_t hang = igt_allow_hang(drm_fd, 0, 0);
		waitboost(drm_fd, true);
		igt_disallow_hang(drm_fd, hang);
	}
}
