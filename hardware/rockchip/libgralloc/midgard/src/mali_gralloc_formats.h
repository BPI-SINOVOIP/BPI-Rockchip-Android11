/*
 * Copyright (C) 2016-2020 ARM Limited. All rights reserved.
 *
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef MALI_GRALLOC_FORMATS_H_
#define MALI_GRALLOC_FORMATS_H_

#include <system/graphics.h>

#include "mali_gralloc_log.h"

/* Internal formats are represented in gralloc as a 64bit identifier
 * where the 32 lower bits are a base format and the 32 upper bits are modifiers.
 *
 * Modifier bits are divided into mutually exclusive ones and those that are not.
 */
/* Internal format type */
typedef uint64_t mali_gralloc_internal_format;

/* Internal format masks */
#define MALI_GRALLOC_INTFMT_FMT_MASK  0x00000000ffffffffULL
#define MALI_GRALLOC_INTFMT_EXT_MASK  0xffffffff00000000ULL
#define MALI_GRALLOC_INTFMT_FMT_WRAP_MASK 0x0000ffffULL
#define MALI_GRALLOC_INTFMT_EXT_WRAP_MASK 0xffff0000ULL
#define MALI_GRALLOC_INTFMT_EXT_WRAP_SHIFT 16

// for mali_so_on_midgard_ddk_r18
#define MALI_GRALLOC_INTFMT_ME_EXT_MASK		MALI_GRALLOC_INTFMT_EXT_MASK

/* Format Modifier Bits Locations */
#define MALI_GRALLOC_INTFMT_EXTENSION_BIT_START 32

/* Internal base formats */

/* Base formats that do not have an identical HAL match
 * are defined starting at the Android private range
 */
#define MALI_GRALLOC_FORMAT_INTERNAL_RANGE_BASE 0x100

typedef enum
{
	MALI_GRALLOC_FORMAT_TYPE_USAGE,
	MALI_GRALLOC_FORMAT_TYPE_INTERNAL,
} mali_gralloc_format_type;

/*
 * Internal formats defined to either match HAL_PIXEL_FORMAT_* or extend
 * where missing. Private formats can be used where no CPU usage is requested.
 * All pixel formats in this list must explicitly define a strict memory
 * layout which can be allocated and used by producer(s) and consumer(s).
 * Flex formats are therefore not included and will be mapped to suitable
 * internal formats.
 */
typedef enum
{
	/* Internal definitions for HAL formats. */
	MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED = 0,
	MALI_GRALLOC_FORMAT_INTERNAL_RGBA_8888 = HAL_PIXEL_FORMAT_RGBA_8888,
	MALI_GRALLOC_FORMAT_INTERNAL_RGBX_8888 = HAL_PIXEL_FORMAT_RGBX_8888,
	MALI_GRALLOC_FORMAT_INTERNAL_RGB_888 = HAL_PIXEL_FORMAT_RGB_888,
	MALI_GRALLOC_FORMAT_INTERNAL_RGB_565 = HAL_PIXEL_FORMAT_RGB_565,
	MALI_GRALLOC_FORMAT_INTERNAL_BGRA_8888 = HAL_PIXEL_FORMAT_BGRA_8888,
	MALI_GRALLOC_FORMAT_INTERNAL_RGBA_1010102 = HAL_PIXEL_FORMAT_RGBA_1010102,
	/* 16-bit floating point format. */
	MALI_GRALLOC_FORMAT_INTERNAL_RGBA_16161616 = HAL_PIXEL_FORMAT_RGBA_FP16,
	MALI_GRALLOC_FORMAT_INTERNAL_YV12 = HAL_PIXEL_FORMAT_YV12,
	MALI_GRALLOC_FORMAT_INTERNAL_Y8 = HAL_PIXEL_FORMAT_Y8,
	MALI_GRALLOC_FORMAT_INTERNAL_Y16 = HAL_PIXEL_FORMAT_Y16,
	MALI_GRALLOC_FORMAT_INTERNAL_NV16 = HAL_PIXEL_FORMAT_YCbCr_422_SP,

	/* Camera specific HAL formats */
	MALI_GRALLOC_FORMAT_INTERNAL_RAW16 = HAL_PIXEL_FORMAT_RAW16,
	MALI_GRALLOC_FORMAT_INTERNAL_RAW12 = HAL_PIXEL_FORMAT_RAW12,
	MALI_GRALLOC_FORMAT_INTERNAL_RAW10 = HAL_PIXEL_FORMAT_RAW10,
	MALI_GRALLOC_FORMAT_INTERNAL_RAW_OPAQUE = HAL_PIXEL_FORMAT_RAW_OPAQUE,
	MALI_GRALLOC_FORMAT_INTERNAL_BLOB = HAL_PIXEL_FORMAT_BLOB,

	/* Depth and stencil formats */
	MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_16 = HAL_PIXEL_FORMAT_DEPTH_16,
	MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_24 = HAL_PIXEL_FORMAT_DEPTH_24,
	MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_24_STENCIL_8 = HAL_PIXEL_FORMAT_DEPTH_24_STENCIL_8,
	MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_32F = HAL_PIXEL_FORMAT_DEPTH_32F,
	MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_32F_STENCIL_8 = HAL_PIXEL_FORMAT_DEPTH_32F_STENCIL_8,
	MALI_GRALLOC_FORMAT_INTERNAL_STENCIL_8 = HAL_PIXEL_FORMAT_STENCIL_8,

	/* Flexible YUV formats would be parsed but not have any representation as
	 * internal format itself but one of the ones below.
	 */

	/* The internal private formats that have no HAL equivivalent are defined
	 * afterwards starting at a specific base range.
	 */
	MALI_GRALLOC_FORMAT_INTERNAL_NV12 = MALI_GRALLOC_FORMAT_INTERNAL_RANGE_BASE,
	MALI_GRALLOC_FORMAT_INTERNAL_NV21,
	MALI_GRALLOC_FORMAT_INTERNAL_YUV422_8BIT,

	/* Extended YUV formats. */
	MALI_GRALLOC_FORMAT_INTERNAL_Y0L2,
	MALI_GRALLOC_FORMAT_INTERNAL_P010,
	MALI_GRALLOC_FORMAT_INTERNAL_P210,
	MALI_GRALLOC_FORMAT_INTERNAL_Y210,
	MALI_GRALLOC_FORMAT_INTERNAL_Y410,

	/*
	 * Single-plane (I = interleaved) variants of 8/10-bit YUV formats,
	 * where previously not defined.
	 */
	MALI_GRALLOC_FORMAT_INTERNAL_YUV420_8BIT_I,
	MALI_GRALLOC_FORMAT_INTERNAL_YUV420_10BIT_I,
	MALI_GRALLOC_FORMAT_INTERNAL_YUV444_10BIT_I,

	/* Add more internal formats here. */

	/* These are legacy 0.3 gralloc formats used only by the wrap/unwrap macros. */
	MALI_GRALLOC_FORMAT_INTERNAL_YV12_WRAP,
	MALI_GRALLOC_FORMAT_INTERNAL_Y8_WRAP,
	MALI_GRALLOC_FORMAT_INTERNAL_Y16_WRAP,

	MALI_GRALLOC_FORMAT_INTERNAL_RANGE_LAST,
} mali_gralloc_pixel_format;

/*
 * Compression type
 */

/* This format will use AFBC */
#define MALI_GRALLOC_INTFMT_AFBC_BASIC (1ULL << (MALI_GRALLOC_INTFMT_EXTENSION_BIT_START + 0))


/*
 * AFBC modifier bits (valid with MALI_GRALLOC_INTFMT_AFBC_BASIC)
 */

/* This format uses AFBC split block mode */
#define MALI_GRALLOC_INTFMT_AFBC_SPLITBLK (1ULL << (MALI_GRALLOC_INTFMT_EXTENSION_BIT_START + 2))

/* This format uses AFBC wide block mode */
#define MALI_GRALLOC_INTFMT_AFBC_WIDEBLK (1ULL << (MALI_GRALLOC_INTFMT_EXTENSION_BIT_START + 3))

/* This format uses AFBC tiled headers */
#define MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS (1ULL << (MALI_GRALLOC_INTFMT_EXTENSION_BIT_START + 4))

/* This format uses AFBC extra wide superblocks. */
#define MALI_GRALLOC_INTFMT_AFBC_EXTRAWIDEBLK (1ULL << (MALI_GRALLOC_INTFMT_EXTENSION_BIT_START + 5))

/* This format is AFBC with double body buffer (used as a frontbuffer) */
#define MALI_GRALLOC_INTFMT_AFBC_DOUBLE_BODY (1ULL << (MALI_GRALLOC_INTFMT_EXTENSION_BIT_START + 6))

/* This format uses AFBC buffer content hints in LSB of superblock offset. */
#define MALI_GRALLOC_INTFMT_AFBC_BCH (1ULL << (MALI_GRALLOC_INTFMT_EXTENSION_BIT_START + 7))

/* This format uses AFBC with YUV transform. */
#define MALI_GRALLOC_INTFMT_AFBC_YUV_TRANSFORM (1ULL << (MALI_GRALLOC_INTFMT_EXTENSION_BIT_START + 8))

/* This format uses Sparse allocated AFBC. */
#define MALI_GRALLOC_INTFMT_AFBC_SPARSE (1ULL << (MALI_GRALLOC_INTFMT_EXTENSION_BIT_START + 9))

/* This mask should be used to check or clear support for AFBC for an internal format.
 */
#define MALI_GRALLOC_INTFMT_AFBCENABLE_MASK (uint64_t)(MALI_GRALLOC_INTFMT_AFBC_BASIC)


/* These are legacy Gralloc 0.3 support macros for passing private formats through the 0.3 alloc interface.
 * It packs modifier bits together with base format into a 32 bit format identifier.
 * Gralloc 1.0 interface should use private functions to set private buffer format in the buffer descriptor.
 *
 * Packing:
 *
 * Bits 15-0:    mali_gralloc_pixel_format format
 * Bits 31-16:   modifier bits
 */
static inline int mali_gralloc_format_wrapper(int format, int modifiers)
{
	/* Internal formats that are identical to HAL formats
	 * have the same definition. This is convenient for
	 * client parsing code to not have to parse them separately.
	 *
	 * For 3 of the HAL YUV formats that have very large definitions
	 * this causes problems for packing in modifier bits.
	 * Because of this reason we redefine these three formats
	 * while packing/unpacking them.
	 */
	if (format == MALI_GRALLOC_FORMAT_INTERNAL_YV12)
	{
		format = MALI_GRALLOC_FORMAT_INTERNAL_YV12_WRAP;
	}
	else if (format == MALI_GRALLOC_FORMAT_INTERNAL_Y8)
	{
		format = MALI_GRALLOC_FORMAT_INTERNAL_Y8_WRAP;
	}
	else if (format == MALI_GRALLOC_FORMAT_INTERNAL_Y16)
	{
		format = MALI_GRALLOC_FORMAT_INTERNAL_Y16_WRAP;
	}

	if (format & ~MALI_GRALLOC_INTFMT_FMT_WRAP_MASK)
	{
		format = MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED;
		MALI_GRALLOC_LOGE("Format is too large for private format wrapping");
	}

	return (modifiers | format);
}

static inline uint64_t mali_gralloc_format_unwrap(int x)
{
	uint64_t internal_format = (uint64_t)(((((uint64_t)(x)) & MALI_GRALLOC_INTFMT_EXT_WRAP_MASK) << MALI_GRALLOC_INTFMT_EXT_WRAP_SHIFT) | // Modifier bits
	                                       (((uint64_t)(x)) & MALI_GRALLOC_INTFMT_FMT_WRAP_MASK)); // Private format

	uint64_t base_format = internal_format & MALI_GRALLOC_INTFMT_FMT_MASK;
	uint64_t modifiers = internal_format & MALI_GRALLOC_INTFMT_EXT_MASK;

	if (base_format == MALI_GRALLOC_FORMAT_INTERNAL_YV12_WRAP)
	{
		base_format = MALI_GRALLOC_FORMAT_INTERNAL_YV12;
	}
	else if (base_format == MALI_GRALLOC_FORMAT_INTERNAL_Y8_WRAP)
	{
	 	base_format = MALI_GRALLOC_FORMAT_INTERNAL_Y8;
	}
	else if (base_format == MALI_GRALLOC_FORMAT_INTERNAL_Y16_WRAP)
	{
		base_format = MALI_GRALLOC_FORMAT_INTERNAL_Y16;
	}

	return (modifiers | base_format);
}

/*
 * Macro to add additional modifier(s) to existing wrapped private format.
 * Arguments include wrapped private format and new modifier(s) to add.
 */
#define GRALLOC_PRIVATE_FORMAT_WRAPPER_ADD_MODIFIER(x, modifiers) \
	((int)((x) | ((unsigned)((modifiers) >> MALI_GRALLOC_INTFMT_EXT_WRAP_SHIFT))))

/*
 * Macro to remove modifier(s) to existing wrapped private format.
 * Arguments include wrapped private format and modifier(s) to remove.
 */
#define GRALLOC_PRIVATE_FORMAT_WRAPPER_REMOVE_MODIFIER(x, modifiers) \
	((int)((x) & ~((unsigned)((modifiers) >> MALI_GRALLOC_INTFMT_EXT_WRAP_SHIFT))))

#define GRALLOC_PRIVATE_FORMAT_WRAPPER(x) (mali_gralloc_format_wrapper(x, 0))

#define GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC(x)                                                           \
	mali_gralloc_format_wrapper(x, (MALI_GRALLOC_INTFMT_AFBC_BASIC | MALI_GRALLOC_INTFMT_AFBC_SPARSE) >> \
	                                   MALI_GRALLOC_INTFMT_EXT_WRAP_SHIFT)

#define GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_SPLITBLK(x)                                 \
	GRALLOC_PRIVATE_FORMAT_WRAPPER_ADD_MODIFIER(GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC(x), \
	                                            MALI_GRALLOC_INTFMT_AFBC_SPLITBLK)

#define GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_WIDEBLK(x)                                  \
	GRALLOC_PRIVATE_FORMAT_WRAPPER_ADD_MODIFIER(GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC(x), \
	                                            MALI_GRALLOC_INTFMT_AFBC_WIDEBLK)

#define GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_WIDE_SPLIT(x)                                        \
	GRALLOC_PRIVATE_FORMAT_WRAPPER_ADD_MODIFIER(GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_SPLITBLK(x), \
	                                            MALI_GRALLOC_INTFMT_AFBC_WIDEBLK)

#define GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_TILED_HEADERS_BASIC(x)                      \
	GRALLOC_PRIVATE_FORMAT_WRAPPER_ADD_MODIFIER(GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC(x), \
	                                            MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS)

#define GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_TILED_HEADERS_WIDE(x)                               \
	GRALLOC_PRIVATE_FORMAT_WRAPPER_ADD_MODIFIER(GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_WIDEBLK(x), \
	                                            MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS)

#define GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_TILED_HEADERS_SPLIT(x)                               \
	GRALLOC_PRIVATE_FORMAT_WRAPPER_ADD_MODIFIER(GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_SPLITBLK(x), \
	                                            MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS)

#define GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_TILED_HEADERS_WIDE_SPLIT(x)                            \
	GRALLOC_PRIVATE_FORMAT_WRAPPER_ADD_MODIFIER(GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_WIDE_SPLIT(x), \
	                                            MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS)

/*
 * AFBC format with extra-wide (64x4) superblocks.
 *
 * NOTE: Tiled headers are mandatory for this format.
 */
#define GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_EXTRAWIDEBLK(x)                                                 \
	GRALLOC_PRIVATE_FORMAT_WRAPPER_ADD_MODIFIER(GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_TILED_HEADERS_BASIC(x), \
	                                            MALI_GRALLOC_INTFMT_AFBC_EXTRAWIDEBLK)

/*
 * AFBC multi-plane YUV format where luma (wide, 32x8) and
 * chroma (extra-wide, 64x4) planes are stored in separate AFBC buffers.
 *
 * NOTE: Tiled headers are mandatory for this format.
 * NOTE: Base format (x) must be a multi-plane YUV format.
 */
#define GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_WIDE_EXTRAWIDE(x)                                        \
	GRALLOC_PRIVATE_FORMAT_WRAPPER_ADD_MODIFIER(GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_EXTRAWIDEBLK(x), \
	                                            MALI_GRALLOC_INTFMT_AFBC_WIDEBLK)

#define GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_TILED_DOUBLE_BODY(x)                                            \
	GRALLOC_PRIVATE_FORMAT_WRAPPER_ADD_MODIFIER(GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_TILED_HEADERS_BASIC(x), \
	                                            MALI_GRALLOC_INTFMT_AFBC_DOUBLE_BODY)

#define GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_TILED_SPLIT_DOUBLE_BODY(x)                                      \
	GRALLOC_PRIVATE_FORMAT_WRAPPER_ADD_MODIFIER(GRALLOC_PRIVATE_FORMAT_WRAPPER_AFBC_TILED_HEADERS_SPLIT(x), \
	                                            MALI_GRALLOC_INTFMT_AFBC_DOUBLE_BODY)

#define GRALLOC_PRIVATE_FORMAT_UNWRAP(x) mali_gralloc_format_unwrap(x)

/* IP block capability masks */
#define MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT ((uint64_t)1 << 0)

/* For IPs which can't read/write YUV with AFBC encoding use flag AFBC_YUV_READ/AFBC_YUV_WRITE */
#define MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_BASIC ((uint64_t)1 << 1)
#define MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_SPLITBLK ((uint64_t)1 << 2)
#define MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WIDEBLK ((uint64_t)1 << 3)
#define MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WRITE_NON_SPARSE ((uint64_t)1 << 4)
#define MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_RESERVED_2 ((uint64_t)1 << 5)
#define MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_RESERVED_3 ((uint64_t)1 << 6)
#define MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_TILED_HEADERS ((uint64_t)1 << 7)
#define MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_EXTRAWIDEBLK ((uint64_t)1 << 8)
#define MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_MULTIPLANE_READ ((uint64_t)1 << 9)
#define MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_DOUBLE_BODY ((uint64_t)1 << 10)
#define MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_YUV_READ ((uint64_t)1 << 11)
#define MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_YUV_WRITE ((uint64_t)1 << 12)
#define MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_RGBA16161616 ((uint64_t)1 << 13)


#define MALI_GRALLOC_FORMAT_CAPABILITY_PIXFMT_RGBA1010102 ((uint64_t)1 << 32)
#define MALI_GRALLOC_FORMAT_CAPABILITY_PIXFMT_RGBA16161616 ((uint64_t)1 << 33)

#define MALI_GRALLOC_FORMAT_CAPABILITY_AFBCENABLE_MASK                                                     \
	((uint64_t)(MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_BASIC | MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_SPLITBLK | \
	            MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WIDEBLK | MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_TILED_HEADERS))

struct mali_gralloc_format_caps
{
	uint64_t caps_mask;
};
typedef struct mali_gralloc_format_caps mali_gralloc_format_caps;

#define MALI_GRALLOC_FORMATCAPS_SYM_NAME mali_gralloc_format_capabilities
#define MALI_GRALLOC_FORMATCAPS_SYM_NAME_STR "mali_gralloc_format_capabilities"

/* Internal prototypes */
#if defined(GRALLOC_LIBRARY_BUILD)

void mali_gralloc_adjust_dimensions(const uint64_t internal_format,
                                    const uint64_t usage,
                                    int* const width,
                                    int* const height);

uint64_t mali_gralloc_select_format(const int width,
				    const int height,
				    const uint64_t req_format,
                                    const mali_gralloc_format_type type,
                                    const uint64_t usage,
                                    const int buffer_size,
                                    uint64_t * const internal_format);

bool is_subsampled_yuv(const uint32_t base_format);

bool is_base_format_used_by_rk_video(const uint32_t base_format);

#endif

#endif /* MALI_GRALLOC_FORMATS_H_ */
