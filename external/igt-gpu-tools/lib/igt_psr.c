/*
 * Copyright © 2018 Intel Corporation
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
 */

#include "igt_psr.h"
#include "igt_sysfs.h"
#include <errno.h>

static bool psr_active_check(int debugfs_fd, enum psr_mode mode)
{
	char buf[PSR_STATUS_MAX_LEN];
	const char *state = mode == PSR_MODE_1 ? "SRDENT" : "DEEP_SLEEP";

	igt_debugfs_simple_read(debugfs_fd, "i915_edp_psr_status", buf,
				sizeof(buf));

	return strstr(buf, state);
}

static inline const char *psr_active_state_get(enum psr_mode mode)
{
	return mode == PSR_MODE_1 ? "SRDENT" : "DEEP_SLEEP";
}

/*
 * For PSR1, we wait until PSR is active. We wait until DEEP_SLEEP for PSR2.
 */
bool psr_wait_entry(int debugfs_fd, enum psr_mode mode)
{
	return igt_wait(psr_active_check(debugfs_fd, mode), 500, 20);
}

bool psr_wait_update(int debugfs_fd, enum psr_mode mode)
{
	return igt_wait(!psr_active_check(debugfs_fd, mode), 40, 10);
}

bool psr_long_wait_update(int debugfs_fd, enum psr_mode mode)
{
	return igt_wait(!psr_active_check(debugfs_fd, mode), 500, 10);
}

static ssize_t psr_write(int debugfs_fd, const char *buf)
{
	return igt_sysfs_write(debugfs_fd, "i915_edp_psr_debug", buf,
			       strlen(buf));
}

static int has_psr_debugfs(int debugfs_fd)
{
	int ret;

	/*
	 * Check if new PSR debugfs api is usable by writing an invalid value.
	 * Legacy mode will return OK here, debugfs api will return -EINVAL.
	 * -ENODEV is returned when PSR is unavailable.
	 */
	ret = psr_write(debugfs_fd, "0xf");
	if (ret == -EINVAL)
		return 0;
	else if (ret < 0)
		return ret;

	/* legacy debugfs api, we enabled irqs by writing, disable them. */
	psr_write(debugfs_fd, "0");
	return -EINVAL;
}

static bool psr_modparam_set(int val)
{
	static int oldval = -1;

	igt_set_module_param_int("enable_psr", val);

	if (val == oldval)
		return false;

	oldval = val;
	return true;
}

static int psr_restore_debugfs_fd = -1;

static void restore_psr_debugfs(int sig)
{
	psr_write(psr_restore_debugfs_fd, "0");
}

static bool psr_set(int debugfs_fd, int mode)
{
	int ret;

	ret = has_psr_debugfs(debugfs_fd);
	if (ret == -ENODEV) {
		igt_skip("PSR not available\n");
		return false;
	}

	if (ret == -EINVAL) {
		/*
		 * We can not control what PSR version is going to be enabled
		 * by setting enable_psr parameter, when unmatched the PSR
		 * version enabled and the PSR version of the test, it will
		 * fail in the first psr_wait_entry() of the test.
		 */
		ret = psr_modparam_set(mode >= PSR_MODE_1);
	} else {
		const char *debug_val;

		switch (mode) {
		case PSR_MODE_1:
			debug_val = "0x3";
			break;
		case PSR_MODE_2:
			debug_val = "0x2";
			break;
		default:
			/* Disables PSR */
			debug_val = "0x1";
		}

		ret = psr_write(debugfs_fd, debug_val);
		igt_assert(ret > 0);
	}

	/* Restore original value on exit */
	if (psr_restore_debugfs_fd == -1) {
		psr_restore_debugfs_fd = dup(debugfs_fd);
		igt_assert(psr_restore_debugfs_fd >= 0);
		igt_install_exit_handler(restore_psr_debugfs);
	}

	return ret;
}

bool psr_enable(int debugfs_fd, enum psr_mode mode)
{
	return psr_set(debugfs_fd, mode);
}

bool psr_disable(int debugfs_fd)
{
	/* Any mode different than PSR_MODE_1/2 will disable PSR */
	return psr_set(debugfs_fd, -1);
}

bool psr_sink_support(int debugfs_fd, enum psr_mode mode)
{
	char buf[PSR_STATUS_MAX_LEN];
	int ret;

	ret = igt_debugfs_simple_read(debugfs_fd, "i915_edp_psr_status", buf,
				      sizeof(buf));
	if (ret < 1)
		return false;

	if (mode == PSR_MODE_1)
		return strstr(buf, "Sink_Support: yes\n") ||
		       strstr(buf, "Sink support: yes");
	else
		/*
		 * i915 requires PSR version 0x03 that is PSR2 + SU with
		 * Y-coordinate to support PSR2
		 */
		return strstr(buf, "Sink support: yes [0x03]");
}

#define PSR2_SU_BLOCK_STR_LOOKUP "PSR2 SU blocks:\n0\t"

static bool
psr2_read_last_num_su_blocks_val(int debugfs_fd, uint16_t *num_su_blocks)
{
	char buf[PSR_STATUS_MAX_LEN];
	char *str;
	int ret;

	ret = igt_debugfs_simple_read(debugfs_fd, "i915_edp_psr_status", buf,
				      sizeof(buf));
	if (ret < 0)
		return false;

	str = strstr(buf, PSR2_SU_BLOCK_STR_LOOKUP);
	if (!str)
		return false;

	str = &str[strlen(PSR2_SU_BLOCK_STR_LOOKUP)];
	*num_su_blocks = (uint16_t)strtol(str, NULL, 10);

	return true;
}

bool psr2_wait_su(int debugfs_fd, uint16_t *num_su_blocks)
{
	return igt_wait(psr2_read_last_num_su_blocks_val(debugfs_fd, num_su_blocks), 40, 1);
}
