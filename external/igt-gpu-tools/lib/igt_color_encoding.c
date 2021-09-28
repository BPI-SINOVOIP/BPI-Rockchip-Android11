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

#include "igt_color_encoding.h"
#include "igt_matrix.h"
#include "igt_core.h"
#include "igt_fb.h"
#include "drmtest.h"

struct color_encoding {
	float kr, kb;
};

static const struct color_encoding color_encodings[IGT_NUM_COLOR_ENCODINGS] = {
	[IGT_COLOR_YCBCR_BT601] = { .kr = .299f, .kb = .114f, },
	[IGT_COLOR_YCBCR_BT709] = { .kr = .2126f, .kb = .0722f, },
	[IGT_COLOR_YCBCR_BT2020] = { .kr = .2627f, .kb = .0593f, },
};

static struct igt_mat4 rgb_to_ycbcr_matrix(const struct color_encoding *e)
{
	float kr = e->kr;
	float kb = e->kb;
	float kg = 1.0f - kr - kb;

	struct igt_mat4 ret = {
		.d[m(0, 0)] = kr,
		.d[m(0, 1)] = kg,
		.d[m(0, 2)] = kb,

		.d[m(1, 0)] = -kr / (1.0f - kb),
		.d[m(1, 1)] = -kg / (1.0f - kb),
		.d[m(1, 2)] = 1.0f,

		.d[m(2, 0)] = 1.0f,
		.d[m(2, 1)] = -kg / (1.0f - kr),
		.d[m(2, 2)] = -kb / (1.0f - kr),

		.d[m(3, 3)] = 1.0f,
	};

	return ret;
}

static struct igt_mat4 ycbcr_to_rgb_matrix(const struct color_encoding *e)
{
	float kr = e->kr;
	float kb = e->kb;
	float kg = 1.0f - kr - kb;

	struct igt_mat4 ret = {
		.d[m(0, 0)] = 1.0f,
		.d[m(0, 1)] = 0.0f,
		.d[m(0, 2)] = 1.0 - kr,

		.d[m(1, 0)] = 1.0f,
		.d[m(1, 1)] = -(1.0 - kb) * kb / kg,
		.d[m(1, 2)] = -(1.0 - kr) * kr / kg,

		.d[m(2, 0)] = 1.0f,
		.d[m(2, 1)] = 1.0 - kb,
		.d[m(2, 2)] = 0.0f,

		.d[m(3, 3)] = 1.0f,
	};

	return ret;
}

static struct igt_mat4 ycbcr_input_convert_matrix(enum igt_color_range color_range, float scale,
						  float ofs_y, float max_y,
						  float ofs_cbcr, float mid_cbcr, float max_cbcr,
						  float max_val)
{
	struct igt_mat4 t, s;

	if (color_range == IGT_COLOR_YCBCR_FULL_RANGE) {
		t = igt_matrix_translate(0.f, -mid_cbcr, -mid_cbcr);
		s = igt_matrix_scale(scale, 2.0f * scale, 2.0f * scale);
	} else {
		t = igt_matrix_translate(-ofs_y, -mid_cbcr, -mid_cbcr);
		s = igt_matrix_scale(scale * max_val / (max_y - ofs_y),
				     scale * max_val / (max_cbcr - mid_cbcr),
				     scale * max_val / (max_cbcr - mid_cbcr));
	}

	return igt_matrix_multiply(&s, &t);
}

static struct igt_mat4 ycbcr_output_convert_matrix(enum igt_color_range color_range, float scale,
						   float ofs_y, float max_y,
						   float ofs_cbcr, float mid_cbcr, float max_cbcr,
						   float max_val)
{
	struct igt_mat4 s, t;

	if (color_range == IGT_COLOR_YCBCR_FULL_RANGE) {
		s = igt_matrix_scale(scale, 0.5f * scale, 0.5f * scale);
		t = igt_matrix_translate(0.f, mid_cbcr, mid_cbcr);
	} else {
		s = igt_matrix_scale(scale * (max_y - ofs_y) / max_val,
				     scale * (max_cbcr - mid_cbcr) / max_val,
				     scale * (max_cbcr - mid_cbcr) / max_val);
		t = igt_matrix_translate(ofs_y, mid_cbcr, mid_cbcr);
	}

	return igt_matrix_multiply(&t, &s);
}

static const struct color_encoding_format {
	uint32_t fourcc;

	float max_val;

	float ofs_y, max_y, ofs_cbcr, mid_cbcr, max_cbcr;
} formats[] = {
	{ DRM_FORMAT_XRGB8888, 255.f, },
	{ IGT_FORMAT_FLOAT, 1.f, },
	{ DRM_FORMAT_NV12, 255.f, 16.f, 235.f, 16.f, 128.f, 240.f },
	{ DRM_FORMAT_NV16, 255.f, 16.f, 235.f, 16.f, 128.f, 240.f },
	{ DRM_FORMAT_NV21, 255.f, 16.f, 235.f, 16.f, 128.f, 240.f },
	{ DRM_FORMAT_NV61, 255.f, 16.f, 235.f, 16.f, 128.f, 240.f },
	{ DRM_FORMAT_YUV420, 255.f, 16.f, 235.f, 16.f, 128.f, 240.f },
	{ DRM_FORMAT_YUV422, 255.f, 16.f, 235.f, 16.f, 128.f, 240.f },
	{ DRM_FORMAT_YVU420, 255.f, 16.f, 235.f, 16.f, 128.f, 240.f },
	{ DRM_FORMAT_YVU422, 255.f, 16.f, 235.f, 16.f, 128.f, 240.f },
	{ DRM_FORMAT_YUYV, 255.f, 16.f, 235.f, 16.f, 128.f, 240.f },
	{ DRM_FORMAT_YVYU, 255.f, 16.f, 235.f, 16.f, 128.f, 240.f },
	{ DRM_FORMAT_UYVY, 255.f, 16.f, 235.f, 16.f, 128.f, 240.f },
	{ DRM_FORMAT_VYUY, 255.f, 16.f, 235.f, 16.f, 128.f, 240.f },
	{ DRM_FORMAT_P010, 65472.f, 4096.f, 60160.f, 4096.f, 32768.f, 61440.f },
	{ DRM_FORMAT_P012, 65520.f, 4096.f, 60160.f, 4096.f, 32768.f, 61440.f },
	{ DRM_FORMAT_P016, 65535.f, 4096.f, 60160.f, 4096.f, 32768.f, 61440.f },
	{ DRM_FORMAT_Y210, 65472.f, 4096.f, 60160.f, 4096.f, 32768.f, 61440.f },
	{ DRM_FORMAT_Y212, 65520.f, 4096.f, 60160.f, 4096.f, 32768.f, 61440.f },
	{ DRM_FORMAT_Y216, 65535.f, 4096.f, 60160.f, 4096.f, 32768.f, 61440.f },
	{ DRM_FORMAT_Y410, 1023.f, 64.f, 940.f, 64.f, 512.f, 960.f },
	{ DRM_FORMAT_Y412, 65520.f, 4096.f, 60160.f, 4096.f, 32768.f, 61440.f },
	{ DRM_FORMAT_Y416, 65535.f, 4096.f, 60160.f, 4096.f, 32768.f, 61440.f },
	{ DRM_FORMAT_XVYU2101010, 1023.f, 64.f, 940.f, 64.f, 512.f, 960.f },
	{ DRM_FORMAT_XVYU12_16161616, 65520.f, 4096.f, 60160.f, 4096.f, 32768.f, 61440.f },
	{ DRM_FORMAT_XVYU16161616, 65535.f, 4096.f, 60160.f, 4096.f, 32768.f, 61440.f },
};

static const struct color_encoding_format *lookup_fourcc(uint32_t fourcc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(formats); i++)
		if (fourcc == formats[i].fourcc)
			return &formats[i];

	igt_assert_f(0, "Could not look up fourcc %.4s\n", (char *)&fourcc);
}

struct igt_mat4 igt_ycbcr_to_rgb_matrix(uint32_t from_fourcc,
					uint32_t to_fourcc,
					enum igt_color_encoding color_encoding,
					enum igt_color_range color_range)
{
	const struct color_encoding *e = &color_encodings[color_encoding];
	struct igt_mat4 r, c;
	const struct color_encoding_format *fycbcr = lookup_fourcc(from_fourcc);
	const struct color_encoding_format *frgb = lookup_fourcc(to_fourcc);
	float scale = frgb->max_val / fycbcr->max_val;

	igt_assert(fycbcr->ofs_y && !frgb->ofs_y);

	r = ycbcr_input_convert_matrix(color_range, scale, fycbcr->ofs_y, fycbcr->max_y, fycbcr->ofs_cbcr, fycbcr->mid_cbcr, fycbcr->max_cbcr, fycbcr->max_val);
	c = ycbcr_to_rgb_matrix(e);

	return igt_matrix_multiply(&c, &r);
}

struct igt_mat4 igt_rgb_to_ycbcr_matrix(uint32_t from_fourcc,
					uint32_t to_fourcc,
					enum igt_color_encoding color_encoding,
					enum igt_color_range color_range)
{
	const struct color_encoding *e = &color_encodings[color_encoding];
	const struct color_encoding_format *frgb = lookup_fourcc(from_fourcc);
	const struct color_encoding_format *fycbcr = lookup_fourcc(to_fourcc);
	struct igt_mat4 c, r;
	float scale = fycbcr->max_val / frgb->max_val;

	igt_assert(fycbcr->ofs_y && !frgb->ofs_y);

	c = rgb_to_ycbcr_matrix(e);
	r = ycbcr_output_convert_matrix(color_range, scale, fycbcr->ofs_y, fycbcr->max_y, fycbcr->ofs_cbcr, fycbcr->mid_cbcr, fycbcr->max_cbcr, fycbcr->max_val);

	return igt_matrix_multiply(&r, &c);
}

const char *igt_color_encoding_to_str(enum igt_color_encoding encoding)
{
	switch (encoding) {
	case IGT_COLOR_YCBCR_BT601: return "ITU-R BT.601 YCbCr";
	case IGT_COLOR_YCBCR_BT709: return "ITU-R BT.709 YCbCr";
	case IGT_COLOR_YCBCR_BT2020: return "ITU-R BT.2020 YCbCr";
	default: igt_assert(0); return NULL;
	}
}

const char *igt_color_range_to_str(enum igt_color_range range)
{
	switch (range) {
	case IGT_COLOR_YCBCR_LIMITED_RANGE: return "YCbCr limited range";
	case IGT_COLOR_YCBCR_FULL_RANGE: return "YCbCr full range";
	default: igt_assert(0); return NULL;
	}
}
