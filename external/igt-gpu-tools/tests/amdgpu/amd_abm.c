/*
 * Copyright 2018 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "igt.h"
#include "drmtest.h"
#include "igt_kms.h"
#include <limits.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#define BACKLIGHT_PATH "/sys/class/backlight/amdgpu_bl0"

typedef struct data {
	igt_display_t display;
	int drm_fd;
	int debugfs;
	uint32_t output_id;
	uint32_t abm_prop_id;
} data_t;

static int read_current_backlight_pwm(int debugfs_dir)
{
	char buf[20];

	igt_debugfs_simple_read(debugfs_dir, "amdgpu_current_backlight_pwm",
			 buf, sizeof(buf));

	return strtol(buf, NULL, 0);
}

static int read_target_backlight_pwm(int debugfs_dir)
{
	char buf[20];

	igt_debugfs_simple_read(debugfs_dir, "amdgpu_target_backlight_pwm",
			 buf, sizeof(buf));

	return strtol(buf, NULL, 0);
}

static int backlight_write_brightness(int value)
{
	int fd;
	char full[PATH_MAX];
	char src[64];
	int len;

	igt_assert(snprintf(full, PATH_MAX, "%s/%s", BACKLIGHT_PATH, "brightness") < PATH_MAX);
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

static void set_abm_level(data_t *data, int level)
{
	uint32_t type = DRM_MODE_OBJECT_CONNECTOR;
	int ret;

	ret = drmModeObjectSetProperty(data->drm_fd, data->output_id, type,
				       data->abm_prop_id, level);
	igt_assert_eq(ret, 0);
}

static int backlight_read_max_brightness(int *result)
{
	int fd;
	char full[PATH_MAX];
	char dst[64];
	int r, e;

	igt_assert(snprintf(full, PATH_MAX, "%s/%s", BACKLIGHT_PATH, "max_brightness") < PATH_MAX);

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

static void test_init(data_t *data)
{
	igt_display_t *display = &data->display;
	int ret, i;
	char buf[20];
	bool abm_prop_exists;
	uint32_t type = DRM_MODE_OBJECT_CONNECTOR;

	ret = igt_debugfs_simple_read(data->debugfs, "amdgpu_current_backlight_pwm",
			 buf, sizeof(buf));

	if (ret < 0)
		igt_skip("No current backlight debugfs entry.\n");

	ret = igt_debugfs_simple_read(data->debugfs, "amdgpu_target_backlight_pwm",
			 buf, sizeof(buf));

	if (ret < 0)
		igt_skip("No target backlight debugfs entry.\n");

	abm_prop_exists = false;

	for (i = 0; i < display->n_outputs; i++) {
		data->output_id = display->outputs[i].id;

		abm_prop_exists = kmstest_get_property(
			data->drm_fd, data->output_id, type, "abm level",
			&data->abm_prop_id, NULL, NULL);

		if (abm_prop_exists)
			break;
	}

	if (!abm_prop_exists)
		igt_skip("No abm level property on any connector.\n");
}


static void backlight_dpms_cycle(data_t *data, igt_output_t *output)
{
	int ret;
	int max_brightness;
	int pwm_1, pwm_2;

	ret = backlight_read_max_brightness(&max_brightness);
	igt_assert_eq(ret, 0);

	set_abm_level(data, 0);
	backlight_write_brightness(max_brightness / 2);
	usleep(100000);
	pwm_1 = read_target_backlight_pwm(data->debugfs);

	kmstest_set_connector_dpms(data->drm_fd, output->config.connector, DRM_MODE_DPMS_OFF);
	kmstest_set_connector_dpms(data->drm_fd, output->config.connector, DRM_MODE_DPMS_ON);
	usleep(100000);
	pwm_2 = read_target_backlight_pwm(data->debugfs);
	igt_assert_eq(pwm_1, pwm_2);
}

static void backlight_monotonic_basic(data_t *data)
{
	int ret;
	int max_brightness;
	int prev_pwm, pwm;
	int brightness_step;
	int brightness;

	ret = backlight_read_max_brightness(&max_brightness);
	igt_assert_eq(ret, 0);

	brightness_step = max_brightness / 10;

	set_abm_level(data, 0);
	backlight_write_brightness(max_brightness);
	usleep(100000);
	prev_pwm = read_target_backlight_pwm(data->debugfs);
	for (brightness = max_brightness - brightness_step;
	     brightness > 0;
	     brightness -= brightness_step) {
		backlight_write_brightness(brightness);
		usleep(100000);
		pwm = read_target_backlight_pwm(data->debugfs);
		igt_assert(pwm < prev_pwm);
		prev_pwm = pwm;
	}

}

static void backlight_monotonic_abm(data_t *data)
{
	int ret, i;
	int max_brightness;
	int prev_pwm, pwm;
	int brightness_step;
	int brightness;

	ret = backlight_read_max_brightness(&max_brightness);
	igt_assert_eq(ret, 0);

	brightness_step = max_brightness / 10;
	for (i = 1; i < 5; i++) {
		set_abm_level(data, i);
		backlight_write_brightness(max_brightness);
		usleep(100000);
		prev_pwm = read_target_backlight_pwm(data->debugfs);
		for (brightness = max_brightness - brightness_step;
		     brightness > 0;
		     brightness -= brightness_step) {
			backlight_write_brightness(brightness);
			usleep(100000);
			pwm = read_target_backlight_pwm(data->debugfs);
			igt_assert(pwm < prev_pwm);
			prev_pwm = pwm;
		}
	}
}

static void abm_enabled(data_t *data)
{
	int ret, i;
	int max_brightness;
	int pwm, prev_pwm, pwm_without_abm;

	ret = backlight_read_max_brightness(&max_brightness);
	igt_assert_eq(ret, 0);

	set_abm_level(data, 0);
	backlight_write_brightness(max_brightness);
	usleep(100000);
	prev_pwm = read_target_backlight_pwm(data->debugfs);
	pwm_without_abm = prev_pwm;

	for (i = 1; i < 5; i++) {
		set_abm_level(data, i);
		usleep(100000);
		pwm = read_target_backlight_pwm(data->debugfs);
		igt_assert(pwm <= prev_pwm);
		igt_assert(pwm < pwm_without_abm);
		prev_pwm = pwm;
	}

}

static void abm_gradual(data_t *data)
{
	int ret, i;
	int convergence_delay = 15;
	int prev_pwm, pwm, curr;
	int max_brightness;

	ret = backlight_read_max_brightness(&max_brightness);

	igt_assert_eq(ret, 0);

	set_abm_level(data, 0);
	backlight_write_brightness(max_brightness);

	sleep(convergence_delay);
	prev_pwm = read_target_backlight_pwm(data->debugfs);
	curr = read_current_backlight_pwm(data->debugfs);

	igt_assert_eq(prev_pwm, curr);
	set_abm_level(data, 4);
	for (i = 0; i < 10; i++) {
		usleep(100000);
		pwm = read_current_backlight_pwm(data->debugfs);
		igt_assert(pwm < prev_pwm);
		prev_pwm = pwm;
	}

	sleep(convergence_delay - 1);

	prev_pwm = read_target_backlight_pwm(data->debugfs);
	curr = read_current_backlight_pwm(data->debugfs);

	igt_assert_eq(prev_pwm, curr);
}

igt_main
{
	data_t data = { 0 };
	enum pipe pipe;
	igt_output_t *output;

	igt_skip_on_simulation();

	igt_fixture {
		data.drm_fd = drm_open_driver_master(DRIVER_AMDGPU);

		if (data.drm_fd == -1)
			igt_skip("Not an amdgpu driver.\n");

		data.debugfs = igt_debugfs_dir(data.drm_fd);

		kmstest_set_vt_graphics_mode();

		igt_display_require(&data.display, data.drm_fd);

		test_init(&data);

		for_each_pipe_with_valid_output(&data.display, pipe, output) {
			if (output->config.connector->connector_type == DRM_MODE_CONNECTOR_eDP)
				break;
		}
	}

	igt_subtest("dpms_cycle")
		backlight_dpms_cycle(&data, output);
	igt_subtest("backlight_monotonic_basic")
		backlight_monotonic_basic(&data);
	igt_subtest("backlight_monotonic_abm")
		backlight_monotonic_abm(&data);
	igt_subtest("abm_enabled")
		abm_enabled(&data);
	igt_subtest("abm_gradual")
		abm_gradual(&data);

	igt_fixture {
		igt_display_fini(&data.display);
	}
}
