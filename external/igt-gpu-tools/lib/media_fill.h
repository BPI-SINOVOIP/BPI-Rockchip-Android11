/*
 * Copyright © 2018 Intel Corporation
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

#ifndef RENDE_MEDIA_FILL_H
#define RENDE_MEDIA_FILL_H

#include <stdint.h>
#include "intel_batchbuffer.h"

void
gen8_media_fillfunc(struct intel_batchbuffer *batch,
		    const struct igt_buf *dst,
		    unsigned int x, unsigned int y,
		    unsigned int width, unsigned int height,
		    uint8_t color);

void
gen7_media_fillfunc(struct intel_batchbuffer *batch,
		    const struct igt_buf *dst,
		    unsigned int x, unsigned int y,
		    unsigned int width, unsigned int height,
		    uint8_t color);

void
gen9_media_fillfunc(struct intel_batchbuffer *batch,
		    const struct igt_buf *dst,
		    unsigned int x, unsigned int y,
		    unsigned int width, unsigned int height,
		    uint8_t color);

void
gen11_media_vme_func(struct intel_batchbuffer *batch,
		     const struct igt_buf *src,
		     unsigned int width, unsigned int height,
		     const struct igt_buf *dst);

#endif /* RENDE_MEDIA_FILL_H */
