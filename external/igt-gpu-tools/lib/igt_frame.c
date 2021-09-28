/*
 * Copyright © 2017 Intel Corporation
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
 *  Paul Kocialkowski <paul.kocialkowski@linux.intel.com>
 */

#include "config.h"

#include <fcntl.h>
#include <pixman.h>
#include <cairo.h>
#include <gsl/gsl_statistics_double.h>
#include <gsl/gsl_fit.h>

#include "igt_frame.h"
#include "igt_core.h"

/**
 * SECTION:igt_frame
 * @short_description: Library for frame-related tests
 * @title: Frame
 * @include: igt_frame.h
 *
 * This library contains helpers for frame-related tests. This includes common
 * frame dumping as well as frame comparison helpers.
 */

/**
 * igt_frame_dump_is_enabled:
 *
 * Get whether frame dumping is enabled.
 *
 * Returns: A boolean indicating whether frame dumping is enabled
 */
bool igt_frame_dump_is_enabled(void)
{
	return igt_frame_dump_path != NULL;
}

static void igt_write_frame_to_png(cairo_surface_t *surface, int fd,
				   const char *qualifier, const char *suffix)
{
	char path[PATH_MAX];
	const char *test_name;
	const char *subtest_name;
	cairo_status_t status;
	int index;

	test_name = igt_test_name();
	subtest_name = igt_subtest_name();

	if (suffix)
		snprintf(path, PATH_MAX, "%s/frame-%s-%s-%s-%s.png",
			 igt_frame_dump_path, test_name, subtest_name, qualifier,
			 suffix);
	else
		snprintf(path, PATH_MAX, "%s/frame-%s-%s-%s.png",
			 igt_frame_dump_path, test_name, subtest_name, qualifier);

	igt_debug("Dumping %s frame to %s...\n", qualifier, path);

	status = cairo_surface_write_to_png(surface, path);

	igt_assert_eq(status, CAIRO_STATUS_SUCCESS);

	index = strlen(path);

	if (fd >= 0 && index < (PATH_MAX - 1)) {
		path[index++] = '\n';
		path[index] = '\0';

		write(fd, path, strlen(path));
	}
}

/**
 * igt_write_compared_frames_to_png:
 * @reference: The reference cairo surface
 * @capture: The captured cairo surface
 * @reference_suffix: The suffix to give to the reference png file
 * @capture_suffix: The suffix to give to the capture png file
 *
 * Write previously compared frames to png files.
 */
void igt_write_compared_frames_to_png(cairo_surface_t *reference,
				      cairo_surface_t *capture,
				      const char *reference_suffix,
				      const char *capture_suffix)
{
	char *id;
	const char *test_name;
	const char *subtest_name;
	char path[PATH_MAX];
	int fd = -1;

	if (!igt_frame_dump_is_enabled())
		return;

	id = getenv("IGT_FRAME_DUMP_ID");

	test_name = igt_test_name();
	subtest_name = igt_subtest_name();

	if (id)
		snprintf(path, PATH_MAX, "%s/frame-%s-%s-%s.txt",
			 igt_frame_dump_path, test_name, subtest_name, id);
	else
		snprintf(path, PATH_MAX, "%s/frame-%s-%s.txt",
			 igt_frame_dump_path, test_name, subtest_name);

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	igt_assert(fd >= 0);

	igt_debug("Writing dump report to %s...\n", path);

	igt_write_frame_to_png(reference, fd, "reference", reference_suffix);
	igt_write_frame_to_png(capture, fd, "capture", capture_suffix);

	close(fd);
}

/**
 * igt_check_analog_frame_match:
 * @reference: The reference cairo surface
 * @capture: The captured cairo surface
 *
 * Checks that the analog image contained in the chamelium frame dump matches
 * the given framebuffer.
 *
 * In order to determine whether the frame matches the reference, the following
 * reasoning is implemented:
 * 1. The absolute error for each color value of the reference is collected.
 * 2. The average absolute error is calculated for each color value of the
 *    reference and must not go above 60 (23.5 % of the total range).
 * 3. A linear fit for the average absolute error from the pixel value is
 *    calculated, as a DAC-ADC chain is expected to have a linear error curve.
 * 4. The linear fit is correlated with the actual average absolute error for
 *    the frame and the correlation coefficient is checked to be > 0.985,
 *    indicating a match with the expected error trend.
 *
 * Most errors (e.g. due to scaling, rotation, color space, etc) can be
 * reliably detected this way, with a minimized number of false-positives.
 * However, the brightest values (250 and up) are ignored as the error trend
 * is often not linear there in practice due to clamping.
 *
 * Returns: a boolean indicating whether the frames match
 */

bool igt_check_analog_frame_match(cairo_surface_t *reference,
				  cairo_surface_t *capture)
{
	pixman_image_t *reference_src, *capture_src;
	int w, h;
	int error_count[3][256][2] = { 0 };
	double error_average[4][250];
	double error_trend[250];
	double c0, c1, cov00, cov01, cov11, sumsq;
	double correlation;
	unsigned char *reference_pixels, *capture_pixels;
	unsigned char *p;
	unsigned char *q;
	bool match = true;
	int diff;
	int x, y;
	int i, j;

	w = cairo_image_surface_get_width(reference);
	h = cairo_image_surface_get_height(reference);

	reference_src = pixman_image_create_bits(
	    PIXMAN_x8r8g8b8, w, h,
	    (void*)cairo_image_surface_get_data(reference),
	    cairo_image_surface_get_stride(reference));
	reference_pixels = (unsigned char *) pixman_image_get_data(reference_src);

	capture_src = pixman_image_create_bits(
	    PIXMAN_x8r8g8b8, w, h,
	    (void*)cairo_image_surface_get_data(capture),
	    cairo_image_surface_get_stride(capture));
	capture_pixels = (unsigned char *) pixman_image_get_data(capture_src);

	/* Collect the absolute error for each color value */
	for (x = 0; x < w; x++) {
		for (y = 0; y < h; y++) {
			p = &capture_pixels[(x + y * w) * 4];
			q = &reference_pixels[(x + y * w) * 4];

			for (i = 0; i < 3; i++) {
				diff = (int) p[i] - q[i];
				if (diff < 0)
					diff = -diff;

				error_count[i][q[i]][0] += diff;
				error_count[i][q[i]][1]++;
			}
		}
	}

	/* Calculate the average absolute error for each color value */
	for (i = 0; i < 250; i++) {
		error_average[0][i] = i;

		for (j = 1; j < 4; j++) {
			error_average[j][i] = (double) error_count[j-1][i][0] /
					      error_count[j-1][i][1];

			if (error_average[j][i] > 60) {
				igt_warn("Error average too high (%f)\n",
					 error_average[j][i]);

				match = false;
				goto complete;
			}
		}
	}

	/*
	 * Calculate error trend from linear fit.
	 * A DAC-ADC chain is expected to have a linear absolute error on
	 * most of its range
	 */
	for (i = 1; i < 4; i++) {
		gsl_fit_linear((const double *) &error_average[0], 1,
			       (const double *) &error_average[i], 1, 250,
			       &c0, &c1, &cov00, &cov01, &cov11, &sumsq);

		for (j = 0; j < 250; j++)
			error_trend[j] = c0 + j * c1;

		correlation = gsl_stats_correlation((const double *) &error_trend,
						    1,
						    (const double *) &error_average[i],
						    1, 250);

		if (correlation < 0.985) {
			igt_warn("Error with reference not correlated (%f)\n",
				 correlation);

			match = false;
			goto complete;
		}
	}

complete:
	pixman_image_unref(reference_src);
	pixman_image_unref(capture_src);

	return match;
}

#define XR24_COLOR_VALUE(data, stride, x, y, c) \
	*((uint8_t *)(data) + (y) * (stride) + 4 * (x) + (c))

/**
 * igt_check_checkerboard_frame_match:
 * @reference: The reference cairo surface
 * @capture: The captured cairo surface
 *
 * Checks that the reference frame matches the captured frame using a
 * method designed for checkerboard patterns. These patterns are made of
 * consecutive rectangular shapes with alternating solid colors.
 *
 * The intent of this method is to cover cases where the captured result is
 * not pixel-perfect due to features such as scaling or YUV conversion and
 * subsampling. Such effects are mostly noticeable on the edges of the
 * patterns, so they are detected and excluded from the comparison.
 *
 * The algorithm works with two major steps. First, the edges of the reference
 * pattern are detected on the x and y axis independently. The detection is done
 * by calculating an absolute difference with a span of a few pixels before and
 * after the current position on the given axis, accumulated for each color
 * component. If the sum is above a given threshold on one of the axis, the
 * position is marked as an edge. In the second step, the pixel values are
 * compared per-component, excluding positions that were marked as edges or
 * that are at a transition between an edge and a non-edge. An error threshold
 * (for each individual color component) is used to mark the position as
 * erroneous or not. The ratio of erroneous pixels over compared pixels (that
 * does not count excluded pixels) is then calculated and compared to the error
 * rate threshold to determine whether the frames match or not.
 *
 * Returns: a boolean indicating whether the frames match
 */
bool igt_check_checkerboard_frame_match(cairo_surface_t *reference,
					cairo_surface_t *capture)
{
	unsigned int width, height, ref_stride, cap_stride;
	void *ref_data, *cap_data;
	unsigned char *edges_map;
	unsigned int x, y, c;
	unsigned int errors = 0, pixels = 0;
	unsigned int edge_threshold = 100;
	unsigned int color_error_threshold = 24;
	double error_rate_threshold = 0.01;
	double error_rate;
	unsigned int span = 2;
	bool match = false;

	width = cairo_image_surface_get_width(reference);
	height = cairo_image_surface_get_height(reference);

	ref_stride = cairo_image_surface_get_stride(reference);
	ref_data = cairo_image_surface_get_data(reference);
	igt_assert(ref_data);

	cap_stride = cairo_image_surface_get_stride(capture);
	cap_data = cairo_image_surface_get_data(capture);
	igt_assert(cap_data);

	edges_map = calloc(1, width * height);
	igt_assert(edges_map);

	/* First pass to detect the pattern edges. */
	for (y = 0; y < height; y++) {
		if (y < span || y > (height - span - 1))
			continue;

		for (x = 0; x < width; x++) {
			unsigned int xdiff = 0, ydiff = 0;

			if (x < span || x > (width - span - 1))
				continue;

			for (c = 0; c < 3; c++) {
				xdiff += abs(XR24_COLOR_VALUE(ref_data, ref_stride, x + span, y, c) -
					     XR24_COLOR_VALUE(ref_data, ref_stride, x - span, y, c));
				ydiff += abs(XR24_COLOR_VALUE(ref_data, ref_stride, x, y + span, c) -
					     XR24_COLOR_VALUE(ref_data, ref_stride, x, y - span, c));
			}

			edges_map[y * width + x] = (xdiff > edge_threshold ||
						    ydiff > edge_threshold);
		}
	}

	/* Second pass to detect errors. */
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			bool error = false;

			if (edges_map[y * width + x])
				continue;

			for (c = 0; c < 3; c++) {
				unsigned int diff;

				/* Compare the reference and capture values. */
				diff = abs(XR24_COLOR_VALUE(ref_data, ref_stride, x, y, c) -
					   XR24_COLOR_VALUE(cap_data, cap_stride, x, y, c));

				if (diff > color_error_threshold)
					error = true;
			}

			/* Allow error if coming on or off an edge (on x). */
			if (error && x >= span && x <= (width - span - 1) &&
			    edges_map[y * width + (x - span)] !=
			    edges_map[y * width + (x + span)])
				continue;

			/* Allow error if coming on or off an edge (on y). */
			if (error && y >= span && y <= (height - span - 1) &&
			    edges_map[(y - span) * width + x] !=
			    edges_map[(y + span) * width + x] && error)
				continue;

			if (error)
				errors++;

			pixels++;
		}
	}

	free(edges_map);

	error_rate = (double) errors / pixels;

	if (error_rate < error_rate_threshold)
		match = true;

	igt_debug("Checkerboard pattern %s with error rate %f %%\n",
		  match ? "matched" : "not matched", error_rate * 100);

	return match;
}
