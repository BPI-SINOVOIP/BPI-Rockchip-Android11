/*
 * Copyright © 2016 Intel Corporation
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
#include "igt.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

#define THRESHOLD_PER_CONNECTOR	10
#define THRESHOLD_TOTAL		50
#define CHECK_TIMES		15

IGT_TEST_DESCRIPTION("This check the time we take to read the content of all "
		     "the possible connectors. Without the edid -ENXIO patch "
		     "(http://permalink.gmane.org/gmane.comp.video.dri.devel/62083), "
		     "we sometimes take a *really* long time. "
		     "So let's just check for some reasonable timing here");


igt_simple_main
{
	DIR *dirp;
	struct dirent *de;

	dirp = opendir("/sys/class/drm");
	igt_assert(dirp != NULL);

	while ((de = readdir(dirp))) {
		struct igt_mean mean = {};
		struct stat st;
		char path[PATH_MAX];
		int i;

		if (*de->d_name == '.')
			continue;;

		snprintf(path, sizeof(path), "/sys/class/drm/%s/status",
				de->d_name);

		if (stat(path, &st))
			continue;

		igt_mean_init(&mean);
		for (i = 0; i < CHECK_TIMES; i++) {
			struct timespec ts = {};
			int fd;

			if ((fd = open(path, O_WRONLY)) < 0)
				continue;

			igt_nsec_elapsed(&ts);
			igt_ignore_warn(write(fd, "detect\n", 7));
			igt_mean_add(&mean, igt_nsec_elapsed(&ts));

			close(fd);
		}

		igt_debug("%s: mean.max %.2fns, %.2fus, %.2fms, "
			  "mean.avg %.2fns, %.2fus, %.2fms\n",
			  de->d_name,
			  mean.max, mean.max / 1e3, mean.max / 1e6,
			  mean.mean, mean.mean / 1e3, mean.mean / 1e6);

		if (mean.max > (THRESHOLD_PER_CONNECTOR * 1e6)) {
			igt_warn("%s: probe time exceed 10ms, "
				 "max=%.2fms, avg=%.2fms\n", de->d_name,
				 mean.max / 1e6, mean.mean / 1e6);
		}
		igt_assert_f(mean.mean < (THRESHOLD_TOTAL * 1e6),
			     "%s: average probe time exceeded 50ms, "
			     "max=%.2fms, avg=%.2fms\n", de->d_name,
			     mean.max / 1e6, mean.mean / 1e6);

	}
	closedir(dirp);

}
