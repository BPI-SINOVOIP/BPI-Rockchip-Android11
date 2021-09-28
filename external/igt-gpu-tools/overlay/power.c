/*
 * Copyright © 2013 Intel Corporation
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
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <locale.h>
#include <math.h>

#include "igt_perf.h"

#include "power.h"
#include "debugfs.h"

static int
filename_to_buf(const char *filename, char *buf, unsigned int bufsize)
{
	int fd;
	ssize_t ret;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return -1;

	ret = read(fd, buf, bufsize - 1);
	close(fd);
	if (ret < 1)
		return -1;

	buf[ret] = '\0';

	return 0;
}

static uint64_t filename_to_u64(const char *filename, int base)
{
	char buf[64], *b;

	if (filename_to_buf(filename, buf, sizeof(buf)))
		return 0;

	/*
	 * Handle both single integer and key=value formats by skipping
	 * leading non-digits.
	 */
	b = buf;
	while (*b && !isdigit(*b))
		b++;

	return strtoull(b, NULL, base);
}

static uint64_t debugfs_file_to_u64(const char *name)
{
	char buf[1024];

	snprintf(buf, sizeof(buf), "%s/%s", debugfs_dri_path, name);

	return filename_to_u64(buf, 0);
}

static uint64_t rapl_type_id(void)
{
	return filename_to_u64("/sys/devices/power/type", 10);
}

static uint64_t rapl_gpu_power(void)
{
	return filename_to_u64("/sys/devices/power/events/energy-gpu", 0);
}

static double filename_to_double(const char *filename)
{
	char *oldlocale;
	char buf[80];
	double v;

	if (filename_to_buf(filename, buf, sizeof(buf)))
		return 0;

	oldlocale = setlocale(LC_ALL, "C");
	v = strtod(buf, NULL);
	setlocale(LC_ALL, oldlocale);

	return v;
}

static double rapl_gpu_power_scale(void)
{
	return filename_to_double("/sys/devices/power/events/energy-gpu.scale");
}

int power_init(struct power *power)
{
	uint64_t val;

	memset(power, 0, sizeof(*power));

	power->fd = igt_perf_open(rapl_type_id(), rapl_gpu_power());
	if (power->fd >= 0) {
		power->rapl_scale = rapl_gpu_power_scale();

		if (power->rapl_scale != NAN) {
			power->rapl_scale *= 1e3; /* from nano to micro */
			return 0;
		}
	}

	val = debugfs_file_to_u64("i915_energy_uJ");
	if (val == 0)
		return power->error = EINVAL;

	return 0;
}

static uint64_t clock_ms_to_u64(void)
{
	struct timespec tv;

	if (clock_gettime(CLOCK_MONOTONIC, &tv) < 0)
		return 0;

	return (uint64_t)tv.tv_sec * 1e3 + tv.tv_nsec / 1e6;
}

int power_update(struct power *power)
{
	struct power_stat *s = &power->stat[power->count++ & 1];
	struct power_stat *d = &power->stat[power->count & 1];
	uint64_t d_time;

	if (power->error)
		return power->error;

	if (power->fd >= 0) {
		uint64_t data[2];
		int len;

		len = read(power->fd, data, sizeof(data));
		if (len != sizeof(data))
			return power->error = errno;

		s->energy = llround((double)data[0] * power->rapl_scale);
		s->timestamp = data[1] / 1e6;
	} else {
		s->energy = debugfs_file_to_u64("i915_energy_uJ") / 1e3;
		s->timestamp = clock_ms_to_u64();
	}

	if (power->count == 1)
		return EAGAIN;

	d_time = s->timestamp - d->timestamp;
	power->power_mW = round((double)(s->energy - d->energy) *
				(1e3f / d_time));
	power->new_sample = 1;

	return 0;
}
