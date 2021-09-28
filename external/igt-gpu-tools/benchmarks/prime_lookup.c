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
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>

#include "drm.h"
#include "ioctl_wrappers.h"
#include "drmtest.h"
#include "intel_io.h"
#include "igt_rand.h"

#define CLOSE_DEVICE 0x1

static double elapsed(const struct timespec *start,
		      const struct timespec *end)
{
	return (end->tv_sec - start->tv_sec) + 1e-9*(end->tv_nsec - start->tv_nsec);
}

static int loop(int nobj, int ndev, int nage, int ncpus, unsigned flags)
{
	uint32_t *handle;
	double *results;
	int parent;
	int size;
	int n;

#if 0
	printf("nobj=%d, ndev=%d, nage=%d, ncpus=%d, flags=%x\n",
	       nobj, ndev, nage, ncpus, flags);
#endif

	parent = drm_open_driver(DRIVER_INTEL);

	size = ncpus * sizeof(*results);
	size = (size + 4095) & -4096;
	results = mmap(0, size, PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);

	handle = malloc(nobj * sizeof(*handle));
	for (n = 0; n < nobj; n++)
		handle[n] = gem_create(parent, 4096);

	igt_fork(child, ncpus) {
		struct timespec start, end;
		unsigned long count = 0;
		int *dev, *fd;

		hars_petruska_f54_1_random_perturb(child);

		fd = malloc(ndev * nage * sizeof(*fd));
		dev = malloc(ndev * sizeof(*dev));
		for (n = 0; n < ndev; n++)
			dev[n] = drm_open_driver(DRIVER_INTEL);
		memset(fd, 0xff, ndev * nage * sizeof(*fd));

		clock_gettime(CLOCK_MONOTONIC, &start);
		do {
			for (n = 0; n < ndev; n++) {
				int h = hars_petruska_f54_1_random_unsafe() % nobj;
				int a = hars_petruska_f54_1_random_unsafe() % nage;

				if (!(flags & CLOSE_DEVICE)) {
					int old = fd[n*nage + a];
					if (old != -1) {
						gem_close(dev[n], prime_fd_to_handle(dev[n], old));
						close(old);
					}
				}

				fd[n*nage + a] =
					prime_handle_to_fd(parent, handle[h]);
				prime_fd_to_handle(dev[n], fd[n*nage + a]);

				if (flags & CLOSE_DEVICE) {
					close(dev[n]);
					dev[n] = drm_open_driver(DRIVER_INTEL);
				}
			}

			count++;
			clock_gettime(CLOCK_MONOTONIC, &end);
		} while (elapsed(&start, &end) < 2.);
		results[child] = 1e6*elapsed(&start, &end) / (ndev * count);
	}
	igt_waitchildren();

	results[ncpus] = 0;
	for (n = 0; n < ncpus; n++)
		results[ncpus] +=  results[n];
	printf("%.3f us\n", results[ncpus] / ncpus);
	return 0;
}

static bool allow_files(unsigned min)
{
	struct rlimit rlim;
	unsigned nofile_rlim = 1024*1024;

	FILE *file = fopen("/proc/sys/fs/file-max", "r");
	if (file) {
		igt_assert(fscanf(file, "%u", &nofile_rlim) == 1);
		igt_info("System limit for open files is %u\n", nofile_rlim);
		fclose(file);
	}

	if (min > nofile_rlim)
		return false;

	if (getrlimit(RLIMIT_NOFILE, &rlim))
		return false;

	igt_info("Current file limit is %ld, estimated we need %d\n",
		 (long)rlim.rlim_cur, min);

	if (rlim.rlim_cur > min)
		return true;

	rlim.rlim_cur = min;
	rlim.rlim_max = min;
	return setrlimit(RLIMIT_NOFILE, &rlim) == 0;
}

int main(int argc, char **argv)
{
	unsigned flags = 0;
	int ncpus = 1;
	int ndev = 512;
	int nobj = 32 << 10;
	int nage = 1024;
	int c;

	while ((c = getopt (argc, argv, "a:d:o:cf")) != -1) {
		switch (c) {
		case 'o':
			nobj = atoi(optarg);
			if (nobj < 1)
				nobj = 1;
			break;

		case 'd':
			ndev = atoi(optarg);
			if (ndev < 1)
				ndev = 1;
			break;

		case 'a':
			nage = atoi(optarg);
			if (nage < 1)
				nage = 1;
			break;

		case 'f':
			ncpus = sysconf(_SC_NPROCESSORS_ONLN);
			break;

		case 'c':
			flags |= CLOSE_DEVICE;
			break;

		default:
			break;
		}
	}

	if (!allow_files((nage + 1)*ndev + 1)) {
		fprintf(stderr, "Unable to relax fd limit\n");
		exit(1);
	}

	return loop(nobj, ndev, nage, ncpus, flags);
}
