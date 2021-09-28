/*
 * Copyright 2019 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "igt.h"

/* (De)gamma LUT. */
typedef struct lut {
	struct drm_color_lut *data;
	uint32_t size;
} lut_t;

/* RGB color. */
typedef struct color {
	double r;
	double g;
	double b;
} color_t;

/* Common test data. */
typedef struct data {
	igt_display_t display;
	igt_plane_t *primary;
	igt_output_t *output;
	igt_pipe_t *pipe;
	igt_pipe_crc_t *pipe_crc;
	drmModeModeInfo *mode;
	enum pipe pipe_id;
	int fd;
	int w;
	int h;
	uint32_t regamma_lut_size;
	uint32_t degamma_lut_size;
} data_t;

static void lut_init(lut_t *lut, uint32_t size)
{
	igt_assert(size > 0);

	lut->size = size;
	lut->data = malloc(size * sizeof(struct drm_color_lut));
	igt_assert(lut);
}

/* Generates the linear gamma LUT. */
static void lut_gen_linear(lut_t *lut, uint16_t mask)
{
	uint32_t n = lut->size - 1;
	uint32_t i;

	for (i = 0; i < lut->size; ++i) {
		uint32_t v = (i * 0xffff / n) & mask;

		lut->data[i].red = v;
		lut->data[i].blue = v;
		lut->data[i].green = v;
	}
}

/* Generates the sRGB degamma LUT. */
static void lut_gen_degamma_srgb(lut_t *lut, uint16_t mask)
{
	double range = lut->size - 1;
	uint32_t i;

	for (i = 0; i < lut->size; ++i) {
		double u, coeff;
		uint32_t v;

		u = i / range;
		coeff = u <= 0.040449936 ? (u / 12.92)
					 : (pow((u + 0.055) / 1.055, 2.4));
		v = (uint32_t)(coeff * 0xffff) & mask;

		lut->data[i].red = v;
		lut->data[i].blue = v;
		lut->data[i].green = v;
	}
}

/* Generates the sRGB gamma LUT. */
static void lut_gen_regamma_srgb(lut_t *lut, uint16_t mask)
{
	double range = lut->size - 1;
	uint32_t i;

	for (i = 0; i < lut->size; ++i) {
		double u, coeff;
		uint32_t v;

		u = i / range;
		coeff = u <= 0.00313080 ? (12.92 * u)
					: (1.055 * pow(u, 1.0 / 2.4) - 0.055);
		v = (uint32_t)(coeff * 0xffff) & mask;

		lut->data[i].red = v;
		lut->data[i].blue = v;
		lut->data[i].green = v;
	}
}

static void lut_free(lut_t *lut)
{
	if (lut->data) {
		free(lut->data);
		lut->data = NULL;
	}

	lut->size = 0;
}

/* Fills a FB with the solid color given. */
static void draw_color(igt_fb_t *fb, double r, double g, double b)
{
	cairo_t *cr = igt_get_cairo_ctx(fb->fd, fb);

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	igt_paint_color(cr, 0, 0, fb->width, fb->height, r, g, b);
	igt_put_cairo_ctx(fb->fd, fb, cr);
}

/* Generates the gamma test pattern. */
static void draw_gamma_test(igt_fb_t *fb)
{
	cairo_t *cr = igt_get_cairo_ctx(fb->fd, fb);
	int gh = fb->height / 4;

	igt_paint_color_gradient(cr, 0, 0, fb->width, gh, 1, 1, 1);
	igt_paint_color_gradient(cr, 0, gh, fb->width, gh, 1, 0, 0);
	igt_paint_color_gradient(cr, 0, gh * 2, fb->width, gh, 0, 1, 0);
	igt_paint_color_gradient(cr, 0, gh * 3, fb->width, gh, 0, 0, 1);

	igt_put_cairo_ctx(fb->fd, fb, cr);
}

/* Sets the degamma LUT. */
static void set_degamma_lut(data_t *data, lut_t const *lut)
{
	size_t size = lut ? sizeof(lut->data[0]) * lut->size : 0;
	const void *ptr = lut ? lut->data : NULL;

	igt_pipe_obj_replace_prop_blob(data->pipe, IGT_CRTC_DEGAMMA_LUT, ptr,
				       size);
}

/* Sets the regamma LUT. */
static void set_regamma_lut(data_t *data, lut_t const *lut)
{
	size_t size = lut ? sizeof(lut->data[0]) * lut->size : 0;
	const void *ptr = lut ? lut->data : NULL;

	igt_pipe_obj_replace_prop_blob(data->pipe, IGT_CRTC_GAMMA_LUT, ptr,
				       size);
}

/* Common test setup. */
static void test_init(data_t *data)
{
	igt_display_t *display = &data->display;

	/* It doesn't matter which pipe we choose on amdpgu. */
	data->pipe_id = PIPE_A;
	data->pipe = &data->display.pipes[data->pipe_id];

	igt_display_reset(display);

	data->output = igt_get_single_output_for_pipe(display, data->pipe_id);
	igt_require(data->output);

	data->mode = igt_output_get_mode(data->output);
	igt_assert(data->mode);

	data->primary =
		igt_pipe_get_plane_type(data->pipe, DRM_PLANE_TYPE_PRIMARY);

	data->pipe_crc = igt_pipe_crc_new(data->fd, data->pipe_id,
					  INTEL_PIPE_CRC_SOURCE_AUTO);

	igt_output_set_pipe(data->output, data->pipe_id);

	data->degamma_lut_size =
		igt_pipe_obj_get_prop(data->pipe, IGT_CRTC_DEGAMMA_LUT_SIZE);
	igt_assert_lt(0, data->degamma_lut_size);

	data->regamma_lut_size =
		igt_pipe_obj_get_prop(data->pipe, IGT_CRTC_GAMMA_LUT_SIZE);
	igt_assert_lt(0, data->regamma_lut_size);

	data->w = data->mode->hdisplay;
	data->h = data->mode->vdisplay;
}

/* Common test cleanup. */
static void test_fini(data_t *data)
{
	igt_pipe_crc_free(data->pipe_crc);
	igt_display_reset(&data->display);
}

/*
 * Older versions of amdgpu would put the pipe into bypass mode for degamma
 * when passed a linear sRGB matrix but would still use an sRGB regamma
 * matrix if not passed any. The whole pipe should be in linear bypass mode
 * when all the matrices are NULL - CRCs for a linear degamma matrix and
 * a NULL one should match.
 */
static void test_crtc_linear_degamma(data_t *data)
{
	igt_display_t *display = &data->display;
	igt_crc_t ref_crc, new_crc;
	igt_fb_t afb;
	lut_t lut_linear;

	test_init(data);

	lut_init(&lut_linear, data->degamma_lut_size);
	lut_gen_linear(&lut_linear, 0xffff);

	igt_create_fb(data->fd, data->w, data->h, DRM_FORMAT_XRGB8888, 0, &afb);
	draw_gamma_test(&afb);

	/* Draw the reference image. */
	igt_plane_set_fb(data->primary, &afb);
	set_regamma_lut(data, NULL);
	set_degamma_lut(data, NULL);
	igt_display_commit_atomic(display, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);

	igt_pipe_crc_collect_crc(data->pipe_crc, &ref_crc);

	/* Apply a linear degamma. The result should remain the same. */
	set_degamma_lut(data, &lut_linear);
	igt_display_commit_atomic(display, 0, NULL);

	igt_pipe_crc_collect_crc(data->pipe_crc, &new_crc);
	igt_assert_crc_equal(&ref_crc, &new_crc);

	test_fini(data);
	igt_remove_fb(data->fd, &afb);
	lut_free(&lut_linear);
}

/*
 * Older versions of amdgpu would apply the CRTC regamma on top of a custom
 * sRGB regamma matrix with incorrect calculations or rounding errors.
 * If we put the pipe into bypass or use the hardware defined sRGB regamma
 * on the plane then we can and should get the correct CRTC when passing a
 * liner regamma matrix to DRM.
 */
static void test_crtc_linear_regamma(data_t *data)
{
	igt_display_t *display = &data->display;
	igt_crc_t ref_crc, new_crc;
	igt_fb_t afb;
	lut_t lut_linear;

	test_init(data);

	lut_init(&lut_linear, data->regamma_lut_size);
	lut_gen_linear(&lut_linear, 0xffff);

	igt_create_fb(data->fd, data->w, data->h, DRM_FORMAT_XRGB8888, 0, &afb);
	draw_gamma_test(&afb);

	/* Draw the reference image. */
	igt_plane_set_fb(data->primary, &afb);
	set_regamma_lut(data, NULL);
	set_degamma_lut(data, NULL);
	igt_display_commit_atomic(display, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);

	igt_pipe_crc_collect_crc(data->pipe_crc, &ref_crc);

	/* Apply a linear degamma. The result should remain the same. */
	set_regamma_lut(data, &lut_linear);
	igt_display_commit_atomic(display, 0, NULL);

	igt_pipe_crc_collect_crc(data->pipe_crc, &new_crc);
	igt_assert_crc_equal(&ref_crc, &new_crc);

	test_fini(data);
	igt_remove_fb(data->fd, &afb);
	lut_free(&lut_linear);
}

/*
 * Tests LUT accuracy. CRTC regamma and CRTC degamma should produce a visually
 * correct image when used. Hardware limitations on degamma prevent this from
 * being CRC level accurate across a full test gradient but most values should
 * still match.
 *
 * This test can't pass on DCE because it doesn't support non-linear degamma.
 */
static void test_crtc_lut_accuracy(data_t *data)
{
	/*
	 * Channels are independent, so we can verify multiple colors at the
	 * same time for improved performance.
	 */
	static const color_t colors[] = {
		{ 1.00, 1.00, 1.00 },
		{ 0.90, 0.85, 0.75 }, /* 0.95 fails */
		{ 0.70, 0.65, 0.60 },
		{ 0.55, 0.50, 0.45 },
		{ 0.40, 0.35, 0.30 },
		{ 0.25, 0.20, 0.15 },
		{ 0.10, 0.04, 0.02 }, /* 0.05 fails */
		{ 0.00, 0.00, 0.00 },
	};
	igt_display_t *display = &data->display;
	igt_crc_t ref_crc, new_crc;
	igt_fb_t afb;
	lut_t lut_degamma, lut_regamma;
	int i, w, h;

	test_init(data);

	lut_init(&lut_degamma, data->degamma_lut_size);
	lut_gen_degamma_srgb(&lut_degamma, 0xffff);

	lut_init(&lut_regamma, data->regamma_lut_size);
	lut_gen_regamma_srgb(&lut_regamma, 0xffff);

	/* Don't draw across the whole screen to improve perf. */
	w = 64;
	h = 64;

	igt_create_fb(data->fd, w, h, DRM_FORMAT_XRGB8888, 0, &afb);
	igt_plane_set_fb(data->primary, &afb);
	igt_display_commit_atomic(display, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);

	/* Test colors. */
	for (i = 0; i < ARRAY_SIZE(colors); ++i) {
		color_t col = colors[i];

		igt_info("Testing color (%.2f, %.2f, %.2f) ...\n", col.r, col.g,
			 col.b);

		draw_color(&afb, col.r, col.g, col.b);

		set_regamma_lut(data, NULL);
		set_degamma_lut(data, NULL);
		igt_display_commit_atomic(display, 0, NULL);

		igt_pipe_crc_collect_crc(data->pipe_crc, &ref_crc);

		set_degamma_lut(data, &lut_degamma);
		set_regamma_lut(data, &lut_regamma);
		igt_display_commit_atomic(display, 0, NULL);

		igt_pipe_crc_collect_crc(data->pipe_crc, &new_crc);

		igt_assert_crc_equal(&ref_crc, &new_crc);
	}

	test_fini(data);
	igt_remove_fb(data->fd, &afb);
	lut_free(&lut_regamma);
	lut_free(&lut_degamma);
}

igt_main
{
	data_t data;

	igt_skip_on_simulation();

	memset(&data, 0, sizeof(data));

	igt_fixture
	{
		data.fd = drm_open_driver_master(DRIVER_AMDGPU);

		kmstest_set_vt_graphics_mode();

		igt_display_require(&data.display, data.fd);
		igt_require(data.display.is_atomic);
		igt_display_require_output(&data.display);
	}

	igt_subtest("crtc-linear-degamma") test_crtc_linear_degamma(&data);
	igt_subtest("crtc-linear-regamma") test_crtc_linear_regamma(&data);
	igt_subtest("crtc-lut-accuracy") test_crtc_lut_accuracy(&data);

	igt_fixture
	{
		igt_display_fini(&data.display);
	}
}
