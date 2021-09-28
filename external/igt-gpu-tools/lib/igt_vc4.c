/*
 * Copyright © 2016 Broadcom
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

#include <assert.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "drmtest.h"
#include "igt_aux.h"
#include "igt_core.h"
#include "igt_fb.h"
#include "igt_vc4.h"
#include "ioctl_wrappers.h"
#include "intel_reg.h"
#include "intel_chipset.h"
#include "vc4_drm.h"
#include "vc4_packet.h"

#if NEW_CONTEXT_PARAM_NO_ERROR_CAPTURE_API
#define LOCAL_CONTEXT_PARAM_NO_ERROR_CAPTURE 0x4
#endif

/**
 * SECTION:igt_vc4
 * @short_description: VC4 support library
 * @title: VC4
 * @include: igt.h
 *
 * This library provides various auxiliary helper functions for writing VC4
 * tests.
 */

bool igt_vc4_is_tiled(uint64_t modifier)
{
	if (modifier >> 56ULL != DRM_FORMAT_MOD_VENDOR_BROADCOM)
		return false;

	switch (fourcc_mod_broadcom_mod(modifier)) {
	case DRM_FORMAT_MOD_BROADCOM_SAND32:
	case DRM_FORMAT_MOD_BROADCOM_SAND64:
	case DRM_FORMAT_MOD_BROADCOM_SAND128:
	case DRM_FORMAT_MOD_BROADCOM_SAND256:
	case DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED:
		return true;
	default:
		return false;
	}
}

/**
 * igt_vc4_get_cleared_bo:
 * @fd: device file descriptor
 * @size: size of the BO in bytes
 * @clearval: u32 value that the buffer should be completely cleared with
 *
 * This helper returns a new BO with the given size, which has just been
 * cleared using the render engine.
 */
uint32_t igt_vc4_get_cleared_bo(int fd, size_t size, uint32_t clearval)
{
	/* A single row will be a page. */
	uint32_t width = 1024;
	uint32_t height = size / (width * 4);
	uint32_t handle = igt_vc4_create_bo(fd, size);
	struct drm_vc4_submit_cl submit = {
		.color_write = {
			.hindex = 0,
			.bits = VC4_SET_FIELD(VC4_RENDER_CONFIG_FORMAT_RGBA8888,
					      VC4_RENDER_CONFIG_FORMAT),
		},

		.color_read = { .hindex = ~0 },
		.zs_read = { .hindex = ~0 },
		.zs_write = { .hindex = ~0 },
		.msaa_color_write = { .hindex = ~0 },
		.msaa_zs_write = { .hindex = ~0 },

		.bo_handles = to_user_pointer(&handle),
		.bo_handle_count = 1,
		.width = width,
		.height = height,
		.max_x_tile = ALIGN(width, 64) / 64 - 1,
		.max_y_tile = ALIGN(height, 64) / 64 - 1,
		.clear_color = { clearval, clearval },
		.flags = VC4_SUBMIT_CL_USE_CLEAR_COLOR,
	};

	igt_assert_eq_u32(width * height * 4, size);

	do_ioctl(fd, DRM_IOCTL_VC4_SUBMIT_CL, &submit);

	return handle;
}

int
igt_vc4_create_bo(int fd, size_t size)
{
	struct drm_vc4_create_bo create = {
		.size = size,
	};

	do_ioctl(fd, DRM_IOCTL_VC4_CREATE_BO, &create);

	return create.handle;
}

void *
igt_vc4_mmap_bo(int fd, uint32_t handle, uint32_t size, unsigned prot)
{
	struct drm_vc4_mmap_bo mmap_bo = {
		.handle = handle,
	};
	void *ptr;

	do_ioctl(fd, DRM_IOCTL_VC4_MMAP_BO, &mmap_bo);

	ptr = mmap(0, size, prot, MAP_SHARED, fd, mmap_bo.offset);
	if (ptr == MAP_FAILED)
		return NULL;
	else
		return ptr;
}

void igt_vc4_set_tiling(int fd, uint32_t handle, uint64_t modifier)
{
	struct drm_vc4_set_tiling set = {
		.handle = handle,
		.modifier = modifier,
	};

	do_ioctl(fd, DRM_IOCTL_VC4_SET_TILING, &set);
}

uint64_t igt_vc4_get_tiling(int fd, uint32_t handle)
{
	struct drm_vc4_get_tiling get = {
		.handle = handle,
	};

	do_ioctl(fd, DRM_IOCTL_VC4_GET_TILING, &get);

	return get.modifier;
}

int igt_vc4_get_param(int fd, uint32_t param, uint64_t *val)
{
	struct drm_vc4_get_param arg = {
		.param = param,
	};
	int ret;

	ret = igt_ioctl(fd, DRM_IOCTL_VC4_GET_PARAM, &arg);
	if (ret)
		return ret;

	*val = arg.value;
	return 0;
}

bool igt_vc4_purgeable_bo(int fd, int handle, bool purgeable)
{
	struct drm_vc4_gem_madvise arg = {
		.handle = handle,
		.madv = purgeable ? VC4_MADV_DONTNEED : VC4_MADV_WILLNEED,
	};

	do_ioctl(fd, DRM_IOCTL_VC4_GEM_MADVISE, &arg);

	return arg.retained;
}


/* Calculate the t-tile width so that size = width * height * bpp / 8. */
#define VC4_T_TILE_W(size, height, bpp) ((size) / (height) / ((bpp) / 8))

static size_t igt_vc4_t_tiled_offset(size_t stride, size_t height, size_t bpp,
				     size_t x, size_t y)
{
	const size_t t1k_map_even[] = { 0, 3, 1, 2 };
	const size_t t1k_map_odd[] = { 2, 1, 3, 0 };
	const size_t t4k_t_h = 32;
	const size_t t1k_t_h = 16;
	const size_t t64_t_h = 4;
	size_t offset = 0;
	size_t t4k_t_w, t4k_w, t4k_x, t4k_y;
	size_t t1k_t_w, t1k_x, t1k_y;
	size_t t64_t_w, t64_x, t64_y;
	size_t pix_x, pix_y;
	unsigned int index;

	/* T-tiling is only supported for 16 and 32 bpp. */
	igt_assert(bpp == 16 || bpp == 32);

	/* T-tiling stride must be aligned to the 4K tiles strides. */
	igt_assert((stride % (4096 / t4k_t_h)) == 0);

	/* Calculate the tile width for the bpp. */
	t4k_t_w = VC4_T_TILE_W(4096, t4k_t_h, bpp);
	t1k_t_w = VC4_T_TILE_W(1024, t1k_t_h, bpp);
	t64_t_w = VC4_T_TILE_W(64, t64_t_h, bpp);

	/* Aligned total width in number of 4K tiles. */
	t4k_w = (stride / (bpp / 8)) / t4k_t_w;

	/* X and y coordinates in number of 4K tiles. */
	t4k_x = x / t4k_t_w;
	t4k_y = y / t4k_t_h;

	/* Increase offset to the beginning of the 4K tile row. */
	offset += t4k_y * t4k_w * 4096;

	/* X and Y coordinates in number of 1K tiles within the 4K tile. */
	t1k_x = (x % t4k_t_w) / t1k_t_w;
	t1k_y = (y % t4k_t_h) / t1k_t_h;

	/* Index for 1K tile map lookup. */
	index = 2 * t1k_y + t1k_x;

	/* Odd rows start from the right, even rows from the left. */
	if (t4k_y % 2) {
		/* Increase offset to the 4K tile (starting from the right). */
		offset += (t4k_w - t4k_x - 1) * 4096;

		/* Incrase offset to the beginning of the (odd) 1K tile. */
		offset += t1k_map_odd[index] * 1024;
	} else {
		/* Increase offset to the 4K tile (starting from the left). */
		offset += t4k_x * 4096;

		/* Incrase offset to the beginning of the (even) 1K tile. */
		offset += t1k_map_even[index] * 1024;
	}

	/* X and Y coordinates in number of 64 byte tiles within the 1K tile. */
	t64_x = (x % t1k_t_w) / t64_t_w;
	t64_y = (y % t1k_t_h) / t64_t_h;

	/* Increase offset to the beginning of the 64-byte tile. */
	offset += (t64_y * (t1k_t_w / t64_t_w) + t64_x) * 64;

	/* X and Y coordinates in number of pixels within the 64-byte tile. */
	pix_x = x % t64_t_w;
	pix_y = y % t64_t_h;

	/* Increase offset to the correct pixel. */
	offset += (pix_y * t64_t_w + pix_x) * bpp / 8;

	return offset;
}

static void vc4_fb_convert_plane_to_t_tiled(struct igt_fb *dst, void *dst_buf,
					    struct igt_fb *src, void *src_buf,
					    unsigned int plane)
{
	size_t bpp = src->plane_bpp[plane];
	unsigned int i, j;

	for (i = 0; i < src->height; i++) {
		for (j = 0; j < src->width; j++) {
			size_t src_offset = src->offsets[plane];
			size_t dst_offset = dst->offsets[plane];

			src_offset += src->strides[plane] * i + j * bpp / 8;
			dst_offset += igt_vc4_t_tiled_offset(dst->strides[plane],
							     dst->height,
							     bpp, j, i);

			switch (bpp) {
			case 16:
				*(uint16_t *)(dst_buf + dst_offset) =
					*(uint16_t *)(src_buf + src_offset);
				break;
			case 32:
				*(uint32_t *)(dst_buf + dst_offset) =
					*(uint32_t *)(src_buf + src_offset);
				break;
			}
		}
	}
}

static void vc4_fb_convert_plane_from_t_tiled(struct igt_fb *dst, void *dst_buf,
					      struct igt_fb *src, void *src_buf,
					      unsigned int plane)
{
	size_t bpp = src->plane_bpp[plane];
	unsigned int i, j;

	for (i = 0; i < src->height; i++) {
		for (j = 0; j < src->width; j++) {
			size_t src_offset = src->offsets[plane];
			size_t dst_offset = dst->offsets[plane];

			src_offset += igt_vc4_t_tiled_offset(src->strides[plane],
							     src->height,
							     bpp, j, i);
			src_offset += dst->strides[plane] * i + j * bpp / 8;

			switch (bpp) {
			case 16:
				*(uint16_t *)(dst_buf + dst_offset) =
					*(uint16_t *)(src_buf + src_offset);
				break;
			case 32:
				*(uint32_t *)(dst_buf + dst_offset) =
					*(uint32_t *)(src_buf + src_offset);
				break;
			}
		}
	}
}

static size_t vc4_sand_tiled_offset(size_t column_width, size_t column_size, size_t x,
				    size_t y, size_t bpp)
{
	size_t offset = 0;
	size_t cols_x;
	size_t pix_x;

	/* Offset to the beginning of the relevant column. */
	cols_x = x / column_width;
	offset += cols_x * column_size;

	/* Offset to the relevant pixel. */
	pix_x = x % column_width;
	offset += (column_width * y + pix_x) * bpp / 8;

	return offset;
}

static void vc4_fb_convert_plane_to_sand_tiled(struct igt_fb *dst, void *dst_buf,
					       struct igt_fb *src, void *src_buf,
					       unsigned int plane)
{
	uint64_t modifier_base = fourcc_mod_broadcom_mod(dst->modifier);
	uint32_t column_height = fourcc_mod_broadcom_param(dst->modifier);
	uint32_t column_width_bytes, column_width, column_size;
	size_t bpp = dst->plane_bpp[plane];
	unsigned int i, j;

	switch (modifier_base) {
	case DRM_FORMAT_MOD_BROADCOM_SAND32:
		column_width_bytes = 32;
		break;
	case DRM_FORMAT_MOD_BROADCOM_SAND64:
		column_width_bytes = 64;
		break;
	case DRM_FORMAT_MOD_BROADCOM_SAND128:
		column_width_bytes = 128;
		break;
	case DRM_FORMAT_MOD_BROADCOM_SAND256:
		column_width_bytes = 256;
		break;
	default:
		igt_assert(false);
	}

	column_width = column_width_bytes * dst->plane_width[plane] / dst->width;
	column_size = column_width_bytes * column_height;

	for (i = 0; i < dst->plane_height[plane]; i++) {
		for (j = 0; j < src->plane_width[plane]; j++) {
			size_t src_offset = src->offsets[plane];
			size_t dst_offset = dst->offsets[plane];

			src_offset += src->strides[plane] * i + j * bpp / 8;
			dst_offset += vc4_sand_tiled_offset(column_width,
							    column_size, j, i,
							    bpp);

			switch (bpp) {
			case 8:
				*(uint8_t *)(dst_buf + dst_offset) =
					*(uint8_t *)(src_buf + src_offset);
				break;
			case 16:
				*(uint16_t *)(dst_buf + dst_offset) =
					*(uint16_t *)(src_buf + src_offset);
				break;
			default:
				igt_assert(false);
			}
		}
	}
}

static void vc4_fb_convert_plane_from_sand_tiled(struct igt_fb *dst, void *dst_buf,
						 struct igt_fb *src, void *src_buf,
						 unsigned int plane)
{
	uint64_t modifier_base = fourcc_mod_broadcom_mod(src->modifier);
	uint32_t column_height = fourcc_mod_broadcom_param(src->modifier);
	uint32_t column_width_bytes, column_width, column_size;
	size_t bpp = src->plane_bpp[plane];
	unsigned int i, j;

	switch (modifier_base) {
	case DRM_FORMAT_MOD_BROADCOM_SAND32:
		column_width_bytes = 32;
		break;
	case DRM_FORMAT_MOD_BROADCOM_SAND64:
		column_width_bytes = 64;
		break;
	case DRM_FORMAT_MOD_BROADCOM_SAND128:
		column_width_bytes = 128;
		break;
	case DRM_FORMAT_MOD_BROADCOM_SAND256:
		column_width_bytes = 256;
		break;
	default:
		igt_assert(false);
	}

	column_width = column_width_bytes * src->plane_width[plane] / src->width;
	column_size = column_width_bytes * column_height;

	for (i = 0; i < dst->plane_height[plane]; i++) {
		for (j = 0; j < src->plane_width[plane]; j++) {
			size_t src_offset = src->offsets[plane];
			size_t dst_offset = dst->offsets[plane];

			src_offset += vc4_sand_tiled_offset(column_width,
							    column_size, j, i,
							    bpp);
			dst_offset += dst->strides[plane] * i + j * bpp / 8;

			switch (bpp) {
			case 8:
				*(uint8_t *)(dst_buf + dst_offset) =
					*(uint8_t *)(src_buf + src_offset);
				break;
			case 16:
				*(uint16_t *)(dst_buf + dst_offset) =
					*(uint16_t *)(src_buf + src_offset);
				break;
			default:
				igt_assert(false);
			}
		}
	}
}

void vc4_fb_convert_plane_to_tiled(struct igt_fb *dst, void *dst_buf,
				     struct igt_fb *src, void *src_buf)
{
	unsigned int plane;

	igt_assert(src->modifier == DRM_FORMAT_MOD_LINEAR);
	igt_assert(igt_vc4_is_tiled(dst->modifier));

	for (plane = 0; plane < src->num_planes; plane++) {
		if (dst->modifier == DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED)
			vc4_fb_convert_plane_to_t_tiled(dst, dst_buf, src, src_buf, plane);
		else
			vc4_fb_convert_plane_to_sand_tiled(dst, dst_buf, src, src_buf, plane);
	}
}

void vc4_fb_convert_plane_from_tiled(struct igt_fb *dst, void *dst_buf,
				       struct igt_fb *src, void *src_buf)
{
	unsigned int plane;

	igt_assert(igt_vc4_is_tiled(src->modifier));
	igt_assert(dst->modifier == DRM_FORMAT_MOD_LINEAR);

	for (plane = 0; plane < src->num_planes; plane++) {
		if (src->modifier == DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED)
			vc4_fb_convert_plane_from_t_tiled(dst, dst_buf, src, src_buf, plane);
		else
			vc4_fb_convert_plane_from_sand_tiled(dst, dst_buf, src, src_buf, plane);
	}
}
