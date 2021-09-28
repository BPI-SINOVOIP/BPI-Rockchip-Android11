/*
 * Copyright © 2015 Intel Corporation
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

#include <math.h>
#include <unistd.h>

#include "drm.h"
#include "drmtest.h"
#include "igt.h"

IGT_TEST_DESCRIPTION("Test Color Features at Pipe level");

/* Internal */
typedef struct {
	double r, g, b;
} color_t;

typedef struct {
	int drm_fd;
	uint32_t devid;
	igt_display_t display;
	igt_pipe_crc_t *pipe_crc;

	uint32_t color_depth;
	uint64_t degamma_lut_size;
	uint64_t gamma_lut_size;
} data_t;

typedef struct {
	int size;
	double coeffs[];
} gamma_lut_t;

static void paint_gradient_rectangles(data_t *data,
				      drmModeModeInfo *mode,
				      color_t *colors,
				      struct igt_fb *fb)
{
	cairo_t *cr = igt_get_cairo_ctx(data->drm_fd, fb);
	int i, l = mode->hdisplay / 3;
	int rows_remaining = mode->hdisplay % 3;

	/* Paint 3 gradient rectangles with red/green/blue between 1.0 and
	 * 0.5. We want to avoid 0 so each max LUTs only affect their own
	 * rectangle.
	 */
	for (i = 0 ; i < 3; i++) {
		igt_paint_color_gradient_range(cr, i * l, 0, l, mode->vdisplay,
					       colors[i].r != 0 ? 0.2 : 0,
					       colors[i].g != 0 ? 0.2 : 0,
					       colors[i].b != 0 ? 0.2 : 0,
					       colors[i].r,
					       colors[i].g,
					       colors[i].b);
	}

	if (rows_remaining > 0)
		igt_paint_color_gradient_range(cr, i * l, 0, rows_remaining,
					       mode->vdisplay,
					       colors[i-1].r != 0 ? 0.2 : 0,
					       colors[i-1].g != 0 ? 0.2 : 0,
					       colors[i-1].b != 0 ? 0.2 : 0,
					       colors[i-1].r,
					       colors[i-1].g,
					       colors[i-1].b);

	igt_put_cairo_ctx(data->drm_fd, fb, cr);
}

static void paint_rectangles(data_t *data,
			     drmModeModeInfo *mode,
			     color_t *colors,
			     struct igt_fb *fb)
{
	cairo_t *cr = igt_get_cairo_ctx(data->drm_fd, fb);
	int i, l = mode->hdisplay / 3;
	int rows_remaining = mode->hdisplay % 3;

	/* Paint 3 solid rectangles. */
	for (i = 0 ; i < 3; i++) {
		igt_paint_color(cr, i * l, 0, l, mode->vdisplay,
				colors[i].r, colors[i].g, colors[i].b);
	}

	if (rows_remaining > 0)
		igt_paint_color(cr, i * l, 0, rows_remaining, mode->vdisplay,
				colors[i-1].r, colors[i-1].g, colors[i-1].b);

	igt_put_cairo_ctx(data->drm_fd, fb, cr);
}

static gamma_lut_t *alloc_lut(int lut_size)
{
	gamma_lut_t *gamma;

	igt_assert_lt(0, lut_size);

	gamma = malloc(sizeof(*gamma) + lut_size * sizeof(gamma->coeffs[0]));
	igt_assert(gamma);
	gamma->size = lut_size;

	return gamma;
}

static void free_lut(gamma_lut_t *gamma)
{
	if (!gamma)
		return;

	free(gamma);
}

static gamma_lut_t *generate_table(int lut_size, double exp)
{
	gamma_lut_t *gamma = alloc_lut(lut_size);
	int i;

	gamma->coeffs[0] = 0.0;
	for (i = 1; i < lut_size; i++)
		gamma->coeffs[i] = pow(i * 1.0 / (lut_size - 1), exp);

	return gamma;
}

static gamma_lut_t *generate_table_max(int lut_size)
{
	gamma_lut_t *gamma = alloc_lut(lut_size);
	int i;

	gamma->coeffs[0] = 0.0;
	for (i = 1; i < lut_size; i++)
		gamma->coeffs[i] = 1.0;

	return gamma;
}

static gamma_lut_t *generate_table_zero(int lut_size)
{
	gamma_lut_t *gamma = alloc_lut(lut_size);
	int i;

	for (i = 0; i < lut_size; i++)
		gamma->coeffs[i] = 0.0;

	return gamma;
}

static struct drm_color_lut *coeffs_to_lut(data_t *data,
					   const gamma_lut_t *gamma,
					   uint32_t color_depth,
					   int off)
{
	struct drm_color_lut *lut;
	int i, lut_size = gamma->size;
	uint32_t max_value = (1 << 16) - 1;
	uint32_t mask;

	if (is_i915_device(data->drm_fd))
		mask = ((1 << color_depth) - 1) << 8;
	else
		mask = max_value;

	lut = malloc(sizeof(struct drm_color_lut) * lut_size);

	if (IS_CHERRYVIEW(data->devid))
		lut_size -= 1;
	for (i = 0; i < lut_size; i++) {
		uint32_t v = (gamma->coeffs[i] * max_value);

		/*
		 * Hardware might encode colors on a different number of bits
		 * than what is in our framebuffer (10 or 12bits for example).
		 * Mask the lower bits not provided by the framebuffer so we
		 * can do CRC comparisons.
		 */
		v &= mask;

		lut[i].red = v;
		lut[i].green = v;
		lut[i].blue = v;
	}

	if (IS_CHERRYVIEW(data->devid))
		lut[lut_size].red =
			lut[lut_size].green =
			lut[lut_size].blue = lut[lut_size - 1].red;

	return lut;
}

static void set_degamma(data_t *data,
			igt_pipe_t *pipe,
			const gamma_lut_t *gamma)
{
	size_t size = sizeof(struct drm_color_lut) * gamma->size;
	struct drm_color_lut *lut = coeffs_to_lut(data, gamma,
						  data->color_depth, 0);

	igt_pipe_obj_replace_prop_blob(pipe, IGT_CRTC_DEGAMMA_LUT, lut, size);

	free(lut);
}

static void set_gamma(data_t *data,
		      igt_pipe_t *pipe,
		      const gamma_lut_t *gamma)
{
	size_t size = sizeof(struct drm_color_lut) * gamma->size;
	struct drm_color_lut *lut = coeffs_to_lut(data, gamma,
						  data->color_depth, 0);

	igt_pipe_obj_replace_prop_blob(pipe, IGT_CRTC_GAMMA_LUT, lut, size);

	free(lut);
}

static void set_ctm(igt_pipe_t *pipe, const double *coefficients)
{
	struct drm_color_ctm ctm;
	int i;

	for (i = 0; i < ARRAY_SIZE(ctm.matrix); i++) {
		if (coefficients[i] < 0) {
			ctm.matrix[i] =
				(int64_t) (-coefficients[i] * ((int64_t) 1L << 32));
			ctm.matrix[i] |= 1ULL << 63;
		} else
			ctm.matrix[i] =
				(int64_t) (coefficients[i] * ((int64_t) 1L << 32));
	}

	igt_pipe_obj_replace_prop_blob(pipe, IGT_CRTC_CTM, &ctm, sizeof(ctm));
}

static void disable_prop(igt_pipe_t *pipe, enum igt_atomic_crtc_properties prop)
{
	if (igt_pipe_obj_has_prop(pipe, prop))
		igt_pipe_obj_replace_prop_blob(pipe, prop, NULL, 0);
}

#define disable_degamma(pipe) disable_prop(pipe, IGT_CRTC_DEGAMMA_LUT)
#define disable_gamma(pipe) disable_prop(pipe, IGT_CRTC_GAMMA_LUT)
#define disable_ctm(pipe) disable_prop(pipe, IGT_CRTC_CTM)

/*
 * Draw 3 gradient rectangles in red, green and blue, with a maxed out
 * degamma LUT and verify we have the same CRC as drawing solid color
 * rectangles with linear degamma LUT.
 */
static void test_pipe_degamma(data_t *data,
			      igt_plane_t *primary)
{
	igt_output_t *output;
	gamma_lut_t *degamma_linear, *degamma_full;
	gamma_lut_t *gamma_linear;
	color_t red_green_blue[] = {
		{ 1.0, 0.0, 0.0 },
		{ 0.0, 1.0, 0.0 },
		{ 0.0, 0.0, 1.0 }
	};

	igt_require(igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_DEGAMMA_LUT));
	igt_require(igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_GAMMA_LUT));

	degamma_linear = generate_table(data->degamma_lut_size, 1.0);
	degamma_full = generate_table_max(data->degamma_lut_size);

	gamma_linear = generate_table(data->gamma_lut_size, 1.0);

	for_each_valid_output_on_pipe(&data->display, primary->pipe->pipe, output) {
		drmModeModeInfo *mode;
		struct igt_fb fb_modeset, fb;
		igt_crc_t crc_fullgamma, crc_fullcolors;
		int fb_id, fb_modeset_id;

		igt_output_set_pipe(output, primary->pipe->pipe);
		mode = igt_output_get_mode(output);

		/* Create a framebuffer at the size of the output. */
		fb_id = igt_create_fb(data->drm_fd,
				      mode->hdisplay,
				      mode->vdisplay,
				      DRM_FORMAT_XRGB8888,
				      LOCAL_DRM_FORMAT_MOD_NONE,
				      &fb);
		igt_assert(fb_id);

		fb_modeset_id = igt_create_fb(data->drm_fd,
					      mode->hdisplay,
					      mode->vdisplay,
					      DRM_FORMAT_XRGB8888,
					      LOCAL_DRM_FORMAT_MOD_NONE,
					      &fb_modeset);
		igt_assert(fb_modeset_id);

		igt_plane_set_fb(primary, &fb_modeset);
		disable_ctm(primary->pipe);
		disable_degamma(primary->pipe);
		set_gamma(data, primary->pipe, gamma_linear);
		igt_display_commit(&data->display);

		/* Draw solid colors with no degamma transformation. */
		paint_rectangles(data, mode, red_green_blue, &fb);
		igt_plane_set_fb(primary, &fb);
		igt_display_commit(&data->display);
		igt_wait_for_vblank(data->drm_fd, primary->pipe->pipe);
		igt_pipe_crc_collect_crc(data->pipe_crc, &crc_fullcolors);

		/* Draw a gradient with degamma LUT to remap all
		 * values to max red/green/blue.
		 */
		paint_gradient_rectangles(data, mode, red_green_blue, &fb);
		igt_plane_set_fb(primary, &fb);
		set_degamma(data, primary->pipe, degamma_full);
		igt_display_commit(&data->display);
		igt_wait_for_vblank(data->drm_fd, primary->pipe->pipe);
		igt_pipe_crc_collect_crc(data->pipe_crc, &crc_fullgamma);

		/* Verify that the CRC of the software computed output is
		 * equal to the CRC of the degamma LUT transformation output.
		 */
		igt_assert_crc_equal(&crc_fullgamma, &crc_fullcolors);

		igt_plane_set_fb(primary, NULL);
		igt_output_set_pipe(output, PIPE_NONE);
	}

	free_lut(degamma_linear);
	free_lut(degamma_full);
	free_lut(gamma_linear);
}

/*
 * Draw 3 gradient rectangles in red, green and blue, with a maxed out gamma
 * LUT and verify we have the same CRC as drawing solid color rectangles.
 */
static void test_pipe_gamma(data_t *data,
			    igt_plane_t *primary)
{
	igt_output_t *output;
	gamma_lut_t *gamma_full;
	color_t red_green_blue[] = {
		{ 1.0, 0.0, 0.0 },
		{ 0.0, 1.0, 0.0 },
		{ 0.0, 0.0, 1.0 }
	};

	igt_require(igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_GAMMA_LUT));

	gamma_full = generate_table_max(data->gamma_lut_size);

	for_each_valid_output_on_pipe(&data->display, primary->pipe->pipe, output) {
		drmModeModeInfo *mode;
		struct igt_fb fb_modeset, fb;
		igt_crc_t crc_fullgamma, crc_fullcolors;
		int fb_id, fb_modeset_id;

		igt_output_set_pipe(output, primary->pipe->pipe);
		mode = igt_output_get_mode(output);

		/* Create a framebuffer at the size of the output. */
		fb_id = igt_create_fb(data->drm_fd,
				      mode->hdisplay,
				      mode->vdisplay,
				      DRM_FORMAT_XRGB8888,
				      LOCAL_DRM_FORMAT_MOD_NONE,
				      &fb);
		igt_assert(fb_id);

		fb_modeset_id = igt_create_fb(data->drm_fd,
					      mode->hdisplay,
					      mode->vdisplay,
					      DRM_FORMAT_XRGB8888,
					      LOCAL_DRM_FORMAT_MOD_NONE,
					      &fb_modeset);
		igt_assert(fb_modeset_id);

		igt_plane_set_fb(primary, &fb_modeset);
		disable_ctm(primary->pipe);
		disable_degamma(primary->pipe);
		set_gamma(data, primary->pipe, gamma_full);
		igt_display_commit(&data->display);

		/* Draw solid colors with no gamma transformation. */
		paint_rectangles(data, mode, red_green_blue, &fb);
		igt_plane_set_fb(primary, &fb);
		igt_display_commit(&data->display);
		igt_wait_for_vblank(data->drm_fd, primary->pipe->pipe);
		igt_pipe_crc_collect_crc(data->pipe_crc, &crc_fullcolors);

		/* Draw a gradient with gamma LUT to remap all values
		 * to max red/green/blue.
		 */
		paint_gradient_rectangles(data, mode, red_green_blue, &fb);
		igt_plane_set_fb(primary, &fb);
		igt_display_commit(&data->display);
		igt_wait_for_vblank(data->drm_fd, primary->pipe->pipe);
		igt_pipe_crc_collect_crc(data->pipe_crc, &crc_fullgamma);

		/* Verify that the CRC of the software computed output is
		 * equal to the CRC of the gamma LUT transformation output.
		 */
		igt_assert_crc_equal(&crc_fullgamma, &crc_fullcolors);

		igt_plane_set_fb(primary, NULL);
		igt_output_set_pipe(output, PIPE_NONE);
	}

	free_lut(gamma_full);
}

/*
 * Draw 3 gradient rectangles in red, green and blue, with a maxed out legacy
 * gamma LUT and verify we have the same CRC as drawing solid color rectangles
 * with linear legacy gamma LUT.
 */
static void test_pipe_legacy_gamma(data_t *data,
				   igt_plane_t *primary)
{
	igt_output_t *output;
	color_t red_green_blue[] = {
		{ 1.0, 0.0, 0.0 },
		{ 0.0, 1.0, 0.0 },
		{ 0.0, 0.0, 1.0 }
	};
	drmModeCrtc *kms_crtc;
	uint32_t i, legacy_lut_size;
	uint16_t *red_lut, *green_lut, *blue_lut;

	kms_crtc = drmModeGetCrtc(data->drm_fd, primary->pipe->crtc_id);
	legacy_lut_size = kms_crtc->gamma_size;
	drmModeFreeCrtc(kms_crtc);

	red_lut = malloc(sizeof(uint16_t) * legacy_lut_size);
	green_lut = malloc(sizeof(uint16_t) * legacy_lut_size);
	blue_lut = malloc(sizeof(uint16_t) * legacy_lut_size);

	for_each_valid_output_on_pipe(&data->display, primary->pipe->pipe, output) {
		drmModeModeInfo *mode;
		struct igt_fb fb_modeset, fb;
		igt_crc_t crc_fullgamma, crc_fullcolors;
		int fb_id, fb_modeset_id;

		igt_output_set_pipe(output, primary->pipe->pipe);
		mode = igt_output_get_mode(output);

		/* Create a framebuffer at the size of the output. */
		fb_id = igt_create_fb(data->drm_fd,
				      mode->hdisplay,
				      mode->vdisplay,
				      DRM_FORMAT_XRGB8888,
				      LOCAL_DRM_FORMAT_MOD_NONE,
				      &fb);
		igt_assert(fb_id);

		fb_modeset_id = igt_create_fb(data->drm_fd,
					      mode->hdisplay,
					      mode->vdisplay,
					      DRM_FORMAT_XRGB8888,
					      LOCAL_DRM_FORMAT_MOD_NONE,
					      &fb_modeset);
		igt_assert(fb_modeset_id);

		igt_plane_set_fb(primary, &fb_modeset);
		disable_degamma(primary->pipe);
		disable_gamma(primary->pipe);
		disable_ctm(primary->pipe);
		igt_display_commit(&data->display);

		/* Draw solid colors with no gamma transformation. */
		paint_rectangles(data, mode, red_green_blue, &fb);
		igt_plane_set_fb(primary, &fb);
		igt_display_commit(&data->display);
		igt_wait_for_vblank(data->drm_fd, primary->pipe->pipe);
		igt_pipe_crc_collect_crc(data->pipe_crc, &crc_fullcolors);

		/* Draw a gradient with gamma LUT to remap all values
		 * to max red/green/blue.
		 */
		paint_gradient_rectangles(data, mode, red_green_blue, &fb);
		igt_plane_set_fb(primary, &fb);

		red_lut[0] = green_lut[0] = blue_lut[0] = 0;
		for (i = 1; i < legacy_lut_size; i++)
			red_lut[i] = green_lut[i] = blue_lut[i] = 0xffff;
		igt_assert_eq(drmModeCrtcSetGamma(data->drm_fd, primary->pipe->crtc_id,
						  legacy_lut_size, red_lut, green_lut, blue_lut), 0);
		igt_display_commit(&data->display);
		igt_wait_for_vblank(data->drm_fd, primary->pipe->pipe);
		igt_pipe_crc_collect_crc(data->pipe_crc, &crc_fullgamma);

		/* Verify that the CRC of the software computed output is
		 * equal to the CRC of the gamma LUT transformation output.
		 */
		igt_assert_crc_equal(&crc_fullgamma, &crc_fullcolors);

		/* Reset output. */
		for (i = 1; i < legacy_lut_size; i++)
			red_lut[i] = green_lut[i] = blue_lut[i] = i << 8;

		igt_assert_eq(drmModeCrtcSetGamma(data->drm_fd, primary->pipe->crtc_id,
						  legacy_lut_size, red_lut, green_lut, blue_lut), 0);
		igt_display_commit(&data->display);

		igt_plane_set_fb(primary, NULL);
		igt_output_set_pipe(output, PIPE_NONE);
	}

	free(red_lut);
	free(green_lut);
	free(blue_lut);
}

static drmModePropertyBlobPtr
get_blob(data_t *data, igt_pipe_t *pipe, enum igt_atomic_crtc_properties prop)
{
	uint64_t prop_value;

	prop_value = igt_pipe_obj_get_prop(pipe, prop);

	if (prop_value == 0)
		return NULL;

	return drmModeGetPropertyBlob(data->drm_fd, prop_value);
}

/*
 * Verify that setting the legacy gamma LUT resets the gamma LUT set
 * through the GAMMA_LUT property.
 */
static void test_pipe_legacy_gamma_reset(data_t *data,
					 igt_plane_t *primary)
{
	const double ctm_identity[] = {
		1.0, 0.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 0.0, 1.0
	};
	drmModeCrtc *kms_crtc;
	gamma_lut_t *degamma_linear = NULL, *gamma_zero;
	uint32_t i, legacy_lut_size;
	uint16_t *red_lut, *green_lut, *blue_lut;
	struct drm_color_lut *lut;
	drmModePropertyBlobPtr blob;
	igt_output_t *output;

	igt_require(igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_GAMMA_LUT));

	if (igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_DEGAMMA_LUT))
		degamma_linear = generate_table(data->degamma_lut_size, 1.0);
	gamma_zero = generate_table_zero(data->gamma_lut_size);

	for_each_valid_output_on_pipe(&data->display, primary->pipe->pipe, output) {
		igt_output_set_pipe(output, primary->pipe->pipe);

		/* Ensure we have a clean state to start with. */
		disable_degamma(primary->pipe);
		disable_ctm(primary->pipe);
		disable_gamma(primary->pipe);
		igt_display_commit(&data->display);

		/* Set a degama & gamma LUT and a CTM using the
		 * properties and verify the content of the
		 * properties. */
		if (igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_DEGAMMA_LUT))
			set_degamma(data, primary->pipe, degamma_linear);
		if (igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_CTM))
			set_ctm(primary->pipe, ctm_identity);
		set_gamma(data, primary->pipe, gamma_zero);
		igt_display_commit(&data->display);

		if (igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_DEGAMMA_LUT)) {
			blob = get_blob(data, primary->pipe, IGT_CRTC_DEGAMMA_LUT);
			igt_assert(blob &&
				   blob->length == (sizeof(struct drm_color_lut) *
						    data->degamma_lut_size));
			drmModeFreePropertyBlob(blob);
		}

		if (igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_CTM)) {
			blob = get_blob(data, primary->pipe, IGT_CRTC_CTM);
			igt_assert(blob &&
				   blob->length == sizeof(struct drm_color_ctm));
			drmModeFreePropertyBlob(blob);
		}

		blob = get_blob(data, primary->pipe, IGT_CRTC_GAMMA_LUT);
		igt_assert(blob &&
			   blob->length == (sizeof(struct drm_color_lut) *
					    data->gamma_lut_size));
		lut = (struct drm_color_lut *) blob->data;
		for (i = 0; i < data->gamma_lut_size; i++)
			igt_assert(lut[i].red == 0 &&
				   lut[i].green == 0 &&
				   lut[i].blue == 0);
		drmModeFreePropertyBlob(blob);

		/* Set a gamma LUT using the legacy ioctl and verify
		 * the content of the GAMMA_LUT property is changed
		 * and that CTM and DEGAMMA_LUT are empty. */
		kms_crtc = drmModeGetCrtc(data->drm_fd, primary->pipe->crtc_id);
		legacy_lut_size = kms_crtc->gamma_size;
		drmModeFreeCrtc(kms_crtc);

		red_lut = malloc(sizeof(uint16_t) * legacy_lut_size);
		green_lut = malloc(sizeof(uint16_t) * legacy_lut_size);
		blue_lut = malloc(sizeof(uint16_t) * legacy_lut_size);

		for (i = 0; i < legacy_lut_size; i++)
			red_lut[i] = green_lut[i] = blue_lut[i] = 0xffff;

		igt_assert_eq(drmModeCrtcSetGamma(data->drm_fd,
						  primary->pipe->crtc_id,
						  legacy_lut_size,
						  red_lut, green_lut, blue_lut),
			      0);
		igt_display_commit(&data->display);

		if (igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_DEGAMMA_LUT))
			igt_assert(get_blob(data, primary->pipe,
					    IGT_CRTC_DEGAMMA_LUT) == NULL);

		if (igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_CTM))
			igt_assert(get_blob(data, primary->pipe, IGT_CRTC_CTM) == NULL);

		blob = get_blob(data, primary->pipe, IGT_CRTC_GAMMA_LUT);
		igt_assert(blob &&
			   blob->length == (sizeof(struct drm_color_lut) *
					    legacy_lut_size));
		lut = (struct drm_color_lut *) blob->data;
		for (i = 0; i < legacy_lut_size; i++)
			igt_assert(lut[i].red == 0xffff &&
				   lut[i].green == 0xffff &&
				   lut[i].blue == 0xffff);
		drmModeFreePropertyBlob(blob);

		igt_plane_set_fb(primary, NULL);
		igt_output_set_pipe(output, PIPE_NONE);
	}

	free_lut(degamma_linear);
	free_lut(gamma_zero);
}

static bool crc_equal(igt_crc_t *a, igt_crc_t *b)
{
  return memcmp(a->crc, b->crc, sizeof(a->crc[0]) * a->n_words) == 0;
}

/*
 * Draw 3 rectangles using before colors with the ctm matrix apply and verify
 * the CRC is equal to using after colors with an identify ctm matrix.
 */
static bool test_pipe_ctm(data_t *data,
			  igt_plane_t *primary,
			  color_t *before,
			  color_t *after,
			  double *ctm_matrix)
{
	const double ctm_identity[] = {
		1.0, 0.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 0.0, 1.0
	};
	gamma_lut_t *degamma_linear, *gamma_linear;
	igt_output_t *output;
	bool ret = true;

	igt_require(igt_pipe_obj_has_prop(primary->pipe, IGT_CRTC_CTM));

	degamma_linear = generate_table(data->degamma_lut_size, 1.0);
	gamma_linear = generate_table(data->gamma_lut_size, 1.0);

	for_each_valid_output_on_pipe(&data->display, primary->pipe->pipe, output) {
		drmModeModeInfo *mode;
		struct igt_fb fb_modeset, fb;
		igt_crc_t crc_software, crc_hardware;
		int fb_id, fb_modeset_id;

		igt_output_set_pipe(output, primary->pipe->pipe);
		mode = igt_output_get_mode(output);

		/* Create a framebuffer at the size of the output. */
		fb_id = igt_create_fb(data->drm_fd,
				      mode->hdisplay,
				      mode->vdisplay,
				      DRM_FORMAT_XRGB8888,
				      LOCAL_DRM_FORMAT_MOD_NONE,
				      &fb);
		igt_assert(fb_id);

		fb_modeset_id = igt_create_fb(data->drm_fd,
					      mode->hdisplay,
					      mode->vdisplay,
					      DRM_FORMAT_XRGB8888,
					      LOCAL_DRM_FORMAT_MOD_NONE,
					      &fb_modeset);
		igt_assert(fb_modeset_id);
		igt_plane_set_fb(primary, &fb_modeset);

		/*
		 * Don't program LUT's for max CTM cases, as limitation of
		 * representing intermediate values between 0 and 1.0 causes
		 * rounding issues and inaccuracies leading to crc mismatch.
		 */
		if (memcmp(before, after, sizeof(color_t))) {
			set_degamma(data, primary->pipe, degamma_linear);
			set_gamma(data, primary->pipe, gamma_linear);
		} else {
			/* Disable Degamma and Gamma for ctm max test */
			disable_degamma(primary->pipe);
			disable_gamma(primary->pipe);
		}

		disable_ctm(primary->pipe);
		igt_display_commit(&data->display);

		paint_rectangles(data, mode, after, &fb);
		igt_plane_set_fb(primary, &fb);
		set_ctm(primary->pipe, ctm_identity);
		igt_display_commit(&data->display);
		igt_wait_for_vblank(data->drm_fd, primary->pipe->pipe);
		igt_pipe_crc_collect_crc(data->pipe_crc, &crc_software);

		/* With CTM transformation. */
		paint_rectangles(data, mode, before, &fb);
		igt_plane_set_fb(primary, &fb);
		set_ctm(primary->pipe, ctm_matrix);
		igt_display_commit(&data->display);
		igt_wait_for_vblank(data->drm_fd, primary->pipe->pipe);
		igt_pipe_crc_collect_crc(data->pipe_crc, &crc_hardware);

		/* Verify that the CRC of the software computed output is
		 * equal to the CRC of the CTM matrix transformation output.
		 */
		ret &= crc_equal(&crc_software, &crc_hardware);

		igt_plane_set_fb(primary, NULL);
		igt_output_set_pipe(output, PIPE_NONE);
	}

	free_lut(degamma_linear);
	free_lut(gamma_linear);

	return ret;
}

/*
 * Hardware computes CRC based on the number of bits it is working with (8,
 * 10, 12, 16 bits), meaning with a framebuffer of 8bits per color will
 * usually leave the remaining lower bits at 0.
 *
 * We're programming the gamma LUT in order to get rid of those lower bits so
 * we can compare the CRC of a framebuffer without any transformation to a CRC
 * with transformation applied and verify the CRCs match.
 *
 * This test is currently disabled as the CRC computed on Intel hardware seems
 * to include data on the lower bits, this is preventing us to CRC checks.
 */
#if 0
static void test_pipe_limited_range_ctm(data_t *data,
					igt_plane_t *primary)
{
	double limited_result = 235.0 / 255.0;
	color_t red_green_blue_limited[] = {
		{ limited_result, 0.0, 0.0 },
		{ 0.0, limited_result, 0.0 },
		{ 0.0, 0.0, limited_result }
	};
	color_t red_green_blue_full[] = {
		{ 0.5, 0.0, 0.0 },
		{ 0.0, 0.5, 0.0 },
		{ 0.0, 0.0, 0.5 }
	};
	double ctm[] = { 1.0, 0.0, 0.0,
			0.0, 1.0, 0.0,
			0.0, 0.0, 1.0 };
	gamma_lut_t *degamma_linear, *gamma_linear;
	igt_output_t *output;
	bool has_broadcast_rgb_output = false;

	degamma_linear = generate_table(data->degamma_lut_size, 1.0);
	gamma_linear = generate_table(data->gamma_lut_size, 1.0);

	for_each_valid_output_on_pipe(&data->display, primary->pipe->pipe, output) {
		drmModeModeInfo *mode;
		struct igt_fb fb_modeset, fb;
		igt_crc_t crc_full, crc_limited;
		int fb_id, fb_modeset_id;

		if (!igt_output_has_prop(output, IGT_CONNECTOR_BROADCAST_RGB))
			continue;

		has_broadcast_rgb_output = true;

		igt_output_set_pipe(output, primary->pipe->pipe);
		mode = igt_output_get_mode(output);

		/* Create a framebuffer at the size of the output. */
		fb_id = igt_create_fb(data->drm_fd,
				      mode->hdisplay,
				      mode->vdisplay,
				      DRM_FORMAT_XRGB8888,
				      LOCAL_DRM_FORMAT_MOD_NONE,
				      &fb);
		igt_assert(fb_id);

		fb_modeset_id = igt_create_fb(data->drm_fd,
					      mode->hdisplay,
					      mode->vdisplay,
					      DRM_FORMAT_XRGB8888,
					      LOCAL_DRM_FORMAT_MOD_NONE,
					      &fb_modeset);
		igt_assert(fb_modeset_id);
		igt_plane_set_fb(primary, &fb_modeset);

		set_degamma(data, primary->pipe, degamma_linear);
		set_gamma(data, primary->pipe, gamma_linear);
		set_ctm(primary->pipe, ctm);

		igt_output_set_prop_value(output, IGT_CONNECTOR_BROADCAST_RGB, BROADCAST_RGB_FULL);
		paint_rectangles(data, mode, red_green_blue_limited, &fb);
		igt_plane_set_fb(primary, &fb);
		igt_display_commit(&data->display);
		igt_wait_for_vblank(data->drm_fd, primary->pipe->pipe);
		igt_pipe_crc_collect_crc(data->pipe_crc, &crc_full);

		/* Set the output into limited range. */
		igt_output_set_prop_value(output, IGT_CONNECTOR_BROADCAST_RGB, BROADCAST_RGB_16_235);
		paint_rectangles(data, mode, red_green_blue_full, &fb);
		igt_plane_set_fb(primary, &fb);
		igt_display_commit(&data->display);
		igt_wait_for_vblank(data->drm_fd, primary->pipe->pipe);
		igt_pipe_crc_collect_crc(data->pipe_crc, &crc_limited);

		/* And reset.. */
		igt_output_set_prop_value(output, IGT_CONNECTOR_BROADCAST_RGB, BROADCAST_RGB_FULL);
		igt_plane_set_fb(primary, NULL);
		igt_output_set_pipe(output, PIPE_NONE);

		/* Verify that the CRC of the software computed output is
		 * equal to the CRC of the CTM matrix transformation output.
		 */
		igt_assert_crc_equal(&crc_full, &crc_limited);
	}

	free_lut(gamma_linear);
	free_lut(degamma_linear);

	igt_require(has_broadcast_rgb_output);
}
#endif

static void
run_tests_for_pipe(data_t *data, enum pipe p)
{
	igt_pipe_t *pipe;
	igt_plane_t *primary;
	double delta;
	int i;
	color_t red_green_blue[] = {
		{ 1.0, 0.0, 0.0 },
		{ 0.0, 1.0, 0.0 },
		{ 0.0, 0.0, 1.0 }
	};

	igt_fixture {
		igt_require_pipe_crc(data->drm_fd);

		igt_require(p < data->display.n_pipes);

		pipe = &data->display.pipes[p];
		igt_require(pipe->n_planes >= 0);

		primary = igt_pipe_get_plane_type(pipe, DRM_PLANE_TYPE_PRIMARY);

		data->pipe_crc = igt_pipe_crc_new(data->drm_fd,
						  primary->pipe->pipe,
						  INTEL_PIPE_CRC_SOURCE_AUTO);

		if (igt_pipe_obj_has_prop(&data->display.pipes[p], IGT_CRTC_DEGAMMA_LUT_SIZE)) {
			data->degamma_lut_size =
				igt_pipe_obj_get_prop(&data->display.pipes[p],
						      IGT_CRTC_DEGAMMA_LUT_SIZE);
			igt_assert_lt(0, data->degamma_lut_size);
		}

		if (igt_pipe_obj_has_prop(&data->display.pipes[p], IGT_CRTC_GAMMA_LUT_SIZE)) {
			data->gamma_lut_size =
				igt_pipe_obj_get_prop(&data->display.pipes[p],
						      IGT_CRTC_GAMMA_LUT_SIZE);
			igt_assert_lt(0, data->gamma_lut_size);
		}

		igt_display_require_output_on_pipe(&data->display, p);
	}

	/* We assume an 8bits depth per color for degamma/gamma LUTs
	 * for CRC checks with framebuffer references. */
	data->color_depth = 8;
	delta = 1.0 / (1 << data->color_depth);

	igt_subtest_f("pipe-%s-ctm-red-to-blue", kmstest_pipe_name(p)) {
		color_t blue_green_blue[] = {
			{ 0.0, 0.0, 1.0 },
			{ 0.0, 1.0, 0.0 },
			{ 0.0, 0.0, 1.0 }
		};
		double ctm[] = { 0.0, 0.0, 0.0,
				0.0, 1.0, 0.0,
				1.0, 0.0, 1.0 };
		igt_assert(test_pipe_ctm(data, primary, red_green_blue,
					 blue_green_blue, ctm));
	}

	igt_subtest_f("pipe-%s-ctm-green-to-red", kmstest_pipe_name(p)) {
		color_t red_red_blue[] = {
			{ 1.0, 0.0, 0.0 },
			{ 1.0, 0.0, 0.0 },
			{ 0.0, 0.0, 1.0 }
		};
		double ctm[] = { 1.0, 1.0, 0.0,
				0.0, 0.0, 0.0,
				0.0, 0.0, 1.0 };
		igt_assert(test_pipe_ctm(data, primary, red_green_blue,
					 red_red_blue, ctm));
	}

	igt_subtest_f("pipe-%s-ctm-blue-to-red", kmstest_pipe_name(p)) {
		color_t red_green_red[] = {
			{ 1.0, 0.0, 0.0 },
			{ 0.0, 1.0, 0.0 },
			{ 1.0, 0.0, 0.0 }
		};
		double ctm[] = { 1.0, 0.0, 1.0,
				0.0, 1.0, 0.0,
				0.0, 0.0, 0.0 };
		igt_assert(test_pipe_ctm(data, primary, red_green_blue,
					 red_green_red, ctm));
	}

	/* We tests a few values around the expected result because
	 * the it depends on the hardware we're dealing with, we can
	 * either get clamped or rounded values and we also need to
	 * account for odd number of items in the LUTs. */
	igt_subtest_f("pipe-%s-ctm-0-25", kmstest_pipe_name(p)) {
		color_t expected_colors[] = {
			{ 0.0, }, { 0.0, }, { 0.0, }
		};
		double ctm[] = { 0.25, 0.0,  0.0,
				 0.0,  0.25, 0.0,
				 0.0,  0.0,  0.25 };
		bool success = false;

		for (i = 0; i < 5; i++) {
			expected_colors[0].r =
				expected_colors[1].g =
				expected_colors[2].b =
				0.25 + delta * (i - 2);
			success |= test_pipe_ctm(data, primary,
						 red_green_blue,
						 expected_colors, ctm);
		}
		igt_assert(success);
	}

	igt_subtest_f("pipe-%s-ctm-0-5", kmstest_pipe_name(p)) {
		color_t expected_colors[] = {
			{ 0.0, }, { 0.0, }, { 0.0, }
		};
		double ctm[] = { 0.5, 0.0, 0.0,
				 0.0, 0.5, 0.0,
				 0.0, 0.0, 0.5 };
		bool success = false;

		for (i = 0; i < 5; i++) {
			expected_colors[0].r =
				expected_colors[1].g =
				expected_colors[2].b =
				0.5 + delta * (i - 2);
			success |= test_pipe_ctm(data, primary,
						 red_green_blue,
						 expected_colors, ctm);
		}
		igt_assert(success);
	}

	igt_subtest_f("pipe-%s-ctm-0-75", kmstest_pipe_name(p)) {
		color_t expected_colors[] = {
			{ 0.0, }, { 0.0, }, { 0.0, }
		};
		double ctm[] = { 0.75, 0.0,  0.0,
				 0.0,  0.75, 0.0,
				 0.0,  0.0,  0.75 };
		bool success = false;

		for (i = 0; i < 7; i++) {
			expected_colors[0].r =
				expected_colors[1].g =
				expected_colors[2].b =
				0.75 + delta * (i - 3);
			success |= test_pipe_ctm(data, primary,
						 red_green_blue,
						 expected_colors, ctm);
		}
		igt_assert(success);
	}

	igt_subtest_f("pipe-%s-ctm-max", kmstest_pipe_name(p)) {
		color_t full_rgb[] = {
			{ 1.0, 0.0, 0.0 },
			{ 0.0, 1.0, 0.0 },
			{ 0.0, 0.0, 1.0 }
		};
		double ctm[] = { 100.0,   0.0,   0.0,
				 0.0,   100.0,   0.0,
				 0.0,     0.0, 100.0 };

		/* CherryView generates values on 10bits that we
		 * produce with an 8 bits per color framebuffer. */
		igt_require(!IS_CHERRYVIEW(data->devid));

		igt_assert(test_pipe_ctm(data, primary, red_green_blue,
					 full_rgb, ctm));
	}

	igt_subtest_f("pipe-%s-ctm-negative", kmstest_pipe_name(p)) {
		color_t all_black[] = {
			{ 0.0, 0.0, 0.0 },
			{ 0.0, 0.0, 0.0 },
			{ 0.0, 0.0, 0.0 }
		};
		double ctm[] = { -1.0,  0.0,  0.0,
				 0.0, -1.0,  0.0,
				 0.0,  0.0, -1.0 };
		igt_assert(test_pipe_ctm(data, primary, red_green_blue,
					 all_black, ctm));
	}

#if 0
	igt_subtest_f("pipe-%s-ctm-limited-range", kmstest_pipe_name(p))
		test_pipe_limited_range_ctm(data, primary);
#endif

	igt_subtest_f("pipe-%s-degamma", kmstest_pipe_name(p))
		test_pipe_degamma(data, primary);

	igt_subtest_f("pipe-%s-gamma", kmstest_pipe_name(p))
		test_pipe_gamma(data, primary);

	igt_subtest_f("pipe-%s-legacy-gamma", kmstest_pipe_name(p))
		test_pipe_legacy_gamma(data, primary);

	igt_subtest_f("pipe-%s-legacy-gamma-reset", kmstest_pipe_name(p))
		test_pipe_legacy_gamma_reset(data, primary);

	igt_fixture {
		disable_degamma(primary->pipe);
		disable_gamma(primary->pipe);
		disable_ctm(primary->pipe);
		igt_display_commit(&data->display);

		igt_pipe_crc_free(data->pipe_crc);
		data->pipe_crc = NULL;
	}
}

static int
pipe_set_property_blob_id(igt_pipe_t *pipe, enum igt_atomic_crtc_properties prop, uint32_t blob_id)
{
	int ret;

	igt_pipe_obj_replace_prop_blob(pipe, prop, NULL, 0);

	igt_pipe_obj_set_prop_value(pipe, prop, blob_id);

	ret = igt_display_try_commit2(pipe->display, pipe->display->is_atomic ? COMMIT_ATOMIC : COMMIT_LEGACY);

	igt_pipe_obj_set_prop_value(pipe, prop, 0);

	return ret;
}

static int
pipe_set_property_blob(igt_pipe_t *pipe, enum igt_atomic_crtc_properties prop, void *ptr, size_t length)
{
	igt_pipe_obj_replace_prop_blob(pipe, prop, ptr, length);

	return igt_display_try_commit2(pipe->display, pipe->display->is_atomic ? COMMIT_ATOMIC : COMMIT_LEGACY);
}

static void
invalid_gamma_lut_sizes(data_t *data)
{
	igt_display_t *display = &data->display;
	igt_pipe_t *pipe = &display->pipes[0];
	size_t gamma_lut_size = data->gamma_lut_size * sizeof(struct drm_color_lut);
	struct drm_color_lut *gamma_lut;

	igt_require(igt_pipe_obj_has_prop(pipe, IGT_CRTC_GAMMA_LUT));

	gamma_lut = malloc(gamma_lut_size * 2);

	igt_display_commit2(display, display->is_atomic ? COMMIT_ATOMIC : COMMIT_LEGACY);

	igt_assert_eq(pipe_set_property_blob(pipe, IGT_CRTC_GAMMA_LUT,
					     gamma_lut, 1),
		      -EINVAL);
	igt_assert_eq(pipe_set_property_blob(pipe, IGT_CRTC_GAMMA_LUT,
					     gamma_lut, gamma_lut_size + 1),
		      -EINVAL);
	igt_assert_eq(pipe_set_property_blob(pipe, IGT_CRTC_GAMMA_LUT,
					     gamma_lut, gamma_lut_size - 1),
		      -EINVAL);
	igt_assert_eq(pipe_set_property_blob(pipe, IGT_CRTC_GAMMA_LUT,
					     gamma_lut, gamma_lut_size + sizeof(struct drm_color_lut)),
		      -EINVAL);
	igt_assert_eq(pipe_set_property_blob_id(pipe, IGT_CRTC_GAMMA_LUT, pipe->crtc_id),
		      -EINVAL);
	igt_assert_eq(pipe_set_property_blob_id(pipe, IGT_CRTC_GAMMA_LUT, 4096 * 4096),
		      -EINVAL);

	free(gamma_lut);
}

static void
invalid_degamma_lut_sizes(data_t *data)
{
	igt_display_t *display = &data->display;
	igt_pipe_t *pipe = &display->pipes[0];
	size_t degamma_lut_size = data->degamma_lut_size * sizeof(struct drm_color_lut);
	struct drm_color_lut *degamma_lut;

	igt_require(igt_pipe_obj_has_prop(pipe, IGT_CRTC_DEGAMMA_LUT));

	degamma_lut = malloc(degamma_lut_size * 2);

	igt_display_commit2(display, display->is_atomic ? COMMIT_ATOMIC : COMMIT_LEGACY);

	igt_assert_eq(pipe_set_property_blob(pipe, IGT_CRTC_DEGAMMA_LUT,
					     degamma_lut, 1), -EINVAL);
	igt_assert_eq(pipe_set_property_blob(pipe, IGT_CRTC_DEGAMMA_LUT,
					     degamma_lut, degamma_lut_size + 1),
		      -EINVAL);
	igt_assert_eq(pipe_set_property_blob(pipe, IGT_CRTC_DEGAMMA_LUT,
					     degamma_lut, degamma_lut_size - 1),
		      -EINVAL);
	igt_assert_eq(pipe_set_property_blob(pipe, IGT_CRTC_DEGAMMA_LUT,
					     degamma_lut, degamma_lut_size + sizeof(struct drm_color_lut)),
		      -EINVAL);
	igt_assert_eq(pipe_set_property_blob_id(pipe, IGT_CRTC_DEGAMMA_LUT, pipe->crtc_id),
		      -EINVAL);
	igt_assert_eq(pipe_set_property_blob_id(pipe, IGT_CRTC_DEGAMMA_LUT, 4096 * 4096),
		      -EINVAL);

	free(degamma_lut);
}

static void
invalid_ctm_matrix_sizes(data_t *data)
{
	igt_display_t *display = &data->display;
	igt_pipe_t *pipe = &display->pipes[0];
	void *ptr;

	igt_require(igt_pipe_obj_has_prop(pipe, IGT_CRTC_CTM));

	ptr = malloc(sizeof(struct drm_color_ctm) * 4);

	igt_assert_eq(pipe_set_property_blob(pipe, IGT_CRTC_CTM, ptr, 1),
		      -EINVAL);
	igt_assert_eq(pipe_set_property_blob(pipe, IGT_CRTC_CTM, ptr,
					     sizeof(struct drm_color_ctm) + 1),
		      -EINVAL);
	igt_assert_eq(pipe_set_property_blob(pipe, IGT_CRTC_CTM, ptr,
					     sizeof(struct drm_color_ctm) - 1),
		      -EINVAL);
	igt_assert_eq(pipe_set_property_blob(pipe, IGT_CRTC_CTM, ptr,
					     sizeof(struct drm_color_ctm) * 2),
		      -EINVAL);
	igt_assert_eq(pipe_set_property_blob_id(pipe, IGT_CRTC_CTM, pipe->crtc_id),
		      -EINVAL);
	igt_assert_eq(pipe_set_property_blob_id(pipe, IGT_CRTC_CTM, 4096 * 4096),
		      -EINVAL);

	free(ptr);
}

igt_main
{
	data_t data = {};
	enum pipe pipe;

	igt_skip_on_simulation();

	igt_fixture {
		data.drm_fd = drm_open_driver_master(DRIVER_INTEL | DRIVER_AMDGPU);
		if (is_i915_device(data.drm_fd))
			data.devid = intel_get_drm_devid(data.drm_fd);
		kmstest_set_vt_graphics_mode();

		igt_display_require(&data.display, data.drm_fd);
	}

	for_each_pipe_static(pipe)
		igt_subtest_group
			run_tests_for_pipe(&data, pipe);

	igt_subtest_f("pipe-invalid-gamma-lut-sizes")
		invalid_gamma_lut_sizes(&data);

	igt_subtest_f("pipe-invalid-degamma-lut-sizes")
		invalid_degamma_lut_sizes(&data);

	igt_subtest_f("pipe-invalid-ctm-matrix-sizes")
		invalid_ctm_matrix_sizes(&data);

	igt_fixture {
		igt_display_fini(&data.display);
	}
}
