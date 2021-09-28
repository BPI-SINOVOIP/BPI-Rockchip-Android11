/*
 * Copyright (C) 2020 Arm Limited.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "drmutils.h"
#include "mali_gralloc_formats.h"

uint32_t drm_fourcc_from_handle(const private_handle_t *hnd)
{
	/* Clean the modifier bits in the internal format. */
	struct table_entry
	{
		uint64_t internal;
		uint32_t fourcc;
	};

	static table_entry table[] = {
		{ MALI_GRALLOC_FORMAT_INTERNAL_RAW16, DRM_FORMAT_R16 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_RGBA_8888, DRM_FORMAT_ABGR8888 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_BGRA_8888, DRM_FORMAT_ARGB8888 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_RGB_565, DRM_FORMAT_RGB565 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_RGBX_8888, DRM_FORMAT_XBGR8888 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_RGB_888, DRM_FORMAT_BGR888 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_RGBA_1010102, DRM_FORMAT_ABGR2101010 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_RGBA_16161616, DRM_FORMAT_ABGR16161616F },
		{ MALI_GRALLOC_FORMAT_INTERNAL_YV12, DRM_FORMAT_YVU420 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_NV12, DRM_FORMAT_NV12 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_NV16, DRM_FORMAT_NV16 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_NV21, DRM_FORMAT_NV21 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_Y0L2, DRM_FORMAT_Y0L2 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_Y210, DRM_FORMAT_Y210 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_P010, DRM_FORMAT_P010 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_P210, DRM_FORMAT_P210 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_Y410, DRM_FORMAT_Y410 },
		{ MALI_GRALLOC_FORMAT_INTERNAL_YUV422_8BIT, DRM_FORMAT_YUYV },
		{ MALI_GRALLOC_FORMAT_INTERNAL_YUV420_8BIT_I, DRM_FORMAT_YUV420_8BIT },
		{ MALI_GRALLOC_FORMAT_INTERNAL_YUV420_10BIT_I, DRM_FORMAT_YUV420_10BIT },

		/* Deprecated legacy formats, mapped to MALI_GRALLOC_FORMAT_INTERNAL_YUV422_8BIT. */
		{ HAL_PIXEL_FORMAT_YCbCr_422_I, DRM_FORMAT_YUYV },
		/* Deprecated legacy formats, mapped to MALI_GRALLOC_FORMAT_INTERNAL_NV21. */
		{ HAL_PIXEL_FORMAT_YCrCb_420_SP, DRM_FORMAT_NV21 },
		/* Format introduced in Android P, mapped to MALI_GRALLOC_FORMAT_INTERNAL_P010. */
		{ HAL_PIXEL_FORMAT_YCBCR_P010, DRM_FORMAT_P010 },
	};

	const uint64_t unmasked_format = hnd->alloc_format;
	const uint64_t internal_format = (unmasked_format & MALI_GRALLOC_INTFMT_FMT_MASK);
	for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
	{
		if (table[i].internal == internal_format)
		{
			bool afbc = (unmasked_format & MALI_GRALLOC_INTFMT_AFBCENABLE_MASK);
			/* The internal RGB565 format describes two different component orderings depending on AFBC. */
			if (afbc && internal_format == MALI_GRALLOC_FORMAT_INTERNAL_RGB_565)
			{
				return DRM_FORMAT_BGR565;
			}
			return table[i].fourcc;
		}
	}

	return DRM_FORMAT_INVALID;
}

uint64_t drm_modifier_from_handle(const private_handle_t *hnd)
{
	const uint64_t internal_format = hnd->alloc_format;
	if ((internal_format & MALI_GRALLOC_INTFMT_AFBCENABLE_MASK) == 0)
	{
		return 0;
	}

	uint64_t modifier = 0;

	if (internal_format & MALI_GRALLOC_INTFMT_AFBC_SPLITBLK)
	{
		modifier |= AFBC_FORMAT_MOD_SPLIT;
	}

	if (internal_format & MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS)
	{
		modifier |= AFBC_FORMAT_MOD_TILED;
	}

	if (internal_format & MALI_GRALLOC_INTFMT_AFBC_DOUBLE_BODY)
	{
		modifier |= AFBC_FORMAT_MOD_DB;
	}

	if (internal_format & MALI_GRALLOC_INTFMT_AFBC_BCH)
	{
		modifier |= AFBC_FORMAT_MOD_BCH;
	}

	if (internal_format & MALI_GRALLOC_INTFMT_AFBC_YUV_TRANSFORM)
	{
		modifier |= AFBC_FORMAT_MOD_YTR;
	}

	if (internal_format & MALI_GRALLOC_INTFMT_AFBC_SPARSE)
	{
		modifier |= AFBC_FORMAT_MOD_SPARSE;
	}

	/* Extract the block-size modifiers. */
	if (internal_format & MALI_GRALLOC_INTFMT_AFBC_WIDEBLK)
	{
		modifier |= (hnd->is_multi_plane() ? AFBC_FORMAT_MOD_BLOCK_SIZE_32x8_64x4 : AFBC_FORMAT_MOD_BLOCK_SIZE_32x8);
	}
	else if (internal_format & MALI_GRALLOC_INTFMT_AFBC_EXTRAWIDEBLK)
	{
		modifier |= AFBC_FORMAT_MOD_BLOCK_SIZE_64x4;
	}
	else
	{
		modifier |= AFBC_FORMAT_MOD_BLOCK_SIZE_16x16;
	}

	return DRM_FORMAT_MOD_ARM_AFBC(modifier);
}
