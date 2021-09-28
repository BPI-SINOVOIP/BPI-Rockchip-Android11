/*
 * Copyright © 2011 Intel Corporation
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
 *    Chris Wilson <chris@chris-wilson.co.uk>
 *
 */

#include "igt.h"
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
#include "igt_sysfs.h"

#define OBJECT_SIZE 16384

#define COPY_BLT_CMD		(2<<29|0x53<<22|0x6)
#define BLT_WRITE_ALPHA		(1<<21)
#define BLT_WRITE_RGB		(1<<20)
#define BLT_SRC_TILED		(1<<15)
#define BLT_DST_TILED		(1<<11)

#define LOCAL_I915_EXEC_NO_RELOC (1<<11)
#define LOCAL_I915_EXEC_HANDLE_LUT (1<<12)

static int gem_linear_blt(int fd,
			  uint32_t *batch,
			  uint32_t src,
			  uint32_t dst,
			  uint32_t length,
			  struct drm_i915_gem_relocation_entry *reloc)
{
	uint32_t *b = batch;
	int height = length / (16 * 1024);

	igt_assert_lte(height, 1 << 16);

	if (height) {
		int i = 0;
		b[i++] = COPY_BLT_CMD | BLT_WRITE_ALPHA | BLT_WRITE_RGB;
		if (intel_gen(intel_get_drm_devid(fd)) >= 8)
			b[i-1]+=2;
		b[i++] = 0xcc << 16 | 1 << 25 | 1 << 24 | (16*1024);
		b[i++] = 0;
		b[i++] = height << 16 | (4*1024);
		b[i++] = 0;
		reloc->offset = (b-batch+4) * sizeof(uint32_t);
		reloc->delta = 0;
		reloc->target_handle = dst;
		reloc->read_domains = I915_GEM_DOMAIN_RENDER;
		reloc->write_domain = I915_GEM_DOMAIN_RENDER;
		reloc->presumed_offset = 0;
		reloc++;
		if (intel_gen(intel_get_drm_devid(fd)) >= 8)
			b[i++] = 0; /* FIXME */

		b[i++] = 0;
		b[i++] = 16*1024;
		b[i++] = 0;
		reloc->offset = (b-batch+7) * sizeof(uint32_t);
		if (intel_gen(intel_get_drm_devid(fd)) >= 8)
			reloc->offset += sizeof(uint32_t);
		reloc->delta = 0;
		reloc->target_handle = src;
		reloc->read_domains = I915_GEM_DOMAIN_RENDER;
		reloc->write_domain = 0;
		reloc->presumed_offset = 0;
		reloc++;
		if (intel_gen(intel_get_drm_devid(fd)) >= 8)
			b[i++] = 0; /* FIXME */

		b += i;
		length -= height * 16*1024;
	}

	if (length) {
		int i = 0;
		b[i++] = COPY_BLT_CMD | BLT_WRITE_ALPHA | BLT_WRITE_RGB;
		if (intel_gen(intel_get_drm_devid(fd)) >= 8)
			b[i-1]+=2;
		b[i++] = 0xcc << 16 | 1 << 25 | 1 << 24 | (16*1024);
		b[i++] = height << 16;
		b[i++] = (1+height) << 16 | (length / 4);
		b[i++] = 0;
		reloc->offset = (b-batch+4) * sizeof(uint32_t);
		reloc->delta = 0;
		reloc->target_handle = dst;
		reloc->read_domains = I915_GEM_DOMAIN_RENDER;
		reloc->write_domain = I915_GEM_DOMAIN_RENDER;
		reloc->presumed_offset = 0;
		reloc++;
		if (intel_gen(intel_get_drm_devid(fd)) >= 8)
			b[i++] = 0; /* FIXME */

		b[i++] = height << 16;
		b[i++] = 16*1024;
		b[i++] = 0;
		reloc->offset = (b-batch+7) * sizeof(uint32_t);
		if (intel_gen(intel_get_drm_devid(fd)) >= 8)
			reloc->offset += sizeof(uint32_t);
		reloc->delta = 0;
		reloc->target_handle = src;
		reloc->read_domains = I915_GEM_DOMAIN_RENDER;
		reloc->write_domain = 0;
		reloc->presumed_offset = 0;
		reloc++;
		if (intel_gen(intel_get_drm_devid(fd)) >= 8)
			b[i++] = 0; /* FIXME */

		b += i;
	}

	b[0] = MI_BATCH_BUFFER_END;
	b[1] = 0;

	return (b+2 - batch) * sizeof(uint32_t);
}

static double elapsed(const struct timeval *start,
		      const struct timeval *end,
		      int loop)
{
	return (1e6*(end->tv_sec - start->tv_sec) + (end->tv_usec - start->tv_usec))/loop;
}

static const char *bytes_per_sec(char *buf, double v)
{
	const char *order[] = {
		"",
		"KiB",
		"MiB",
		"GiB",
		"TiB",
		"PiB",
		NULL,
	}, **o = order;

	while (v > 1024 && o[1]) {
		v /= 1024;
		o++;
	}
	sprintf(buf, "%.1f%s/s", v, *o);
	return buf;
}

static int dcmp(const void *A, const void *B)
{
	const double *a = A, *b = B;
	if (*a < *b)
		return -1;
	else if (*a > *b)
		return 1;
	else
		return 0;
}

static void run(int fd, int object_size, bool dumb)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 exec[3];
	struct drm_i915_gem_relocation_entry reloc[4];
	uint32_t buf[20];
	uint32_t handle, src, dst;
	int len, count;
	int ring;

	if (dumb)
		handle = kmstest_dumb_create(fd, 32, 32, 32, NULL, NULL);
	else
		handle = gem_create(fd, 4096);

	src = gem_create(fd, object_size);
	dst = gem_create(fd, object_size);

	len = gem_linear_blt(fd, buf, 0, 1, object_size, reloc);
	gem_write(fd, handle, 0, buf, len);

	memset(exec, 0, sizeof(exec));
	exec[0].handle = src;
	exec[1].handle = dst;

	exec[2].handle = handle;
	if (intel_gen(intel_get_drm_devid(fd)) >= 8)
		exec[2].relocation_count = len > 56 ? 4 : 2;
	else
		exec[2].relocation_count = len > 40 ? 4 : 2;
	exec[2].relocs_ptr = to_user_pointer(reloc);

	ring = 0;
	if (HAS_BLT_RING(intel_get_drm_devid(fd)))
		ring = I915_EXEC_BLT;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(exec);
	execbuf.buffer_count = 3;
	execbuf.batch_len = len;
	execbuf.flags = ring;
	execbuf.flags |= LOCAL_I915_EXEC_HANDLE_LUT;
	execbuf.flags |= LOCAL_I915_EXEC_NO_RELOC;

	if (__gem_execbuf(fd, &execbuf)) {
		len = gem_linear_blt(fd, buf, src, dst, object_size, reloc);
		igt_assert(len == execbuf.batch_len);
		gem_write(fd, handle, 0, buf, len);
		execbuf.flags = ring;
		gem_execbuf(fd, &execbuf);
	}
	gem_sync(fd, handle);

	for (count = 1; count <= 1<<12; count <<= 1) {
		struct timeval start, end;
		const int reps = 9;
		double t[reps], sum;
		int n;

		for (n = 0; n < reps; n++) {
			gettimeofday(&start, NULL);
			for (int loop = 0; loop < count; loop++)
				gem_execbuf(fd, &execbuf);
			gem_sync(fd, handle);
			gettimeofday(&end, NULL);
			t[n] = elapsed(&start, &end, count);
		}
		qsort(t, n, sizeof(double), dcmp);
		sum = 0;
		for (n = 2; n < reps - 2; n++)
			sum += t[n];
		sum /= reps - 4;
		igt_info("Time to blt %d bytes x %6d:	%7.3fµs, %s\n",
			 object_size, count, sum,
			 bytes_per_sec((char *)buf, object_size/sum*1e6));
		fflush(stdout);
	}
	gem_close(fd, handle);
}

static void set_auto_freq(int sysfs)
{
	int min = igt_sysfs_get_u32(sysfs, "gt_RPn_freq_mhz");
	int max = igt_sysfs_get_u32(sysfs, "gt_RP0_freq_mhz");
	if (max <= min)
		return;

	igt_debug("Setting min to %dMHz, and max to %dMHz\n", min, max);
	igt_sysfs_set_u32(sysfs, "gt_min_freq_mhz", min);
	igt_sysfs_set_u32(sysfs, "gt_max_freq_mhz", max);
}

static void set_min_freq(int sysfs)
{
	int min = igt_sysfs_get_u32(sysfs, "gt_RPn_freq_mhz");
	igt_require(min > 0);
	igt_debug("Setting min/max to %dMHz\n", min);
	igt_require(igt_sysfs_set_u32(sysfs, "gt_min_freq_mhz", min) &&
		    igt_sysfs_set_u32(sysfs, "gt_max_freq_mhz", min));
}

static void set_max_freq(int sysfs)
{
	int max = igt_sysfs_get_u32(sysfs, "gt_RP0_freq_mhz");
	igt_require(max > 0);
	igt_debug("Setting min/max to %dMHz\n", max);
	igt_require(igt_sysfs_set_u32(sysfs, "gt_max_freq_mhz", max) &&
		    igt_sysfs_set_u32(sysfs, "gt_min_freq_mhz", max));
}


igt_main
{
	const struct {
		const char *suffix;
		void (*func)(int);
	} rps[] = {
		{ "", set_auto_freq },
		{ "-min", set_min_freq },
		{ "-max", set_max_freq },
		{ NULL, NULL },
	}, *r;
	int min = -1, max = -1;
	int fd, sysfs;

	igt_skip_on_simulation();

	igt_fixture {
		fd = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(fd);

		sysfs = igt_sysfs_open(fd);
		igt_require(sysfs >= 0);

		min = igt_sysfs_get_u32(sysfs, "gt_min_freq_mhz");
		max = igt_sysfs_get_u32(sysfs, "gt_max_freq_mhz");
	}

	for (r = rps; r->suffix; r++) {
		igt_fixture r->func(sysfs);

		igt_subtest_f("cold%s", r->suffix)
			run(fd, OBJECT_SIZE, false);

		igt_subtest_f("normal%s", r->suffix)
			run(fd, OBJECT_SIZE, false);

		igt_subtest_f("dumb-buf%s", r->suffix)
			run(fd, OBJECT_SIZE, true);
	}

	igt_fixture {
		if (min > 0)
			igt_sysfs_set_u32(sysfs, "gt_min_freq_mhz", min);
		if (max > 0)
			igt_sysfs_set_u32(sysfs, "gt_max_freq_mhz", max);

		close(sysfs);
		close(fd);
	}
}
