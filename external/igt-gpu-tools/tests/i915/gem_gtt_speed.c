/*
 * Copyright © 2010 Intel Corporation
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
 *    Eric Anholt <eric@anholt.net>
 *    Chris Wilson <chris@chris-wilson.co.uk>
 *
 */

#include "igt.h"
#include "igt_x86.h"
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
#include "drm.h"

#define OBJECT_SIZE 16384

static double elapsed(const struct timeval *start,
		      const struct timeval *end,
		      int loop)
{
	return (1e6*(end->tv_sec - start->tv_sec) + (end->tv_usec - start->tv_usec))/loop;
}

#if defined(__x86_64__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC target("sse4.1")
#include <smmintrin.h>
__attribute__((noinline))
static void streaming_load(void *src, int len)
{
	__m128i tmp, *s = src;

	igt_assert((len & 15) == 0);
	igt_assert((((uintptr_t)src) & 15) == 0);

	while (len >= 16) {
		tmp += _mm_stream_load_si128(s++);
		len -= 16;

	}

	*(volatile __m128i *)src = tmp;
}
static inline unsigned x86_64_features(void)
{
	return igt_x86_features();
}
#pragma GCC pop_options
#else
static inline unsigned x86_64_features(void)
{
	return 0;
}
static void streaming_load(void *src, int len)
{
	igt_assert(!"reached");
}
#endif

int size = OBJECT_SIZE;

static int opt_handler(int opt, int opt_index, void *data)
{
	switch (opt) {
	case 's':
		size = atoi(optarg);
		break;
	default:
		return IGT_OPT_HANDLER_ERROR;
	}

	return IGT_OPT_HANDLER_SUCCESS;
}

const char *help_str = "  -s\tObject size in bytes\n";

igt_simple_main_args("s:", NULL, help_str, opt_handler, NULL)
{
	struct timeval start, end;
	uint8_t *buf;
	uint32_t handle;
	unsigned cpu = x86_64_features();
	int loop, i, tiling;
	int fd;

	igt_skip_on_simulation();

	igt_assert_f(size != 0, "Invalid object size specified\n");

	if (cpu) {
		char str[1024];
		igt_info("Detected cpu faatures: %s\n",
			 igt_x86_features_to_string(cpu, str));
	}

	buf = malloc(size);
	memset(buf, 0, size);
	fd = drm_open_driver(DRIVER_INTEL);

	handle = gem_create(fd, size);
	igt_assert(handle);

	for (tiling = I915_TILING_NONE; tiling <= I915_TILING_Y; tiling++) {
		if (tiling != I915_TILING_NONE) {
			igt_info("\nSetting tiling mode to %s\n",
				 tiling == I915_TILING_X ? "X" : "Y");
			gem_set_tiling(fd, handle, tiling, 512);
		}

		if (tiling == I915_TILING_NONE) {
			gem_set_domain(fd, handle,
				       I915_GEM_DOMAIN_CPU,
				       I915_GEM_DOMAIN_CPU);

			{
				uint32_t *base = gem_mmap__cpu(fd, handle, 0, size, PROT_READ | PROT_WRITE);
				volatile uint32_t *ptr = base;
				int x = 0;

				for (i = 0; i < size/sizeof(*ptr); i++)
					x += ptr[i];

				/* force overly clever gcc to actually compute x */
				ptr[0] = x;

				munmap(base, size);

				/* mmap read */
				gettimeofday(&start, NULL);
				for (loop = 0; loop < 1000; loop++) {
					base = gem_mmap__cpu(fd, handle, 0,
							     size,
							     PROT_READ | PROT_WRITE);
					ptr = base;
					x = 0;

					for (i = 0; i < size/sizeof(*ptr); i++)
						x += ptr[i];

					/* force overly clever gcc to actually compute x */
					ptr[0] = x;

					munmap(base, size);
				}
				gettimeofday(&end, NULL);
				igt_info("Time to read %dk through a CPU map:		%7.3fµs\n",
					 size/1024, elapsed(&start, &end, loop));
				{
					base = gem_mmap__cpu(fd, handle, 0,
							     size,
							     PROT_READ | PROT_WRITE);
					gettimeofday(&start, NULL);
					for (loop = 0; loop < 1000; loop++) {
						ptr = base;
						x = 0;

						for (i = 0; i < size/sizeof(*ptr); i++)
							x += ptr[i];

						/* force overly clever gcc to actually compute x */
						ptr[0] = x;

					}
					gettimeofday(&end, NULL);
					munmap(base, size);
					igt_info("Time to read %dk through a cached CPU map:	%7.3fµs\n",
						 size/1024, elapsed(&start, &end, loop));
				}

				/* mmap write */
				gettimeofday(&start, NULL);
				for (loop = 0; loop < 1000; loop++) {
					base = gem_mmap__cpu(fd, handle, 0,
							     size,
							     PROT_READ | PROT_WRITE);
					ptr = base;

					for (i = 0; i < size/sizeof(*ptr); i++)
						ptr[i] = i;

					munmap(base, size);
				}
				gettimeofday(&end, NULL);
				igt_info("Time to write %dk through a CPU map:		%7.3fµs\n",
					 size/1024, elapsed(&start, &end, loop));

				gettimeofday(&start, NULL);
				for (loop = 0; loop < 1000; loop++) {
					base = gem_mmap__cpu(fd, handle, 0,
							     size,
							     PROT_READ | PROT_WRITE);
					memset(base, 0, size);
					munmap(base, size);
				}
				gettimeofday(&end, NULL);
				igt_info("Time to clear %dk through a CPU map:		%7.3fµs\n",
					 size/1024, elapsed(&start, &end, loop));

				gettimeofday(&start, NULL);
				base = gem_mmap__cpu(fd, handle, 0, size,
						     PROT_READ | PROT_WRITE);
				for (loop = 0; loop < 1000; loop++)
					memset(base, 0, size);
				munmap(base, size);
				gettimeofday(&end, NULL);
				igt_info("Time to clear %dk through a cached CPU map:	%7.3fµs\n",
					 size/1024, elapsed(&start, &end, loop));
			}

			/* CPU pwrite */
			gettimeofday(&start, NULL);
			for (loop = 0; loop < 1000; loop++)
				gem_write(fd, handle, 0, buf, size);
			gettimeofday(&end, NULL);
			igt_info("Time to pwrite %dk through the CPU:		%7.3fµs\n",
				 size/1024, elapsed(&start, &end, loop));

			/* CPU pread */
			gettimeofday(&start, NULL);
			for (loop = 0; loop < 1000; loop++)
				gem_read(fd, handle, 0, buf, size);
			gettimeofday(&end, NULL);
			igt_info("Time to pread %dk through the CPU:		%7.3fµs\n",
				 size/1024, elapsed(&start, &end, loop));
		}

		/* prefault into gtt */
		{
			uint32_t *base = gem_mmap__gtt(fd, handle, size, PROT_READ | PROT_WRITE);
			volatile uint32_t *ptr = base;
			int x = 0;

			for (i = 0; i < size/sizeof(*ptr); i++)
				x += ptr[i];

			/* force overly clever gcc to actually compute x */
			ptr[0] = x;

			munmap(base, size);
		}
		/* mmap read */
		gettimeofday(&start, NULL);
		for (loop = 0; loop < 1000; loop++) {
			uint32_t *base = gem_mmap__gtt(fd, handle, size, PROT_READ | PROT_WRITE);
			volatile uint32_t *ptr = base;
			int x = 0;

			for (i = 0; i < size/sizeof(*ptr); i++)
				x += ptr[i];

			/* force overly clever gcc to actually compute x */
			ptr[0] = x;

			munmap(base, size);
		}
		gettimeofday(&end, NULL);
		igt_info("Time to read %dk through a GTT map:		%7.3fµs\n",
			 size/1024, elapsed(&start, &end, loop));

		if (gem_mmap__has_wc(fd)) {
			gettimeofday(&start, NULL);
			for (loop = 0; loop < 1000; loop++) {
				uint32_t *base = gem_mmap__wc(fd, handle, 0, size, PROT_READ | PROT_WRITE);
				volatile uint32_t *ptr = base;
				int x = 0;

				for (i = 0; i < size/sizeof(*ptr); i++)
					x += ptr[i];

				/* force overly clever gcc to actually compute x */
				ptr[0] = x;

				munmap(base, size);
			}
			gettimeofday(&end, NULL);
			igt_info("Time to read %dk through a WC map:		%7.3fµs\n",
					size/1024, elapsed(&start, &end, loop));

			{
				uint32_t *base = gem_mmap__wc(fd, handle, 0, size, PROT_READ | PROT_WRITE);
				gettimeofday(&start, NULL);
				for (loop = 0; loop < 1000; loop++) {
					volatile uint32_t *ptr = base;
					int x = 0;

					for (i = 0; i < size/sizeof(*ptr); i++)
						x += ptr[i];

					/* force overly clever gcc to actually compute x */
					ptr[0] = x;

				}
				gettimeofday(&end, NULL);
				munmap(base, size);
			}
			igt_info("Time to read %dk through a cached WC map:	%7.3fµs\n",
				 size/1024, elapsed(&start, &end, loop));

			/* Check streaming loads from WC */
			if (cpu & SSE4_1) {
				gettimeofday(&start, NULL);
				for (loop = 0; loop < 1000; loop++) {
					uint32_t *base = gem_mmap__wc(fd, handle, 0, size, PROT_READ | PROT_WRITE);
					streaming_load(base, size);

					munmap(base, size);
				}
				gettimeofday(&end, NULL);
				igt_info("Time to stream %dk from a WC map:		%7.3fµs\n",
					 size/1024, elapsed(&start, &end, loop));

				{
					uint32_t *base = gem_mmap__wc(fd, handle, 0, size, PROT_READ | PROT_WRITE);
					gettimeofday(&start, NULL);
					for (loop = 0; loop < 1000; loop++)
						streaming_load(base, size);
					gettimeofday(&end, NULL);
					munmap(base, size);
				}
				igt_info("Time to stream %dk from a cached WC map:	%7.3fµs\n",
					 size/1024, elapsed(&start, &end, loop));
			}
		}


		/* mmap write */
		gettimeofday(&start, NULL);
		for (loop = 0; loop < 1000; loop++) {
			uint32_t *base = gem_mmap__gtt(fd, handle, size, PROT_READ | PROT_WRITE);
			volatile uint32_t *ptr = base;

			for (i = 0; i < size/sizeof(*ptr); i++)
				ptr[i] = i;

			munmap(base, size);
		}
		gettimeofday(&end, NULL);
		igt_info("Time to write %dk through a GTT map:		%7.3fµs\n",
			 size/1024, elapsed(&start, &end, loop));

		if (gem_mmap__has_wc(fd)) {
			/* mmap write */
			gettimeofday(&start, NULL);
			for (loop = 0; loop < 1000; loop++) {
				uint32_t *base = gem_mmap__wc(fd, handle, 0, size, PROT_READ | PROT_WRITE);
				volatile uint32_t *ptr = base;

				for (i = 0; i < size/sizeof(*ptr); i++)
					ptr[i] = i;

				munmap(base, size);
			}
			gettimeofday(&end, NULL);
			igt_info("Time to write %dk through a WC map:		%7.3fµs\n",
					size/1024, elapsed(&start, &end, loop));
		}

		/* mmap clear */
		gettimeofday(&start, NULL);
		for (loop = 0; loop < 1000; loop++) {
			uint32_t *base = gem_mmap__gtt(fd, handle, size, PROT_READ | PROT_WRITE);
			memset(base, 0, size);
			munmap(base, size);
		}
		gettimeofday(&end, NULL);
		igt_info("Time to clear %dk through a GTT map:		%7.3fµs\n",
			 size/1024, elapsed(&start, &end, loop));

		if (gem_mmap__has_wc(fd)) {
			/* mmap clear */
			gettimeofday(&start, NULL);
			for (loop = 0; loop < 1000; loop++) {
				uint32_t *base = gem_mmap__wc(fd, handle, 0, size, PROT_READ | PROT_WRITE);
				memset(base, 0, size);
				munmap(base, size);
			}
			gettimeofday(&end, NULL);
			igt_info("Time to clear %dk through a WC map:		%7.3fµs\n",
					size/1024, elapsed(&start, &end, loop));
		}

		gettimeofday(&start, NULL);{
			uint32_t *base = gem_mmap__gtt(fd, handle, size, PROT_READ | PROT_WRITE);
			for (loop = 0; loop < 1000; loop++)
				memset(base, 0, size);
			munmap(base, size);
		} gettimeofday(&end, NULL);
		igt_info("Time to clear %dk through a cached GTT map:	%7.3fµs\n",
			 size/1024, elapsed(&start, &end, loop));

		if (gem_mmap__has_wc(fd)) {
			gettimeofday(&start, NULL);{
				uint32_t *base = gem_mmap__wc(fd, handle, 0, size, PROT_READ | PROT_WRITE);
				for (loop = 0; loop < 1000; loop++)
					memset(base, 0, size);
				munmap(base, size);
			} gettimeofday(&end, NULL);
			igt_info("Time to clear %dk through a cached WC map:	%7.3fµs\n",
					size/1024, elapsed(&start, &end, loop));
		}

		/* mmap read */
		gettimeofday(&start, NULL);
		for (loop = 0; loop < 1000; loop++) {
			uint32_t *base = gem_mmap__gtt(fd, handle, size, PROT_READ | PROT_WRITE);
			volatile uint32_t *ptr = base;
			int x = 0;

			for (i = 0; i < size/sizeof(*ptr); i++)
				x += ptr[i];

			/* force overly clever gcc to actually compute x */
			ptr[0] = x;

			munmap(base, size);
		}
		gettimeofday(&end, NULL);
		igt_info("Time to read %dk (again) through a GTT map:	%7.3fµs\n",
			 size/1024, elapsed(&start, &end, loop));

		if (tiling == I915_TILING_NONE) {
			/* GTT pwrite */
			gettimeofday(&start, NULL);
			for (loop = 0; loop < 1000; loop++)
				gem_write(fd, handle, 0, buf, size);
			gettimeofday(&end, NULL);
			igt_info("Time to pwrite %dk through the GTT:		%7.3fµs\n",
				 size/1024, elapsed(&start, &end, loop));

			/* GTT pread */
			gettimeofday(&start, NULL);
			for (loop = 0; loop < 1000; loop++)
				gem_read(fd, handle, 0, buf, size);
			gettimeofday(&end, NULL);
			igt_info("Time to pread %dk through the GTT:		%7.3fµs\n",
				 size/1024, elapsed(&start, &end, loop));

			/* GTT pwrite, including clflush */
			gettimeofday(&start, NULL);
			for (loop = 0; loop < 1000; loop++) {
				gem_write(fd, handle, 0, buf, size);
				gem_sync(fd, handle);
			}
			gettimeofday(&end, NULL);
			igt_info("Time to pwrite %dk through the GTT (clflush):	%7.3fµs\n",
				 size/1024, elapsed(&start, &end, loop));

			/* GTT pread, including clflush */
			gettimeofday(&start, NULL);
			for (loop = 0; loop < 1000; loop++) {
				gem_sync(fd, handle);
				gem_read(fd, handle, 0, buf, size);
			}
			gettimeofday(&end, NULL);
			igt_info("Time to pread %dk through the GTT (clflush):	%7.3fµs\n",
				 size/1024, elapsed(&start, &end, loop));

			/* partial writes */
			igt_info("Now partial writes.\n");
			size /= 4;

			/* partial GTT pwrite, including clflush */
			gettimeofday(&start, NULL);
			for (loop = 0; loop < 1000; loop++) {
				gem_write(fd, handle, 0, buf, size);
				gem_sync(fd, handle);
			}
			gettimeofday(&end, NULL);
			igt_info("Time to pwrite %dk through the GTT (clflush):	%7.3fµs\n",
			       size/1024, elapsed(&start, &end, loop));

			/* partial GTT pread, including clflush */
			gettimeofday(&start, NULL);
			for (loop = 0; loop < 1000; loop++) {
				gem_sync(fd, handle);
				gem_read(fd, handle, 0, buf, size);
			}
			gettimeofday(&end, NULL);
			igt_info("Time to pread %dk through the GTT (clflush):	%7.3fµs\n",
			       size/1024, elapsed(&start, &end, loop));

			size *= 4;
		}
	}

	gem_close(fd, handle);
	close(fd);
}
