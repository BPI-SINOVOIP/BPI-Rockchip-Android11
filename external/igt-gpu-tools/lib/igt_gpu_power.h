/*
 * Copyright © 2019 Intel Corporation
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

#ifndef IGT_GPU_POWER_H
#define IGT_GPU_POWER_H

#include <stdbool.h>
#include <stdint.h>

struct gpu_power {
	int fd;
	double scale;
};

struct gpu_power_sample {
	uint64_t energy;
	uint64_t time;
};

int gpu_power_open(struct gpu_power *power);
bool gpu_power_read(struct gpu_power *power, struct gpu_power_sample *s);
void gpu_power_close(struct gpu_power *power);

static inline double gpu_power_J(const struct gpu_power *p,
				 const struct gpu_power_sample *p0,
				 const struct gpu_power_sample *p1)
{
	return (p1->energy - p0->energy) * p->scale;
}

static inline double gpu_power_s(const struct gpu_power *p,
				 const struct gpu_power_sample *p0,
				 const struct gpu_power_sample *p1)
{
	return (p1->time - p0->time) * 1e-9;
}

static inline double gpu_power_W(const struct gpu_power *p,
				 const struct gpu_power_sample *p0,
				 const struct gpu_power_sample *p1)
{
	return gpu_power_J(p, p0, p1) / gpu_power_s(p, p0, p1);
}

#endif /* IGT_GPU_POWER_H */
