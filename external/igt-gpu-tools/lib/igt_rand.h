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

#ifndef IGT_RAND_H
#define IGT_RAND_H

#include <stdint.h>

uint32_t hars_petruska_f54_1_random(uint32_t *state);

uint32_t hars_petruska_f54_1_random_seed(uint32_t seed);
uint32_t hars_petruska_f54_1_random_unsafe(void);

static inline void hars_petruska_f54_1_random_perturb(uint32_t xor)
{
	uint32_t seed = hars_petruska_f54_1_random_seed(0) ^ xor;
	hars_petruska_f54_1_random_seed(seed);
	hars_petruska_f54_1_random_seed(hars_petruska_f54_1_random_unsafe());
}

/* Returns: pseudo-random number in interval [0, ep_ro) */
static inline uint32_t hars_petruska_f54_1_random_unsafe_max(uint32_t ep_ro)
{
	return ((uint64_t)hars_petruska_f54_1_random_unsafe() * ep_ro) >> 32;
}

#endif /* IGT_RAND_H */
