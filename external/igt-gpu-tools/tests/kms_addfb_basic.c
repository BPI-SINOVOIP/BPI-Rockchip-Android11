/*
 * Copyright © 2013 Intel Corporation
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
 *    Daniel Vetter <daniel.vetter@ffwll.ch>
 *
 */

#include "igt.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "drm.h"
#include "drm_fourcc.h"

#include "igt_rand.h"
#include "igt_device.h"

uint32_t gem_bo;
uint32_t gem_bo_small;

static int legacy_addfb(int fd, struct drm_mode_fb_cmd *arg)
{
	int err;

	err = 0;
	if (igt_ioctl(fd, DRM_IOCTL_MODE_ADDFB, arg))
		err = -errno;

	errno = 0;
	return err;
}

static int rmfb(int fd, uint32_t id)
{
	int err;

	err = 0;
	if (igt_ioctl(fd, DRM_IOCTL_MODE_RMFB, &id))
		err = -errno;

	errno = 0;
	return err;
}

static void invalid_tests(int fd)
{
	struct local_drm_mode_fb_cmd2 f = {};

	f.width = 512;
	f.height = 512;
	f.pixel_format = DRM_FORMAT_XRGB8888;
	f.pitches[0] = 512*4;

	igt_fixture {
		gem_bo = igt_create_bo_with_dimensions(fd, 1024, 1024,
			DRM_FORMAT_XRGB8888, 0, 0, NULL, NULL, NULL);
		igt_assert(gem_bo);
		gem_bo_small = igt_create_bo_with_dimensions(fd, 1024, 1023,
			DRM_FORMAT_XRGB8888, 0, 0, NULL, NULL, NULL);
		igt_assert(gem_bo_small);

		f.handles[0] = gem_bo;

		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f) == 0);
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_RMFB, &f.fb_id) == 0);
		f.fb_id = 0;
	}

	f.flags = LOCAL_DRM_MODE_FB_MODIFIERS;

	igt_subtest("unused-handle") {
		igt_require_fb_modifiers(fd);

		f.handles[1] = gem_bo_small;
		igt_assert(drmIoctl(fd, LOCAL_DRM_IOCTL_MODE_ADDFB2, &f) == -1 &&
			   errno == EINVAL);
		f.handles[1] = 0;
	}

	igt_subtest("unused-pitches") {
		igt_require_fb_modifiers(fd);

		f.pitches[1] = 512;
		igt_assert(drmIoctl(fd, LOCAL_DRM_IOCTL_MODE_ADDFB2, &f) == -1 &&
			   errno == EINVAL);
		f.pitches[1] = 0;
	}

	igt_subtest("unused-offsets") {
		igt_require_fb_modifiers(fd);

		f.offsets[1] = 512;
		igt_assert(drmIoctl(fd, LOCAL_DRM_IOCTL_MODE_ADDFB2, &f) == -1 &&
			   errno == EINVAL);
		f.offsets[1] = 0;
	}

	igt_subtest("unused-modifier") {
		igt_require_fb_modifiers(fd);

		f.modifier[1] =  LOCAL_I915_FORMAT_MOD_X_TILED;
		igt_assert(drmIoctl(fd, LOCAL_DRM_IOCTL_MODE_ADDFB2, &f) == -1 &&
			   errno == EINVAL);
		f.modifier[1] = 0;
	}

	igt_subtest("clobberred-modifier") {
		igt_require_intel(fd);
		f.flags = 0;
		f.modifier[0] = 0;
		gem_set_tiling(fd, gem_bo, I915_TILING_X, 512*4);
		igt_assert(drmIoctl(fd, LOCAL_DRM_IOCTL_MODE_ADDFB2, &f) == 0);
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_RMFB, &f.fb_id) == 0);
		f.fb_id = 0;
		igt_assert(f.modifier[0] == 0);
	}

	igt_subtest("legacy-format") {
		struct {
			/* drm_mode_legacy_fb_format() */
			int bpp, depth;
			int expect;
		} known_formats[] = {
			{  8,  8 }, /* c8 (palette) */
			{ 16, 15 }, /* x1r5g5b5 */
			{ 16, 16 }, /* r5g6b5 or a1r5g5b5! */
			{ 24, 24 }, /* r8g8b8 */
			{ 32, 24 }, /* x8r8g8b8 */
			{ 32, 30 }, /* x2r10g10b10 */
			{ 32, 32 }, /* a8r8g8b8 or a2r10g10b10! */
		};
		struct drm_mode_fb_cmd arg = {
			.handle = f.handles[0],
			.width  = f.width,
			.height = f.height,
			.pitch  = f.pitches[0],
		};
		uint32_t prng = 0x12345678;
		unsigned long timeout = 1;
		unsigned long count = 0;

		/*
		 * First confirm the kernel recognises our known_formats;
		 * some may be invalid for different devices.
		 */
		for (int i = 0; i < ARRAY_SIZE(known_formats); i++) {
			arg.bpp = known_formats[i].bpp;
			arg.depth = known_formats[i].depth;
			known_formats[i].expect = legacy_addfb(fd, &arg);
			igt_debug("{bpp:%d, depth:%d} -> expect:%d\n",
				  arg.bpp, arg.depth, known_formats[i].expect);
			if (arg.fb_id) {
				igt_assert_eq(rmfb(fd, arg.fb_id), 0);
				arg.fb_id = 0;
			}
		}

		igt_until_timeout(timeout) {
			int expect = -EINVAL;
			int err;

			arg.bpp = hars_petruska_f54_1_random(&prng);
			arg.depth = hars_petruska_f54_1_random(&prng);
			for (int start = 0, end = ARRAY_SIZE(known_formats);
			     start < end; ) {
				int mid = start + (end - start) / 2;
				typeof(*known_formats) *tbl = &known_formats[mid];

				if (arg.bpp < tbl->bpp) {
					end = mid;
				} else if (arg.bpp > tbl->bpp) {
					start = mid + 1;
				} else {
					if (arg.depth < tbl->depth) {
						end = mid;
					} else if (arg.depth > tbl->depth) {
						start = mid + 1;
					} else {
						expect = tbl->expect;
						break;
					}
				}
			}

			err = legacy_addfb(fd, &arg);
			igt_assert_f(err == expect,
				     "Expected %d with {bpp:%d, depth:%d}, got %d instead\n",
				     expect, arg.bpp, arg.depth, err);
			if (arg.fb_id) {
				igt_assert_eq(rmfb(fd, arg.fb_id), 0);
				arg.fb_id = 0;
			}

			count++;
		}

		/* After all the abuse, confirm the known_formats */
		for (int i = 0; i < ARRAY_SIZE(known_formats); i++) {
			int err;

			arg.bpp = known_formats[i].bpp;
			arg.depth = known_formats[i].depth;

			err = legacy_addfb(fd, &arg);
			igt_assert_f(err == known_formats[i].expect,
				     "Expected %d with {bpp:%d, depth:%d}, got %d instead\n",
				     known_formats[i].expect,
				     arg.bpp, arg.depth,
				     err);
			if (arg.fb_id) {
				igt_assert_eq(rmfb(fd, arg.fb_id), 0);
				arg.fb_id = 0;
			}
		}

		igt_info("Successfully fuzzed %lu {bpp, depth} variations\n",
			 count);
	}

	igt_fixture {
		gem_close(fd, gem_bo);
		gem_close(fd, gem_bo_small);
	}
}

static void pitch_tests(int fd)
{
	struct drm_mode_fb_cmd2 f = {};
	int bad_pitches[] = { 0, 32, 63, 128, 256, 256*4, 999, 64*1024 };
	int i;

	f.width = 512;
	f.height = 512;
	f.pixel_format = DRM_FORMAT_XRGB8888;
	f.pitches[0] = 1024*4;

	igt_fixture {
		gem_bo = igt_create_bo_with_dimensions(fd, 1024, 1024,
			DRM_FORMAT_XRGB8888, 0, 0, NULL, NULL, NULL);
		igt_assert(gem_bo);
	}

	igt_subtest("no-handle") {
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f) == -1 &&
			   errno == EINVAL);
	}

	f.handles[0] = gem_bo;
	igt_subtest("basic") {
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f) == 0);
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_RMFB, &f.fb_id) == 0);
		f.fb_id = 0;
	}

	for (i = 0; i < ARRAY_SIZE(bad_pitches); i++) {
		igt_subtest_f("bad-pitch-%i", bad_pitches[i]) {
			f.pitches[0] = bad_pitches[i];
			igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f) == -1 &&
				   errno == EINVAL);
		}
	}

	igt_fixture
		gem_close(fd, gem_bo);
}

static void tiling_tests(int fd)
{
	struct drm_mode_fb_cmd2 f = {};
	uint32_t tiled_x_bo = 0;
	uint32_t tiled_y_bo = 0;

	f.width = 512;
	f.height = 512;
	f.pixel_format = DRM_FORMAT_XRGB8888;
	f.pitches[0] = 1024*4;

	igt_subtest_group {
		igt_fixture {
			igt_require_intel(fd);
			tiled_x_bo = igt_create_bo_with_dimensions(fd, 1024, 1024,
				DRM_FORMAT_XRGB8888, LOCAL_I915_FORMAT_MOD_X_TILED,
				1024*4, NULL, NULL, NULL);
			igt_assert(tiled_x_bo);

			tiled_y_bo = igt_create_bo_with_dimensions(fd, 1024, 1024,
				DRM_FORMAT_XRGB8888, LOCAL_I915_FORMAT_MOD_Y_TILED,
				1024*4, NULL, NULL, NULL);
			igt_assert(tiled_y_bo);

			gem_bo = igt_create_bo_with_dimensions(fd, 1024, 1024,
				DRM_FORMAT_XRGB8888, 0, 0, NULL, NULL, NULL);
			igt_assert(gem_bo);
		}

		f.pitches[0] = 1024*4;
		igt_subtest("basic-X-tiled") {
			f.handles[0] = tiled_x_bo;

			igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f) == 0);
			igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_RMFB, &f.fb_id) == 0);
			f.fb_id = 0;
		}

		igt_subtest("framebuffer-vs-set-tiling") {
			f.handles[0] = gem_bo;

			gem_set_tiling(fd, gem_bo, I915_TILING_X, 1024*4);
			igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f) == 0);
			igt_assert(__gem_set_tiling(fd, gem_bo, I915_TILING_X, 512*4) == -EBUSY);
			igt_assert(__gem_set_tiling(fd, gem_bo, I915_TILING_Y, 1024*4) == -EBUSY);
			igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_RMFB, &f.fb_id) == 0);
			f.fb_id = 0;
		}

		f.pitches[0] = 512*4;
		igt_subtest("tile-pitch-mismatch") {
			f.handles[0] = tiled_x_bo;

			igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f) == -1 &&
				   errno == EINVAL);
		}

		f.pitches[0] = 1024*4;
		igt_subtest("basic-Y-tiled") {
			f.handles[0] = tiled_y_bo;

			igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f) == -1 &&
				   errno == EINVAL);
		}

		igt_fixture {
			gem_close(fd, tiled_x_bo);
			gem_close(fd, tiled_y_bo);
		}
	}
}

static void size_tests(int fd)
{
	struct drm_mode_fb_cmd2 f = {};
	struct drm_mode_fb_cmd2 f_16 = {};
	struct drm_mode_fb_cmd2 f_8 = {};

	f.width = 1024;
	f.height = 1024;
	f.pixel_format = DRM_FORMAT_XRGB8888;
	f.pitches[0] = 1024*4;

	f_16.width = 1024;
	f_16.height = 1024*2;
	f_16.pixel_format = DRM_FORMAT_RGB565;
	f_16.pitches[0] = 1024*2;

	f_8.width = 1024*2;
	f_8.height = 1024*2;
	f_8.pixel_format = DRM_FORMAT_C8;
	f_8.pitches[0] = 1024*2;

	igt_fixture {
		gem_bo = igt_create_bo_with_dimensions(fd, 1024, 1024,
			DRM_FORMAT_XRGB8888, 0, 0, NULL, NULL, NULL);
		igt_assert(gem_bo);
		gem_bo_small = igt_create_bo_with_dimensions(fd, 1024, 1023,
			DRM_FORMAT_XRGB8888, 0, 0, NULL, NULL, NULL);
		igt_assert(gem_bo_small);
	}

	f.handles[0] = gem_bo;
	f_16.handles[0] = gem_bo;
	f_8.handles[0] = gem_bo;

	igt_subtest("size-max") {
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f) == 0);
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_RMFB, &f.fb_id) == 0);
		f.fb_id = 0;
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f_16) == 0);
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_RMFB, &f_16.fb_id) == 0);
		f.fb_id = 0;
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f_8) == 0);
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_RMFB, &f_8.fb_id) == 0);
		f.fb_id = 0;
	}

	f.width++;
	f_16.width++;
	f_8.width++;
	igt_subtest("too-wide") {
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f) == -1 &&
			   errno == EINVAL);
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f_16) == -1 &&
			   errno == EINVAL);
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f_8) == -1 &&
			   errno == EINVAL);
	}
	f.width--;
	f_16.width--;
	f_8.width--;
	f.height++;
	f_16.height++;
	f_8.height++;
	igt_subtest("too-high") {
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f) == -1 &&
			   errno == EINVAL);
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f_16) == -1 &&
			   errno == EINVAL);
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f_8) == -1 &&
			   errno == EINVAL);
	}

	f.handles[0] = gem_bo_small;
	igt_subtest("bo-too-small") {
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f) == -1 &&
			   errno == EINVAL);
	}

	/* Just to check that the parameters would work. */
	f.height = 1020;
	igt_subtest("small-bo") {
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f) == 0);
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_RMFB, &f.fb_id) == 0);
		f.fb_id = 0;
	}

	igt_subtest("bo-too-small-due-to-tiling") {
		igt_require_intel(fd);
		gem_set_tiling(fd, gem_bo_small, I915_TILING_X, 1024*4);
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f) == -1 &&
			   errno == EINVAL);
	}


	igt_fixture {
		gem_close(fd, gem_bo);
		gem_close(fd, gem_bo_small);
	}
}

static void addfb25_tests(int fd)
{
	struct local_drm_mode_fb_cmd2 f = {};

	igt_fixture {
		gem_bo = igt_create_bo_with_dimensions(fd, 1024, 1024,
			DRM_FORMAT_XRGB8888, 0, 0, NULL, NULL, NULL);
		igt_assert(gem_bo);

		memset(&f, 0, sizeof(f));

		f.width = 1024;
		f.height = 1024;
		f.pixel_format = DRM_FORMAT_XRGB8888;
		f.pitches[0] = 1024*4;
		f.modifier[0] = LOCAL_DRM_FORMAT_MOD_NONE;

		f.handles[0] = gem_bo;
	}

	igt_subtest("addfb25-modifier-no-flag") {
		igt_require_fb_modifiers(fd);

		f.modifier[0] = LOCAL_I915_FORMAT_MOD_X_TILED;
		igt_assert(drmIoctl(fd, LOCAL_DRM_IOCTL_MODE_ADDFB2, &f) < 0 && errno == EINVAL);
	}

	igt_fixture
		f.flags = LOCAL_DRM_MODE_FB_MODIFIERS;

	igt_subtest("addfb25-bad-modifier") {
		igt_require_fb_modifiers(fd);

		f.modifier[0] = ~0;
		igt_assert(drmIoctl(fd, LOCAL_DRM_IOCTL_MODE_ADDFB2, &f) < 0 && errno == EINVAL);
	}

	igt_subtest_group {
		igt_fixture {
			igt_require_intel(fd);
			gem_set_tiling(fd, gem_bo, I915_TILING_X, 1024*4);
			igt_require_fb_modifiers(fd);
		}

		igt_subtest("addfb25-X-tiled-mismatch") {
			f.modifier[0] = LOCAL_DRM_FORMAT_MOD_NONE;
			igt_assert(drmIoctl(fd, LOCAL_DRM_IOCTL_MODE_ADDFB2, &f) < 0 && errno == EINVAL);
		}

		igt_subtest("addfb25-X-tiled") {
			f.modifier[0] = LOCAL_I915_FORMAT_MOD_X_TILED;
			igt_assert(drmIoctl(fd, LOCAL_DRM_IOCTL_MODE_ADDFB2, &f) == 0);
			igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_RMFB, &f.fb_id) == 0);
			f.fb_id = 0;
		}

		igt_subtest("addfb25-framebuffer-vs-set-tiling") {
			f.modifier[0] = LOCAL_I915_FORMAT_MOD_X_TILED;
			igt_assert(drmIoctl(fd, LOCAL_DRM_IOCTL_MODE_ADDFB2, &f) == 0);
			igt_assert(__gem_set_tiling(fd, gem_bo, I915_TILING_X, 512*4) == -EBUSY);
			igt_assert(__gem_set_tiling(fd, gem_bo, I915_TILING_Y, 1024*4) == -EBUSY);
			igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_RMFB, &f.fb_id) == 0);
			f.fb_id = 0;
		}
	}
	igt_fixture
		gem_close(fd, gem_bo);
}

static int addfb_expected_ret(int fd, uint64_t modifier)
{
	int gen;

	if (!is_i915_device(fd))
		return 0;

	gen = intel_gen(intel_get_drm_devid(fd));

	if (modifier == LOCAL_I915_FORMAT_MOD_Yf_TILED)
		return gen >= 9 && gen < 12 ? 0 : -1;
	return gen >= 9 ? 0 : -1;
}

static void addfb25_ytile(int fd)
{
	struct local_drm_mode_fb_cmd2 f = {};
	int gen;

	igt_fixture {
		gem_bo = igt_create_bo_with_dimensions(fd, 1024, 1024,
			DRM_FORMAT_XRGB8888, 0, 0, NULL, NULL, NULL);
		igt_assert(gem_bo);
		gem_bo_small = igt_create_bo_with_dimensions(fd, 1024, 1023,
			DRM_FORMAT_XRGB8888, 0, 0, NULL, NULL, NULL);
		igt_assert(gem_bo_small);

		memset(&f, 0, sizeof(f));

		f.width = 1024;
		f.height = 1024;
		f.pixel_format = DRM_FORMAT_XRGB8888;
		f.pitches[0] = 1024*4;
		f.flags = LOCAL_DRM_MODE_FB_MODIFIERS;
		f.modifier[0] = LOCAL_DRM_FORMAT_MOD_NONE;

		f.handles[0] = gem_bo;
	}

	igt_subtest("addfb25-Y-tiled") {
		igt_require_fb_modifiers(fd);

		f.modifier[0] = LOCAL_I915_FORMAT_MOD_Y_TILED;
		igt_assert(drmIoctl(fd, LOCAL_DRM_IOCTL_MODE_ADDFB2, &f) ==
			   addfb_expected_ret(fd, f.modifier[0]));
		if (!addfb_expected_ret(fd, f.modifier[0]))
			igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_RMFB, &f.fb_id) == 0);
		f.fb_id = 0;
	}

	igt_subtest("addfb25-Yf-tiled") {
		igt_require_fb_modifiers(fd);

		f.modifier[0] = LOCAL_I915_FORMAT_MOD_Yf_TILED;
		igt_assert(drmIoctl(fd, LOCAL_DRM_IOCTL_MODE_ADDFB2, &f) ==
			   addfb_expected_ret(fd, f.modifier[0]));
		if (!addfb_expected_ret(fd, f.modifier[0]))
			igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_RMFB, &f.fb_id) == 0);
		f.fb_id = 0;
	}

	igt_subtest("addfb25-Y-tiled-small") {
		igt_require_fb_modifiers(fd);

		gen = intel_gen(intel_get_drm_devid(fd));
		igt_require(gen >= 9);

		f.modifier[0] = LOCAL_I915_FORMAT_MOD_Y_TILED;
		f.height = 1023;
		f.handles[0] = gem_bo_small;
		igt_assert(drmIoctl(fd, LOCAL_DRM_IOCTL_MODE_ADDFB2, &f) < 0 && errno == EINVAL);
		f.fb_id = 0;
	}

	igt_fixture {
		gem_close(fd, gem_bo);
		gem_close(fd, gem_bo_small);
	}
}

static void prop_tests(int fd)
{
	struct drm_mode_fb_cmd2 f = {};
	struct drm_mode_obj_get_properties get_props = {};
	struct drm_mode_obj_set_property set_prop = {};
	uint64_t prop, prop_val;

	f.width = 1024;
	f.height = 1024;
	f.pixel_format = DRM_FORMAT_XRGB8888;
	f.pitches[0] = 1024*4;

	igt_fixture {
		gem_bo = igt_create_bo_with_dimensions(fd, 1024, 1024,
			DRM_FORMAT_XRGB8888, 0, 0, NULL, NULL, NULL);
		igt_assert(gem_bo);

		f.handles[0] = gem_bo;

		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f) == 0);
	}

	get_props.props_ptr = (uintptr_t) &prop;
	get_props.prop_values_ptr = (uintptr_t) &prop_val;
	get_props.count_props = 1;
	get_props.obj_id = f.fb_id;

	igt_subtest("invalid-get-prop-any") {
		get_props.obj_type = 0; /* DRM_MODE_OBJECT_ANY */

		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_OBJ_GETPROPERTIES,
				    &get_props) == -1 && errno == EINVAL);
	}

	igt_subtest("invalid-get-prop") {
		get_props.obj_type = DRM_MODE_OBJECT_FB;

		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_OBJ_GETPROPERTIES,
				    &get_props) == -1 && errno == EINVAL);
	}

	set_prop.value = 0;
	set_prop.prop_id = 1;
	set_prop.obj_id = f.fb_id;

	igt_subtest("invalid-set-prop-any") {
		set_prop.obj_type = 0; /* DRM_MODE_OBJECT_ANY */

		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_OBJ_SETPROPERTY,
				    &set_prop) == -1 && errno == EINVAL);
	}

	igt_subtest("invalid-set-prop") {
		set_prop.obj_type = DRM_MODE_OBJECT_FB;

		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_OBJ_SETPROPERTY,
				    &set_prop) == -1 && errno == EINVAL);
	}

	igt_fixture
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_RMFB, &f.fb_id) == 0);

}

static void master_tests(int fd)
{
	struct drm_mode_fb_cmd2 f = {};

	f.width = 1024;
	f.height = 1024;
	f.pixel_format = DRM_FORMAT_XRGB8888;
	f.pitches[0] = 1024*4;

	igt_fixture {
		gem_bo = igt_create_bo_with_dimensions(fd, 1024, 1024,
			DRM_FORMAT_XRGB8888, 0, 0, NULL, NULL, NULL);
		igt_assert(gem_bo);

		f.handles[0] = gem_bo;

		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f) == 0);
	}

	igt_subtest("master-rmfb") {
		int master2_fd;

		igt_device_drop_master(fd);

		master2_fd = drm_open_driver_master(DRIVER_ANY);

		igt_assert_eq(rmfb(master2_fd, f.fb_id), -ENOENT);

		igt_device_drop_master(master2_fd);
		close(master2_fd);

		igt_device_set_master(fd);
	}

	igt_fixture
		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_RMFB, &f.fb_id) == 0);

}

static bool has_addfb2_iface(int fd)
{
	struct local_drm_mode_fb_cmd2 f = {};
	int err;

	err = 0;
	if (drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &f))
		err = -errno;
	switch (err) {
	case -ENOTTY: /* ioctl unrecognised (kernel too old) */
	case -ENOTSUP: /* driver doesn't support KMS */
		return false;

		/*
		 * The only other valid response is -EINVAL, but we leave
		 * that for the actual tests themselves to discover for
		 * more accurate reporting.
		 */
	default:
		return true;
	}
}

int fd;

igt_main
{
	igt_fixture {
		fd = drm_open_driver_master(DRIVER_ANY);
		igt_require(has_addfb2_iface(fd));
	}

	invalid_tests(fd);

	pitch_tests(fd);

	size_tests(fd);

	addfb25_tests(fd);

	addfb25_ytile(fd);

	tiling_tests(fd);

	prop_tests(fd);

	master_tests(fd);

	igt_fixture
		close(fd);
}
