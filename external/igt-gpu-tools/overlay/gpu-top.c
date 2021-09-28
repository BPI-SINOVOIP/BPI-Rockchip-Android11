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
#include <errno.h>
#include <assert.h>

#include "igt_perf.h"

#include "gpu-top.h"

#define RING_TAIL      0x00
#define RING_HEAD      0x04
#define ADDR_MASK      0x001FFFFC
#define RING_CTL       0x0C
#define   RING_WAIT		(1<<11)
#define   RING_WAIT_SEMAPHORE	(1<<10)

static int perf_init(struct gpu_top *gt)
{
	struct engine_desc {
		unsigned class, inst;
		const char *name;
	} *d, engines[] = {
		{ I915_ENGINE_CLASS_RENDER, 0, "rcs0" },
		{ I915_ENGINE_CLASS_COPY, 0, "bcs0" },
		{ I915_ENGINE_CLASS_VIDEO, 0, "vcs0" },
		{ I915_ENGINE_CLASS_VIDEO, 1, "vcs1" },
		{ I915_ENGINE_CLASS_VIDEO_ENHANCE, 0, "vecs0" },
		{ 0, 0, NULL }
	};

	d = &engines[0];

	gt->fd = perf_i915_open_group(I915_PMU_ENGINE_BUSY(d->class, d->inst),
				      -1);
	if (gt->fd < 0)
		return -1;

	if (perf_i915_open_group(I915_PMU_ENGINE_WAIT(d->class, d->inst),
				 gt->fd) >= 0)
		gt->have_wait = 1;

	if (perf_i915_open_group(I915_PMU_ENGINE_SEMA(d->class, d->inst),
				 gt->fd) >= 0)
		gt->have_sema = 1;

	gt->ring[0].name = d->name;
	gt->num_rings = 1;

	for (d++; d->name; d++) {
		if (perf_i915_open_group(I915_PMU_ENGINE_BUSY(d->class,
							      d->inst),
					gt->fd) < 0)
			continue;

		if (gt->have_wait &&
		    perf_i915_open_group(I915_PMU_ENGINE_WAIT(d->class,
							      d->inst),
					 gt->fd) < 0)
			return -1;

		if (gt->have_sema &&
		    perf_i915_open_group(I915_PMU_ENGINE_SEMA(d->class,
							      d->inst),
				   gt->fd) < 0)
			return -1;

		gt->ring[gt->num_rings++].name = d->name;
	}

	return 0;
}

void gpu_top_init(struct gpu_top *gt)
{
	memset(gt, 0, sizeof(*gt));
	gt->fd = -1;

	perf_init(gt);
}

int gpu_top_update(struct gpu_top *gt)
{
	uint32_t data[1024];
	int update, len;

	if (gt->fd < 0)
		return 0;

	if (gt->type == PERF) {
		struct gpu_top_stat *s = &gt->stat[gt->count++&1];
		struct gpu_top_stat *d = &gt->stat[gt->count&1];
		uint64_t *sample, d_time;
		int n, m;

		len = read(gt->fd, data, sizeof(data));
		if (len < 0)
			return 0;

		sample = (uint64_t *)data + 1;

		s->time = *sample++;
		for (n = m = 0; n < gt->num_rings; n++) {
			s->busy[n] = sample[m++];
			if (gt->have_wait)
				s->wait[n] = sample[m++];
			if (gt->have_sema)
				s->sema[n] = sample[m++];
		}

		if (gt->count == 1)
			return 0;

		d_time = s->time - d->time;
		for (n = 0; n < gt->num_rings; n++) {
			gt->ring[n].u.u.busy = (100 * (s->busy[n] - d->busy[n]) + d_time/2) / d_time;
			if (gt->have_wait)
				gt->ring[n].u.u.wait = (100 * (s->wait[n] - d->wait[n]) + d_time/2) / d_time;
			if (gt->have_sema)
				gt->ring[n].u.u.sema = (100 * (s->sema[n] - d->sema[n]) + d_time/2) / d_time;

			/* in case of rounding + sampling errors, fudge */
			if (gt->ring[n].u.u.busy > 100)
				gt->ring[n].u.u.busy = 100;
			if (gt->ring[n].u.u.wait > 100)
				gt->ring[n].u.u.wait = 100;
			if (gt->ring[n].u.u.sema > 100)
				gt->ring[n].u.u.sema = 100;
		}

		update = 1;
	} else {
		while ((len = read(gt->fd, data, sizeof(data))) > 0) {
			uint32_t *ptr = &data[len/sizeof(uint32_t) - MAX_RINGS];
			gt->ring[0].u.payload = ptr[0];
			gt->ring[1].u.payload = ptr[1];
			gt->ring[2].u.payload = ptr[2];
			gt->ring[3].u.payload = ptr[3];
			update = 1;
		}
	}

	return update;
}
