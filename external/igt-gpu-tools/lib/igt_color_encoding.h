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
 */

#ifndef __IGT_COLOR_ENCODING_H__
#define __IGT_COLOR_ENCODING_H__

#include <stdbool.h>
#include <stdint.h>

#include "igt_matrix.h"

enum igt_color_encoding {
	IGT_COLOR_YCBCR_BT601,
	IGT_COLOR_YCBCR_BT709,
	IGT_COLOR_YCBCR_BT2020,
	IGT_NUM_COLOR_ENCODINGS,
};

enum igt_color_range {
	IGT_COLOR_YCBCR_LIMITED_RANGE,
	IGT_COLOR_YCBCR_FULL_RANGE,
	IGT_NUM_COLOR_RANGES,
};

const char *igt_color_encoding_to_str(enum igt_color_encoding encoding);
const char *igt_color_range_to_str(enum igt_color_range range);

struct igt_mat4 igt_ycbcr_to_rgb_matrix(uint32_t ycbcr_fourcc,
					uint32_t rgb_fourcc,
					enum igt_color_encoding color_encoding,
					enum igt_color_range color_range);
struct igt_mat4 igt_rgb_to_ycbcr_matrix(uint32_t rgb_fourcc,
					uint32_t ycbcr_fourcc,
					enum igt_color_encoding color_encoding,
					enum igt_color_range color_range);

#endif /* __IGT_COLOR_ENCODING_H__ */
