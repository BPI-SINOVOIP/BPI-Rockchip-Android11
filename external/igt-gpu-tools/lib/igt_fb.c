/*
 * Copyright © 2013,2014 Intel Corporation
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
 * 	Daniel Vetter <daniel.vetter@ffwll.ch>
 * 	Damien Lespiau <damien.lespiau@intel.com>
 */

#include <stdio.h>
#include <math.h>
#include <wchar.h>
#include <inttypes.h>

#if defined(USE_CAIRO_PIXMAN)
#include <pixman.h>
#endif

#include "drmtest.h"
#include "igt_aux.h"
#include "igt_color_encoding.h"
#include "igt_fb.h"
#include "igt_halffloat.h"
#include "igt_kms.h"
#include "igt_matrix.h"
#if defined(USE_VC4)
#include "igt_vc4.h"
#endif
#include "igt_amd.h"
#include "igt_x86.h"
#include "ioctl_wrappers.h"
#include "intel_batchbuffer.h"
#include "intel_chipset.h"
#include "i915/gem_mman.h"

/**
 * SECTION:igt_fb
 * @short_description: Framebuffer handling and drawing library
 * @title: Framebuffer
 * @include: igt.h
 *
 * This library contains helper functions for handling kms framebuffer objects
 * using #igt_fb structures to track all the metadata.  igt_create_fb() creates
 * a basic framebuffer and igt_remove_fb() cleans everything up again.
 *
 * It also supports drawing using the cairo library and provides some simplified
 * helper functions to easily draw test patterns. The main function to create a
 * cairo drawing context for a framebuffer object is igt_get_cairo_ctx().
 *
 * Finally it also pulls in the drm fourcc headers and provides some helper
 * functions to work with these pixel format codes.
 */

#if defined(USE_CAIRO_PIXMAN)
#define PIXMAN_invalid	0

#if CAIRO_VERSION < CAIRO_VERSION_ENCODE(1, 17, 2)
/*
 * We need cairo 1.17.2 to use HDR formats, but the only thing added is a value
 * to cairo_format_t.
 *
 * To prevent going outside the enum, make cairo_format_t an int and define
 * ourselves.
 */

#define CAIRO_FORMAT_RGB96F (6)
#define CAIRO_FORMAT_RGBA128F (7)
#define cairo_format_t int
#endif
#endif /*defined(USE_CAIRO_PIXMAN)*/

/* drm fourcc/cairo format maps */
static const struct format_desc_struct {
	const char *name;
	uint32_t drm_id;
#if defined(USE_CAIRO_PIXMAN)
	cairo_format_t cairo_id;
	pixman_format_code_t pixman_id;
#endif
	int depth;
	int num_planes;
	int plane_bpp[4];
	uint8_t hsub;
	uint8_t vsub;
} format_desc[] = {
	{ .name = "ARGB1555", .depth = -1, .drm_id = DRM_FORMAT_ARGB1555,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_INVALID,
	  .pixman_id = PIXMAN_a1r5g5b5,
#endif
	  .num_planes = 1, .plane_bpp = { 16, },
	  .hsub = 1, .vsub = 1,
	},
	{ .name = "C8", .depth = -1, .drm_id = DRM_FORMAT_C8,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_INVALID,
	  .pixman_id = PIXMAN_r3g3b2,
#endif
	  .num_planes = 1, .plane_bpp = { 8, },
	  .hsub = 1, .vsub = 1,
	},
	{ .name = "XRGB1555", .depth = -1, .drm_id = DRM_FORMAT_XRGB1555,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_INVALID,
	  .pixman_id = PIXMAN_x1r5g5b5,
#endif
	  .num_planes = 1, .plane_bpp = { 16, },
	  .hsub = 1, .vsub = 1,
	},
	{ .name = "RGB565", .depth = 16, .drm_id = DRM_FORMAT_RGB565,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB16_565,
	  .pixman_id = PIXMAN_r5g6b5,
#endif
	  .num_planes = 1, .plane_bpp = { 16, },
	  .hsub = 1, .vsub = 1,
	},
	{ .name = "BGR565", .depth = -1, .drm_id = DRM_FORMAT_BGR565,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_INVALID,
	  .pixman_id = PIXMAN_b5g6r5,
#endif
	  .num_planes = 1, .plane_bpp = { 16, },
	  .hsub = 1, .vsub = 1,
	},
	{ .name = "BGR888", .depth = -1, .drm_id = DRM_FORMAT_BGR888,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_INVALID,
	  .pixman_id = PIXMAN_b8g8r8,
#endif
	  .num_planes = 1, .plane_bpp = { 24, },
	  .hsub = 1, .vsub = 1,
	},
	{ .name = "RGB888", .depth = -1, .drm_id = DRM_FORMAT_RGB888,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_INVALID,
	  .pixman_id = PIXMAN_r8g8b8,
#endif
	  .num_planes = 1, .plane_bpp = { 24, },
	  .hsub = 1, .vsub = 1,
	},
	{ .name = "XYUV8888", .depth = -1, .drm_id = DRM_FORMAT_XYUV8888,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB24,
	  .num_planes = 1, .plane_bpp = { 32, },
#endif
	  .hsub = 1, .vsub = 1,
	},
	{ .name = "XRGB8888", .depth = 24, .drm_id = DRM_FORMAT_XRGB8888,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB24,
	  .pixman_id = PIXMAN_x8r8g8b8,
#endif
	  .num_planes = 1, .plane_bpp = { 32, },
	  .hsub = 1, .vsub = 1,
	},
	{ .name = "XBGR8888", .depth = -1, .drm_id = DRM_FORMAT_XBGR8888,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_INVALID,
	  .pixman_id = PIXMAN_x8b8g8r8,
#endif
	  .num_planes = 1, .plane_bpp = { 32, },
	  .hsub = 1, .vsub = 1,
	},
	{ .name = "XRGB2101010", .depth = 30, .drm_id = DRM_FORMAT_XRGB2101010,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB30,
	  .pixman_id = PIXMAN_x2r10g10b10,
#endif
	  .num_planes = 1, .plane_bpp = { 32, },
	  .hsub = 1, .vsub = 1,
	},
	{ .name = "ARGB8888", .depth = 32, .drm_id = DRM_FORMAT_ARGB8888,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_ARGB32,
	  .pixman_id = PIXMAN_a8r8g8b8,
#endif
	  .num_planes = 1, .plane_bpp = { 32, },
	  .hsub = 1, .vsub = 1,
	},
	{ .name = "ABGR8888", .depth = -1, .drm_id = DRM_FORMAT_ABGR8888,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_INVALID,
	  .pixman_id = PIXMAN_a8b8g8r8,
#endif
	  .num_planes = 1, .plane_bpp = { 32, },
	  .hsub = 1, .vsub = 1,
	},
	{ .name = "XRGB16161616F", .depth = -1, .drm_id = DRM_FORMAT_XRGB16161616F,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGBA128F,
#endif
	  .num_planes = 1, .plane_bpp = { 64, },
	},
	{ .name = "ARGB16161616F", .depth = -1, .drm_id = DRM_FORMAT_ARGB16161616F,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGBA128F,
#endif
	  .num_planes = 1, .plane_bpp = { 64, },
	},
	{ .name = "XBGR16161616F", .depth = -1, .drm_id = DRM_FORMAT_XBGR16161616F,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGBA128F,
#endif
	  .num_planes = 1, .plane_bpp = { 64, },
	},
	{ .name = "ABGR16161616F", .depth = -1, .drm_id = DRM_FORMAT_ABGR16161616F,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGBA128F,
#endif
	  .num_planes = 1, .plane_bpp = { 64, },
	},
	{ .name = "NV12", .depth = -1, .drm_id = DRM_FORMAT_NV12,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB24,
#endif
	  .num_planes = 2, .plane_bpp = { 8, 16, },
	  .hsub = 2, .vsub = 2,
	},
	{ .name = "NV16", .depth = -1, .drm_id = DRM_FORMAT_NV16,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB24,
#endif
	  .num_planes = 2, .plane_bpp = { 8, 16, },
	  .hsub = 2, .vsub = 1,
	},
	{ .name = "NV21", .depth = -1, .drm_id = DRM_FORMAT_NV21,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB24,
#endif
	  .num_planes = 2, .plane_bpp = { 8, 16, },
	  .hsub = 2, .vsub = 2,
	},
	{ .name = "NV61", .depth = -1, .drm_id = DRM_FORMAT_NV61,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB24,
#endif
	  .num_planes = 2, .plane_bpp = { 8, 16, },
	  .hsub = 2, .vsub = 1,
	},
	{ .name = "YUYV", .depth = -1, .drm_id = DRM_FORMAT_YUYV,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB24,
#endif
	  .num_planes = 1, .plane_bpp = { 16, },
	  .hsub = 2, .vsub = 1,
	},
	{ .name = "YVYU", .depth = -1, .drm_id = DRM_FORMAT_YVYU,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB24,
#endif
	  .num_planes = 1, .plane_bpp = { 16, },
	  .hsub = 2, .vsub = 1,
	},
	{ .name = "UYVY", .depth = -1, .drm_id = DRM_FORMAT_UYVY,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB24,
#endif
	  .num_planes = 1, .plane_bpp = { 16, },
	  .hsub = 2, .vsub = 1,
	},
	{ .name = "VYUY", .depth = -1, .drm_id = DRM_FORMAT_VYUY,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB24,
#endif
	  .num_planes = 1, .plane_bpp = { 16, },
	  .hsub = 2, .vsub = 1,
	},
	{ .name = "YU12", .depth = -1, .drm_id = DRM_FORMAT_YUV420,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB24,
#endif
	  .num_planes = 3, .plane_bpp = { 8, 8, 8, },
	  .hsub = 2, .vsub = 2,
	},
	{ .name = "YU16", .depth = -1, .drm_id = DRM_FORMAT_YUV422,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB24,
#endif
	  .num_planes = 3, .plane_bpp = { 8, 8, 8, },
	  .hsub = 2, .vsub = 1,
	},
	{ .name = "YV12", .depth = -1, .drm_id = DRM_FORMAT_YVU420,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB24,
#endif
	  .num_planes = 3, .plane_bpp = { 8, 8, 8, },
	  .hsub = 2, .vsub = 2,
	},
	{ .name = "YV16", .depth = -1, .drm_id = DRM_FORMAT_YVU422,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB24,
#endif
	  .num_planes = 3, .plane_bpp = { 8, 8, 8, },
	  .hsub = 2, .vsub = 1,
	},
	{ .name = "Y410", .depth = -1, .drm_id = DRM_FORMAT_Y410,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGBA128F,
#endif
	  .num_planes = 1, .plane_bpp = { 32, },
	  .hsub = 1, .vsub = 1,
	},
	{ .name = "Y412", .depth = -1, .drm_id = DRM_FORMAT_Y412,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGBA128F,
#endif
	  .num_planes = 1, .plane_bpp = { 64, },
	  .hsub = 1, .vsub = 1,
	},
	{ .name = "Y416", .depth = -1, .drm_id = DRM_FORMAT_Y416,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGBA128F,
#endif
	  .num_planes = 1, .plane_bpp = { 64, },
	  .hsub = 1, .vsub = 1,
	},
	{ .name = "XV30", .depth = -1, .drm_id = DRM_FORMAT_XVYU2101010,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB96F,
#endif
	  .num_planes = 1, .plane_bpp = { 32, },
	  .hsub = 1, .vsub = 1,
	},
	{ .name = "XV36", .depth = -1, .drm_id = DRM_FORMAT_XVYU12_16161616,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB96F,
#endif
	  .num_planes = 1, .plane_bpp = { 64, },
	  .hsub = 1, .vsub = 1,
	},
	{ .name = "XV48", .depth = -1, .drm_id = DRM_FORMAT_XVYU16161616,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB96F,
#endif
	  .num_planes = 1, .plane_bpp = { 64, },
	  .hsub = 1, .vsub = 1,
	},
	{ .name = "P010", .depth = -1, .drm_id = DRM_FORMAT_P010,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB96F,
#endif
	  .num_planes = 2, .plane_bpp = { 16, 32 },
	  .vsub = 2, .hsub = 2,
	},
	{ .name = "P012", .depth = -1, .drm_id = DRM_FORMAT_P012,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB96F,
#endif
	  .num_planes = 2, .plane_bpp = { 16, 32 },
	  .vsub = 2, .hsub = 2,
	},
	{ .name = "P016", .depth = -1, .drm_id = DRM_FORMAT_P016,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB96F,
#endif
	  .num_planes = 2, .plane_bpp = { 16, 32 },
	  .vsub = 2, .hsub = 2,
	},
	{ .name = "Y210", .depth = -1, .drm_id = DRM_FORMAT_Y210,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB96F,
#endif
	  .num_planes = 1, .plane_bpp = { 32, },
	  .hsub = 2, .vsub = 1,
	},
	{ .name = "Y212", .depth = -1, .drm_id = DRM_FORMAT_Y212,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB96F,
#endif
	  .num_planes = 1, .plane_bpp = { 32, },
	  .hsub = 2, .vsub = 1,
	},
	{ .name = "Y216", .depth = -1, .drm_id = DRM_FORMAT_Y216,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_RGB96F,
#endif
	  .num_planes = 1, .plane_bpp = { 32, },
	  .hsub = 2, .vsub = 1,
	},
	{ .name = "IGT-FLOAT", .depth = -1, .drm_id = IGT_FORMAT_FLOAT,
#if defined(USE_CAIRO_PIXMAN)
	  .cairo_id = CAIRO_FORMAT_INVALID,
#endif
	  .num_planes = 1, .plane_bpp = { 128 },
	},
};
#define for_each_format(f)	\
	for (f = format_desc; f - format_desc < ARRAY_SIZE(format_desc); f++)

static const struct format_desc_struct *lookup_drm_format(uint32_t drm_format)
{
	const struct format_desc_struct *format;

	for_each_format(format) {
		if (format->drm_id != drm_format)
			continue;

		return format;
	}

	return NULL;
}

/**
 * igt_get_fb_tile_size:
 * @fd: the DRM file descriptor
 * @modifier: tiling layout of the framebuffer (as framebuffer modifier)
 * @fb_bpp: bits per pixel of the framebuffer
 * @width_ret: width of the tile in bytes
 * @height_ret: height of the tile in lines
 *
 * This function returns width and height of a tile based on the given tiling
 * format.
 */
void igt_get_fb_tile_size(int fd, uint64_t modifier, int fb_bpp,
			  unsigned *width_ret, unsigned *height_ret)
{
	uint32_t vc4_modifier_param = 0;

	if (is_vc4_device(fd)) {
		vc4_modifier_param = fourcc_mod_broadcom_param(modifier);
		modifier = fourcc_mod_broadcom_mod(modifier);
	}

	switch (modifier) {
	case LOCAL_DRM_FORMAT_MOD_NONE:
		if (is_i915_device(fd))
			*width_ret = 64;
		else
			*width_ret = 1;

		*height_ret = 1;
		break;
#if defined(USE_INTEL)
	case LOCAL_I915_FORMAT_MOD_X_TILED:
		igt_require_intel(fd);
		if (intel_gen(intel_get_drm_devid(fd)) == 2) {
			*width_ret = 128;
			*height_ret = 16;
		} else {
			*width_ret = 512;
			*height_ret = 8;
		}
		break;
	case LOCAL_I915_FORMAT_MOD_Y_TILED:
	case LOCAL_I915_FORMAT_MOD_Y_TILED_CCS:
		igt_require_intel(fd);
		if (intel_gen(intel_get_drm_devid(fd)) == 2) {
			*width_ret = 128;
			*height_ret = 16;
		} else if (IS_915(intel_get_drm_devid(fd))) {
			*width_ret = 512;
			*height_ret = 8;
		} else {
			*width_ret = 128;
			*height_ret = 32;
		}
		break;
	case LOCAL_I915_FORMAT_MOD_Yf_TILED:
	case LOCAL_I915_FORMAT_MOD_Yf_TILED_CCS:
		igt_require_intel(fd);
		switch (fb_bpp) {
		case 8:
			*width_ret = 64;
			*height_ret = 64;
			break;
		case 16:
		case 32:
			*width_ret = 128;
			*height_ret = 32;
			break;
		case 64:
		case 128:
			*width_ret = 256;
			*height_ret = 16;
			break;
		default:
			igt_assert(false);
		}
		break;
#endif
#if defined(USE_VC4)
	case DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED:
		igt_require_vc4(fd);
		*width_ret = 128;
		*height_ret = 32;
		break;
	case DRM_FORMAT_MOD_BROADCOM_SAND32:
		igt_require_vc4(fd);
		*width_ret = 32;
		*height_ret = vc4_modifier_param;
		break;
	case DRM_FORMAT_MOD_BROADCOM_SAND64:
		igt_require_vc4(fd);
		*width_ret = 64;
		*height_ret = vc4_modifier_param;
		break;
	case DRM_FORMAT_MOD_BROADCOM_SAND128:
		igt_require_vc4(fd);
		*width_ret = 128;
		*height_ret = vc4_modifier_param;
		break;
	case DRM_FORMAT_MOD_BROADCOM_SAND256:
		igt_require_vc4(fd);
		*width_ret = 256;
		*height_ret = vc4_modifier_param;
		break;
#endif
	default:
		igt_assert(false);
	}
}

static bool is_ccs_modifier(uint64_t modifier)
{
	return modifier == LOCAL_I915_FORMAT_MOD_Y_TILED_CCS ||
		modifier == LOCAL_I915_FORMAT_MOD_Yf_TILED_CCS;
}

static unsigned fb_plane_width(const struct igt_fb *fb, int plane)
{
	const struct format_desc_struct *format = lookup_drm_format(fb->drm_format);

	if (is_ccs_modifier(fb->modifier) && plane == 1)
		return DIV_ROUND_UP(fb->width, 1024) * 128;

	if (plane == 0)
		return fb->width;

	return DIV_ROUND_UP(fb->width, format->hsub);
}

static unsigned fb_plane_bpp(const struct igt_fb *fb, int plane)
{
	const struct format_desc_struct *format = lookup_drm_format(fb->drm_format);

	if (is_ccs_modifier(fb->modifier) && plane == 1)
		return 8;
	else
		return format->plane_bpp[plane];
}

static unsigned fb_plane_height(const struct igt_fb *fb, int plane)
{
	const struct format_desc_struct *format = lookup_drm_format(fb->drm_format);

	if (is_ccs_modifier(fb->modifier) && plane == 1)
		return DIV_ROUND_UP(fb->height, 512) * 32;

	if (plane == 0)
		return fb->height;

	return DIV_ROUND_UP(fb->height, format->vsub);
}

static int fb_num_planes(const struct igt_fb *fb)
{
	const struct format_desc_struct *format = lookup_drm_format(fb->drm_format);

	if (is_ccs_modifier(fb->modifier))
		return 2;
	else
		return format->num_planes;
}

void igt_init_fb(struct igt_fb *fb, int fd, int width, int height,
		 uint32_t drm_format, uint64_t modifier,
		 enum igt_color_encoding color_encoding,
		 enum igt_color_range color_range)
{
	const struct format_desc_struct *f = lookup_drm_format(drm_format);

	igt_assert_f(f, "DRM format %08x not found\n", drm_format);

	memset(fb, 0, sizeof(*fb));

	fb->width = width;
	fb->height = height;
	fb->modifier = modifier;
	fb->drm_format = drm_format;
	fb->fd = fd;
	fb->num_planes = fb_num_planes(fb);
	fb->color_encoding = color_encoding;
	fb->color_range = color_range;

	for (int i = 0; i < fb->num_planes; i++) {
		fb->plane_bpp[i] = fb_plane_bpp(fb, i);
		fb->plane_height[i] = fb_plane_height(fb, i);
		fb->plane_width[i] = fb_plane_width(fb, i);
	}
}

static uint32_t calc_plane_stride(struct igt_fb *fb, int plane)
{
	uint32_t min_stride = fb->plane_width[plane] *
		(fb->plane_bpp[plane] / 8);

	if (fb->modifier != LOCAL_DRM_FORMAT_MOD_NONE &&
	    is_i915_device(fb->fd) &&
	    intel_gen(intel_get_drm_devid(fb->fd)) <= 3) {
		uint32_t stride;

		/* Round the tiling up to the next power-of-two and the region
		 * up to the next pot fence size so that this works on all
		 * generations.
		 *
		 * This can still fail if the framebuffer is too large to be
		 * tiled. But then that failure is expected.
		 */

		stride = max(min_stride, 512);
		stride = roundup_power_of_two(stride);

		return stride;
	} else if (igt_format_is_yuv(fb->drm_format) && is_amdgpu_device(fb->fd)) {
		/*
		 * Chroma address needs to be aligned to 256 bytes on AMDGPU
		 * so the easiest way is to align the luma stride to 256.
		 */
		return ALIGN(min_stride, 256);
	} else {
		unsigned int tile_width, tile_height;

		igt_get_fb_tile_size(fb->fd, fb->modifier, fb->plane_bpp[plane],
				     &tile_width, &tile_height);

		return ALIGN(min_stride, tile_width);
	}
}

static uint64_t calc_plane_size(struct igt_fb *fb, int plane)
{
	if (fb->modifier != LOCAL_DRM_FORMAT_MOD_NONE &&
	    is_i915_device(fb->fd) &&
	    intel_gen(intel_get_drm_devid(fb->fd)) <= 3) {
		uint64_t min_size = (uint64_t) fb->strides[plane] *
			fb->plane_height[plane];
		uint64_t size;

		/* Round the tiling up to the next power-of-two and the region
		 * up to the next pot fence size so that this works on all
		 * generations.
		 *
		 * This can still fail if the framebuffer is too large to be
		 * tiled. But then that failure is expected.
		 */

		size = max(min_size, 1024*1024);
		size = roundup_power_of_two(size);

		return size;
	} else {
		unsigned int tile_width, tile_height;

		igt_get_fb_tile_size(fb->fd, fb->modifier, fb->plane_bpp[plane],
				     &tile_width, &tile_height);

		/* Special case where the "tile height" represents a
		 * height-based stride, such as with VC4 SAND tiling modes.
		 */

		if (tile_height > fb->plane_height[plane])
			return fb->strides[plane] * tile_height;

		return (uint64_t) fb->strides[plane] *
			ALIGN(fb->plane_height[plane], tile_height);
	}
}

static uint64_t calc_fb_size(struct igt_fb *fb)
{
	uint64_t size = 0;
	int plane;

	for (plane = 0; plane < fb->num_planes; plane++) {
		/* respect the stride requested by the caller */
		if (!fb->strides[plane])
			fb->strides[plane] = calc_plane_stride(fb, plane);

		fb->offsets[plane] = size;

		size += calc_plane_size(fb, plane);
	}

	return size;
}

/**
 * igt_calc_fb_size:
 * @fd: the DRM file descriptor
 * @width: width of the framebuffer in pixels
 * @height: height of the framebuffer in pixels
 * @format: drm fourcc pixel format code
 * @modifier: tiling layout of the framebuffer (as framebuffer modifier)
 * @size_ret: returned size for the framebuffer
 * @stride_ret: returned stride for the framebuffer
 *
 * This function returns valid stride and size values for a framebuffer with the
 * specified parameters.
 */
void igt_calc_fb_size(int fd, int width, int height, uint32_t drm_format, uint64_t modifier,
		      uint64_t *size_ret, unsigned *stride_ret)
{
	struct igt_fb fb;

	igt_init_fb(&fb, fd, width, height, drm_format, modifier,
		    IGT_COLOR_YCBCR_BT709, IGT_COLOR_YCBCR_LIMITED_RANGE);

	fb.size = calc_fb_size(&fb);

	if (size_ret)
		*size_ret = fb.size;
	if (stride_ret)
		*stride_ret = fb.strides[0];
}

/**
 * igt_fb_mod_to_tiling:
 * @modifier: DRM framebuffer modifier
 *
 * This function converts a DRM framebuffer modifier to its corresponding
 * tiling constant.
 *
 * Returns:
 * A tiling constant
 */
uint64_t igt_fb_mod_to_tiling(uint64_t modifier)
{
	switch (modifier) {
	case LOCAL_DRM_FORMAT_MOD_NONE:
		return I915_TILING_NONE;
	case LOCAL_I915_FORMAT_MOD_X_TILED:
		return I915_TILING_X;
	case LOCAL_I915_FORMAT_MOD_Y_TILED:
	case LOCAL_I915_FORMAT_MOD_Y_TILED_CCS:
		return I915_TILING_Y;
	case LOCAL_I915_FORMAT_MOD_Yf_TILED:
	case LOCAL_I915_FORMAT_MOD_Yf_TILED_CCS:
		return I915_TILING_Yf;
	default:
		igt_assert(0);
	}
}

/**
 * igt_fb_tiling_to_mod:
 * @tiling: DRM framebuffer tiling
 *
 * This function converts a DRM framebuffer tiling to its corresponding
 * modifier constant.
 *
 * Returns:
 * A modifier constant
 */
uint64_t igt_fb_tiling_to_mod(uint64_t tiling)
{
	switch (tiling) {
	case I915_TILING_NONE:
		return LOCAL_DRM_FORMAT_MOD_NONE;
	case I915_TILING_X:
		return LOCAL_I915_FORMAT_MOD_X_TILED;
	case I915_TILING_Y:
		return LOCAL_I915_FORMAT_MOD_Y_TILED;
	case I915_TILING_Yf:
		return LOCAL_I915_FORMAT_MOD_Yf_TILED;
	default:
		igt_assert(0);
	}
}

static void memset64(uint64_t *s, uint64_t c, size_t n)
{
	for (int i = 0; i < n; i++)
		s[i] = c;
}

static void clear_yuv_buffer(struct igt_fb *fb)
{
	bool full_range = fb->color_range == IGT_COLOR_YCBCR_FULL_RANGE;
	uint8_t *ptr;

	igt_assert(igt_format_is_yuv(fb->drm_format));

	/* Ensure the framebuffer is preallocated */
	ptr = igt_fb_map_buffer(fb->fd, fb);
	igt_assert(*(uint32_t *)ptr == 0);

	switch (fb->drm_format) {
	case DRM_FORMAT_NV12:
		memset(ptr + fb->offsets[0],
		       full_range ? 0x00 : 0x10,
		       fb->strides[0] * fb->plane_height[0]);
		memset(ptr + fb->offsets[1],
		       0x80,
		       fb->strides[1] * fb->plane_height[1]);
		break;
	case DRM_FORMAT_XYUV8888:
		wmemset((wchar_t*)(ptr + fb->offsets[0]), full_range ? 0x00008080 : 0x00108080,
			fb->strides[0] * fb->plane_height[0] / sizeof(wchar_t));
		break;
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
		wmemset((wchar_t*)(ptr + fb->offsets[0]),
			full_range ? 0x80008000 : 0x80108010,
			fb->strides[0] * fb->plane_height[0] / sizeof(wchar_t));
		break;
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
		wmemset((wchar_t*)(ptr + fb->offsets[0]),
			full_range ? 0x00800080 : 0x10801080,
			fb->strides[0] * fb->plane_height[0] / sizeof(wchar_t));
		break;
	case DRM_FORMAT_P010:
	case DRM_FORMAT_P012:
	case DRM_FORMAT_P016:
		wmemset((wchar_t*)ptr, full_range ? 0 : 0x10001000,
			fb->offsets[1] / sizeof(wchar_t));
		wmemset((wchar_t*)(ptr + fb->offsets[1]), 0x80008000,
			fb->strides[1] * fb->plane_height[1] / sizeof(wchar_t));
		break;
	case DRM_FORMAT_Y210:
	case DRM_FORMAT_Y212:
	case DRM_FORMAT_Y216:
		wmemset((wchar_t*)(ptr + fb->offsets[0]),
			full_range ? 0x80000000 : 0x80001000,
			fb->strides[0] * fb->plane_height[0] / sizeof(wchar_t));
		break;

	case DRM_FORMAT_XVYU2101010:
	case DRM_FORMAT_Y410:
		wmemset((wchar_t*)(ptr + fb->offsets[0]),
			full_range ? 0x20000200 : 0x20010200,
		fb->strides[0] * fb->plane_height[0] / sizeof(wchar_t));
		break;

	case DRM_FORMAT_XVYU12_16161616:
	case DRM_FORMAT_XVYU16161616:
	case DRM_FORMAT_Y412:
	case DRM_FORMAT_Y416:
		memset64((uint64_t*)(ptr + fb->offsets[0]),
			 full_range ? 0x800000008000ULL : 0x800010008000ULL,
			 fb->strides[0] * fb->plane_height[0] / sizeof(uint64_t));
		break;
	}

	igt_fb_unmap_buffer(fb, ptr);
}

/* helpers to create nice-looking framebuffers */
static int create_bo_for_fb(struct igt_fb *fb)
{
	const struct format_desc_struct *fmt = lookup_drm_format(fb->drm_format);
	unsigned int bpp = 0;
	unsigned int plane;
	unsigned *strides = &fb->strides[0];
	bool device_bo = false;
	int fd = fb->fd;
	uint64_t size;

	/*
	 * The current dumb buffer allocation API doesn't really allow to
	 * specify a custom size or stride. Yet the caller is free to specify
	 * them, so we need to make sure to use a device BO then.
	 */
	if (fb->modifier || fb->size || fb->strides[0] ||
	    (is_i915_device(fd) && igt_format_is_yuv(fb->drm_format)) ||
	    (is_i915_device(fd) && igt_format_is_fp16(fb->drm_format)) ||
	    (is_amdgpu_device(fd) && igt_format_is_yuv(fb->drm_format)))
		device_bo = true;

	/* Sets offets and stride if necessary. */
	size = calc_fb_size(fb);

	/* Respect the size requested by the caller. */
	if (fb->size == 0)
		fb->size = size;

	if (device_bo) {
		fb->is_dumb = false;

		if (is_i915_device(fd)) {
			fb->gem_handle = gem_create(fd, fb->size);
			gem_set_tiling(fd, fb->gem_handle,
				       igt_fb_mod_to_tiling(fb->modifier),
				       fb->strides[0]);
#if defined(USE_VC4)
		} else if (is_vc4_device(fd)) {
			fb->gem_handle = igt_vc4_create_bo(fd, fb->size);

			if (fb->modifier == DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED)
				igt_vc4_set_tiling(fd, fb->gem_handle,
						   fb->modifier);
#endif
#if defined(USE_AMD)
		} else if (is_amdgpu_device(fd)) {
			fb->gem_handle = igt_amd_create_bo(fd, fb->size);
#endif
		} else {
			igt_assert(false);
		}

		goto out;
	}

	for (plane = 0; plane < fb->num_planes; plane++)
		bpp += DIV_ROUND_UP(fb->plane_bpp[plane],
				    plane ? fmt->hsub * fmt->vsub : 1);

	fb->is_dumb = true;

	/*
	 * We can't really pass the stride array here since the dumb
	 * buffer allocation is assuming that it operates on one
	 * plane, and therefore will calculate the stride as if each
	 * pixel was stored on a single plane.
	 *
	 * This might cause issues at some point on drivers that would
	 * change the stride of YUV buffers, but we haven't
	 * encountered any yet.
	 */
	if (fb->num_planes > 1)
		strides = NULL;

	fb->gem_handle = kmstest_dumb_create(fd, fb->width, fb->height,
					     bpp, strides, &fb->size);

out:
	if (igt_format_is_yuv(fb->drm_format))
		clear_yuv_buffer(fb);

	return fb->gem_handle;
}

void igt_create_bo_for_fb(int fd, int width, int height,
			  uint32_t format, uint64_t modifier,
			  struct igt_fb *fb /* out */)
{
	igt_init_fb(fb, fd, width, height, format, modifier,
		    IGT_COLOR_YCBCR_BT709, IGT_COLOR_YCBCR_LIMITED_RANGE);
	create_bo_for_fb(fb);
}

/**
 * igt_create_bo_with_dimensions:
 * @fd: open drm file descriptor
 * @width: width of the buffer object in pixels
 * @height: height of the buffer object in pixels
 * @format: drm fourcc pixel format code
 * @modifier: modifier corresponding to the tiling layout of the buffer object
 * @stride: stride of the buffer object in bytes (0 for automatic stride)
 * @size_ret: size of the buffer object as created by the kernel
 * @stride_ret: stride of the buffer object as created by the kernel
 * @is_dumb: whether the created buffer object is a dumb buffer or not
 *
 * This function allocates a gem buffer object matching the requested
 * properties.
 *
 * Returns:
 * The kms id of the created buffer object.
 */
int igt_create_bo_with_dimensions(int fd, int width, int height,
				  uint32_t format, uint64_t modifier,
				  unsigned stride, uint64_t *size_ret,
				  unsigned *stride_ret, bool *is_dumb)
{
	struct igt_fb fb;

	igt_init_fb(&fb, fd, width, height, format, modifier,
		    IGT_COLOR_YCBCR_BT709, IGT_COLOR_YCBCR_LIMITED_RANGE);

	for (int i = 0; i < fb.num_planes; i++)
		fb.strides[i] = stride;

	create_bo_for_fb(&fb);

	if (size_ret)
		*size_ret = fb.size;
	if (stride_ret)
		*stride_ret = fb.strides[0];
	if (is_dumb)
		*is_dumb = fb.is_dumb;

	return fb.gem_handle;
}

#define get_u16_bit(x, n) 	((x & (1 << n)) >> n )
#define set_u16_bit(x, n, val)	((x & ~(1 << n)) | (val << n))
/*
 * update_crc16_dp:
 * @crc_old: old 16-bit CRC value to be updated
 * @d: input 16-bit data on which to calculate 16-bit CRC
 *
 * CRC algorithm implementation described in DP 1.4 spec Appendix J
 * the 16-bit CRC IBM is applied, with the following polynomial:
 *
 *       f(x) = x ^ 16 + x ^ 15 + x ^ 2 + 1
 *
 * the MSB is shifted in first, for any color format that is less than 16 bits
 * per component, the LSB is zero-padded.
 *
 * The following implementation is based on the hardware parallel 16-bit CRC
 * generation and ported to C code.
 *
 * Reference: VESA DisplayPort Standard v1.4, appendix J
 *
 * Returns:
 * updated 16-bit CRC value.
 */
static uint16_t update_crc16_dp(uint16_t crc_old, uint16_t d)
{
	uint16_t crc_new = 0;	/* 16-bit CRC output */

	/* internal use */
	uint16_t b = crc_old;
	uint8_t val;

	/* b[15] */
	val = get_u16_bit(b, 0) ^ get_u16_bit(b, 1) ^ get_u16_bit(b, 2) ^
	      get_u16_bit(b, 3) ^ get_u16_bit(b, 4) ^ get_u16_bit(b, 5) ^
	      get_u16_bit(b, 6) ^ get_u16_bit(b, 7) ^ get_u16_bit(b, 8) ^
	      get_u16_bit(b, 9) ^ get_u16_bit(b, 10) ^ get_u16_bit(b, 11) ^
	      get_u16_bit(b, 12) ^ get_u16_bit(b, 14) ^ get_u16_bit(b, 15) ^
	      get_u16_bit(d, 0) ^ get_u16_bit(d, 1) ^ get_u16_bit(d, 2) ^
	      get_u16_bit(d, 3) ^ get_u16_bit(d, 4) ^ get_u16_bit(d, 5) ^
	      get_u16_bit(d, 6) ^ get_u16_bit(d, 7) ^ get_u16_bit(d, 8) ^
	      get_u16_bit(d, 9) ^ get_u16_bit(d, 10) ^ get_u16_bit(d, 11) ^
	      get_u16_bit(d, 12) ^ get_u16_bit(d, 14) ^ get_u16_bit(d, 15);
	crc_new = set_u16_bit(crc_new, 15, val);

	/* b[14] */
	val = get_u16_bit(b, 12) ^ get_u16_bit(b, 13) ^
	      get_u16_bit(d, 12) ^ get_u16_bit(d, 13);
	crc_new = set_u16_bit(crc_new, 14, val);

	/* b[13] */
	val = get_u16_bit(b, 11) ^ get_u16_bit(b, 12) ^
	      get_u16_bit(d, 11) ^ get_u16_bit(d, 12);
	crc_new = set_u16_bit(crc_new, 13, val);

	/* b[12] */
	val = get_u16_bit(b, 10) ^ get_u16_bit(b, 11) ^
	      get_u16_bit(d, 10) ^ get_u16_bit(d, 11);
	crc_new = set_u16_bit(crc_new, 12, val);

	/* b[11] */
	val = get_u16_bit(b, 9) ^ get_u16_bit(b, 10) ^
	      get_u16_bit(d, 9) ^ get_u16_bit(d, 10);
	crc_new = set_u16_bit(crc_new, 11, val);

	/* b[10] */
	val = get_u16_bit(b, 8) ^ get_u16_bit(b, 9) ^
	      get_u16_bit(d, 8) ^ get_u16_bit(d, 9);
	crc_new = set_u16_bit(crc_new, 10, val);

	/* b[9] */
	val = get_u16_bit(b, 7) ^ get_u16_bit(b, 8) ^
	      get_u16_bit(d, 7) ^ get_u16_bit(d, 8);
	crc_new = set_u16_bit(crc_new, 9, val);

	/* b[8] */
	val = get_u16_bit(b, 6) ^ get_u16_bit(b, 7) ^
	      get_u16_bit(d, 6) ^ get_u16_bit(d, 7);
	crc_new = set_u16_bit(crc_new, 8, val);

	/* b[7] */
	val = get_u16_bit(b, 5) ^ get_u16_bit(b, 6) ^
	      get_u16_bit(d, 5) ^ get_u16_bit(d, 6);
	crc_new = set_u16_bit(crc_new, 7, val);

	/* b[6] */
	val = get_u16_bit(b, 4) ^ get_u16_bit(b, 5) ^
	      get_u16_bit(d, 4) ^ get_u16_bit(d, 5);
	crc_new = set_u16_bit(crc_new, 6, val);

	/* b[5] */
	val = get_u16_bit(b, 3) ^ get_u16_bit(b, 4) ^
	      get_u16_bit(d, 3) ^ get_u16_bit(d, 4);
	crc_new = set_u16_bit(crc_new, 5, val);

	/* b[4] */
	val = get_u16_bit(b, 2) ^ get_u16_bit(b, 3) ^
	      get_u16_bit(d, 2) ^ get_u16_bit(d, 3);
	crc_new = set_u16_bit(crc_new, 4, val);

	/* b[3] */
	val = get_u16_bit(b, 1) ^ get_u16_bit(b, 2) ^ get_u16_bit(b, 15) ^
	      get_u16_bit(d, 1) ^ get_u16_bit(d, 2) ^ get_u16_bit(d, 15);
	crc_new = set_u16_bit(crc_new, 3, val);

	/* b[2] */
	val = get_u16_bit(b, 0) ^ get_u16_bit(b, 1) ^ get_u16_bit(b, 14) ^
	      get_u16_bit(d, 0) ^ get_u16_bit(d, 1) ^ get_u16_bit(d, 14);
	crc_new = set_u16_bit(crc_new, 2, val);

	/* b[1] */
	val = get_u16_bit(b, 1) ^ get_u16_bit(b, 2) ^ get_u16_bit(b, 3) ^
	      get_u16_bit(b, 4) ^ get_u16_bit(b, 5) ^ get_u16_bit(b, 6) ^
	      get_u16_bit(b, 7) ^ get_u16_bit(b, 8) ^ get_u16_bit(b, 9) ^
	      get_u16_bit(b, 10) ^ get_u16_bit(b, 11) ^ get_u16_bit(b, 12) ^
	      get_u16_bit(b, 13) ^ get_u16_bit(b, 14) ^
	      get_u16_bit(d, 1) ^ get_u16_bit(d, 2) ^ get_u16_bit(d, 3) ^
	      get_u16_bit(d, 4) ^ get_u16_bit(d, 5) ^ get_u16_bit(d, 6) ^
	      get_u16_bit(d, 7) ^ get_u16_bit(d, 8) ^ get_u16_bit(d, 9) ^
	      get_u16_bit(d, 10) ^ get_u16_bit(d, 11) ^ get_u16_bit(d, 12) ^
	      get_u16_bit(d, 13) ^ get_u16_bit(d, 14);
	crc_new = set_u16_bit(crc_new, 1, val);

	/* b[0] */
	val = get_u16_bit(b, 0) ^ get_u16_bit(b, 1) ^ get_u16_bit(b, 2) ^
	      get_u16_bit(b, 3) ^ get_u16_bit(b, 4) ^ get_u16_bit(b, 5) ^
	      get_u16_bit(b, 6) ^ get_u16_bit(b, 7) ^ get_u16_bit(b, 8) ^
	      get_u16_bit(b, 9) ^ get_u16_bit(b, 10) ^ get_u16_bit(b, 11) ^
	      get_u16_bit(b, 12) ^ get_u16_bit(b, 13) ^ get_u16_bit(b, 15) ^
	      get_u16_bit(d, 0) ^ get_u16_bit(d, 1) ^ get_u16_bit(d, 2) ^
	      get_u16_bit(d, 3) ^ get_u16_bit(d, 4) ^ get_u16_bit(d, 5) ^
	      get_u16_bit(d, 6) ^ get_u16_bit(d, 7) ^ get_u16_bit(d, 8) ^
	      get_u16_bit(d, 9) ^ get_u16_bit(d, 10) ^ get_u16_bit(d, 11) ^
	      get_u16_bit(d, 12) ^ get_u16_bit(d, 13) ^ get_u16_bit(d, 15);
	crc_new = set_u16_bit(crc_new, 0, val);

	return crc_new;
}

/**
 * igt_fb_calc_crc:
 * @fb: pointer to an #igt_fb structure
 * @crc: pointer to an #igt_crc_t structure
 *
 * This function calculate the 16-bit frame CRC of RGB components over all
 * the active pixels.
 */
void igt_fb_calc_crc(struct igt_fb *fb, igt_crc_t *crc)
{
	int x, y, i;
	uint8_t *ptr;
	uint8_t *data;
	uint16_t din;

	igt_assert(fb && crc);

	ptr = igt_fb_map_buffer(fb->fd, fb);
	igt_assert(ptr);

	/* set for later CRC comparison */
	crc->has_valid_frame = true;
	crc->frame = 0;
	crc->n_words = 3;
	crc->crc[0] = 0;	/* R */
	crc->crc[1] = 0;	/* G */
	crc->crc[2] = 0;	/* B */

	data = ptr + fb->offsets[0];
	for (y = 0; y < fb->height; ++y) {
		for (x = 0; x < fb->width; ++x) {
			switch (fb->drm_format) {
			case DRM_FORMAT_XRGB8888:
				i = x * 4 + y * fb->strides[0];

				din = data[i + 2] << 8; /* padding-zeros */
				crc->crc[0] = update_crc16_dp(crc->crc[0], din);

				/* Green-component */
				din = data[i + 1] << 8;
				crc->crc[1] = update_crc16_dp(crc->crc[1], din);

				/* Blue-component */
				din = data[i] << 8;
				crc->crc[2] = update_crc16_dp(crc->crc[2], din);
				break;
			default:
				igt_assert_f(0, "DRM Format Invalid");
				break;
			}
		}
	}

	igt_fb_unmap_buffer(fb, ptr);
}

#if defined(USE_CAIRO_PIXMAN)
/**
 * igt_paint_color:
 * @cr: cairo drawing context
 * @x: pixel x-coordination of the fill rectangle
 * @y: pixel y-coordination of the fill rectangle
 * @w: width of the fill rectangle
 * @h: height of the fill rectangle
 * @r: red value to use as fill color
 * @g: green value to use as fill color
 * @b: blue value to use as fill color
 *
 * This functions draws a solid rectangle with the given color using the drawing
 * context @cr.
 */
void igt_paint_color(cairo_t *cr, int x, int y, int w, int h,
		     double r, double g, double b)
{
	cairo_rectangle(cr, x, y, w, h);
	cairo_set_source_rgb(cr, r, g, b);
	cairo_fill(cr);
}

/**
 * igt_paint_color_alpha:
 * @cr: cairo drawing context
 * @x: pixel x-coordination of the fill rectangle
 * @y: pixel y-coordination of the fill rectangle
 * @w: width of the fill rectangle
 * @h: height of the fill rectangle
 * @r: red value to use as fill color
 * @g: green value to use as fill color
 * @b: blue value to use as fill color
 * @a: alpha value to use as fill color
 *
 * This functions draws a rectangle with the given color and alpha values using
 * the drawing context @cr.
 */
void igt_paint_color_alpha(cairo_t *cr, int x, int y, int w, int h,
			   double r, double g, double b, double a)
{
	cairo_rectangle(cr, x, y, w, h);
	cairo_set_source_rgba(cr, r, g, b, a);
	cairo_fill(cr);
}

/**
 * igt_paint_color_gradient:
 * @cr: cairo drawing context
 * @x: pixel x-coordination of the fill rectangle
 * @y: pixel y-coordination of the fill rectangle
 * @w: width of the fill rectangle
 * @h: height of the fill rectangle
 * @r: red value to use as fill color
 * @g: green value to use as fill color
 * @b: blue value to use as fill color
 *
 * This functions draws a gradient into the rectangle which fades in from black
 * to the given values using the drawing context @cr.
 */
void
igt_paint_color_gradient(cairo_t *cr, int x, int y, int w, int h,
			 int r, int g, int b)
{
	cairo_pattern_t *pat;

	pat = cairo_pattern_create_linear(x, y, x + w, y + h);
	cairo_pattern_add_color_stop_rgba(pat, 1, 0, 0, 0, 1);
	cairo_pattern_add_color_stop_rgba(pat, 0, r, g, b, 1);

	cairo_rectangle(cr, x, y, w, h);
	cairo_set_source(cr, pat);
	cairo_fill(cr);
	cairo_pattern_destroy(pat);
}

/**
 * igt_paint_color_gradient_range:
 * @cr: cairo drawing context
 * @x: pixel x-coordination of the fill rectangle
 * @y: pixel y-coordination of the fill rectangle
 * @w: width of the fill rectangle
 * @h: height of the fill rectangle
 * @sr: red value to use as start gradient color
 * @sg: green value to use as start gradient color
 * @sb: blue value to use as start gradient color
 * @er: red value to use as end gradient color
 * @eg: green value to use as end gradient color
 * @eb: blue value to use as end gradient color
 *
 * This functions draws a gradient into the rectangle which fades in
 * from one color to the other using the drawing context @cr.
 */
void
igt_paint_color_gradient_range(cairo_t *cr, int x, int y, int w, int h,
			       double sr, double sg, double sb,
			       double er, double eg, double eb)
{
	cairo_pattern_t *pat;

	pat = cairo_pattern_create_linear(x, y, x + w, y + h);
	cairo_pattern_add_color_stop_rgba(pat, 1, sr, sg, sb, 1);
	cairo_pattern_add_color_stop_rgba(pat, 0, er, eg, eb, 1);

	cairo_rectangle(cr, x, y, w, h);
	cairo_set_source(cr, pat);
	cairo_fill(cr);
	cairo_pattern_destroy(pat);
}

static void
paint_test_patterns(cairo_t *cr, int width, int height)
{
	double gr_height, gr_width;
	int x, y;

	y = height * 0.10;
	gr_width = width * 0.75;
	gr_height = height * 0.08;
	x = (width / 2) - (gr_width / 2);

	igt_paint_color_gradient(cr, x, y, gr_width, gr_height, 1, 0, 0);

	y += gr_height;
	igt_paint_color_gradient(cr, x, y, gr_width, gr_height, 0, 1, 0);

	y += gr_height;
	igt_paint_color_gradient(cr, x, y, gr_width, gr_height, 0, 0, 1);

	y += gr_height;
	igt_paint_color_gradient(cr, x, y, gr_width, gr_height, 1, 1, 1);
}

/**
 * igt_cairo_printf_line:
 * @cr: cairo drawing context
 * @align: text alignment
 * @yspacing: additional y-direction feed after this line
 * @fmt: format string
 * @...: optional arguments used in the format string
 *
 * This is a little helper to draw text onto framebuffers. All the initial setup
 * (like setting the font size and the moving to the starting position) still
 * needs to be done manually with explicit cairo calls on @cr.
 *
 * Returns:
 * The width of the drawn text.
 */
int igt_cairo_printf_line(cairo_t *cr, enum igt_text_align align,
				double yspacing, const char *fmt, ...)
{
	double x, y, xofs, yofs;
	cairo_text_extents_t extents;
	char *text;
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vasprintf(&text, fmt, ap);
	igt_assert(ret >= 0);
	va_end(ap);

	cairo_text_extents(cr, text, &extents);

	xofs = yofs = 0;
	if (align & align_right)
		xofs = -extents.width;
	else if (align & align_hcenter)
		xofs = -extents.width / 2;

	if (align & align_top)
		yofs = extents.height;
	else if (align & align_vcenter)
		yofs = extents.height / 2;

	cairo_get_current_point(cr, &x, &y);
	if (xofs || yofs)
		cairo_rel_move_to(cr, xofs, yofs);

	cairo_text_path(cr, text);
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_fill(cr);

	cairo_move_to(cr, x, y + extents.height + yspacing);

	free(text);

	return extents.width;
}

static void
paint_marker(cairo_t *cr, int x, int y)
{
	enum igt_text_align align;
	int xoff, yoff;

	cairo_move_to(cr, x, y - 20);
	cairo_line_to(cr, x, y + 20);
	cairo_move_to(cr, x - 20, y);
	cairo_line_to(cr, x + 20, y);
	cairo_new_sub_path(cr);
	cairo_arc(cr, x, y, 10, 0, M_PI * 2);
	cairo_set_line_width(cr, 4);
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_set_line_width(cr, 2);
	cairo_stroke(cr);

	xoff = x ? -20 : 20;
	align = x ? align_right : align_left;

	yoff = y ? -20 : 20;
	align |= y ? align_bottom : align_top;

	cairo_move_to(cr, x + xoff, y + yoff);
	cairo_set_font_size(cr, 18);
	igt_cairo_printf_line(cr, align, 0, "(%d, %d)", x, y);
}

/**
 * igt_paint_test_pattern:
 * @cr: cairo drawing context
 * @width: width of the visible area
 * @height: height of the visible area
 *
 * This functions draws an entire set of test patterns for the given visible
 * area using the drawing context @cr. This is useful for manual visual
 * inspection of displayed framebuffers.
 *
 * The test patterns include
 *  - corner markers to check for over/underscan and
 *  - a set of color and b/w gradients.
 */
void igt_paint_test_pattern(cairo_t *cr, int width, int height)
{
	paint_test_patterns(cr, width, height);

	cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);

	/* Paint corner markers */
	paint_marker(cr, 0, 0);
	paint_marker(cr, width, 0);
	paint_marker(cr, 0, height);
	paint_marker(cr, width, height);

	igt_assert(!cairo_status(cr));
}

static cairo_status_t
stdio_read_func(void *closure, unsigned char* data, unsigned int size)
{
	if (fread(data, 1, size, (FILE*)closure) != size)
		return CAIRO_STATUS_READ_ERROR;

	return CAIRO_STATUS_SUCCESS;
}

cairo_surface_t *igt_cairo_image_surface_create_from_png(const char *filename)
{
	cairo_surface_t *image;
	FILE *f;

	f = igt_fopen_data(filename);
	image = cairo_image_surface_create_from_png_stream(&stdio_read_func, f);
	fclose(f);

	return image;
}

/**
 * igt_paint_image:
 * @cr: cairo drawing context
 * @filename: filename of the png image to draw
 * @dst_x: pixel x-coordination of the destination rectangle
 * @dst_y: pixel y-coordination of the destination rectangle
 * @dst_width: width of the destination rectangle
 * @dst_height: height of the destination rectangle
 *
 * This function can be used to draw a scaled version of the supplied png image,
 * which is loaded from the package data directory.
 */
void igt_paint_image(cairo_t *cr, const char *filename,
		     int dst_x, int dst_y, int dst_width, int dst_height)
{
	cairo_surface_t *image;
	int img_width, img_height;
	double scale_x, scale_y;

	image = igt_cairo_image_surface_create_from_png(filename);
	igt_assert(cairo_surface_status(image) == CAIRO_STATUS_SUCCESS);

	img_width = cairo_image_surface_get_width(image);
	img_height = cairo_image_surface_get_height(image);

	scale_x = (double)dst_width / img_width;
	scale_y = (double)dst_height / img_height;

	cairo_save(cr);

	cairo_translate(cr, dst_x, dst_y);
	cairo_scale(cr, scale_x, scale_y);
	cairo_set_source_surface(cr, image, 0, 0);
	cairo_paint(cr);

	cairo_surface_destroy(image);

	cairo_restore(cr);
}
#endif /*defined(USE_CAIRO_PIXMAN)*/

/**
 * igt_create_fb_with_bo_size:
 * @fd: open i915 drm file descriptor
 * @width: width of the framebuffer in pixel
 * @height: height of the framebuffer in pixel
 * @format: drm fourcc pixel format code
 * @modifier: tiling layout of the framebuffer (as framebuffer modifier)
 * @color_encoding: color encoding for YCbCr formats (ignored otherwise)
 * @color_range: color range for YCbCr formats (ignored otherwise)
 * @fb: pointer to an #igt_fb structure
 * @bo_size: size of the backing bo (0 for automatic size)
 * @bo_stride: stride of the backing bo (0 for automatic stride)
 *
 * This function allocates a gem buffer object suitable to back a framebuffer
 * with the requested properties and then wraps it up in a drm framebuffer
 * object of the requested size. All metadata is stored in @fb.
 *
 * The backing storage of the framebuffer is filled with all zeros, i.e. black
 * for rgb pixel formats.
 *
 * Returns:
 * The kms id of the created framebuffer.
 */
unsigned int
igt_create_fb_with_bo_size(int fd, int width, int height,
			   uint32_t format, uint64_t modifier,
			   enum igt_color_encoding color_encoding,
			   enum igt_color_range color_range,
			   struct igt_fb *fb, uint64_t bo_size,
			   unsigned bo_stride)
{
	uint32_t flags = 0;

	igt_init_fb(fb, fd, width, height, format, modifier,
		    color_encoding, color_range);

	for (int i = 0; i < fb->num_planes; i++)
		fb->strides[i] = bo_stride;

	fb->size = bo_size;

	igt_debug("%s(width=%d, height=%d, format=" IGT_FORMAT_FMT
		  ", modifier=0x%"PRIx64", size=%"PRIu64")\n",
		  __func__, width, height, IGT_FORMAT_ARGS(format), modifier,
		  bo_size);

	create_bo_for_fb(fb);
	igt_assert(fb->gem_handle > 0);

	igt_debug("%s(handle=%d, pitch=%d)\n",
		  __func__, fb->gem_handle, fb->strides[0]);

	if (fb->modifier || igt_has_fb_modifiers(fd))
		flags = LOCAL_DRM_MODE_FB_MODIFIERS;

	do_or_die(__kms_addfb(fb->fd, fb->gem_handle,
			      fb->width, fb->height,
			      fb->drm_format, fb->modifier,
			      fb->strides, fb->offsets, fb->num_planes, flags,
			      &fb->fb_id));

	return fb->fb_id;
}

/**
 * igt_create_fb:
 * @fd: open i915 drm file descriptor
 * @width: width of the framebuffer in pixel
 * @height: height of the framebuffer in pixel
 * @format: drm fourcc pixel format code
 * @modifier: tiling layout of the framebuffer
 * @fb: pointer to an #igt_fb structure
 *
 * This function allocates a gem buffer object suitable to back a framebuffer
 * with the requested properties and then wraps it up in a drm framebuffer
 * object. All metadata is stored in @fb.
 *
 * The backing storage of the framebuffer is filled with all zeros, i.e. black
 * for rgb pixel formats.
 *
 * Returns:
 * The kms id of the created framebuffer.
 */
unsigned int igt_create_fb(int fd, int width, int height, uint32_t format,
			   uint64_t modifier, struct igt_fb *fb)
{
	return igt_create_fb_with_bo_size(fd, width, height, format, modifier,
					  IGT_COLOR_YCBCR_BT709,
					  IGT_COLOR_YCBCR_LIMITED_RANGE,
					  fb, 0, 0);
}

/**
 * igt_create_color_fb:
 * @fd: open i915 drm file descriptor
 * @width: width of the framebuffer in pixel
 * @height: height of the framebuffer in pixel
 * @format: drm fourcc pixel format code
 * @modifier: tiling layout of the framebuffer
 * @r: red value to use as fill color
 * @g: green value to use as fill color
 * @b: blue value to use as fill color
 * @fb: pointer to an #igt_fb structure
 *
 * This function allocates a gem buffer object suitable to back a framebuffer
 * with the requested properties and then wraps it up in a drm framebuffer
 * object. All metadata is stored in @fb.
 *
 * Compared to igt_create_fb() this function also fills the entire framebuffer
 * with the given color, which is useful for some simple pipe crc based tests.
 *
 * Returns:
 * The kms id of the created framebuffer on success or a negative error code on
 * failure.
 */
unsigned int igt_create_color_fb(int fd, int width, int height,
				 uint32_t format, uint64_t modifier,
				 double r, double g, double b,
				 struct igt_fb *fb /* out */)
{
	unsigned int fb_id;
	cairo_t *cr;

	fb_id = igt_create_fb(fd, width, height, format, modifier, fb);
	igt_assert(fb_id);

#if defined(USE_CAIRO_PIXMAN)
	cr = igt_get_cairo_ctx(fd, fb);
	igt_paint_color(cr, 0, 0, width, height, r, g, b);
	igt_put_cairo_ctx(fd, fb, cr);
#endif

	return fb_id;
}

/**
 * igt_create_pattern_fb:
 * @fd: open i915 drm file descriptor
 * @width: width of the framebuffer in pixel
 * @height: height of the framebuffer in pixel
 * @format: drm fourcc pixel format code
 * @modifier: tiling layout of the framebuffer
 * @fb: pointer to an #igt_fb structure
 *
 * This function allocates a gem buffer object suitable to back a framebuffer
 * with the requested properties and then wraps it up in a drm framebuffer
 * object. All metadata is stored in @fb.
 *
 * Compared to igt_create_fb() this function also draws the standard test pattern
 * into the framebuffer.
 *
 * Returns:
 * The kms id of the created framebuffer on success or a negative error code on
 * failure.
 */
unsigned int igt_create_pattern_fb(int fd, int width, int height,
				   uint32_t format, uint64_t modifier,
				   struct igt_fb *fb /* out */)
{
	unsigned int fb_id;
	cairo_t *cr;

	fb_id = igt_create_fb(fd, width, height, format, modifier, fb);
	igt_assert(fb_id);

#if defined(USE_CAIRO_PIXMAN)
	cr = igt_get_cairo_ctx(fd, fb);
	igt_paint_test_pattern(cr, width, height);
	igt_put_cairo_ctx(fd, fb, cr);
#endif

	return fb_id;
}

/**
 * igt_create_color_pattern_fb:
 * @fd: open i915 drm file descriptor
 * @width: width of the framebuffer in pixel
 * @height: height of the framebuffer in pixel
 * @format: drm fourcc pixel format code
 * @modifier: tiling layout of the framebuffer
 * @r: red value to use as fill color
 * @g: green value to use as fill color
 * @b: blue value to use as fill color
 * @fb: pointer to an #igt_fb structure
 *
 * This function allocates a gem buffer object suitable to back a framebuffer
 * with the requested properties and then wraps it up in a drm framebuffer
 * object. All metadata is stored in @fb.
 *
 * Compared to igt_create_fb() this function also fills the entire framebuffer
 * with the given color, and then draws the standard test pattern into the
 * framebuffer.
 *
 * Returns:
 * The kms id of the created framebuffer on success or a negative error code on
 * failure.
 */
unsigned int igt_create_color_pattern_fb(int fd, int width, int height,
					 uint32_t format, uint64_t modifier,
					 double r, double g, double b,
					 struct igt_fb *fb /* out */)
{
	unsigned int fb_id;
	cairo_t *cr;

	fb_id = igt_create_fb(fd, width, height, format, modifier, fb);
	igt_assert(fb_id);

#if defined(USE_CAIRO_PIXMAN)
	cr = igt_get_cairo_ctx(fd, fb);
	igt_paint_color(cr, 0, 0, width, height, r, g, b);
	igt_paint_test_pattern(cr, width, height);
	igt_put_cairo_ctx(fd, fb, cr);
#endif

	return fb_id;
}

#if defined(USE_CAIRO_PIXMAN)
/**
 * igt_create_image_fb:
 * @drm_fd: open i915 drm file descriptor
 * @width: width of the framebuffer in pixel or 0
 * @height: height of the framebuffer in pixel or 0
 * @format: drm fourcc pixel format code
 * @modifier: tiling layout of the framebuffer
 * @filename: filename of the png image to draw
 * @fb: pointer to an #igt_fb structure
 *
 * Create a framebuffer with the specified image. If @width is zero the
 * image width will be used. If @height is zero the image height will be used.
 *
 * Returns:
 * The kms id of the created framebuffer on success or a negative error code on
 * failure.
 */
unsigned int igt_create_image_fb(int fd, int width, int height,
				 uint32_t format, uint64_t modifier,
				 const char *filename,
				 struct igt_fb *fb /* out */)
{
	cairo_surface_t *image;
	uint32_t fb_id;
	cairo_t *cr;

	image = igt_cairo_image_surface_create_from_png(filename);
	igt_assert(cairo_surface_status(image) == CAIRO_STATUS_SUCCESS);
	if (width == 0)
		width = cairo_image_surface_get_width(image);
	if (height == 0)
		height = cairo_image_surface_get_height(image);
	cairo_surface_destroy(image);

	fb_id = igt_create_fb(fd, width, height, format, modifier, fb);

	cr = igt_get_cairo_ctx(fd, fb);
	igt_paint_image(cr, filename, 0, 0, width, height);
	igt_put_cairo_ctx(fd, fb, cr);

	return fb_id;
}
#endif

struct box {
	int x, y, width, height;
};

struct stereo_fb_layout {
	int fb_width, fb_height;
	struct box left, right;
};

static void box_init(struct box *box, int x, int y, int bwidth, int bheight)
{
	box->x = x;
	box->y = y;
	box->width = bwidth;
	box->height = bheight;
}


static void stereo_fb_layout_from_mode(struct stereo_fb_layout *layout,
				       drmModeModeInfo *mode)
{
	unsigned int format = mode->flags & DRM_MODE_FLAG_3D_MASK;
	const int hdisplay = mode->hdisplay, vdisplay = mode->vdisplay;
	int middle;

	switch (format) {
	case DRM_MODE_FLAG_3D_TOP_AND_BOTTOM:
		layout->fb_width = hdisplay;
		layout->fb_height = vdisplay;

		middle = vdisplay / 2;
		box_init(&layout->left, 0, 0, hdisplay, middle);
		box_init(&layout->right,
			 0, middle, hdisplay, vdisplay - middle);
		break;
	case DRM_MODE_FLAG_3D_SIDE_BY_SIDE_HALF:
		layout->fb_width = hdisplay;
		layout->fb_height = vdisplay;

		middle = hdisplay / 2;
		box_init(&layout->left, 0, 0, middle, vdisplay);
		box_init(&layout->right,
			 middle, 0, hdisplay - middle, vdisplay);
		break;
	case DRM_MODE_FLAG_3D_FRAME_PACKING:
	{
		int vactive_space = mode->vtotal - vdisplay;

		layout->fb_width = hdisplay;
		layout->fb_height = 2 * vdisplay + vactive_space;

		box_init(&layout->left,
			 0, 0, hdisplay, vdisplay);
		box_init(&layout->right,
			 0, vdisplay + vactive_space, hdisplay, vdisplay);
		break;
	}
	default:
		igt_assert(0);
	}
}

/**
 * igt_create_stereo_fb:
 * @drm_fd: open i915 drm file descriptor
 * @mode: A stereo 3D mode.
 * @format: drm fourcc pixel format code
 * @modifier: tiling layout of the framebuffer
 *
 * Create a framebuffer for use with the stereo 3D mode specified by @mode.
 *
 * Returns:
 * The kms id of the created framebuffer on success or a negative error code on
 * failure.
 */
unsigned int igt_create_stereo_fb(int drm_fd, drmModeModeInfo *mode,
				  uint32_t format, uint64_t modifier)
{
	struct stereo_fb_layout layout;
	cairo_t *cr;
	uint32_t fb_id;
	struct igt_fb fb;

	stereo_fb_layout_from_mode(&layout, mode);
	fb_id = igt_create_fb(drm_fd, layout.fb_width, layout.fb_height, format,
			      modifier, &fb);
	cr = igt_get_cairo_ctx(drm_fd, &fb);

	igt_paint_image(cr, "1080p-left.png",
			layout.left.x, layout.left.y,
			layout.left.width, layout.left.height);
	igt_paint_image(cr, "1080p-right.png",
			layout.right.x, layout.right.y,
			layout.right.width, layout.right.height);

	igt_put_cairo_ctx(drm_fd, &fb, cr);

	return fb_id;
}

#if defined(USE_CAIRO_PIXMAN)
static pixman_format_code_t drm_format_to_pixman(uint32_t drm_format)
{
	const struct format_desc_struct *f;

	for_each_format(f)
		if (f->drm_id == drm_format)
			return f->pixman_id;

	igt_assert_f(0, "can't find a pixman format for %08x (%s)\n",
		     drm_format, igt_format_str(drm_format));
}

static cairo_format_t drm_format_to_cairo(uint32_t drm_format)
{
	const struct format_desc_struct *f;

	for_each_format(f)
		if (f->drm_id == drm_format)
			return f->cairo_id;

	igt_assert_f(0, "can't find a cairo format for %08x (%s)\n",
		     drm_format, igt_format_str(drm_format));
}
#endif

struct fb_blit_linear {
	struct igt_fb fb;
	uint8_t *map;
};

struct fb_blit_upload {
	int fd;
	struct igt_fb *fb;
	struct fb_blit_linear linear;
	drm_intel_bufmgr *bufmgr;
	struct intel_batchbuffer *batch;
};

#if defined(USE_CAIRO_PIXMAN)
static bool blitter_ok(const struct igt_fb *fb)
{
	for (int i = 0; i < fb->num_planes; i++) {
		/*
		 * gen4+ stride limit is 4x this with tiling,
		 * but since our blits are always between tiled
		 * and linear surfaces (and we do this check just
		 * for the tiled surface) we must use the lower
		 * linear stride limit here.
		 */
		if (fb->plane_width[i] > 32767 ||
		    fb->plane_height[i] > 32767 ||
		    fb->strides[i] > 32767)
			return false;
	}

	return true;
}

static bool use_rendercopy(const struct igt_fb *fb)
{
	return is_ccs_modifier(fb->modifier) ||
		(fb->modifier == I915_FORMAT_MOD_Yf_TILED &&
		 !blitter_ok(fb));
}

static bool use_blitter(const struct igt_fb *fb)
{
	return (fb->modifier == I915_FORMAT_MOD_Y_TILED ||
		fb->modifier == I915_FORMAT_MOD_Yf_TILED) &&
		blitter_ok(fb);
}

static void init_buf(struct fb_blit_upload *blit,
		     struct igt_buf *buf,
		     const struct igt_fb *fb,
		     const char *name)
{
	igt_assert_eq(fb->offsets[0], 0);

	buf->bo = gem_handle_to_libdrm_bo(blit->bufmgr, blit->fd,
					  name, fb->gem_handle);
	buf->tiling = igt_fb_mod_to_tiling(fb->modifier);
	buf->stride = fb->strides[0];
	buf->bpp = fb->plane_bpp[0];
	buf->size = fb->size;

	if (is_ccs_modifier(fb->modifier)) {
		igt_assert_eq(fb->strides[0] & 127, 0);
		igt_assert_eq(fb->strides[1] & 127, 0);

		buf->aux.offset = fb->offsets[1];
		buf->aux.stride = fb->strides[1];
	}
}

static void fini_buf(struct igt_buf *buf)
{
	drm_intel_bo_unreference(buf->bo);
}

static void rendercopy(struct fb_blit_upload *blit,
		       const struct igt_fb *dst_fb,
		       const struct igt_fb *src_fb)
{
	struct igt_buf src = {}, dst = {};
	igt_render_copyfunc_t render_copy =
		igt_get_render_copyfunc(intel_get_drm_devid(blit->fd));

	igt_require(render_copy);

	igt_assert_eq(dst_fb->offsets[0], 0);
	igt_assert_eq(src_fb->offsets[0], 0);

	init_buf(blit, &src, src_fb, "cairo rendercopy src");
	init_buf(blit, &dst, dst_fb, "cairo rendercopy dst");

	render_copy(blit->batch, NULL,
		    &src, 0, 0, dst_fb->plane_width[0], dst_fb->plane_height[0],
		    &dst, 0, 0);

	fini_buf(&dst);
	fini_buf(&src);
}

static void blitcopy(const struct igt_fb *dst_fb,
		     const struct igt_fb *src_fb)
{
	igt_assert_eq(dst_fb->fd, src_fb->fd);
	igt_assert_eq(dst_fb->num_planes, src_fb->num_planes);

	for (int i = 0; i < dst_fb->num_planes; i++) {
		igt_assert_eq(dst_fb->plane_bpp[i], src_fb->plane_bpp[i]);
		igt_assert_eq(dst_fb->plane_width[i], src_fb->plane_width[i]);
		igt_assert_eq(dst_fb->plane_height[i], src_fb->plane_height[i]);

		igt_blitter_fast_copy__raw(dst_fb->fd,
					   src_fb->gem_handle,
					   src_fb->offsets[i],
					   src_fb->strides[i],
					   igt_fb_mod_to_tiling(src_fb->modifier),
					   0, 0, /* src_x, src_y */
					   dst_fb->plane_width[i], dst_fb->plane_height[i],
					   dst_fb->plane_bpp[i],
					   dst_fb->gem_handle,
					   dst_fb->offsets[i],
					   dst_fb->strides[i],
					   igt_fb_mod_to_tiling(dst_fb->modifier),
					   0, 0 /* dst_x, dst_y */);
	}
}

static void free_linear_mapping(struct fb_blit_upload *blit)
{
	int fd = blit->fd;
	struct igt_fb *fb = blit->fb;
	struct fb_blit_linear *linear = &blit->linear;

	if (igt_vc4_is_tiled(fb->modifier)) {
		void *map = igt_vc4_mmap_bo(fd, fb->gem_handle, fb->size, PROT_WRITE);

		vc4_fb_convert_plane_to_tiled(fb, map, &linear->fb, &linear->map);

		munmap(map, fb->size);
	} else {
		gem_munmap(linear->map, linear->fb.size);
		gem_set_domain(fd, linear->fb.gem_handle,
			I915_GEM_DOMAIN_GTT, 0);

		if (blit->batch)
			rendercopy(blit, fb, &linear->fb);
		else
			blitcopy(fb, &linear->fb);

		gem_sync(fd, linear->fb.gem_handle);
		gem_close(fd, linear->fb.gem_handle);
	}

	if (blit->batch) {
		intel_batchbuffer_free(blit->batch);
		drm_intel_bufmgr_destroy(blit->bufmgr);
	}
}

static void destroy_cairo_surface__gpu(void *arg)
{
	struct fb_blit_upload *blit = arg;

	blit->fb->cairo_surface = NULL;

	free_linear_mapping(blit);

	free(blit);
}

static void setup_linear_mapping(struct fb_blit_upload *blit)
{
	int fd = blit->fd;
	struct igt_fb *fb = blit->fb;
	struct fb_blit_linear *linear = &blit->linear;

	if (!igt_vc4_is_tiled(fb->modifier) && use_rendercopy(fb)) {
		blit->bufmgr = drm_intel_bufmgr_gem_init(fd, 4096);
		blit->batch = intel_batchbuffer_alloc(blit->bufmgr,
						      intel_get_drm_devid(fd));
	}

	/*
	 * We create a linear BO that we'll map for the CPU to write to (using
	 * cairo). This linear bo will be then blitted to its final
	 * destination, tiling it at the same time.
	 */

	igt_init_fb(&linear->fb, fb->fd, fb->width, fb->height,
		    fb->drm_format, LOCAL_DRM_FORMAT_MOD_NONE,
		    fb->color_encoding, fb->color_range);

	create_bo_for_fb(&linear->fb);

	igt_assert(linear->fb.gem_handle > 0);

	if (igt_vc4_is_tiled(fb->modifier)) {
		void *map = igt_vc4_mmap_bo(fd, fb->gem_handle, fb->size, PROT_READ);

		linear->map = igt_vc4_mmap_bo(fd, linear->fb.gem_handle,
					      linear->fb.size,
					      PROT_READ | PROT_WRITE);

		vc4_fb_convert_plane_from_tiled(&linear->fb, &linear->map, fb, map);

		munmap(map, fb->size);
	} else {
		/* Copy fb content to linear BO */
		gem_set_domain(fd, linear->fb.gem_handle,
				I915_GEM_DOMAIN_GTT, 0);

		if (blit->batch)
			rendercopy(blit, &linear->fb, fb);
		else
			blitcopy(&linear->fb, fb);

		gem_sync(fd, linear->fb.gem_handle);

		gem_set_domain(fd, linear->fb.gem_handle,
			I915_GEM_DOMAIN_CPU, I915_GEM_DOMAIN_CPU);

		/* Setup cairo context */
		linear->map = gem_mmap__cpu(fd, linear->fb.gem_handle,
					0, linear->fb.size, PROT_READ | PROT_WRITE);
	}
}

static void create_cairo_surface__gpu(int fd, struct igt_fb *fb)
{
	struct fb_blit_upload *blit;
	cairo_format_t cairo_format;

	blit = calloc(1, sizeof(*blit));
	igt_assert(blit);

	blit->fd = fd;
	blit->fb = fb;
	setup_linear_mapping(blit);

	cairo_format = drm_format_to_cairo(fb->drm_format);
	fb->cairo_surface =
		cairo_image_surface_create_for_data(blit->linear.map,
						    cairo_format,
						    fb->width, fb->height,
						    blit->linear.fb.strides[0]);
	fb->domain = I915_GEM_DOMAIN_GTT;

	cairo_surface_set_user_data(fb->cairo_surface,
				    (cairo_user_data_key_t *)create_cairo_surface__gpu,
				    blit, destroy_cairo_surface__gpu);
}
#endif /*defined(USE_CAIRO_PIXMAN)*/

/**
 * igt_dirty_fb:
 * @fd: open drm file descriptor
 * @fb: pointer to an #igt_fb structure
 *
 * Flushes out the whole framebuffer.
 *
 * Returns: 0 upon success.
 */
int igt_dirty_fb(int fd, struct igt_fb *fb)
{
	return drmModeDirtyFB(fb->fd, fb->fb_id, NULL, 0);
}

static void unmap_bo(struct igt_fb *fb, void *ptr)
{
	gem_munmap(ptr, fb->size);

	if (fb->is_dumb)
		igt_dirty_fb(fb->fd, fb);
}

#if defined(USE_CAIRO_PIXMAN)
static void destroy_cairo_surface__gtt(void *arg)
{
	struct igt_fb *fb = arg;

	unmap_bo(fb, cairo_image_surface_get_data(fb->cairo_surface));
	fb->cairo_surface = NULL;
}
#endif

static void *map_bo(int fd, struct igt_fb *fb)
{
	void *ptr;

	if (is_i915_device(fd))
		gem_set_domain(fd, fb->gem_handle,
			       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);

	if (fb->is_dumb)
		ptr = kmstest_dumb_map_buffer(fd, fb->gem_handle, fb->size,
					      PROT_READ | PROT_WRITE);
	else if (is_i915_device(fd))
		ptr = gem_mmap__gtt(fd, fb->gem_handle, fb->size,
				    PROT_READ | PROT_WRITE);
#if defined(USE_VC4)
	else if (is_vc4_device(fd))
		ptr = igt_vc4_mmap_bo(fd, fb->gem_handle, fb->size,
				      PROT_READ | PROT_WRITE);
#endif
#if defined(USE_AMD)
	else if (is_amdgpu_device(fd))
		ptr = igt_amd_mmap_bo(fd, fb->gem_handle, fb->size,
				      PROT_READ | PROT_WRITE);
#endif
	else
		igt_assert(false);

	return ptr;
}

#if defined(USE_CAIRO_PIXMAN)
static void create_cairo_surface__gtt(int fd, struct igt_fb *fb)
{
	void *ptr = map_bo(fd, fb);

	fb->cairo_surface =
		cairo_image_surface_create_for_data(ptr,
						    drm_format_to_cairo(fb->drm_format),
						    fb->width, fb->height, fb->strides[0]);
	igt_require_f(cairo_surface_status(fb->cairo_surface) == CAIRO_STATUS_SUCCESS,
		      "Unable to create a cairo surface: %s\n",
		      cairo_status_to_string(cairo_surface_status(fb->cairo_surface)));

	fb->domain = I915_GEM_DOMAIN_GTT;

	cairo_surface_set_user_data(fb->cairo_surface,
				    (cairo_user_data_key_t *)create_cairo_surface__gtt,
				    fb, destroy_cairo_surface__gtt);
}
#endif

struct fb_convert_blit_upload {
	struct fb_blit_upload base;

	struct igt_fb shadow_fb;
	uint8_t *shadow_ptr;
};

#if defined(USE_CAIRO_PIXMAN)
static void *igt_fb_create_cairo_shadow_buffer(int fd,
					       unsigned drm_format,
					       unsigned int width,
					       unsigned int height,
					       struct igt_fb *shadow)
{
	void *ptr;

	igt_assert(shadow);

	igt_init_fb(shadow, fd, width, height,
		    drm_format, LOCAL_DRM_FORMAT_MOD_NONE,
		    IGT_COLOR_YCBCR_BT709, IGT_COLOR_YCBCR_LIMITED_RANGE);

	shadow->strides[0] = ALIGN(width * (shadow->plane_bpp[0] / 8), 16);
	shadow->size = ALIGN((uint64_t)shadow->strides[0] * height,
			     sysconf(_SC_PAGESIZE));
	ptr = mmap(NULL, shadow->size, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	igt_assert(ptr != MAP_FAILED);

	return ptr;
}

static void igt_fb_destroy_cairo_shadow_buffer(struct igt_fb *shadow,
					       void *ptr)
{
	munmap(ptr, shadow->size);
}

static uint8_t clamprgb(float val)
{
	return clamp((int)(val + 0.5f), 0, 255);
}

static void read_rgb(struct igt_vec4 *rgb, const uint8_t *rgb24)
{
	rgb->d[0] = rgb24[2];
	rgb->d[1] = rgb24[1];
	rgb->d[2] = rgb24[0];
	rgb->d[3] = 1.0f;
}

static void write_rgb(uint8_t *rgb24, const struct igt_vec4 *rgb)
{
	rgb24[2] = clamprgb(rgb->d[0]);
	rgb24[1] = clamprgb(rgb->d[1]);
	rgb24[0] = clamprgb(rgb->d[2]);
}

struct fb_convert_buf {
	void			*ptr;
	struct igt_fb		*fb;
	bool                     slow_reads;
};

struct fb_convert {
	struct fb_convert_buf	dst;
	struct fb_convert_buf	src;
};

static void *convert_src_get(const struct fb_convert *cvt)
{
	void *buf;

	if (!cvt->src.slow_reads)
		return cvt->src.ptr;

	/*
	 * Reading from the BO is awfully slow because of lack of read caching,
	 * it's faster to copy the whole BO to a temporary buffer and convert
	 * from there.
	 */
	buf = malloc(cvt->src.fb->size);
	if (!buf)
		return cvt->src.ptr;

	igt_memcpy_from_wc(buf, cvt->src.ptr, cvt->src.fb->size);

	return buf;
}

static void convert_src_put(const struct fb_convert *cvt,
			    void *src_buf)
{
	if (src_buf != cvt->src.ptr)
		free(src_buf);
}

struct yuv_parameters {
	unsigned	ay_inc;
	unsigned	uv_inc;
	unsigned	ay_stride;
	unsigned	uv_stride;
	unsigned	a_offset;
	unsigned	y_offset;
	unsigned	u_offset;
	unsigned	v_offset;
};

static void get_yuv_parameters(struct igt_fb *fb, struct yuv_parameters *params)
{
	igt_assert(igt_format_is_yuv(fb->drm_format));

	switch (fb->drm_format) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_P010:
	case DRM_FORMAT_P012:
	case DRM_FORMAT_P016:
		params->ay_inc = 1;
		params->uv_inc = 2;
		break;

	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YVU422:
		params->ay_inc = 1;
		params->uv_inc = 1;
		break;

	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_Y210:
	case DRM_FORMAT_Y212:
	case DRM_FORMAT_Y216:
		params->ay_inc = 2;
		params->uv_inc = 4;
		break;

	case DRM_FORMAT_XVYU12_16161616:
	case DRM_FORMAT_XVYU16161616:
	case DRM_FORMAT_Y412:
	case DRM_FORMAT_Y416:
	case DRM_FORMAT_XYUV8888:
		params->ay_inc = 4;
		params->uv_inc = 4;
		break;
	}

	switch (fb->drm_format) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YVU422:
	case DRM_FORMAT_P010:
	case DRM_FORMAT_P012:
	case DRM_FORMAT_P016:
		params->ay_stride = fb->strides[0];
		params->uv_stride = fb->strides[1];
		break;

	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_Y210:
	case DRM_FORMAT_Y212:
	case DRM_FORMAT_Y216:
	case DRM_FORMAT_XYUV8888:
	case DRM_FORMAT_XVYU12_16161616:
	case DRM_FORMAT_XVYU16161616:
	case DRM_FORMAT_Y412:
	case DRM_FORMAT_Y416:
		params->ay_stride = fb->strides[0];
		params->uv_stride = fb->strides[0];
		break;
	}

	switch (fb->drm_format) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV16:
		params->y_offset = fb->offsets[0];
		params->u_offset = fb->offsets[1];
		params->v_offset = fb->offsets[1] + 1;
		break;

	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV61:
		params->y_offset = fb->offsets[0];
		params->u_offset = fb->offsets[1] + 1;
		params->v_offset = fb->offsets[1];
		break;

	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YUV422:
		params->y_offset = fb->offsets[0];
		params->u_offset = fb->offsets[1];
		params->v_offset = fb->offsets[2];
		break;

	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YVU422:
		params->y_offset = fb->offsets[0];
		params->u_offset = fb->offsets[2];
		params->v_offset = fb->offsets[1];
		break;

	case DRM_FORMAT_P010:
	case DRM_FORMAT_P012:
	case DRM_FORMAT_P016:
		params->y_offset = fb->offsets[0];
		params->u_offset = fb->offsets[1];
		params->v_offset = fb->offsets[1] + 2;
		break;

	case DRM_FORMAT_YUYV:
		params->y_offset = fb->offsets[0];
		params->u_offset = fb->offsets[0] + 1;
		params->v_offset = fb->offsets[0] + 3;
		break;

	case DRM_FORMAT_YVYU:
		params->y_offset = fb->offsets[0];
		params->u_offset = fb->offsets[0] + 3;
		params->v_offset = fb->offsets[0] + 1;
		break;

	case DRM_FORMAT_UYVY:
		params->y_offset = fb->offsets[0] + 1;
		params->u_offset = fb->offsets[0];
		params->v_offset = fb->offsets[0] + 2;
		break;

	case DRM_FORMAT_VYUY:
		params->y_offset = fb->offsets[0] + 1;
		params->u_offset = fb->offsets[0] + 2;
		params->v_offset = fb->offsets[0];
		break;

	case DRM_FORMAT_Y210:
	case DRM_FORMAT_Y212:
	case DRM_FORMAT_Y216:
		params->y_offset = fb->offsets[0];
		params->u_offset = fb->offsets[0] + 2;
		params->v_offset = fb->offsets[0] + 6;
		break;

	case DRM_FORMAT_XVYU12_16161616:
	case DRM_FORMAT_XVYU16161616:
	case DRM_FORMAT_Y412:
	case DRM_FORMAT_Y416:
		params->a_offset = fb->offsets[0] + 6;
		params->y_offset = fb->offsets[0] + 2;
		params->u_offset = fb->offsets[0];
		params->v_offset = fb->offsets[0] + 4;
		break;

	case DRM_FORMAT_XYUV8888:
		params->y_offset = fb->offsets[0] + 1;
		params->u_offset = fb->offsets[0] + 2;
		params->v_offset = fb->offsets[0] + 3;
		break;
	}
}

static void convert_yuv_to_rgb24(struct fb_convert *cvt)
{
	const struct format_desc_struct *src_fmt =
		lookup_drm_format(cvt->src.fb->drm_format);
	int i, j;
	uint8_t bpp = 4;
	uint8_t *y, *u, *v;
	uint8_t *rgb24 = cvt->dst.ptr;
	unsigned int rgb24_stride = cvt->dst.fb->strides[0];
	struct igt_mat4 m = igt_ycbcr_to_rgb_matrix(cvt->src.fb->drm_format,
						    cvt->dst.fb->drm_format,
						    cvt->src.fb->color_encoding,
						    cvt->src.fb->color_range);
	uint8_t *buf;
	struct yuv_parameters params = { };

	igt_assert(cvt->dst.fb->drm_format == DRM_FORMAT_XRGB8888 &&
		   igt_format_is_yuv(cvt->src.fb->drm_format));

	buf = convert_src_get(cvt);
	get_yuv_parameters(cvt->src.fb, &params);
	y = buf + params.y_offset;
	u = buf + params.u_offset;
	v = buf + params.v_offset;

	for (i = 0; i < cvt->dst.fb->height; i++) {
		const uint8_t *y_tmp = y;
		const uint8_t *u_tmp = u;
		const uint8_t *v_tmp = v;
		uint8_t *rgb_tmp = rgb24;

		for (j = 0; j < cvt->dst.fb->width; j++) {
			struct igt_vec4 rgb, yuv;

			yuv.d[0] = *y_tmp;
			yuv.d[1] = *u_tmp;
			yuv.d[2] = *v_tmp;
			yuv.d[3] = 1.0f;

			rgb = igt_matrix_transform(&m, &yuv);
			write_rgb(rgb_tmp, &rgb);

			rgb_tmp += bpp;
			y_tmp += params.ay_inc;

			if ((src_fmt->hsub == 1) || (j % src_fmt->hsub)) {
				u_tmp += params.uv_inc;
				v_tmp += params.uv_inc;
			}
		}

		rgb24 += rgb24_stride;
		y += params.ay_stride;

		if ((src_fmt->vsub == 1) || (i % src_fmt->vsub)) {
			u += params.uv_stride;
			v += params.uv_stride;
		}
	}

	convert_src_put(cvt, buf);
}

static void convert_rgb24_to_yuv(struct fb_convert *cvt)
{
	const struct format_desc_struct *dst_fmt =
		lookup_drm_format(cvt->dst.fb->drm_format);
	int i, j;
	uint8_t *y, *u, *v;
	const uint8_t *rgb24 = cvt->src.ptr;
	uint8_t bpp = 4;
	unsigned rgb24_stride = cvt->src.fb->strides[0];
	struct igt_mat4 m = igt_rgb_to_ycbcr_matrix(cvt->src.fb->drm_format,
						    cvt->dst.fb->drm_format,
						    cvt->dst.fb->color_encoding,
						    cvt->dst.fb->color_range);
	struct yuv_parameters params = { };

	igt_assert(cvt->src.fb->drm_format == DRM_FORMAT_XRGB8888 &&
		   igt_format_is_yuv(cvt->dst.fb->drm_format));

	get_yuv_parameters(cvt->dst.fb, &params);
	y = (uint8_t*)cvt->dst.ptr + params.y_offset;
	u = (uint8_t*)cvt->dst.ptr + params.u_offset;
	v = (uint8_t*)cvt->dst.ptr + params.v_offset;

	for (i = 0; i < cvt->dst.fb->height; i++) {
		const uint8_t *rgb_tmp = rgb24;
		uint8_t *y_tmp = y;
		uint8_t *u_tmp = u;
		uint8_t *v_tmp = v;

		for (j = 0; j < cvt->dst.fb->width; j++) {
			const uint8_t *pair_rgb24 = rgb_tmp;
			struct igt_vec4 pair_rgb, rgb;
			struct igt_vec4 pair_yuv, yuv;

			read_rgb(&rgb, rgb_tmp);
			yuv = igt_matrix_transform(&m, &rgb);

			rgb_tmp += bpp;

			*y_tmp = yuv.d[0];
			y_tmp += params.ay_inc;

			if ((i % dst_fmt->vsub) || (j % dst_fmt->hsub))
				continue;

			/*
			 * We assume the MPEG2 chroma siting convention, where
			 * pixel center for Cb'Cr' is between the left top and
			 * bottom pixel in a 2x2 block, so take the average.
			 *
			 * Therefore, if we use subsampling, we only really care
			 * about two pixels all the time, either the two
			 * subsequent pixels horizontally, vertically, or the
			 * two corners in a 2x2 block.
			 *
			 * The only corner case is when we have an odd number of
			 * pixels, but this can be handled pretty easily by not
			 * incrementing the paired pixel pointer in the
			 * direction it's odd in.
			 */
			if (j != (cvt->dst.fb->width - 1))
				pair_rgb24 += (dst_fmt->hsub - 1) * bpp;

			if (i != (cvt->dst.fb->height - 1))
				pair_rgb24 += rgb24_stride * (dst_fmt->vsub - 1);

			read_rgb(&pair_rgb, pair_rgb24);
			pair_yuv = igt_matrix_transform(&m, &pair_rgb);

			*u_tmp = (yuv.d[1] + pair_yuv.d[1]) / 2.0f;
			*v_tmp = (yuv.d[2] + pair_yuv.d[2]) / 2.0f;

			u_tmp += params.uv_inc;
			v_tmp += params.uv_inc;
		}

		rgb24 += rgb24_stride;
		y += params.ay_stride;

		if ((i % dst_fmt->vsub) == (dst_fmt->vsub - 1)) {
			u += params.uv_stride;
			v += params.uv_stride;
		}
	}
}

static void read_rgbf(struct igt_vec4 *rgb, const float *rgb24)
{
	rgb->d[0] = rgb24[0];
	rgb->d[1] = rgb24[1];
	rgb->d[2] = rgb24[2];
	rgb->d[3] = 1.0f;
}

static void write_rgbf(float *rgb24, const struct igt_vec4 *rgb)
{
	rgb24[0] = rgb->d[0];
	rgb24[1] = rgb->d[1];
	rgb24[2] = rgb->d[2];
}

static void convert_yuv16_to_float(struct fb_convert *cvt, bool alpha)
{
	const struct format_desc_struct *src_fmt =
		lookup_drm_format(cvt->src.fb->drm_format);
	int i, j;
	uint8_t fpp = alpha ? 4 : 3;
	uint16_t *a, *y, *u, *v;
	float *ptr = cvt->dst.ptr;
	unsigned int float_stride = cvt->dst.fb->strides[0] / sizeof(*ptr);
	struct igt_mat4 m = igt_ycbcr_to_rgb_matrix(cvt->src.fb->drm_format,
						    cvt->dst.fb->drm_format,
						    cvt->src.fb->color_encoding,
						    cvt->src.fb->color_range);
	uint16_t *buf;
	struct yuv_parameters params = { };

	igt_assert(cvt->dst.fb->drm_format == IGT_FORMAT_FLOAT &&
		   igt_format_is_yuv(cvt->src.fb->drm_format));

	buf = convert_src_get(cvt);
	get_yuv_parameters(cvt->src.fb, &params);
	igt_assert(!(params.y_offset % sizeof(*buf)) &&
		   !(params.u_offset % sizeof(*buf)) &&
		   !(params.v_offset % sizeof(*buf)));

	a = buf + params.a_offset / sizeof(*buf);
	y = buf + params.y_offset / sizeof(*buf);
	u = buf + params.u_offset / sizeof(*buf);
	v = buf + params.v_offset / sizeof(*buf);

	for (i = 0; i < cvt->dst.fb->height; i++) {
		const uint16_t *a_tmp = a;
		const uint16_t *y_tmp = y;
		const uint16_t *u_tmp = u;
		const uint16_t *v_tmp = v;
		float *rgb_tmp = ptr;

		for (j = 0; j < cvt->dst.fb->width; j++) {
			struct igt_vec4 rgb, yuv;

			yuv.d[0] = *y_tmp;
			yuv.d[1] = *u_tmp;
			yuv.d[2] = *v_tmp;
			yuv.d[3] = 1.0f;

			rgb = igt_matrix_transform(&m, &yuv);
			write_rgbf(rgb_tmp, &rgb);

			if (alpha) {
				rgb_tmp[3] = ((float)*a_tmp) / 65535.f;
				a_tmp += params.ay_inc;
			}

			rgb_tmp += fpp;
			y_tmp += params.ay_inc;

			if ((src_fmt->hsub == 1) || (j % src_fmt->hsub)) {
				u_tmp += params.uv_inc;
				v_tmp += params.uv_inc;
			}
		}

		ptr += float_stride;

		a += params.ay_stride / sizeof(*a);
		y += params.ay_stride / sizeof(*y);

		if ((src_fmt->vsub == 1) || (i % src_fmt->vsub)) {
			u += params.uv_stride / sizeof(*u);
			v += params.uv_stride / sizeof(*v);
		}
	}

	convert_src_put(cvt, buf);
}

static void convert_float_to_yuv16(struct fb_convert *cvt, bool alpha)
{
	const struct format_desc_struct *dst_fmt =
		lookup_drm_format(cvt->dst.fb->drm_format);
	int i, j;
	uint16_t *a, *y, *u, *v;
	const float *ptr = cvt->src.ptr;
	uint8_t fpp = alpha ? 4 : 3;
	unsigned float_stride = cvt->src.fb->strides[0] / sizeof(*ptr);
	struct igt_mat4 m = igt_rgb_to_ycbcr_matrix(cvt->src.fb->drm_format,
						    cvt->dst.fb->drm_format,
						    cvt->dst.fb->color_encoding,
						    cvt->dst.fb->color_range);
	struct yuv_parameters params = { };

	igt_assert(cvt->src.fb->drm_format == IGT_FORMAT_FLOAT &&
		   igt_format_is_yuv(cvt->dst.fb->drm_format));

	get_yuv_parameters(cvt->dst.fb, &params);
	igt_assert(!(params.a_offset % sizeof(*a)) &&
		   !(params.y_offset % sizeof(*y)) &&
		   !(params.u_offset % sizeof(*u)) &&
		   !(params.v_offset % sizeof(*v)));

	a = (uint16_t*)((uint8_t*)cvt->dst.ptr + params.a_offset);
	y = (uint16_t*)((uint8_t*)cvt->dst.ptr + params.y_offset);
	u = (uint16_t*)((uint8_t*)cvt->dst.ptr + params.u_offset);
	v = (uint16_t*)((uint8_t*)cvt->dst.ptr + params.v_offset);

	for (i = 0; i < cvt->dst.fb->height; i++) {
		const float *rgb_tmp = ptr;
		uint16_t *a_tmp = a;
		uint16_t *y_tmp = y;
		uint16_t *u_tmp = u;
		uint16_t *v_tmp = v;

		for (j = 0; j < cvt->dst.fb->width; j++) {
			const float *pair_float = rgb_tmp;
			struct igt_vec4 pair_rgb, rgb;
			struct igt_vec4 pair_yuv, yuv;

			read_rgbf(&rgb, rgb_tmp);
			yuv = igt_matrix_transform(&m, &rgb);

			if (alpha) {
				*a_tmp = rgb_tmp[3] * 65535.f + .5f;
				a_tmp += params.ay_inc;
			}

			rgb_tmp += fpp;

			*y_tmp = yuv.d[0];
			y_tmp += params.ay_inc;

			if ((i % dst_fmt->vsub) || (j % dst_fmt->hsub))
				continue;

			/*
			 * We assume the MPEG2 chroma siting convention, where
			 * pixel center for Cb'Cr' is between the left top and
			 * bottom pixel in a 2x2 block, so take the average.
			 *
			 * Therefore, if we use subsampling, we only really care
			 * about two pixels all the time, either the two
			 * subsequent pixels horizontally, vertically, or the
			 * two corners in a 2x2 block.
			 *
			 * The only corner case is when we have an odd number of
			 * pixels, but this can be handled pretty easily by not
			 * incrementing the paired pixel pointer in the
			 * direction it's odd in.
			 */
			if (j != (cvt->dst.fb->width - 1))
				pair_float += (dst_fmt->hsub - 1) * fpp;

			if (i != (cvt->dst.fb->height - 1))
				pair_float += float_stride * (dst_fmt->vsub - 1);

			read_rgbf(&pair_rgb, pair_float);
			pair_yuv = igt_matrix_transform(&m, &pair_rgb);

			*u_tmp = (yuv.d[1] + pair_yuv.d[1]) / 2.0f;
			*v_tmp = (yuv.d[2] + pair_yuv.d[2]) / 2.0f;

			u_tmp += params.uv_inc;
			v_tmp += params.uv_inc;
		}

		ptr += float_stride;
		a += params.ay_stride / sizeof(*a);
		y += params.ay_stride / sizeof(*y);

		if ((i % dst_fmt->vsub) == (dst_fmt->vsub - 1)) {
			u += params.uv_stride / sizeof(*u);
			v += params.uv_stride / sizeof(*v);
		}
	}
}

static void convert_Y410_to_float(struct fb_convert *cvt, bool alpha)
{
	int i, j;
	const uint32_t *uyv;
	uint32_t *buf;
	float *ptr = cvt->dst.ptr;
	unsigned int float_stride = cvt->dst.fb->strides[0] / sizeof(*ptr);
	unsigned int uyv_stride = cvt->src.fb->strides[0] / sizeof(*uyv);
	struct igt_mat4 m = igt_ycbcr_to_rgb_matrix(cvt->src.fb->drm_format,
						    cvt->dst.fb->drm_format,
						    cvt->src.fb->color_encoding,
						    cvt->src.fb->color_range);
	unsigned bpp = alpha ? 4 : 3;

	igt_assert((cvt->src.fb->drm_format == DRM_FORMAT_Y410 ||
		    cvt->src.fb->drm_format == DRM_FORMAT_XVYU2101010) &&
		   cvt->dst.fb->drm_format == IGT_FORMAT_FLOAT);

	uyv = buf = convert_src_get(cvt);

	for (i = 0; i < cvt->dst.fb->height; i++) {
		for (j = 0; j < cvt->dst.fb->width; j++) {
			/* Convert 2x1 pixel blocks */
			struct igt_vec4 yuv;
			struct igt_vec4 rgb;

			yuv.d[0] = (uyv[j] >> 10) & 0x3ff;
			yuv.d[1] = uyv[j] & 0x3ff;
			yuv.d[2] = (uyv[j] >> 20) & 0x3ff;
			yuv.d[3] = 1.f;

			rgb = igt_matrix_transform(&m, &yuv);

			write_rgbf(&ptr[j * bpp], &rgb);
			if (alpha)
				ptr[j * bpp + 3] = (float)(uyv[j] >> 30) / 3.f;
		}

		ptr += float_stride;
		uyv += uyv_stride;
	}

	convert_src_put(cvt, buf);
}

static void convert_float_to_Y410(struct fb_convert *cvt, bool alpha)
{
	int i, j;
	uint32_t *uyv = cvt->dst.ptr;
	const float *ptr = cvt->src.ptr;
	unsigned float_stride = cvt->src.fb->strides[0] / sizeof(*ptr);
	unsigned uyv_stride = cvt->dst.fb->strides[0] / sizeof(*uyv);
	struct igt_mat4 m = igt_rgb_to_ycbcr_matrix(cvt->src.fb->drm_format,
						    cvt->dst.fb->drm_format,
						    cvt->dst.fb->color_encoding,
						    cvt->dst.fb->color_range);
	unsigned bpp = alpha ? 4 : 3;

	igt_assert(cvt->src.fb->drm_format == IGT_FORMAT_FLOAT &&
		   (cvt->dst.fb->drm_format == DRM_FORMAT_Y410 ||
		    cvt->dst.fb->drm_format == DRM_FORMAT_XVYU2101010));

	for (i = 0; i < cvt->dst.fb->height; i++) {
		for (j = 0; j < cvt->dst.fb->width; j++) {
			struct igt_vec4 rgb;
			struct igt_vec4 yuv;
			uint8_t a = 0;
			uint16_t y, cb, cr;

			read_rgbf(&rgb, &ptr[j * bpp]);
			if (alpha)
				 a = ptr[j * bpp + 3] * 3.f + .5f;

			yuv = igt_matrix_transform(&m, &rgb);
			y = yuv.d[0];
			cb = yuv.d[1];
			cr = yuv.d[2];

			uyv[j] = ((cb & 0x3ff) << 0) |
				  ((y & 0x3ff) << 10) |
				  ((cr & 0x3ff) << 20) |
				  (a << 30);
		}

		ptr += float_stride;
		uyv += uyv_stride;
	}
}

/* { R, G, B, X } */
static const unsigned char swizzle_rgbx[] = { 0, 1, 2, 3 };
static const unsigned char swizzle_bgrx[] = { 2, 1, 0, 3 };

static const unsigned char *rgbx_swizzle(uint32_t format)
{
	switch (format) {
	default:
	case DRM_FORMAT_XRGB16161616F:
	case DRM_FORMAT_ARGB16161616F:
		return swizzle_bgrx;
	case DRM_FORMAT_XBGR16161616F:
	case DRM_FORMAT_ABGR16161616F:
		return swizzle_rgbx;
	}
}

static void convert_fp16_to_float(struct fb_convert *cvt)
{
	int i, j;
	uint16_t *fp16;
	float *ptr = cvt->dst.ptr;
	unsigned int float_stride = cvt->dst.fb->strides[0] / sizeof(*ptr);
	unsigned int fp16_stride = cvt->src.fb->strides[0] / sizeof(*fp16);
	const unsigned char *swz = rgbx_swizzle(cvt->src.fb->drm_format);
	bool needs_reswizzle = swz != swizzle_rgbx;

	uint16_t *buf = convert_src_get(cvt);
	fp16 = buf + cvt->src.fb->offsets[0] / sizeof(*buf);

	for (i = 0; i < cvt->dst.fb->height; i++) {
		if (needs_reswizzle) {
			const uint16_t *fp16_tmp = fp16;
			float *rgb_tmp = ptr;

			for (j = 0; j < cvt->dst.fb->width; j++) {
				struct igt_vec4 rgb;

				igt_half_to_float(fp16_tmp, rgb.d, 4);

				rgb_tmp[0] = rgb.d[swz[0]];
				rgb_tmp[1] = rgb.d[swz[1]];
				rgb_tmp[2] = rgb.d[swz[2]];
				rgb_tmp[3] = rgb.d[swz[3]];

				rgb_tmp += 4;
				fp16_tmp += 4;
			}
		} else {
			igt_half_to_float(fp16, ptr, cvt->dst.fb->width * 4);
		}

		ptr += float_stride;
		fp16 += fp16_stride;
	}

	convert_src_put(cvt, buf);
}

static void convert_float_to_fp16(struct fb_convert *cvt)
{
	int i, j;
	uint16_t *fp16 = (uint16_t*)((uint8_t*)cvt->dst.ptr + cvt->dst.fb->offsets[0]);
	const float *ptr = cvt->src.ptr;
	unsigned float_stride = cvt->src.fb->strides[0] / sizeof(*ptr);
	unsigned fp16_stride = cvt->dst.fb->strides[0] / sizeof(*fp16);
	const unsigned char *swz = rgbx_swizzle(cvt->dst.fb->drm_format);
	bool needs_reswizzle = swz != swizzle_rgbx;

	for (i = 0; i < cvt->dst.fb->height; i++) {
		if (needs_reswizzle) {
			const float *rgb_tmp = ptr;
			uint16_t *fp16_tmp = fp16;

			for (j = 0; j < cvt->dst.fb->width; j++) {
				struct igt_vec4 rgb;

				rgb.d[0] = rgb_tmp[swz[0]];
				rgb.d[1] = rgb_tmp[swz[1]];
				rgb.d[2] = rgb_tmp[swz[2]];
				rgb.d[3] = rgb_tmp[swz[3]];

				igt_float_to_half(rgb.d, fp16_tmp, 4);

				rgb_tmp += 4;
				fp16_tmp += 4;
			}
		} else {
			igt_float_to_half(ptr, fp16, cvt->dst.fb->width * 4);
		}

		ptr += float_stride;
		fp16 += fp16_stride;
	}
}

static void convert_pixman(struct fb_convert *cvt)
{
	pixman_format_code_t src_pixman = drm_format_to_pixman(cvt->src.fb->drm_format);
	pixman_format_code_t dst_pixman = drm_format_to_pixman(cvt->dst.fb->drm_format);
	pixman_image_t *dst_image, *src_image;
	void *src_ptr;

	igt_assert((src_pixman != PIXMAN_invalid) &&
		   (dst_pixman != PIXMAN_invalid));

	/* Pixman requires the stride to be aligned to 32 bits. */
	igt_assert((cvt->src.fb->strides[0] % sizeof(uint32_t)) == 0);
	igt_assert((cvt->dst.fb->strides[0] % sizeof(uint32_t)) == 0);

	src_ptr = convert_src_get(cvt);

	src_image = pixman_image_create_bits(src_pixman,
					     cvt->src.fb->width,
					     cvt->src.fb->height,
					     src_ptr,
					     cvt->src.fb->strides[0]);
	igt_assert(src_image);

	dst_image = pixman_image_create_bits(dst_pixman,
					     cvt->dst.fb->width,
					     cvt->dst.fb->height,
					     cvt->dst.ptr,
					     cvt->dst.fb->strides[0]);
	igt_assert(dst_image);

	pixman_image_composite(PIXMAN_OP_SRC, src_image, NULL, dst_image,
			       0, 0, 0, 0, 0, 0,
			       cvt->dst.fb->width, cvt->dst.fb->height);
	pixman_image_unref(dst_image);
	pixman_image_unref(src_image);

	convert_src_put(cvt, src_ptr);
}

static void fb_convert(struct fb_convert *cvt)
{
	if ((drm_format_to_pixman(cvt->src.fb->drm_format) != PIXMAN_invalid) &&
	    (drm_format_to_pixman(cvt->dst.fb->drm_format) != PIXMAN_invalid)) {
		convert_pixman(cvt);
		return;
	} else if (cvt->dst.fb->drm_format == DRM_FORMAT_XRGB8888) {
		switch (cvt->src.fb->drm_format) {
		case DRM_FORMAT_XYUV8888:
		case DRM_FORMAT_NV12:
		case DRM_FORMAT_NV16:
		case DRM_FORMAT_NV21:
		case DRM_FORMAT_NV61:
		case DRM_FORMAT_UYVY:
		case DRM_FORMAT_VYUY:
		case DRM_FORMAT_YUV420:
		case DRM_FORMAT_YUV422:
		case DRM_FORMAT_YUYV:
		case DRM_FORMAT_YVU420:
		case DRM_FORMAT_YVU422:
		case DRM_FORMAT_YVYU:
			convert_yuv_to_rgb24(cvt);
			return;
		}
	} else if (cvt->src.fb->drm_format == DRM_FORMAT_XRGB8888) {
		switch (cvt->dst.fb->drm_format) {
		case DRM_FORMAT_XYUV8888:
		case DRM_FORMAT_NV12:
		case DRM_FORMAT_NV16:
		case DRM_FORMAT_NV21:
		case DRM_FORMAT_NV61:
		case DRM_FORMAT_UYVY:
		case DRM_FORMAT_VYUY:
		case DRM_FORMAT_YUV420:
		case DRM_FORMAT_YUV422:
		case DRM_FORMAT_YUYV:
		case DRM_FORMAT_YVU420:
		case DRM_FORMAT_YVU422:
		case DRM_FORMAT_YVYU:
			convert_rgb24_to_yuv(cvt);
			return;
		}
	} else if (cvt->dst.fb->drm_format == IGT_FORMAT_FLOAT) {
		switch (cvt->src.fb->drm_format) {
		case DRM_FORMAT_P010:
		case DRM_FORMAT_P012:
		case DRM_FORMAT_P016:
		case DRM_FORMAT_Y210:
		case DRM_FORMAT_Y212:
		case DRM_FORMAT_Y216:
		case DRM_FORMAT_XVYU12_16161616:
		case DRM_FORMAT_XVYU16161616:
			convert_yuv16_to_float(cvt, false);
			return;
		case DRM_FORMAT_Y410:
			convert_Y410_to_float(cvt, true);
			return;
		case DRM_FORMAT_XVYU2101010:
			convert_Y410_to_float(cvt, false);
			return;
		case DRM_FORMAT_Y412:
		case DRM_FORMAT_Y416:
			convert_yuv16_to_float(cvt, true);
			return;
		case DRM_FORMAT_XRGB16161616F:
		case DRM_FORMAT_XBGR16161616F:
		case DRM_FORMAT_ARGB16161616F:
		case DRM_FORMAT_ABGR16161616F:
			convert_fp16_to_float(cvt);
			return;
		}
	} else if (cvt->src.fb->drm_format == IGT_FORMAT_FLOAT) {
		switch (cvt->dst.fb->drm_format) {
		case DRM_FORMAT_P010:
		case DRM_FORMAT_P012:
		case DRM_FORMAT_P016:
		case DRM_FORMAT_Y210:
		case DRM_FORMAT_Y212:
		case DRM_FORMAT_Y216:
		case DRM_FORMAT_XVYU12_16161616:
		case DRM_FORMAT_XVYU16161616:
			convert_float_to_yuv16(cvt, false);
			return;
		case DRM_FORMAT_Y410:
			convert_float_to_Y410(cvt, true);
			return;
		case DRM_FORMAT_XVYU2101010:
			convert_float_to_Y410(cvt, false);
			return;
		case DRM_FORMAT_Y412:
		case DRM_FORMAT_Y416:
			convert_float_to_yuv16(cvt, true);
			return;
		case DRM_FORMAT_XRGB16161616F:
		case DRM_FORMAT_XBGR16161616F:
		case DRM_FORMAT_ARGB16161616F:
		case DRM_FORMAT_ABGR16161616F:
			convert_float_to_fp16(cvt);
			return;
		}
	}

	igt_assert_f(false,
		     "Conversion not implemented (from format 0x%x to 0x%x)\n",
		     cvt->src.fb->drm_format, cvt->dst.fb->drm_format);
}

static void destroy_cairo_surface__convert(void *arg)
{
	struct fb_convert_blit_upload *blit = arg;
	struct igt_fb *fb = blit->base.fb;
	struct fb_convert cvt = {
		.dst	= {
			.ptr	= blit->base.linear.map,
			.fb	= &blit->base.linear.fb,
		},

		.src	= {
			.ptr	= blit->shadow_ptr,
			.fb	= &blit->shadow_fb,
		},
	};

	fb_convert(&cvt);
	igt_fb_destroy_cairo_shadow_buffer(&blit->shadow_fb, blit->shadow_ptr);

	if (blit->base.linear.fb.gem_handle)
		free_linear_mapping(&blit->base);
	else
		unmap_bo(fb, blit->base.linear.map);

	free(blit);

	fb->cairo_surface = NULL;
}

static void create_cairo_surface__convert(int fd, struct igt_fb *fb)
{
	struct fb_convert_blit_upload *blit = calloc(1, sizeof(*blit));
	struct fb_convert cvt = { };
	const struct format_desc_struct *f = lookup_drm_format(fb->drm_format);
	unsigned drm_format;
	cairo_format_t cairo_id;

	if (f->cairo_id != CAIRO_FORMAT_INVALID) {
		cairo_id = f->cairo_id;

		switch (f->cairo_id) {
		case CAIRO_FORMAT_RGB96F:
		case CAIRO_FORMAT_RGBA128F:
			drm_format = IGT_FORMAT_FLOAT;
			break;
		case CAIRO_FORMAT_RGB24:
			drm_format = DRM_FORMAT_XRGB8888;
			break;
		default:
			igt_assert_f(0, "Unsupported format %u", f->cairo_id);
		}
	} else if (PIXMAN_FORMAT_A(f->pixman_id)) {
		cairo_id = CAIRO_FORMAT_ARGB32;
		drm_format = DRM_FORMAT_ARGB8888;
	} else {
		cairo_id = CAIRO_FORMAT_RGB24;
		drm_format = DRM_FORMAT_XRGB8888;
	}

	igt_assert(blit);

	blit->base.fd = fd;
	blit->base.fb = fb;

	blit->shadow_ptr = igt_fb_create_cairo_shadow_buffer(fd, drm_format,
							     fb->width,
							     fb->height,
							     &blit->shadow_fb);
	igt_assert(blit->shadow_ptr);

	if (use_rendercopy(fb) || use_blitter(fb) || igt_vc4_is_tiled(fb->modifier)) {
		setup_linear_mapping(&blit->base);
	} else {
		blit->base.linear.fb = *fb;
		blit->base.linear.fb.gem_handle = 0;
		blit->base.linear.map = map_bo(fd, fb);
		igt_assert(blit->base.linear.map);

		/* reading via gtt mmap is slow */
		cvt.src.slow_reads = is_i915_device(fd);
	}

	cvt.dst.ptr = blit->shadow_ptr;
	cvt.dst.fb = &blit->shadow_fb;
	cvt.src.ptr = blit->base.linear.map;
	cvt.src.fb = &blit->base.linear.fb;
	fb_convert(&cvt);

	fb->cairo_surface =
		cairo_image_surface_create_for_data(blit->shadow_ptr,
						    cairo_id,
						    fb->width, fb->height,
						    blit->shadow_fb.strides[0]);

	cairo_surface_set_user_data(fb->cairo_surface,
				    (cairo_user_data_key_t *)create_cairo_surface__convert,
				    blit, destroy_cairo_surface__convert);
}
#endif /*defined(USE_CAIRO_PIXMAN)*/


/**
 * igt_fb_map_buffer:
 * @fd: open drm file descriptor
 * @fb: pointer to an #igt_fb structure
 *
 * This function will creating a new mapping of the buffer and return a pointer
 * to the content of the supplied framebuffer's plane. This mapping needs to be
 * deleted using igt_fb_unmap_buffer().
 *
 * Returns:
 * A pointer to a buffer with the contents of the framebuffer
 */
void *igt_fb_map_buffer(int fd, struct igt_fb *fb)
{
	return map_bo(fd, fb);
}

/**
 * igt_fb_unmap_buffer:
 * @fb: pointer to the backing igt_fb structure
 * @buffer: pointer to the buffer previously mappped
 *
 * This function will unmap a buffer mapped previously with
 * igt_fb_map_buffer().
 */
void igt_fb_unmap_buffer(struct igt_fb *fb, void *buffer)
{
	return unmap_bo(fb, buffer);
}

#if defined(USE_CAIRO_PIXMAN)
/**
 * igt_get_cairo_surface:
 * @fd: open drm file descriptor
 * @fb: pointer to an #igt_fb structure
 *
 * This function stores the contents of the supplied framebuffer's plane
 * into a cairo surface and returns it.
 *
 * Returns:
 * A pointer to a cairo surface with the contents of the framebuffer.
 */
cairo_surface_t *igt_get_cairo_surface(int fd, struct igt_fb *fb)
{
	const struct format_desc_struct *f = lookup_drm_format(fb->drm_format);

	if (fb->cairo_surface == NULL) {
		if (igt_format_is_yuv(fb->drm_format) ||
		    igt_format_is_fp16(fb->drm_format) ||
		    ((f->cairo_id == CAIRO_FORMAT_INVALID) &&
		     (f->pixman_id != PIXMAN_invalid)))
			create_cairo_surface__convert(fd, fb);
		else if (use_blitter(fb) || use_rendercopy(fb) || igt_vc4_is_tiled(fb->modifier))
			create_cairo_surface__gpu(fd, fb);
		else
			create_cairo_surface__gtt(fd, fb);

		if (f->cairo_id == CAIRO_FORMAT_RGB96F ||
		    f->cairo_id == CAIRO_FORMAT_RGBA128F) {
			cairo_status_t status = cairo_surface_status(fb->cairo_surface);

			igt_skip_on_f(status == CAIRO_STATUS_INVALID_FORMAT &&
				      cairo_version() < CAIRO_VERSION_ENCODE(1, 17, 2),
				      "Cairo version too old, need 1.17.2, have %s\n",
				      cairo_version_string());

			igt_skip_on_f(status == CAIRO_STATUS_NO_MEMORY &&
				      pixman_version() < PIXMAN_VERSION_ENCODE(0, 36, 0),
				      "Pixman version too old, need 0.36.0, have %s\n",
				      pixman_version_string());
		}
	}

	igt_assert(cairo_surface_status(fb->cairo_surface) == CAIRO_STATUS_SUCCESS);
	return fb->cairo_surface;
}

/**
 * igt_get_cairo_ctx:
 * @fd: open i915 drm file descriptor
 * @fb: pointer to an #igt_fb structure
 *
 * This initializes a cairo surface for @fb and then allocates a drawing context
 * for it. The return cairo drawing context should be released by calling
 * igt_put_cairo_ctx(). This also sets a default font for drawing text on
 * framebuffers.
 *
 * Returns:
 * The created cairo drawing context.
 */
cairo_t *igt_get_cairo_ctx(int fd, struct igt_fb *fb)
{
	cairo_surface_t *surface;
	cairo_t *cr;

	surface = igt_get_cairo_surface(fd, fb);
	cr = cairo_create(surface);
	cairo_surface_destroy(surface);
	igt_assert(cairo_status(cr) == CAIRO_STATUS_SUCCESS);

	cairo_select_font_face(cr, "Helvetica", CAIRO_FONT_SLANT_NORMAL,
			       CAIRO_FONT_WEIGHT_NORMAL);
	igt_assert(cairo_status(cr) == CAIRO_STATUS_SUCCESS);

	return cr;
}

/**
 * igt_put_cairo_ctx:
 * @fd: open i915 drm file descriptor
 * @fb: pointer to an #igt_fb structure
 * @cr: the cairo context returned by igt_get_cairo_ctx.
 *
 * This releases the cairo surface @cr returned by igt_get_cairo_ctx()
 * for @fb, and writes the changes out to the framebuffer if cairo doesn't
 * have native support for the format.
 */
void igt_put_cairo_ctx(int fd, struct igt_fb *fb, cairo_t *cr)
{
	cairo_status_t ret = cairo_status(cr);
	igt_assert_f(ret == CAIRO_STATUS_SUCCESS, "Cairo failed to draw with %s\n", cairo_status_to_string(ret));

	cairo_destroy(cr);
}
#endif /*defined(USE_CAIRO_PIXMAN)*/

/**
 * igt_remove_fb:
 * @fd: open i915 drm file descriptor
 * @fb: pointer to an #igt_fb structure
 *
 * This function releases all resources allocated in igt_create_fb() for @fb.
 * Note that if this framebuffer is still in use on a primary plane the kernel
 * will disable the corresponding crtc.
 */
void igt_remove_fb(int fd, struct igt_fb *fb)
{
	if (!fb || !fb->fb_id)
		return;

#if defined(USE_CAIRO_PIXMAN)
	cairo_surface_destroy(fb->cairo_surface);
#endif
	do_or_die(drmModeRmFB(fd, fb->fb_id));
	if (fb->is_dumb)
		kmstest_dumb_destroy(fd, fb->gem_handle);
	else
		gem_close(fd, fb->gem_handle);
	fb->fb_id = 0;
}

#if defined(USE_CAIRO_PIXMAN)
/**
 * igt_fb_convert_with_stride:
 * @dst: pointer to the #igt_fb structure that will store the conversion result
 * @src: pointer to the #igt_fb structure that stores the frame we convert
 * @dst_fourcc: DRM format specifier to convert to
 * @dst_modifier: DRM format modifier to convert to
 * @dst_stride: Stride for the resulting framebuffer (0 for automatic stride)
 *
 * This will convert a given @src content to the @dst_fourcc format,
 * storing the result in the @dst fb, allocating the @dst fb
 * underlying buffer with a stride of @dst_stride stride.
 *
 * Once done with @dst, the caller will have to call igt_remove_fb()
 * on it to free the associated resources.
 *
 * Returns:
 * The kms id of the created framebuffer.
 */
unsigned int igt_fb_convert_with_stride(struct igt_fb *dst, struct igt_fb *src,
					uint32_t dst_fourcc,
					uint64_t dst_modifier,
					unsigned int dst_stride)
{
	/* Use the cairo api to convert */
	cairo_surface_t *surf = igt_get_cairo_surface(src->fd, src);
	cairo_t *cr;
	int fb_id;

	fb_id = igt_create_fb_with_bo_size(src->fd, src->width,
					   src->height, dst_fourcc,
					   dst_modifier,
					   IGT_COLOR_YCBCR_BT709,
					   IGT_COLOR_YCBCR_LIMITED_RANGE,
					   dst, 0,
					   dst_stride);
	igt_assert(fb_id > 0);

	cr = igt_get_cairo_ctx(dst->fd, dst);
	cairo_set_source_surface(cr, surf, 0, 0);
	cairo_paint(cr);
	igt_put_cairo_ctx(dst->fd, dst, cr);

	cairo_surface_destroy(surf);

	return fb_id;
}

/**
 * igt_fb_convert:
 * @dst: pointer to the #igt_fb structure that will store the conversion result
 * @src: pointer to the #igt_fb structure that stores the frame we convert
 * @dst_fourcc: DRM format specifier to convert to
 * @dst_modifier: DRM format modifier to convert to
 *
 * This will convert a given @src content to the @dst_fourcc format,
 * storing the result in the @dst fb, allocating the @dst fb
 * underlying buffer.
 *
 * Once done with @dst, the caller will have to call igt_remove_fb()
 * on it to free the associated resources.
 *
 * Returns:
 * The kms id of the created framebuffer.
 */
unsigned int igt_fb_convert(struct igt_fb *dst, struct igt_fb *src,
			    uint32_t dst_fourcc, uint64_t dst_modifier)
{
	return igt_fb_convert_with_stride(dst, src, dst_fourcc, dst_modifier,
					  0);
}
#endif /*defined(USE_CAIRO_PIXMAN)*/

/**
 * igt_bpp_depth_to_drm_format:
 * @bpp: desired bits per pixel
 * @depth: desired depth
 *
 * Returns:
 * The rgb drm fourcc pixel format code corresponding to the given @bpp and
 * @depth values.  Fails hard if no match was found.
 */
uint32_t igt_bpp_depth_to_drm_format(int bpp, int depth)
{
	const struct format_desc_struct *f;

	for_each_format(f)
		if (f->plane_bpp[0] == bpp && f->depth == depth)
			return f->drm_id;


	igt_assert_f(0, "can't find drm format with bpp=%d, depth=%d\n", bpp,
		     depth);
}

/**
 * igt_drm_format_to_bpp:
 * @drm_format: drm fourcc pixel format code
 *
 * Returns:
 * The bits per pixel for the given drm fourcc pixel format code. Fails hard if
 * no match was found.
 */
uint32_t igt_drm_format_to_bpp(uint32_t drm_format)
{
	const struct format_desc_struct *f = lookup_drm_format(drm_format);

	igt_assert_f(f, "can't find a bpp format for %08x (%s)\n",
		     drm_format, igt_format_str(drm_format));

	return f->plane_bpp[0];
}

/**
 * igt_format_str:
 * @drm_format: drm fourcc pixel format code
 *
 * Returns:
 * Human-readable fourcc pixel format code for @drm_format or "invalid" no match
 * was found.
 */
const char *igt_format_str(uint32_t drm_format)
{
	const struct format_desc_struct *f = lookup_drm_format(drm_format);

	return f ? f->name : "invalid";
}

/**
 * igt_fb_supported_format:
 * @drm_format: drm fourcc to test.
 *
 * This functions returns whether @drm_format can be succesfully created by
 * igt_create_fb() and drawn to by igt_get_cairo_ctx().
 */
bool igt_fb_supported_format(uint32_t drm_format)
{
#if defined (USE_CAIRO_PIXMAN)
	const struct format_desc_struct *f;

	/*
	 * C8 needs a LUT which (at least for the time being)
	 * is the responsibility of each test. Not all tests
	 * have the required code so let's keep C8 hidden from
	 * most eyes.
	 */
	if (drm_format == DRM_FORMAT_C8)
		return false;

	for_each_format(f)
		if (f->drm_id == drm_format)
			return (f->cairo_id != CAIRO_FORMAT_INVALID) ||
				(f->pixman_id != PIXMAN_invalid);

	return false;
#else
	/* If we don't use Cairo/Pixman, all formats are equally good */
	return true;
#endif
}

/**
 * igt_format_is_yuv:
 * @drm_format: drm fourcc
 *
 * This functions returns whether @drm_format is YUV (as opposed to RGB).
 */
bool igt_format_is_yuv(uint32_t drm_format)
{
	switch (drm_format) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YVU422:
	case DRM_FORMAT_P010:
	case DRM_FORMAT_P012:
	case DRM_FORMAT_P016:
	case DRM_FORMAT_Y210:
	case DRM_FORMAT_Y212:
	case DRM_FORMAT_Y216:
	case DRM_FORMAT_XVYU2101010:
	case DRM_FORMAT_XVYU12_16161616:
	case DRM_FORMAT_XVYU16161616:
	case DRM_FORMAT_Y410:
	case DRM_FORMAT_Y412:
	case DRM_FORMAT_Y416:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_XYUV8888:
		return true;
	default:
		return false;
	}
}

/**
 * igt_format_is_fp16
 * @drm_format: drm fourcc
 *
 * Check if the format is fp16.
 */
bool igt_format_is_fp16(uint32_t drm_format)
{
	switch (drm_format) {
	case DRM_FORMAT_XRGB16161616F:
	case DRM_FORMAT_ARGB16161616F:
	case DRM_FORMAT_XBGR16161616F:
	case DRM_FORMAT_ABGR16161616F:
		return true;
	default:
		return false;
	}
}

/**
 * igt_format_plane_bpp:
 * @drm_format: drm fourcc
 * @plane: format plane index
 *
 * This functions returns the number of bits per pixel for the given @plane
 * index of the @drm_format.
 */
int igt_format_plane_bpp(uint32_t drm_format, int plane)
{
	const struct format_desc_struct *format =
		lookup_drm_format(drm_format);

	return format->plane_bpp[plane];
}

/**
 * igt_format_array_fill:
 * @formats_array: a pointer to the formats array pointer to be allocated
 * @count: a pointer to the number of elements contained in the allocated array
 * @allow_yuv: a boolean indicating whether YUV formats should be included
 *
 * This functions allocates and fills a @formats_array that lists the DRM
 * formats current available.
 */
void igt_format_array_fill(uint32_t **formats_array, unsigned int *count,
			   bool allow_yuv)
{
	const struct format_desc_struct *format;
	unsigned int index = 0;

	*count = 0;

	for_each_format(format) {
		if (!allow_yuv && igt_format_is_yuv(format->drm_id))
			continue;

		(*count)++;
	}

	*formats_array = calloc(*count, sizeof(uint32_t));
	igt_assert(*formats_array);

	for_each_format(format) {
		if (!allow_yuv && igt_format_is_yuv(format->drm_id))
			continue;

		(*formats_array)[index++] = format->drm_id;
	}
}
