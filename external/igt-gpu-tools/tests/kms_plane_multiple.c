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
 *
 */

#include "igt.h"
#include "drmtest.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

IGT_TEST_DESCRIPTION("Test atomic mode setting with multiple planes.");

#define SIZE_PLANE      256
#define SIZE_CURSOR     128
#define LOOP_FOREVER     -1

typedef struct {
	float red;
	float green;
	float blue;
} color_t;

typedef struct {
	int drm_fd;
	igt_display_t display;
	igt_crc_t ref_crc;
	igt_pipe_crc_t *pipe_crc;
	igt_plane_t **plane;
	struct igt_fb *fb;
} data_t;

/* Command line parameters. */
struct {
	int iterations;
	bool user_seed;
	int seed;
} opt = {
	.iterations = 1,
	.user_seed = false,
	.seed = 1,
};

/*
 * Common code across all tests, acting on data_t
 */
static void test_init(data_t *data, enum pipe pipe, int n_planes)
{
	data->pipe_crc = igt_pipe_crc_new(data->drm_fd, pipe, INTEL_PIPE_CRC_SOURCE_AUTO);

	data->plane = calloc(n_planes, sizeof(*data->plane));
	igt_assert_f(data->plane != NULL, "Failed to allocate memory for planes\n");

	data->fb = calloc(n_planes, sizeof(struct igt_fb));
	igt_assert_f(data->fb != NULL, "Failed to allocate memory for FBs\n");
}

static void test_fini(data_t *data, igt_output_t *output, int n_planes)
{
	igt_pipe_crc_stop(data->pipe_crc);

	/* reset the constraint on the pipe */
	igt_output_set_pipe(output, PIPE_ANY);

	igt_pipe_crc_free(data->pipe_crc);
	data->pipe_crc = NULL;

	free(data->plane);
	data->plane = NULL;

	free(data->fb);
	data->fb = NULL;

	igt_display_reset(&data->display);
}

static void
get_reference_crc(data_t *data, igt_output_t *output, enum pipe pipe,
	      color_t *color, uint64_t tiling)
{
	drmModeModeInfo *mode;
	igt_plane_t *primary;
	int ret;

	igt_output_set_pipe(output, pipe);

	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);
	data->plane[primary->index] = primary;

	mode = igt_output_get_mode(output);

	igt_create_color_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
			    DRM_FORMAT_XRGB8888,
			    LOCAL_DRM_FORMAT_MOD_NONE,
			    color->red, color->green, color->blue,
			    &data->fb[primary->index]);

	igt_plane_set_fb(data->plane[primary->index], &data->fb[primary->index]);

	ret = igt_display_try_commit2(&data->display, COMMIT_ATOMIC);
	igt_skip_on(ret != 0);

	igt_pipe_crc_start(data->pipe_crc);
	igt_pipe_crc_get_single(data->pipe_crc, &data->ref_crc);
}

static void
create_fb_for_mode_position(data_t *data, igt_output_t *output, drmModeModeInfo *mode,
			    color_t *color, int *rect_x, int *rect_y,
			    int *rect_w, int *rect_h, uint64_t tiling,
			    int max_planes)
{
	unsigned int fb_id;
	cairo_t *cr;
	igt_plane_t *primary;

	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);

	igt_skip_on(!igt_display_has_format_mod(&data->display,
						DRM_FORMAT_XRGB8888,
						tiling));

	fb_id = igt_create_fb(data->drm_fd,
			      mode->hdisplay, mode->vdisplay,
			      DRM_FORMAT_XRGB8888,
			      tiling,
			      &data->fb[primary->index]);
	igt_assert(fb_id);

	cr = igt_get_cairo_ctx(data->drm_fd, &data->fb[primary->index]);
	igt_paint_color(cr, rect_x[0], rect_y[0],
			mode->hdisplay, mode->vdisplay,
			color->red, color->green, color->blue);

	for (int i = 0; i < max_planes; i++) {
		if (data->plane[i]->type == DRM_PLANE_TYPE_PRIMARY)
			continue;
		igt_paint_color(cr, rect_x[i], rect_y[i],
				rect_w[i], rect_h[i], 0.0, 0.0, 0.0);
		}

	igt_put_cairo_ctx(data->drm_fd, &data->fb[primary->index], cr);
}


static void
prepare_planes(data_t *data, enum pipe pipe_id, color_t *color,
	       uint64_t tiling, int max_planes, igt_output_t *output)
{
	drmModeModeInfo *mode;
	igt_pipe_t *pipe;
	igt_plane_t *primary;
	int *x;
	int *y;
	int *size;
	int i;
	int* suffle;

	igt_output_set_pipe(output, pipe_id);
	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);
	pipe = primary->pipe;

	x = malloc(pipe->n_planes * sizeof(*x));
	igt_assert_f(x, "Failed to allocate %ld bytes for variable x\n", (long int) (pipe->n_planes * sizeof(*x)));
	y = malloc(pipe->n_planes * sizeof(*y));
	igt_assert_f(y, "Failed to allocate %ld bytes for variable y\n", (long int) (pipe->n_planes * sizeof(*y)));
	size = malloc(pipe->n_planes * sizeof(*size));
	igt_assert_f(size, "Failed to allocate %ld bytes for variable size\n", (long int) (pipe->n_planes * sizeof(*size)));
	suffle = malloc(pipe->n_planes * sizeof(*suffle));
	igt_assert_f(suffle, "Failed to allocate %ld bytes for variable size\n", (long int) (pipe->n_planes * sizeof(*suffle)));

	for (i = 0; i < pipe->n_planes; i++)
		suffle[i] = i;

	/*
	 * suffle table for planes. using rand() should keep it
	 * 'randomized in expected way'
	 */
	for (i = 0; i < 256; i++) {
		int n, m;
		int a, b;

		n = rand() % (pipe->n_planes-1);
		m = rand() % (pipe->n_planes-1);

		/*
		 * keep primary plane at its place for test's sake.
		 */
		if(n == primary->index || m == primary->index)
			continue;

		a = suffle[n];
		b = suffle[m];
		suffle[n] = b;
		suffle[m] = a;
	}

	mode = igt_output_get_mode(output);

	/* planes with random positions */
	x[primary->index] = 0;
	y[primary->index] = 0;
	for (i = 0; i < max_planes; i++) {
		/*
		 * Here is made assumption primary plane will have
		 * index zero.
		 */
		igt_plane_t *plane = igt_output_get_plane(output, suffle[i]);
		uint32_t plane_format;
		uint64_t plane_tiling;

		if (plane->type == DRM_PLANE_TYPE_PRIMARY)
			continue;
		else if (plane->type == DRM_PLANE_TYPE_CURSOR)
			size[i] = SIZE_CURSOR;
		else
			size[i] = SIZE_PLANE;

		x[i] = rand() % (mode->hdisplay - size[i]);
		y[i] = rand() % (mode->vdisplay - size[i]);

		data->plane[i] = plane;

		plane_format = data->plane[i]->type == DRM_PLANE_TYPE_CURSOR ? DRM_FORMAT_ARGB8888 : DRM_FORMAT_XRGB8888;
		plane_tiling = data->plane[i]->type == DRM_PLANE_TYPE_CURSOR ? LOCAL_DRM_FORMAT_MOD_NONE : tiling;

		igt_skip_on(!igt_plane_has_format_mod(plane, plane_format,
						      plane_tiling));

		igt_create_color_fb(data->drm_fd,
				    size[i], size[i],
				    plane_format,
				    plane_tiling,
				    color->red, color->green, color->blue,
				    &data->fb[i]);

		igt_plane_set_position(data->plane[i], x[i], y[i]);
		igt_plane_set_fb(data->plane[i], &data->fb[i]);
	}

	/* primary plane */
	data->plane[primary->index] = primary;
	create_fb_for_mode_position(data, output, mode, color, x, y,
				    size, size, tiling, max_planes);
	igt_plane_set_fb(data->plane[primary->index], &data->fb[primary->index]);
	free((void*)x);
	free((void*)y);
	free((void*)size);
	free((void*)suffle);
}

/*
 * Multiple plane position test.
 *   - We start by grabbing a reference CRC of a full blue fb being scanned
 *     out on the primary plane
 *   - Then we scannout number of planes:
 *      * the primary plane uses a blue fb with a black rectangle holes
 *      * planes, on top of the primary plane, with a blue fb that is set-up
 *        to cover the black rectangles of the primary plane
 *     The resulting CRC should be identical to the reference CRC
 */

static void
test_plane_position_with_output(data_t *data, enum pipe pipe,
				igt_output_t *output, int n_planes,
				uint64_t tiling)
{
	color_t blue  = { 0.0f, 0.0f, 1.0f };
	igt_crc_t crc;
	igt_plane_t *plane;
	int i;
	int err, c = 0;
	int iterations = opt.iterations < 1 ? 1 : opt.iterations;
	bool loop_forever;
	char info[256];

	if (opt.iterations == LOOP_FOREVER) {
		loop_forever = true;
		sprintf(info, "forever");
	} else {
		loop_forever = false;
		sprintf(info, "for %d %s",
			iterations, iterations > 1 ? "iterations" : "iteration");
	}

	test_init(data, pipe, n_planes);

	get_reference_crc(data, output, pipe, &blue, tiling);

	/* Find out how many planes are allowed simultaneously */
	do {
		c++;
		prepare_planes(data, pipe, &blue, tiling, c, output);
		err = igt_display_try_commit2(&data->display, COMMIT_ATOMIC);

		for_each_plane_on_pipe(&data->display, pipe, plane)
			igt_plane_set_fb(plane, NULL);

		igt_display_commit2(&data->display, COMMIT_ATOMIC);

		for (int x = 0; x < c; x++)
			igt_remove_fb(data->drm_fd, &data->fb[x]);
	} while (!err && c < n_planes);

	if (err)
		c--;

	igt_info("Testing connector %s using pipe %s with %d planes %s with seed %d\n",
		 igt_output_name(output), kmstest_pipe_name(pipe), c,
		 info, opt.seed);

	i = 0;
	while (i < iterations || loop_forever) {
		/* randomize planes and set up the holes */
		prepare_planes(data, pipe, &blue, tiling, c, output);

		igt_display_commit2(&data->display, COMMIT_ATOMIC);

		igt_pipe_crc_get_current(data->display.drm_fd, data->pipe_crc, &crc);

		for_each_plane_on_pipe(&data->display, pipe, plane)
			igt_plane_set_fb(plane, NULL);

		igt_display_commit2(&data->display, COMMIT_ATOMIC);

		for (int x = 0; x < c; x++)
			igt_remove_fb(data->drm_fd, &data->fb[x]);

		igt_assert_crc_equal(&data->ref_crc, &crc);

		i++;
	}

	test_fini(data, output, n_planes);
}

static void
test_plane_position(data_t *data, enum pipe pipe, uint64_t tiling)
{
	igt_output_t *output;
	int n_planes = data->display.pipes[pipe].n_planes;

	output = igt_get_single_output_for_pipe(&data->display, pipe);
	igt_require(output);

	if (!opt.user_seed)
		opt.seed = time(NULL);

	srand(opt.seed);

	test_plane_position_with_output(data, pipe, output,
					n_planes, tiling);
}

static void
run_tests_for_pipe(data_t *data, enum pipe pipe)
{
	igt_fixture {
		igt_skip_on(pipe >= data->display.n_pipes);
		igt_require(data->display.pipes[pipe].n_planes > 0);
	}

	igt_subtest_f("atomic-pipe-%s-tiling-x", kmstest_pipe_name(pipe))
		test_plane_position(data, pipe, LOCAL_I915_FORMAT_MOD_X_TILED);

	igt_subtest_f("atomic-pipe-%s-tiling-y", kmstest_pipe_name(pipe))
		test_plane_position(data, pipe, LOCAL_I915_FORMAT_MOD_Y_TILED);

	igt_subtest_f("atomic-pipe-%s-tiling-yf", kmstest_pipe_name(pipe))
		test_plane_position(data, pipe, LOCAL_I915_FORMAT_MOD_Yf_TILED);

	igt_subtest_f("atomic-pipe-%s-tiling-none", kmstest_pipe_name(pipe))
		test_plane_position(data, pipe, LOCAL_DRM_FORMAT_MOD_NONE);
}

static data_t data;

static int opt_handler(int option, int option_index, void *input)
{
	switch (option) {
	case 'i':
		opt.iterations = strtol(optarg, NULL, 0);

		if (opt.iterations < LOOP_FOREVER || opt.iterations == 0) {
			igt_info("incorrect number of iterations: %d\n", opt.iterations);
			return IGT_OPT_HANDLER_ERROR;
		}

		break;
	case 's':
		opt.user_seed = true;
		opt.seed = strtol(optarg, NULL, 0);
		break;
	default:
		return IGT_OPT_HANDLER_ERROR;
	}

	return IGT_OPT_HANDLER_SUCCESS;
}

const char *help_str =
	"  --iterations Number of iterations for test coverage. -1 loop forever, default 64 iterations\n"
	"  --seed       Seed for random number generator\n";

struct option long_options[] = {
	{ "iterations", required_argument, NULL, 'i'},
	{ "seed",    required_argument, NULL, 's'},
	{ 0, 0, 0, 0 }
};

igt_main_args("", long_options, help_str, opt_handler, NULL)
{
	enum pipe pipe;

	igt_skip_on_simulation();

	igt_fixture {
		data.drm_fd = drm_open_driver_master(DRIVER_INTEL | DRIVER_AMDGPU);
		kmstest_set_vt_graphics_mode();
		igt_require_pipe_crc(data.drm_fd);
		igt_display_require(&data.display, data.drm_fd);
		igt_require(data.display.is_atomic);
	}

	for_each_pipe_static(pipe) {
		igt_describe("Check that the kernel handles atomic updates of "
			     "multiple planes correctly by changing their "
			     "geometry and making sure the changes are "
			     "reflected immediately after each commit.");
		igt_subtest_group
			run_tests_for_pipe(&data, pipe);
	}

	igt_fixture {
		igt_display_fini(&data.display);
	}
}
