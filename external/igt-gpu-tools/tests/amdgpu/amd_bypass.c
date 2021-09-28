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
 *
 */
#include "igt.h"

/*
 * internal use
 * Common test data
 * */
typedef struct {
	int drm_fd;
	int width;
	int height;
	enum pipe pipe_id;
	igt_display_t display;
	igt_plane_t *primary;
	igt_output_t *output;
	igt_pipe_t *pipe;
	igt_pipe_crc_t *pipe_crc;
	igt_crc_t crc_fb;
	igt_crc_t crc_dprx;
	drmModeModeInfo *mode;
} data_t;

enum pattern {
	TEST_PATTERN_DP_COLOR_RAMP,
	TEST_PATTERN_DP_BLACK_WHITE_VERT_LINES,
	TEST_PATTERN_DP_BLACK_WHITE_HORZ_LINES,
	TEST_PATTERN_DP_COLOR_SQUARES_VESA,
	/* please don't add pattern after the below line */
	TEST_PATTERN_MAX,
};

const char *ptnstr[TEST_PATTERN_MAX] = {
	"DP Color Ramp",
	"DP Vertical Lines",
	"DP Horizontal Lines",
	"DP Color Squares VESA"
};

/* Common test setup. */
static void test_init(data_t *data)
{
	igt_display_t *display = &data->display;

	/* It doesn't matter which pipe we choose on amdpgu. */
	data->pipe_id = PIPE_A;
	data->pipe = &data->display.pipes[data->pipe_id];

	igt_display_reset(display);

	data->output = igt_get_single_output_for_pipe(display, data->pipe_id);
	igt_assert(data->output);

	data->mode = igt_output_get_mode(data->output);
	igt_assert(data->mode);

	data->primary =
		igt_pipe_get_plane_type(data->pipe, DRM_PLANE_TYPE_PRIMARY);

	data->pipe_crc = igt_pipe_crc_new(data->drm_fd, data->pipe_id,
					  AMDGPU_PIPE_CRC_SOURCE_DPRX);

	igt_output_set_pipe(data->output, data->pipe_id);

	data->width = data->mode->hdisplay;
	data->height = data->mode->vdisplay;
}

/* Common test cleanup. */
static void test_fini(data_t *data)
{
	igt_pipe_crc_free(data->pipe_crc);
	igt_display_reset(&data->display);
}

/*
 * draw the DP color ramp test pattern
 * Reference: DP Link CTS 1.2 Core r1.1, sec. 3.1.5.1
 */
static void draw_dp_test_pattern_color_ramp(igt_fb_t *fb)
{
	const int h = 64;  /* test pattern rectangle height */
	const int block_h = h * 4; /* block height of R-G-B-White rectangles */
	void *ptr_fb;
	uint8_t *data;
	int x,y;
	int i;
	uint8_t val;

	/*
	 * 64-by-256 pixels per rectangle
	 * R-G-B-White rectangle in order in vertical
	 * duplicate in horizontal
	 */
	ptr_fb = igt_fb_map_buffer(fb->fd, fb);
	igt_assert(ptr_fb);
	data = ptr_fb + fb->offsets[0];

	switch (fb->drm_format) {
	case DRM_FORMAT_XRGB8888:
		for (y = 0; y < fb->height; ++y) {
			for (x = 0, val = 0; x < fb->width; ++x, ++val) {
				i = x * 4 + y * fb->strides[0];

				/* vertical R-G-B-White rect */
				if ((y % block_h) < h) {	    /* Red */
					data[i + 2] = val;
					data[i + 1] = 0;
					data[i + 0] = 0;
				} else if ((y % block_h) < 2 * h) { /* Green */
					data[i + 2] = 0;
					data[i + 1] = val;
					data[i + 0] = 0;
				} else if ((y % block_h) < 3 * h) { /* Blue */
					data[i + 2] = 0;
					data[i + 1] = 0;
					data[i + 0] = val;
				} else {			    /* White */
					data[i + 2] = val;
					data[i + 1] = val;
					data[i + 0] = val;
				}
			}
		}
		break;
	default:
		igt_assert_f(0, "DRM Format Invalid");
		break;
	}

	igt_fb_unmap_buffer(fb, ptr_fb);
}

/*
 * draw the DP vertical lines test pattern
 * Reference: DP Link CTS 1.2 Core r1.1, sec. 3.1.5.2
 */
static void draw_dp_test_pattern_vert_lines(igt_fb_t *fb)
{
	void *ptr_fb;
	uint8_t *data;
	int x, y;
	int i;

	/* alternating black and white lines, 1 pixel wide */
	ptr_fb = igt_fb_map_buffer(fb->fd, fb);
	igt_assert(ptr_fb);
	data = ptr_fb + fb->offsets[0];

	switch (fb->drm_format) {
	case DRM_FORMAT_XRGB8888:
		for (y = 0; y < fb->height; ++y) {
			for (x = 0; x < fb->width; ++x) {
				i = x * 4 + y * fb->strides[0];

				if ((x & 1) == 0) {
					data[i + 2] = 0xff; /* R */
					data[i + 1] = 0xff; /* G */
					data[i + 0] = 0xff; /* B */
				} else {
					data[i + 2] = 0;
					data[i + 1] = 0;
					data[i + 0] = 0;
				}
			}
		}
		break;
	default:
		igt_assert_f(0, "DRM Format Invalid");
		break;
	}

	igt_fb_unmap_buffer(fb, ptr_fb);
}

/* draw the DP horizontal lines test pattern */
static void draw_dp_test_pattern_horz_lines(igt_fb_t *fb)
{
	void *ptr_fb;
	uint8_t *data;
	int x, y;
	int i;

	/* alternating black and white horizontal lines, 1 pixel high */
	ptr_fb = igt_fb_map_buffer(fb->fd, fb);
	igt_assert(ptr_fb);
	data = ptr_fb + fb->offsets[0];

	switch (fb->drm_format) {
	case DRM_FORMAT_XRGB8888:
		for (y = 0; y < fb->height; ++y) {
			for (x = 0; x < fb->width; ++x) {

				i = x * 4 + y * fb->strides[0];

				if ((y & 1) == 0) {
					data[i + 2] = 0xff; /* R */
					data[i + 1] = 0xff; /* G */
					data[i + 0] = 0xff; /* B */
				} else {
					data[i + 2] = 0;
					data[i + 1] = 0;
					data[i + 0] = 0;
				}
			}
		}
		break;
	default:
		igt_assert_f(0, "DRM Format Invalid");
		break;
	}

	igt_fb_unmap_buffer(fb, ptr_fb);
}

/*
 * draw the DP color squares VESA test pattern
 * Reference: DP Link CTS 1.2 Core r1.1, sec. 3.1.5.3
 */
static void draw_dp_test_pattern_color_squares_vesa(igt_fb_t *fb)
{
	const int h = 64; /* test pattern square height/width */
	const int block_h = h * 2; /* block height of the repetition pattern */
	const int block_w = h * 8; /* block width of the repetition pattern */
	const uint8_t rgb[3][2][8] = {
		{/* Red table of the pattern squares */
			{255, 255, 0, 0, 255, 255, 0, 0},
			{0, 255, 255, 0, 0, 255, 255, 0},
		},
		{/* Green table */
			{255, 255, 255, 255, 0, 0, 0, 0},
			{0, 0, 0, 255, 255, 255, 255, 0},
		},
		{/* Blue table */
			{255, 0, 255, 0, 255, 0, 255, 0},
			{255, 0, 255, 0, 255, 0, 255, 0},
		},
	};

	void *ptr_fb;
	uint8_t *data;
	int x, y;
	int i, j, k;

	ptr_fb = igt_fb_map_buffer(fb->fd, fb);
	igt_assert(ptr_fb);
	data = ptr_fb + fb->offsets[0];

	switch (fb->drm_format) {
	case DRM_FORMAT_XRGB8888:
		for (y = 0; y < fb->height; ++y) {
			for (x = 0; x < fb->width; ++x) {

				i = x * 4 + y * fb->strides[0];
				j = (y % block_h) / h;
				k = (x % block_w) / h;

				data[i + 2] = rgb[0][j][k]; /* R */
				data[i + 1] = rgb[1][j][k]; /* G */
				data[i + 0] = rgb[2][j][k]; /* B */
			}
		}
		break;

	default:
		igt_assert_f(0, "DRM Format Invalid");
		break;
	}


	igt_fb_unmap_buffer(fb, ptr_fb);
}

/* generate test pattern and fills a FB */
static void generate_test_pattern(igt_fb_t *fb, data_t *data, enum pattern ptn)
{
	igt_assert(fb->fd && (ptn < TEST_PATTERN_MAX));

	if (ptn == TEST_PATTERN_DP_COLOR_RAMP) {
		draw_dp_test_pattern_color_ramp(fb);
	} else if (ptn == TEST_PATTERN_DP_BLACK_WHITE_VERT_LINES) {
		draw_dp_test_pattern_vert_lines(fb);
	} else if (ptn == TEST_PATTERN_DP_BLACK_WHITE_HORZ_LINES) {
		draw_dp_test_pattern_horz_lines(fb);
	} else if (ptn == TEST_PATTERN_DP_COLOR_SQUARES_VESA) {
		draw_dp_test_pattern_color_squares_vesa(fb);
	}
}

static void bypass_8bpc_test(data_t *data)
{
	igt_display_t *display = &data->display;
	igt_fb_t fb;
	enum pattern ptn;

	test_init(data);

	igt_create_fb(data->drm_fd, data->width, data->height,
		      DRM_FORMAT_XRGB8888, LOCAL_DRM_FORMAT_MOD_NONE, &fb);

	/*
	 * Settings:
	 *   no degamma
	 *   no regamma
	 *   no CTM
	 */
	igt_pipe_obj_replace_prop_blob(data->pipe, IGT_CRTC_DEGAMMA_LUT, NULL, 0);
	igt_pipe_obj_replace_prop_blob(data->pipe, IGT_CRTC_GAMMA_LUT, NULL, 0);
	igt_pipe_obj_replace_prop_blob(data->pipe, IGT_CRTC_CTM, NULL, 0);

	igt_plane_set_fb(data->primary, &fb);
	igt_display_commit_atomic(display, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);

	/* traverse all the test pattern to validate 8bpc bypass mode */
	for (ptn = TEST_PATTERN_DP_COLOR_RAMP; ptn < TEST_PATTERN_MAX; ++ptn) {
		igt_info("Test Pattern: %s\n", ptnstr[ptn]);

		generate_test_pattern(&fb, data, ptn);

		/* Grab FB and DPRX CRCs and compare */
		igt_fb_calc_crc(&fb, &data->crc_fb);
		igt_pipe_crc_collect_crc(data->pipe_crc, &data->crc_dprx);

		igt_assert_crc_equal(&data->crc_fb, &data->crc_dprx);
	}

	igt_plane_set_fb(data->primary, NULL);
	test_fini(data);
	igt_remove_fb(data->drm_fd, &fb);
}

igt_main
{
	data_t data;
	memset(&data, 0, sizeof(data));

	igt_skip_on_simulation();

	igt_fixture {
		data.drm_fd = drm_open_driver_master(DRIVER_AMDGPU);
		if (data.drm_fd == -1)
			igt_skip("Not an amdgpu driver.\n");
		igt_require_pipe_crc(data.drm_fd);

		kmstest_set_vt_graphics_mode();

		igt_display_require(&data.display, data.drm_fd);
		igt_require(data.display.is_atomic);
		igt_display_require_output(&data.display);
	}

	igt_subtest("8bpc-bypass-mode")
		bypass_8bpc_test(&data);

	igt_fixture {
		igt_display_fini(&data.display);
	}
}
