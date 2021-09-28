/*
 * Copyright © 2019 Intel Corporation
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

#include "igt.h"
#include "igt_vgem.h"

#include <sys/ioctl.h>
#include <sys/poll.h>
#include <time.h>

struct crc_info {
	igt_crc_t crc;
	char *str;
	const char *name;
};

static struct {
	double r, g, b;
	uint32_t color;
	struct crc_info prime_crc, direct_crc;
} colors[3] = {
	{ .r = 0.0, .g = 0.0, .b = 0.0, .color = 0xff000000 },
	{ .r = 1.0, .g = 1.0, .b = 1.0, .color = 0xffffffff },
	{ .r = 1.0, .g = 0.0, .b = 0.0, .color = 0xffff0000 },
};

IGT_TEST_DESCRIPTION("Prime tests, focusing on KMS side");

static bool has_prime_import(int fd)
{
	uint64_t value;

	if (drmGetCap(fd, DRM_CAP_PRIME, &value))
		return false;

	return value & DRM_PRIME_CAP_IMPORT;
}

static bool has_prime_export(int fd)
{
	uint64_t value;

	if (drmGetCap(fd, DRM_CAP_PRIME, &value))
		return false;

	return value & DRM_PRIME_CAP_EXPORT;
}

static igt_output_t *setup_display(int importer_fd, igt_display_t *display,
				   enum pipe pipe)
{
	igt_display_require(display, importer_fd);
	igt_skip_on(pipe >= display->n_pipes);
	igt_output_t *output = igt_get_single_output_for_pipe(display, pipe);

	igt_require_f(output, "No connector found for pipe %s\n",
		      kmstest_pipe_name(pipe));

	igt_display_reset(display);
	igt_output_set_pipe(output, pipe);
	return output;
}

static void prepare_scratch(int exporter_fd, struct vgem_bo *scratch,
			    drmModeModeInfo *mode, uint32_t color)
{
	uint32_t *ptr;

	scratch->width = mode->hdisplay;
	scratch->height = mode->vdisplay;
	scratch->bpp = 32;
	vgem_create(exporter_fd, scratch);

	ptr = vgem_mmap(exporter_fd, scratch, PROT_WRITE);
	for (size_t idx = 0; idx < scratch->size / sizeof(*ptr); ++idx)
		ptr[idx] = color;

	munmap(ptr, scratch->size);
}

static void prepare_fb(int importer_fd, struct vgem_bo *scratch, struct igt_fb *fb)
{
	enum igt_color_encoding color_encoding = IGT_COLOR_YCBCR_BT709;
	enum igt_color_range color_range = IGT_COLOR_YCBCR_LIMITED_RANGE;

	igt_init_fb(fb, importer_fd, scratch->width, scratch->height,
		    DRM_FORMAT_XRGB8888, LOCAL_DRM_FORMAT_MOD_NONE,
		    color_encoding, color_range);
}

static void import_fb(int importer_fd, struct igt_fb *fb,
		      int dmabuf_fd, uint32_t pitch)
{
	uint32_t offsets[4] = {}, pitches[4] = {}, handles[4] = {};
	int ret;

	fb->gem_handle = prime_fd_to_handle(importer_fd, dmabuf_fd);

	handles[0] = fb->gem_handle;
	pitches[0] = pitch;
	offsets[0] = 0;

	ret = drmModeAddFB2(importer_fd, fb->width, fb->height,
			    DRM_FORMAT_XRGB8888,
			    handles, pitches, offsets,
			    &fb->fb_id, 0);
	igt_assert(ret == 0);
}

static void set_fb(struct igt_fb *fb,
		   igt_display_t *display,
		   igt_output_t *output)
{
	igt_plane_t *primary;
	int ret;

	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);
	igt_assert(primary);

	igt_plane_set_fb(primary, fb);
	ret = igt_display_commit(display);

	igt_assert(ret == 0);
}

static void collect_crc_for_fb(int importer_fd, struct igt_fb *fb, igt_display_t *display,
			       igt_output_t *output, igt_pipe_crc_t *pipe_crc,
			       uint32_t color, struct crc_info *info)
{
	set_fb(fb, display, output);
	igt_pipe_crc_collect_crc(pipe_crc, &info->crc);
	info->str = igt_crc_to_string(&info->crc);
	igt_debug("CRC through '%s' method for %#08x is %s\n",
		  info->name, color, info->str);
	igt_remove_fb(importer_fd, fb);
}

static void test_crc(int exporter_fd, int importer_fd)
{
	igt_display_t display;
	igt_output_t *output;
	igt_pipe_crc_t *pipe_crc;
	enum pipe pipe = PIPE_A;
	struct igt_fb fb;
	int dmabuf_fd;
	struct vgem_bo scratch = {}; /* despite the name, it suits for any
				      * gem-compatible device
				      * TODO: rename
				      */
	int i, j;
	drmModeModeInfo *mode;

	bool crc_equal = false;

	output = setup_display(importer_fd, &display, pipe);

	mode = igt_output_get_mode(output);
	pipe_crc = igt_pipe_crc_new(importer_fd, pipe, INTEL_PIPE_CRC_SOURCE_AUTO);

	for (i = 0; i < ARRAY_SIZE(colors); i++) {
		prepare_scratch(exporter_fd, &scratch, mode, colors[i].color);
		dmabuf_fd = prime_handle_to_fd(exporter_fd, scratch.handle);
		gem_close(exporter_fd, scratch.handle);

		prepare_fb(importer_fd, &scratch, &fb);
		import_fb(importer_fd, &fb, dmabuf_fd, scratch.pitch);
		close(dmabuf_fd);

		colors[i].prime_crc.name = "prime";
		collect_crc_for_fb(importer_fd, &fb, &display, output,
				   pipe_crc, colors[i].color, &colors[i].prime_crc);

		igt_create_color_fb(importer_fd,
				    mode->hdisplay, mode->vdisplay,
				    DRM_FORMAT_XRGB8888, LOCAL_DRM_FORMAT_MOD_NONE,
				    colors[i].r, colors[i].g, colors[i].b,
				    &fb);

		colors[i].direct_crc.name = "direct";
		collect_crc_for_fb(importer_fd, &fb, &display, output,
				   pipe_crc, colors[i].color, &colors[i].direct_crc);
	}
	igt_pipe_crc_free(pipe_crc);

	igt_debug("CRC table:\n");
	igt_debug("Color\t\tPrime\t\tDirect\n");
	for (i = 0; i < ARRAY_SIZE(colors); i++) {
		igt_debug("%#08x\t%.8s\t%.8s\n", colors[i].color,
			  colors[i].prime_crc.str, colors[i].direct_crc.str);
		free(colors[i].prime_crc.str);
		free(colors[i].direct_crc.str);
	}

	for (i = 0; i < ARRAY_SIZE(colors); i++) {
		for (j = 0; j < ARRAY_SIZE(colors); j++) {
			if (i == j) {
				igt_assert_crc_equal(&colors[i].prime_crc.crc,
						     &colors[j].direct_crc.crc);
				continue;
			}
			crc_equal = igt_check_crc_equal(&colors[i].prime_crc.crc,
							&colors[j].direct_crc.crc);
			igt_assert_f(!crc_equal, "CRC should be different");
		}
	}
	igt_display_fini(&display);
}

static void run_test_crc(int export_chipset, int import_chipset)
{
	int importer_fd = -1;
	int exporter_fd = -1;

	exporter_fd = drm_open_driver(export_chipset);
	importer_fd = drm_open_driver_master(import_chipset);

	igt_require(has_prime_export(exporter_fd));
	igt_require(has_prime_import(importer_fd));
	igt_require_pipe_crc(importer_fd);

	test_crc(exporter_fd, importer_fd);
	close(importer_fd);
	close(exporter_fd);
}

igt_main
{
	igt_fixture {
		kmstest_set_vt_graphics_mode();
	}
	igt_describe("Make a dumb buffer inside vgem, fill it, export to another device and compare the CRC");
	igt_subtest("basic-crc")
		run_test_crc(DRIVER_VGEM, DRIVER_ANY);
}
