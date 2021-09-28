/*
 * Copyright © 2015 Intel Corporation
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
 * Author:
 *    Antti Koskipaa <antti.koskipaa@linux.intel.com>
 *
 */

#include "igt.h"
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

struct context {
	int max;
};


#define TOLERANCE 5 /* percent */
#define BACKLIGHT_PATH "/sys/class/backlight/intel_backlight"

#define FADESTEPS 10
#define FADESPEED 100 /* milliseconds between steps */

IGT_TEST_DESCRIPTION("Basic backlight sysfs test");
static int8_t *pm_data = NULL;

static int backlight_read(int *result, const char *fname)
{
	int fd;
	char full[PATH_MAX];
	char dst[64];
	int r, e;

	igt_assert(snprintf(full, PATH_MAX, "%s/%s", BACKLIGHT_PATH, fname) < PATH_MAX);

	fd = open(full, O_RDONLY);
	if (fd == -1)
		return -errno;

	r = read(fd, dst, sizeof(dst));
	e = errno;
	close(fd);

	if (r < 0)
		return -e;

	errno = 0;
	*result = strtol(dst, NULL, 10);
	return errno;
}

static int backlight_write(int value, const char *fname)
{
	int fd;
	char full[PATH_MAX];
	char src[64];
	int len;

	igt_assert(snprintf(full, PATH_MAX, "%s/%s", BACKLIGHT_PATH, fname) < PATH_MAX);
	fd = open(full, O_WRONLY);
	if (fd == -1)
		return -errno;

	len = snprintf(src, sizeof(src), "%i", value);
	len = write(fd, src, len);
	close(fd);

	if (len < 0)
		return len;

	return 0;
}

static void test_and_verify(struct context *context, int val)
{
	const int tolerance = val * TOLERANCE / 100;
	int result;

	igt_assert_eq(backlight_write(val, "brightness"), 0);
	igt_assert_eq(backlight_read(&result, "brightness"), 0);
	/* Check that the exact value sticks */
	igt_assert_eq(result, val);

	igt_assert_eq(backlight_read(&result, "actual_brightness"), 0);
	/* Some rounding may happen depending on hw */
	igt_assert_f(result >= max(0, val - tolerance) &&
		     result <= min(context->max, val + tolerance),
		     "actual_brightness [%d] did not match expected brightness [%d +- %d]\n",
		     result, val, tolerance);
}

static void test_brightness(struct context *context)
{
	test_and_verify(context, 0);
	test_and_verify(context, context->max);
	test_and_verify(context, context->max / 2);
}

static void test_bad_brightness(struct context *context)
{
	int val;
	/* First write some sane value */
	backlight_write(context->max / 2, "brightness");
	/* Writing invalid values should fail and not change the value */
	igt_assert_lt(backlight_write(-1, "brightness"), 0);
	backlight_read(&val, "brightness");
	igt_assert_eq(val, context->max / 2);
	igt_assert_lt(backlight_write(context->max + 1, "brightness"), 0);
	backlight_read(&val, "brightness");
	igt_assert_eq(val, context->max / 2);
	igt_assert_lt(backlight_write(INT_MAX, "brightness"), 0);
	backlight_read(&val, "brightness");
	igt_assert_eq(val, context->max / 2);
}

static void test_fade(struct context *context)
{
	int i;
	static const struct timespec ts = { .tv_sec = 0, .tv_nsec = FADESPEED*1000000 };

	/* Fade out, then in */
	for (i = context->max; i > 0; i -= context->max / FADESTEPS) {
		test_and_verify(context, i);
		nanosleep(&ts, NULL);
	}
	for (i = 0; i <= context->max; i += context->max / FADESTEPS) {
		test_and_verify(context, i);
		nanosleep(&ts, NULL);
	}
}

static void
test_fade_with_dpms(struct context *context, igt_output_t *output)
{
	igt_require(igt_setup_runtime_pm());

	kmstest_set_connector_dpms(output->display->drm_fd,
				   output->config.connector,
				   DRM_MODE_DPMS_OFF);
	igt_require(igt_wait_for_pm_status(IGT_RUNTIME_PM_STATUS_SUSPENDED));

	kmstest_set_connector_dpms(output->display->drm_fd,
				   output->config.connector,
				   DRM_MODE_DPMS_ON);
	igt_assert(igt_wait_for_pm_status(IGT_RUNTIME_PM_STATUS_ACTIVE));

	test_fade(context);
}

static void
test_fade_with_suspend(struct context *context, igt_output_t *output)
{
	igt_system_suspend_autoresume(SUSPEND_STATE_MEM, SUSPEND_TEST_NONE);

	test_fade(context);
}

igt_main
{
	struct context context = {0};
	int old;
	igt_display_t display;
	igt_output_t *output;
	struct igt_fb fb;

	igt_skip_on_simulation();

	igt_fixture {
		enum pipe pipe;
		bool found = false;
		char full_name[32] = {};
		char *name;
		drmModeModeInfo *mode;
		igt_plane_t *primary;

		/* Get the max value and skip the whole test if sysfs interface not available */
		igt_skip_on(backlight_read(&old, "brightness"));
		igt_assert(backlight_read(&context.max, "max_brightness") > -1);

		/*
		 * Backlight tests requires the output to be enabled,
		 * try to enable all.
		 */
		kmstest_set_vt_graphics_mode();
		igt_display_require(&display, drm_open_driver(DRIVER_INTEL));

		/* should be ../../cardX-$output */
		igt_assert_lt(12, readlink(BACKLIGHT_PATH "/device", full_name, sizeof(full_name) - 1));
		name = basename(full_name);

		for_each_pipe_with_valid_output(&display, pipe, output) {
			if (strcmp(name + 6, output->name))
				continue;
			found = true;
			break;
		}

		igt_require_f(found,
			      "Could not map backlight for \"%s\" to connected output\n",
			      name);

		igt_output_set_pipe(output, pipe);
		mode = igt_output_get_mode(output);

		igt_create_pattern_fb(display.drm_fd,
				      mode->hdisplay, mode->vdisplay,
				      DRM_FORMAT_XRGB8888,
				      LOCAL_DRM_FORMAT_MOD_NONE, &fb);
		primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);
		igt_plane_set_fb(primary, &fb);

		igt_display_commit2(&display, display.is_atomic ? COMMIT_ATOMIC : COMMIT_LEGACY);
		pm_data = igt_pm_enable_sata_link_power_management();
	}

	igt_subtest("basic-brightness")
		test_brightness(&context);
	igt_subtest("bad-brightness")
		test_bad_brightness(&context);
	igt_subtest("fade")
		test_fade(&context);
	igt_subtest("fade_with_dpms")
		test_fade_with_dpms(&context, output);
	igt_subtest("fade_with_suspend")
		test_fade_with_suspend(&context, output);

	igt_fixture {
		/* Restore old brightness */
		backlight_write(old, "brightness");

		igt_display_fini(&display);
		igt_remove_fb(display.drm_fd, &fb);
		igt_pm_restore_sata_link_power_management(pm_data);
		free(pm_data);
		close(display.drm_fd);
	}
}
