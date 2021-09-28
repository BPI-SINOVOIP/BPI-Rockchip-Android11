/*
 * Copyright © 2014 Intel Corporation
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
 *   Ander Conselvan de Oliveira <ander.conselvan.de.oliveira@intel.com>
 */

#include "igt.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

IGT_TEST_DESCRIPTION("Test page flips and tiling scenarios");

typedef struct {
	int drm_fd;
	igt_display_t display;
	int gen;
} data_t;

static igt_pipe_crc_t *_pipe_crc;

static igt_pipe_crc_t *pipe_crc_new(data_t *data, int pipe)
{
	if (_pipe_crc) {
		igt_pipe_crc_free(_pipe_crc);
		_pipe_crc = NULL;
	}

	_pipe_crc = igt_pipe_crc_new(data->drm_fd, pipe, INTEL_PIPE_CRC_SOURCE_AUTO);
	igt_assert(_pipe_crc);

	return _pipe_crc;
}

static void pipe_crc_free(void)
{
	if (_pipe_crc) {
		igt_pipe_crc_free(_pipe_crc);
		_pipe_crc = NULL;
	}
}

static void
test_flip_tiling(data_t *data, enum pipe pipe, igt_output_t *output, uint64_t tiling[2])
{
	drmModeModeInfo *mode;
	igt_plane_t *primary;
	struct igt_fb fb[2];
	igt_pipe_crc_t *pipe_crc;
	igt_crc_t reference_crc, crc;
	int fb_id, ret, width;

	pipe_crc = pipe_crc_new(data, pipe);
	igt_output_set_pipe(output, pipe);

	mode = igt_output_get_mode(output);

	/* Interlaced modes don't support Y/Yf tiling */
	if (tiling[0] == LOCAL_I915_FORMAT_MOD_Y_TILED ||
	    tiling[0] == LOCAL_I915_FORMAT_MOD_Yf_TILED ||
	    tiling[1] == LOCAL_I915_FORMAT_MOD_Y_TILED ||
	    tiling[1] == LOCAL_I915_FORMAT_MOD_Yf_TILED)
		igt_require(!(mode->flags & DRM_MODE_FLAG_INTERLACE));

	primary = igt_output_get_plane(output, 0);

	width = mode->hdisplay;

	if (tiling[0] != tiling[1] &&
	    (tiling[0] != LOCAL_DRM_FORMAT_MOD_NONE ||
	     tiling[1] != LOCAL_DRM_FORMAT_MOD_NONE)) {
		/*
		 * Since a page flip to a buffer with different stride
		 * doesn't work, choose width so that the stride of both
		 * buffers is the same.
		 */
		width = 512;
		while (width < mode->hdisplay)
			width *= 2;
	}

	fb_id = igt_create_pattern_fb(data->drm_fd, width, mode->vdisplay,
				      DRM_FORMAT_XRGB8888, tiling[0],
				      &fb[0]);
	igt_assert(fb_id);

	/* Second fb has different background so CRC does not match. */
	fb_id = igt_create_color_pattern_fb(data->drm_fd, width, mode->vdisplay,
				      DRM_FORMAT_XRGB8888, tiling[1],
				      0.5, 0.5, 0.5, &fb[1]);
	igt_assert(fb_id);

	/* Set the crtc and generate a reference CRC. */
	igt_plane_set_fb(primary, &fb[1]);
	igt_display_commit(&data->display);
	igt_pipe_crc_collect_crc(pipe_crc, &reference_crc);

	/* Commit the first fb. */
	igt_plane_set_fb(primary, &fb[0]);
	igt_display_commit(&data->display);

	/* Flip to the second fb. */
	ret = drmModePageFlip(data->drm_fd, output->config.crtc->crtc_id,
			      fb[1].fb_id, DRM_MODE_PAGE_FLIP_EVENT, NULL);
	/*
	 * Page flip should work but some transitions may be temporarily
	 * on some kernels.
	 */
	igt_require(ret == 0);

	kmstest_wait_for_pageflip(data->drm_fd);

	/* Get a crc and compare with the reference. */
	igt_pipe_crc_collect_crc(pipe_crc, &crc);
	igt_assert_crc_equal(&reference_crc, &crc);

	/* Clean up. */
	igt_plane_set_fb(primary, NULL);
	pipe_crc_free();
	igt_output_set_pipe(output, PIPE_ANY);
	igt_display_commit(&data->display);

	igt_remove_fb(data->drm_fd, &fb[0]);
	igt_remove_fb(data->drm_fd, &fb[1]);
}

static data_t data;
igt_output_t *output;

igt_main
{
	igt_skip_on_simulation();

	igt_fixture {
		data.drm_fd = drm_open_driver_master(DRIVER_INTEL);
		data.gen = intel_gen(intel_get_drm_devid(data.drm_fd));

		kmstest_set_vt_graphics_mode();

		igt_require_pipe_crc(data.drm_fd);
		igt_display_require(&data.display, data.drm_fd);
	}

	/*
	 * Test that a page flip from a tiled buffer to a linear one works
	 * correctly. First, it sets the crtc with the linear buffer and
	 * generates a reference crc for the pipe. Then, the crtc is set with
	 * the tiled one and page flip to the linear one issued. A new crc is
	 * generated and compared to the reference one.
	 */

	igt_subtest_f("flip-changes-tiling") {
		uint64_t tiling[2] = { LOCAL_I915_FORMAT_MOD_X_TILED,
				       LOCAL_DRM_FORMAT_MOD_NONE };
		enum pipe pipe;

		for_each_pipe_with_valid_output(&data.display, pipe, output)
			test_flip_tiling(&data, pipe, output, tiling);
	}

	igt_subtest_f("flip-changes-tiling-Y") {
		uint64_t tiling[2] = { LOCAL_I915_FORMAT_MOD_Y_TILED,
				       LOCAL_DRM_FORMAT_MOD_NONE };
		enum pipe pipe;

		igt_require_fb_modifiers(data.drm_fd);
		igt_require(data.gen >= 9);

		for_each_pipe_with_valid_output(&data.display, pipe, output)
			test_flip_tiling(&data, pipe, output, tiling);
	}

	igt_subtest_f("flip-changes-tiling-Yf") {
		uint64_t tiling[2] = { LOCAL_I915_FORMAT_MOD_Yf_TILED,
				       LOCAL_DRM_FORMAT_MOD_NONE };
		enum pipe pipe;

		igt_require_fb_modifiers(data.drm_fd);
		igt_require(data.gen >= 9);

		for_each_pipe_with_valid_output(&data.display, pipe, output)
			test_flip_tiling(&data, pipe, output, tiling);
	}

	/*
	 * Test that a page flip from a tiled buffer to another tiled one works
	 * correctly. First, it sets the crtc with the tiled buffer and
	 * generates a reference crc for the pipe. Then a page flip to second
	 * tiled buffer is issued. A new crc is generated and compared to the
	 * reference one.
	 */

	igt_subtest_f("flip-X-tiled") {
		uint64_t tiling[2] = { LOCAL_I915_FORMAT_MOD_X_TILED,
				       LOCAL_I915_FORMAT_MOD_X_TILED };
		enum pipe pipe;

		for_each_pipe_with_valid_output(&data.display, pipe, output)
			test_flip_tiling(&data, pipe, output, tiling);
	}

	igt_subtest_f("flip-Y-tiled") {
		uint64_t tiling[2] = { LOCAL_I915_FORMAT_MOD_Y_TILED,
				       LOCAL_I915_FORMAT_MOD_Y_TILED };
		enum pipe pipe;

		igt_require_fb_modifiers(data.drm_fd);
		igt_require(data.gen >= 9);

		for_each_pipe_with_valid_output(&data.display, pipe, output)
			test_flip_tiling(&data, pipe, output, tiling);
	}

	igt_subtest_f("flip-Yf-tiled") {
		uint64_t tiling[2] = { LOCAL_I915_FORMAT_MOD_Yf_TILED,
				       LOCAL_I915_FORMAT_MOD_Yf_TILED };
		enum pipe pipe;

		igt_require_fb_modifiers(data.drm_fd);
		igt_require(data.gen >= 9);

		for_each_pipe_with_valid_output(&data.display, pipe, output)
			test_flip_tiling(&data, pipe, output, tiling);
	}

	/*
	 * Test that a page flip from a linear buffer to a tiled one works
	 * correctly. First, it sets the crtc with the linear buffer and
	 * generates a reference crc for the pipe. Then a page flip to a tiled
	 * buffer is issued. A new crc is generated and compared to the
	 * reference one.
	 */

	igt_subtest_f("flip-to-X-tiled") {
		uint64_t tiling[2] = { LOCAL_DRM_FORMAT_MOD_NONE,
				       LOCAL_I915_FORMAT_MOD_X_TILED };
		enum pipe pipe;

		for_each_pipe_with_valid_output(&data.display, pipe, output)
			test_flip_tiling(&data, pipe, output, tiling);
	}

	igt_subtest_f("flip-to-Y-tiled") {
		uint64_t tiling[2] = { LOCAL_DRM_FORMAT_MOD_NONE,
				       LOCAL_I915_FORMAT_MOD_Y_TILED };
		enum pipe pipe;

		igt_require_fb_modifiers(data.drm_fd);
		igt_require(data.gen >= 9);

		for_each_pipe_with_valid_output(&data.display, pipe, output)
			test_flip_tiling(&data, pipe, output, tiling);
	}

	igt_subtest_f("flip-to-Yf-tiled") {
		uint64_t tiling[2] = { LOCAL_DRM_FORMAT_MOD_NONE,
				       LOCAL_I915_FORMAT_MOD_Yf_TILED };
		enum pipe pipe;

		igt_require_fb_modifiers(data.drm_fd);
		igt_require(data.gen >= 9);

		for_each_pipe_with_valid_output(&data.display, pipe, output)
			test_flip_tiling(&data, pipe, output, tiling);
	}

	igt_fixture {
		igt_display_fini(&data.display);
	}
}
