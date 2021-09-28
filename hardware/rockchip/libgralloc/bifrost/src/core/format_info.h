/*
 * Copyright (C) 2018-2020 ARM Limited. All rights reserved.
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

#ifndef FORMAT_INFO_H_
#define FORMAT_INFO_H_

#include "mali_gralloc_buffer.h"

typedef uint8_t format_support_flags;

/* Base format unsupported */
#define F_NONE 0
/* Base format supports uncompressed */
#define F_LIN ((uint8_t)1 << 0)
/* Base format supports AFBC */
#define F_AFBC ((uint8_t)1 << 1)
/* Base format supports AFBC with different swizzle */
#define F_AFBC_SWIZ ((uint8_t)1 << 2)


typedef struct
{
	uint16_t width;
	uint16_t height;
} rect_t;


/*
 * Pixel format information.
 *
 * These properties are used by gralloc for buffer allocation.
 * Each format is uniquely identified with 'id'.
 */
typedef struct
{
	uint32_t id;                    /* Format ID. */
	uint8_t npln;                   /* Number of planes. */
	uint8_t ncmp[MAX_PLANES];       /* Number of components in each plane. */
	uint8_t bps;                    /* Bits per sample (primary/largest). */
	uint8_t bpp_afbc[MAX_PLANES];   /* Bits per pixel (AFBC), without implicit padding. 'X' in RGBX is still included. */
	uint8_t bpp[MAX_PLANES];        /* Bits per pixel (linear/uncompressed), including any implicit sample padding defined by format (e.g. 10-bit Y210 padded to 16-bits).
	                                 * NOTE: bpp[n] and/or (bpp[n] * align_w_cpu) must be multiples of 8. */
	uint8_t hsub;                   /* Horizontal sub-sampling (YUV formats). Pixel rounding in width (all formats). Must be a power of 2. */
	uint8_t vsub;                   /* Vertical sub-sampling (YUV formats). Pixel rounding in height (all formats). Must be a power of 2. */
	uint8_t align_w;                /* Alignment of width (per plane, in pixels). Must be a power of 2. NOTE: where 'is_yuv == true', this must be a multiple of 'hsub'. */
	uint8_t align_h;                /* Alignment of height (per plane, in pixels). Must be a power of 2. NOTE: where 'is_yuv == true', this must be a multiple of 'vsub'. */
	uint8_t align_w_cpu;            /* Alignment of width for CPU access (per plane, in pixels). ALIGN_W_CPU_DEFAULT: 1. Must be a power of 2. */
	uint16_t tile_size;             /* Tile size (in pixels), assumed square. Uncompressed only. */
	bool has_alpha;                 /* Alpha channel present. */
	bool is_rgb;                    /* RGB format. */
	bool is_yuv;                    /* YUV format. */
	bool afbc;                      /* AFBC supported (per specification and by gralloc). IP support not considered. */
	bool linear;                    /* Linear/uncompressed supported. */
	bool yuv_transform;             /* Supports AFBC YUV transform: 3+ channel RGB (strict R-G-B-? order) with less than 12-bit per sample. */
	bool flex;                      /* Linear version of format can be represented as flex. */
	/* Computes the total number of components in the format. */
	int total_components() const
	{
		int sum = 0;
		for (auto n: ncmp)
		{
			sum += n;
		}
		return sum;
	}
} format_info_t;

typedef struct
{
	uint32_t id;                       /* Format ID. */
	format_support_flags cpu_wr;       /* CPU producer. */
	format_support_flags cpu_rd;       /* CPU consumer. */
	format_support_flags gpu_wr;       /* GPU producer. */
	format_support_flags gpu_rd;       /* GPU consumer. */
	format_support_flags dpu_wr;       /* DPU producer. */
	format_support_flags dpu_rd;       /* DPU consumer. */
	format_support_flags dpu_aeu_wr;   /* DPU AEU producer. */
	format_support_flags vpu_wr;       /* VPU producer. */
	format_support_flags vpu_rd;       /* VPU consumer. */
	format_support_flags cam_wr;       /* Camera producer. */

} format_ip_support_t;


extern const format_info_t formats[];
extern const format_ip_support_t formats_ip_support[];
extern const size_t num_formats;
extern const size_t num_ip_formats;

extern int32_t get_format_index(const uint32_t base_format);
extern int32_t get_ip_format_index(const uint32_t base_format);
extern uint32_t get_internal_format(const uint32_t base_format, const bool map_to_internal);
void get_format_dataspace(uint32_t base_format,
                          uint64_t usage,
                          int width,
                          int height,
                          android_dataspace_t *dataspace,
                          mali_gralloc_yuv_info *yuv_info);
extern bool sanitize_formats(void);

#endif
