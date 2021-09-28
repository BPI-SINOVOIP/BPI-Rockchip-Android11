/*
 * Copyright ©2016 Intel Corporation
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
#include <sys/time.h>
#include <time.h>

#include "igt.h"
#include "igt_vgem.h"

static double elapsed(const struct timespec *start,
		      const struct timespec *end)
{
	return (end->tv_sec - start->tv_sec) + 1e-9*(end->tv_nsec - start->tv_nsec);
}

int main(int argc, char **argv)
{
	enum dir {READ, WRITE, CLEAR, FAULT} dir = READ;
	struct timespec start, end;
	struct vgem_bo bo;
	void *buf;
	void *ptr, *src, *dst;
	int reps = 1;
	int loops;
	int vgem;
	int c;

	while ((c = getopt (argc, argv, "d:r:")) != -1) {
		switch (c) {
		case 'd':
			if (strcmp(optarg, "read") == 0)
				dir = READ;
			else if (strcmp(optarg, "write") == 0)
				dir = WRITE;
			else if (strcmp(optarg, "clear") == 0)
				dir = CLEAR;
			else if (strcmp(optarg, "fault") == 0)
				dir = FAULT;
			else
				abort();
			break;

		case 'r':
			reps = atoi(optarg);
			if (reps < 1)
				reps = 1;
			break;

		default:
			break;
		}
	}

	vgem = drm_open_driver(DRIVER_VGEM);

	bo.width = 2024;
	bo.height = 2024;
	bo.bpp = 4;
	vgem_create(vgem, &bo);
	ptr = vgem_mmap(vgem, &bo, PROT_WRITE);
	buf = malloc(bo.size);

	if (dir == READ) {
		src = ptr;
		dst = buf;
	} else {
		src = buf;
		dst = ptr;
	}

	clock_gettime(CLOCK_MONOTONIC, &start);
	switch (dir) {
	case CLEAR:
	case FAULT:
		memset(dst, 0, bo.size);
		break;
	default:
		memcpy(dst, src, bo.size);
		break;
	}
	clock_gettime(CLOCK_MONOTONIC, &end);

	loops = 2 / elapsed(&start, &end);
	while (reps--) {
		clock_gettime(CLOCK_MONOTONIC, &start);
		for (c = 0; c < loops; c++) {
			int page;

			switch (dir) {
			case CLEAR:
				memset(dst, 0, bo.size);
				break;
			case FAULT:
				munmap(ptr, bo.size);
				ptr = vgem_mmap(vgem, &bo, PROT_WRITE);
				for (page = 0; page < bo.size; page += 4096) {
					uint32_t *x = (uint32_t *)ptr + page/4;
					__asm__ __volatile__("": : :"memory");
					page += *x; /* should be zero! */
				}
				break;
			default:
				memcpy(dst, src, bo.size);
				break;
			}
		}
		clock_gettime(CLOCK_MONOTONIC, &end);
		printf("%7.3f\n", bo.size / elapsed(&start, &end) * loops / (1024*1024));
	}

	return 0;
}
