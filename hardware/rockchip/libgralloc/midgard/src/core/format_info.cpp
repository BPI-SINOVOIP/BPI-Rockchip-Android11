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
#include <inttypes.h>
#include "gralloc_helper.h"
#include "mali_gralloc_formats.h"
#include "format_info.h"
#include "mali_gralloc_usages.h"

/* Default width aligned to whole pixel (CPU access). */
#define ALIGN_W_CPU_DEFAULT .align_w_cpu = 1

/*
 * Format table, containing format properties.
 *
 * NOTE: This table should only be used within
 * the gralloc library and not by clients directly.
 */
const format_info_t formats[] = {
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_RGB_565,
		.npln = 1, .ncmp = { 3, 0, 0 }, .bps = 6, .bpp_afbc = { 16, 0, 0 }, .bpp = { 16, 0, 0 },
		.hsub = 0, .vsub = 0, .align_w = 1, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = true, .is_yuv = false,
		.afbc = true, .linear = true, .yuv_transform = true, .flex = false,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_RGB_888,
		.npln = 1, .ncmp = { 3, 0, 0 }, .bps = 8, .bpp_afbc = { 24, 0, 0 }, .bpp = { 24, 0, 0 },
		.hsub = 0, .vsub = 0, .align_w = 1, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = true, .is_yuv = false,
		.afbc = true, .linear = true, .yuv_transform = true, .flex = true,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_RGBA_8888,
		.npln = 1, .ncmp = { 4, 0, 0 }, .bps = 8, .bpp_afbc = { 32, 0, 0 }, .bpp = { 32, 0, 0 },
		.hsub = 0, .vsub = 0, .align_w = 1, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = true, .is_rgb = true, .is_yuv = false,
		.afbc = true, .linear = true, .yuv_transform = true, .flex = true,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_BGRA_8888,
		.npln = 1, .ncmp = { 4, 0, 0 }, .bps = 8, .bpp_afbc = { 32, 0, 0 }, .bpp = { 32, 0, 0 },
		.hsub = 0, .vsub = 0, .align_w = 1, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = true, .is_rgb = true, .is_yuv = false,
		.afbc = true, .linear = true, .yuv_transform = false, .flex = true,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_RGBX_8888,
		.npln = 1, .ncmp = { 3, 0, 0 }, .bps = 8, .bpp_afbc = { 32, 0, 0 }, .bpp = { 32, 0, 0 },
		.hsub = 0, .vsub = 0, .align_w = 1, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = true, .is_yuv = false,
		.afbc = true, .linear = true, .yuv_transform = true, .flex = true,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_RGBA_1010102,
		.npln = 1, .ncmp = { 4, 0, 0 }, .bps = 10, .bpp_afbc = { 32, 0, 0 }, .bpp = { 32, 0, 0 },
		.hsub = 0, .vsub = 0, .align_w = 1, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = true, .is_rgb = true, .is_yuv = false,
		.afbc = true, .linear = true, .yuv_transform = true, .flex = false,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_RGBA_16161616,
		.npln = 1, .ncmp = { 4, 0, 0 }, .bps = 16, .bpp_afbc = { 64, 0, 0 }, .bpp = { 64, 0, 0 },
		.hsub = 0, .vsub = 0, .align_w = 1, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = true, .is_rgb = true, .is_yuv = false,
		.afbc = true, .linear = true, .yuv_transform = true, .flex = true,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_Y8,
		.npln = 1, .ncmp = { 1, 0, 0 }, .bps = 8, .bpp_afbc = { 8, 0, 0 }, .bpp = { 8, 0, 0 },
		.hsub = 1, .vsub = 1, .align_w = 2, .align_h = 2, .align_w_cpu = 16,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = true,
		.afbc = true, .linear = true, .yuv_transform = false, .flex = true,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_Y16,
		.npln = 1, .ncmp = { 1, 0, 0 }, .bps = 16, .bpp_afbc = { 16, 0, 0 }, .bpp = { 16, 0, 0 },
		.hsub = 1, .vsub = 1, .align_w = 2, .align_h = 2, .align_w_cpu = 16,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = true,
		.afbc = true, .linear = true, .yuv_transform = false, .flex = true,
	},
	/* 420 (8-bit) */
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_YUV420_8BIT_I,
		.npln = 1, .ncmp = { 3, 0, 0 }, .bps = 8, .bpp_afbc = { 12, 0, 0 }, .bpp = { 0, 0, 0 },
		.hsub = 2, .vsub = 2, .align_w = 2, .align_h = 2, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = true,
		.afbc = true, .linear = false, .yuv_transform = false, .flex = false,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_NV12,
		.npln = 2, .ncmp = { 1, 2, 0 }, .bps = 8, .bpp_afbc = { 8, 16, 0 }, .bpp = { 8, 16, 0 },
		.hsub = 2, .vsub = 2, .align_w = 2, .align_h = 2, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = true,
		.afbc = true, .linear = true, .yuv_transform = false, .flex = true,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_NV21,
		.npln = 2, .ncmp = { 1, 2, 0 }, .bps = 8, .bpp_afbc = { 8, 16, 0 }, .bpp = { 8, 16, 0 },
		.hsub = 2, .vsub = 2, .align_w = 2, .align_h = 2, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = true,
		.afbc = true, .linear = true, .yuv_transform = false, .flex = true,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_YV12,
		.npln = 3, .ncmp = { 1, 1, 1 }, .bps = 8, .bpp_afbc = { 8, 8, 8 }, .bpp = { 8, 8, 8 },
		.hsub = 2, .vsub = 2, .align_w = 2, .align_h = 2, .align_w_cpu = 16,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = true,
		.afbc = true, .linear = true, .yuv_transform = false, .flex = true,
	},
	/* 422 (8-bit) */
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_YUV422_8BIT,
		.npln = 1, .ncmp = { 3, 0, 0 }, .bps = 8, .bpp_afbc = { 16, 0, 0 }, .bpp = { 16, 0, 0 },
		.hsub = 2, .vsub = 1, .align_w = 2, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = true,
		.afbc = true, .linear = true, .yuv_transform = false, .flex = true,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_NV16,
		.npln = 2, .ncmp = { 1, 2, 0 }, .bps = 8, .bpp_afbc = { 8, 16, 0 }, .bpp = { 8, 16, 0 },
		.hsub = 2, .vsub = 1, .align_w = 2, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = true,
		.afbc = true, .linear = true, .yuv_transform = false, .flex = true,
	},
	/* 420 (10-bit) */
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_YUV420_10BIT_I,
		.npln = 1, .ncmp = { 3, 0, 0 }, .bps = 10, .bpp_afbc = { 15, 0, 0 }, .bpp = { 0, 0, 0 },
		.hsub = 2, .vsub = 2, .align_w = 2, .align_h = 2, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = true,
		.afbc = true, .linear = false, .yuv_transform = false, .flex = false,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_Y0L2,
		.npln = 1, .ncmp = { 4, 0, 0 }, .bps = 10, .bpp_afbc = { 0, 0, 0 }, .bpp = { 16, 0, 0 },
		.hsub = 2, .vsub = 2, .align_w = 2, .align_h = 2, ALIGN_W_CPU_DEFAULT,
		.tile_size = 2, .has_alpha = true, .is_rgb = false, .is_yuv = true,
		.afbc = false, .linear = true, .yuv_transform = false, .flex = false,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_P010,
		.npln = 2, .ncmp = { 1, 2, 0 }, .bps = 10, .bpp_afbc = { 10, 20, 0 }, .bpp = { 16, 32, 0 },
		.hsub = 2, .vsub = 2, .align_w = 2, .align_h = 2, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = true,
		.afbc = true, .linear = true, .yuv_transform = false, .flex = true,
	},
	/* 422 (10-bit) */
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_Y210,
		.npln = 1, .ncmp = { 3, 0, 0 }, .bps = 10, .bpp_afbc = { 20, 0, 0 }, .bpp = { 32, 0, 0 },
		.hsub = 2, .vsub = 1, .align_w = 2, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = true,
		.afbc = true, .linear = true, .yuv_transform = false, .flex = true,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_P210,
		.npln = 2, .ncmp = { 1, 2, 0 }, .bps = 10, .bpp_afbc = { 10, 20, 0 }, .bpp = { 16, 32, 0 },
		.hsub = 2, .vsub = 1, .align_w = 2, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = true,
		.afbc = true, .linear = true, .yuv_transform = false, .flex = true,
	},
	/* 444 (10-bit) */
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_YUV444_10BIT_I,
		.npln = 1, .ncmp = { 3, 0, 0 }, .bps = 10, .bpp_afbc = { 30, 0, 0 }, .bpp = { 0, 0, 0 },
		.hsub = 1, .vsub = 1, .align_w = 1, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = true,
		.afbc = true, .linear = false, .yuv_transform = false, .flex = false,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_Y410,
		.npln = 1, .ncmp = { 4, 0, 0 }, .bps = 10, .bpp_afbc = { 0, 0, 0 }, .bpp = { 32, 0, 0 },
		.hsub = 1, .vsub = 1, .align_w = 1, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = true, .is_rgb = false, .is_yuv = true,
		.afbc = false, .linear = true, .yuv_transform = false, .flex = false,
	},
	/* Other */
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_RAW16,
		.npln = 1, .ncmp = { 1, 0, 0 }, .bps = 16, .bpp_afbc = { 0, 0, 0 }, .bpp = { 16, 0, 0 },
		.hsub = 0, .vsub = 0, .align_w = 2, .align_h = 2, .align_w_cpu = 16,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = false,
		.afbc = false, .linear = true, .yuv_transform = false, .flex = false,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_RAW12,
		.npln = 1, .ncmp = { 1, 0, 0 }, .bps = 12, .bpp_afbc = { 0, 0, 0 }, .bpp = { 12, 0, 0 },
		.hsub = 0, .vsub = 0, .align_w = 4, .align_h = 2, .align_w_cpu = 4,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = false,
		.afbc = false, .linear = true, .yuv_transform = false, .flex = false,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_RAW10,
		.npln = 1, .ncmp = { 1, 0, 0 }, .bps = 10, .bpp_afbc = { 0, 0, 0 }, .bpp = { 10, 0, 0 },
		.hsub = 0, .vsub = 0, .align_w = 4, .align_h = 2, .align_w_cpu = 4,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = false,
		.afbc = false, .linear = true, .yuv_transform = false, .flex = false,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_BLOB,
		.npln = 1, .ncmp = { 1, 0, 0 }, .bps = 8, .bpp_afbc = { 0, 0, 0 }, .bpp = { 8, 0, 0 },
		.hsub = 0, .vsub = 0, .align_w = 1, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = false,
		.afbc = false, .linear = true, .yuv_transform = false, .flex = false,
	},
	/* Depth and Stencil */
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_16,
		.npln = 1, .ncmp = { 1, 0, 0 }, .bps = 16, .bpp_afbc = { 0, 0, 0}, .bpp = { 16, 0, 0 },
		.hsub = 0, .vsub = 0, .align_w = 1, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = false,
		.afbc = false, .linear = true, .yuv_transform = false, .flex = false,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_24,
		.npln = 1, .ncmp = { 1, 0, 0 }, .bps = 24, .bpp_afbc = { 0, 0, 0 }, .bpp = { 24, 0, 0 },
		.hsub = 0, .vsub = 0, .align_w = 1, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = false,
		.afbc = false, .linear = true, .yuv_transform = false, .flex = false,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_24_STENCIL_8,
		.npln = 1, .ncmp = { 2, 0, 0 }, .bps = 24, .bpp_afbc = { 0, 0, 0 }, .bpp = { 32, 0, 0 },
		.hsub = 0, .vsub = 0, .align_w = 1, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = false,
		.afbc = false, .linear = true, .yuv_transform = false, .flex = false,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_32F,
		.npln = 1, .ncmp = { 1, 0, 0 }, .bps = 32, .bpp_afbc = { 0, 0, 0 }, .bpp = { 32, 0, 0 },
		.hsub = 0, .vsub = 0, .align_w = 1, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = false,
		.afbc = false, .linear = true, .yuv_transform = false, .flex = false,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_32F_STENCIL_8,
		.npln = 1, .ncmp = { 2, 0, 0 }, .bps = 32, .bpp_afbc = { 0, 0, 0 }, .bpp = { 40, 0, 0 },
		.hsub = 0, .vsub = 0, .align_w = 1, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = false,
		.afbc = false, .linear = true, .yuv_transform = false, .flex = false,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_STENCIL_8,
		.npln = 1, .ncmp = { 1, 0, 0 }, .bps = 8, .bpp_afbc = { 0, 0, 0 }, .bpp = { 8, 0, 0 },
		.hsub = 0, .vsub = 0, .align_w = 1, .align_h = 1, ALIGN_W_CPU_DEFAULT,
		.tile_size = 1, .has_alpha = false, .is_rgb = false, .is_yuv = false,
		.afbc = false, .linear = true, .yuv_transform = false, .flex = false,
	},
};
const size_t num_formats = sizeof(formats)/sizeof(formats[0]);

/*
 * This table represents the superset of flags for each base format and producer/consumer.
 * Where IP does not support a capability, it should be defined and not set.
 */
const format_ip_support_t formats_ip_support[] = {
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_RGB_565,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_LIN | F_AFBC,
		.gpu_wr = F_LIN | F_AFBC,
		.dpu_rd = F_LIN | F_AFBC,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_AFBC,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_RGB_888,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_LIN | F_AFBC,
		.gpu_wr = F_LIN | F_AFBC,
		.dpu_rd = F_LIN | F_AFBC,
		.dpu_wr = F_LIN,
		.dpu_aeu_wr = F_AFBC,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_RGBA_8888,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_LIN | F_AFBC,
		.gpu_wr = F_LIN | F_AFBC,
		.dpu_rd = F_LIN | F_AFBC,
		.dpu_wr = F_LIN,
		.dpu_aeu_wr = F_AFBC,
		.vpu_rd = F_LIN,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_BGRA_8888,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_LIN,
		.gpu_wr = F_LIN,
		.dpu_rd = F_LIN,
		.dpu_wr = F_LIN,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_LIN,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_RGBX_8888,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_LIN | F_AFBC,
		.gpu_wr = F_LIN | F_AFBC,
		.dpu_rd = F_LIN | F_AFBC,
		.dpu_wr = F_LIN,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_RGBA_1010102,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_LIN | F_AFBC,
		.gpu_wr = F_LIN | F_AFBC,
		.dpu_rd = F_LIN | F_AFBC,
		.dpu_wr = F_LIN,
		.dpu_aeu_wr = F_AFBC,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_RGBA_16161616,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_LIN | F_AFBC,
		.gpu_wr = F_LIN | F_AFBC,
		.dpu_rd = F_NONE,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_Y8,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_NONE,
		.gpu_wr = F_NONE,
		.dpu_rd = F_NONE,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_Y16,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_NONE,
		.gpu_wr = F_NONE,
		.dpu_rd = F_NONE,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	/* 420 (8-bit) */
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_YUV420_8BIT_I,
		.cpu_rd = F_NONE,
		.cpu_wr = F_NONE,
		.gpu_rd = F_AFBC,
		.gpu_wr = F_AFBC,
		.dpu_rd = F_AFBC,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_AFBC,
		.vpu_rd = F_AFBC,
		.vpu_wr = F_AFBC,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_NV12,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_LIN | F_AFBC,
		.gpu_wr = F_LIN,
		.dpu_rd = F_LIN,
		.dpu_wr = F_LIN,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_LIN,
		.vpu_wr = F_LIN,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_NV21,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_LIN,
		.gpu_wr = F_LIN,
		.dpu_rd = F_NONE,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_LIN,
		.vpu_wr = F_LIN,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_YV12,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_LIN,
		.gpu_wr = F_LIN,
		.dpu_rd = F_LIN,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_LIN,
		.vpu_wr = F_LIN,
		.cam_wr = F_NONE,
	},
	/* 422 (8-bit) */
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_YUV422_8BIT,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_LIN | F_AFBC,
		.gpu_wr = F_LIN | F_AFBC,
		.dpu_rd = F_LIN | F_AFBC,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_AFBC,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_NV16,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_LIN | F_AFBC,
		.gpu_wr = F_LIN,
		.dpu_rd = F_NONE,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	/* 420 (10-bit) */
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_YUV420_10BIT_I,
		.cpu_rd = F_NONE,
		.cpu_wr = F_NONE,
		.gpu_rd = F_AFBC,
		.gpu_wr = F_AFBC,
		.dpu_rd = F_AFBC,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_AFBC,
		.vpu_rd = F_AFBC,
		.vpu_wr = F_AFBC,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_Y0L2,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_LIN,
		.gpu_wr = F_LIN,
		.dpu_rd = F_LIN,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_LIN,
		.vpu_wr = F_LIN,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_P010,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_LIN,
		.gpu_wr = F_LIN,
		.dpu_rd = F_LIN,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_LIN,
		.vpu_wr = F_LIN,
		.cam_wr = F_NONE,
	},
	/* 422 (10-bit) */
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_Y210,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_LIN | F_AFBC,
		.gpu_wr = F_LIN | F_AFBC,
		.dpu_rd = F_NONE,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_P210,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_LIN,
		.gpu_wr = F_LIN,
		.dpu_rd = F_NONE,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	/* 444 (10-bit) */
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_YUV444_10BIT_I,
		.cpu_rd = F_NONE,
		.cpu_wr = F_NONE,
		.gpu_rd = F_NONE,
		.gpu_wr = F_NONE,
		.dpu_rd = F_NONE,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_Y410,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_LIN,
		.gpu_wr = F_LIN | F_AFBC,
		.dpu_rd = F_NONE,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	/* Other */
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_RAW16,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_LIN,
		.gpu_wr = F_NONE,
		.dpu_rd = F_NONE,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_LIN,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_RAW12,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_NONE,
		.gpu_wr = F_NONE,
		.dpu_rd = F_NONE,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_RAW10,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_NONE,
		.gpu_wr = F_NONE,
		.dpu_rd = F_NONE,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_BLOB,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_LIN,
		.gpu_wr = F_LIN,
		.dpu_rd = F_NONE,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	/* Depth and Stencil */
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_16,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_NONE,
		.gpu_wr = F_NONE,
		.dpu_rd = F_NONE,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_24,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_NONE,
		.gpu_wr = F_NONE,
		.dpu_rd = F_NONE,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_24_STENCIL_8,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_NONE,
		.gpu_wr = F_NONE,
		.dpu_rd = F_NONE,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_32F,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_NONE,
		.gpu_wr = F_NONE,
		.dpu_rd = F_NONE,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_32F_STENCIL_8,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_NONE,
		.gpu_wr = F_NONE,
		.dpu_rd = F_NONE,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
	{
		.id = MALI_GRALLOC_FORMAT_INTERNAL_STENCIL_8,
		.cpu_rd = F_LIN,
		.cpu_wr = F_LIN,
		.gpu_rd = F_NONE,
		.gpu_wr = F_NONE,
		.dpu_rd = F_NONE,
		.dpu_wr = F_NONE,
		.dpu_aeu_wr = F_NONE,
		.vpu_rd = F_NONE,
		.vpu_wr = F_NONE,
		.cam_wr = F_NONE,
	},
};

const size_t num_ip_formats = sizeof(formats_ip_support)/sizeof(formats_ip_support[0]);

typedef struct
{
    uint32_t hal_format;
    bool is_flex;
    uint32_t internal_format;
} hal_int_fmt;


static const hal_int_fmt hal_to_internal_format[] =
{
	{ HAL_PIXEL_FORMAT_RGBA_8888,              false, MALI_GRALLOC_FORMAT_INTERNAL_RGBA_8888 },
	{ HAL_PIXEL_FORMAT_RGBX_8888,              false, MALI_GRALLOC_FORMAT_INTERNAL_RGBX_8888 },
	{ HAL_PIXEL_FORMAT_RGB_888,                false, MALI_GRALLOC_FORMAT_INTERNAL_RGB_888 },
	{ HAL_PIXEL_FORMAT_RGB_565,                false, MALI_GRALLOC_FORMAT_INTERNAL_RGB_565 },
	{ HAL_PIXEL_FORMAT_BGRA_8888,              false, MALI_GRALLOC_FORMAT_INTERNAL_BGRA_8888 },
	{ HAL_PIXEL_FORMAT_YCbCr_422_SP,           false, MALI_GRALLOC_FORMAT_INTERNAL_NV16 },
	{ HAL_PIXEL_FORMAT_YCrCb_420_SP,           false, MALI_GRALLOC_FORMAT_INTERNAL_NV21 },
	{ HAL_PIXEL_FORMAT_YCbCr_422_I,            false, MALI_GRALLOC_FORMAT_INTERNAL_YUV422_8BIT },
	{ HAL_PIXEL_FORMAT_RGBA_FP16,              false, MALI_GRALLOC_FORMAT_INTERNAL_RGBA_16161616 },
	{ HAL_PIXEL_FORMAT_RAW16,                  false, MALI_GRALLOC_FORMAT_INTERNAL_RAW16 },
	{ HAL_PIXEL_FORMAT_BLOB,                   false, MALI_GRALLOC_FORMAT_INTERNAL_BLOB },
	{ HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, true,  MALI_GRALLOC_FORMAT_INTERNAL_NV12 },
	{ HAL_PIXEL_FORMAT_YCbCr_420_888,          true,  MALI_GRALLOC_FORMAT_INTERNAL_NV12 },
	{ HAL_PIXEL_FORMAT_RAW_OPAQUE,             false, MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED },
	{ HAL_PIXEL_FORMAT_RAW10,                  false, MALI_GRALLOC_FORMAT_INTERNAL_RAW10 },
	{ HAL_PIXEL_FORMAT_RAW12,                  false, MALI_GRALLOC_FORMAT_INTERNAL_RAW12 },
#if GRALLOC_USE_LEGACY_LOCK
	{ HAL_PIXEL_FORMAT_YCbCr_422_888,          true,  MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED },
#else
	{ HAL_PIXEL_FORMAT_YCbCr_422_888,          true,  MALI_GRALLOC_FORMAT_INTERNAL_YUV422_8BIT },
#endif
	{ HAL_PIXEL_FORMAT_YCbCr_444_888,          true,  MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED },
	{ HAL_PIXEL_FORMAT_FLEX_RGB_888,           true,  MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED },
	{ HAL_PIXEL_FORMAT_FLEX_RGBA_8888,         true,  MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED },
	{ HAL_PIXEL_FORMAT_RGBA_1010102,           false, MALI_GRALLOC_FORMAT_INTERNAL_RGBA_1010102 },
	{ HAL_PIXEL_FORMAT_DEPTH_16,               false, MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_16 },
	{ HAL_PIXEL_FORMAT_DEPTH_24,               false, MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_24 },
	{ HAL_PIXEL_FORMAT_DEPTH_24_STENCIL_8,     false, MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_24_STENCIL_8 },
	{ HAL_PIXEL_FORMAT_DEPTH_32F,              false, MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_32F },
	{ HAL_PIXEL_FORMAT_DEPTH_32F_STENCIL_8,    false, MALI_GRALLOC_FORMAT_INTERNAL_DEPTH_32F_STENCIL_8 },
	{ HAL_PIXEL_FORMAT_STENCIL_8,              false, MALI_GRALLOC_FORMAT_INTERNAL_STENCIL_8 },
	{ HAL_PIXEL_FORMAT_YCBCR_P010,             false, MALI_GRALLOC_FORMAT_INTERNAL_P010 },
	{ HAL_PIXEL_FORMAT_Y8,                     false, MALI_GRALLOC_FORMAT_INTERNAL_Y8 },
	{ HAL_PIXEL_FORMAT_Y16,                    false, MALI_GRALLOC_FORMAT_INTERNAL_Y16 },
	{ HAL_PIXEL_FORMAT_YV12,                   false, MALI_GRALLOC_FORMAT_INTERNAL_YV12 },
};

const size_t num_hal_formats = sizeof(hal_to_internal_format)/sizeof(hal_to_internal_format[0]);


/*
 *  Finds "Look-up Table" index for the given format
 *
 * @param base_format [in]   Format for which index is required.
 *
 * @return index, when the format is found in the look up table
 *         -1, otherwise
 *
 */
int32_t get_format_index(const uint32_t base_format)
{
	int32_t format_idx;
	for (format_idx = 0; format_idx < (int32_t)num_formats; format_idx++)
	{
		if (formats[format_idx].id == base_format)
		{
			break;
		}
	}
	if (format_idx >= (int32_t)num_formats)
	{
		MALI_GRALLOC_LOGE("ERROR: Format allocation info not found for format: %" PRIx32, base_format);
		return -1;
	}

	return format_idx;
}


int32_t get_ip_format_index(const uint32_t base_format)
{
	int32_t format_idx;
	for (format_idx = 0; format_idx < (int32_t)num_ip_formats; format_idx++)
	{
		if (formats_ip_support[format_idx].id == base_format)
		{
			break;
		}
	}
	if (format_idx >= (int32_t)num_ip_formats)
	{
		MALI_GRALLOC_LOGE("ERROR: IP support not found for format: %" PRIx32, base_format);
		return -1;
	}

	return format_idx;
}


/*
 * Attempt to map base HAL format to an internal format and
 * validate format is supported for allocation.
 *
 * @param map_to_internal [in]  1: Forces mapping to (and validation of) internal format
 *                              0: Only maps flex HAL formats to internal
 *
 * @return internal_format, where internal format is found
 *         HAL format, where map_to_internal == 0 and base_format is not flex
 *         MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED, otherwise
 *
 * NOTE: Base format might be either a HAL format or (already) an internal format.
 *
 */
uint32_t get_internal_format(const uint32_t base_format, const bool map_to_internal)
{
	uint32_t internal_format = base_format;

	for (int idx = 0; idx < (int)num_hal_formats; idx++)
	{
		if (hal_to_internal_format[idx].hal_format == base_format)
		{
			if (hal_to_internal_format[idx].is_flex || map_to_internal)
			{
				internal_format = hal_to_internal_format[idx].internal_format;
			}
			break;
		}
	}

	/* Ensure internal format is valid when expected. */
	if (map_to_internal && get_format_index(internal_format) < 0)
	{
		internal_format = MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED;
	}

	return internal_format;
}


/* Get the dataspace to use based on private usage and format. */
void get_format_dataspace(uint32_t base_format,
                          uint64_t usage,
                          int width,
                          int height,
                          android_dataspace_t *dataspace,
                          mali_gralloc_yuv_info *yuv_info)
{
	uint64_t color_space = HAL_DATASPACE_STANDARD_UNSPECIFIED;
	uint64_t range = HAL_DATASPACE_RANGE_UNSPECIFIED;
	*dataspace = HAL_DATASPACE_UNKNOWN;
	*yuv_info = MALI_YUV_NO_INFO;

	const int32_t format_idx = get_format_index(base_format);
	if (format_idx < 0)
	{
		return;
	}

	/* This resolution is the cut-off point at which BT709 is used (as default)
	 * instead of BT601 for YUV formats < 10 bits.
	 */
	constexpr int yuv_bt601_max_width = 1280;
	constexpr int yuv_bt601_max_height = 720;

	if (formats[format_idx].is_yuv)
	{
		/* Default YUV dataspace. */
		color_space = HAL_DATASPACE_STANDARD_BT709;
		range = HAL_DATASPACE_RANGE_LIMITED;

		/* 10-bit YUV is assumed to be wide BT2020.
		 */
		if (formats[format_idx].bps >= 10)
		{
			color_space = HAL_DATASPACE_STANDARD_BT2020;
			range = HAL_DATASPACE_RANGE_FULL;
		}
		else if (width < yuv_bt601_max_width || height < yuv_bt601_max_height)
		{
			color_space = HAL_DATASPACE_STANDARD_BT601_625;
			range = HAL_DATASPACE_RANGE_LIMITED;
		}

		/* Override YUV dataspace based on private usage. */
		switch (usage & MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_MASK)
		{
		case MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_BT601:
			color_space = HAL_DATASPACE_STANDARD_BT601_625;
			break;
		case MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_BT709:
			color_space = HAL_DATASPACE_STANDARD_BT709;
			break;
		case MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_BT2020:
			color_space = HAL_DATASPACE_STANDARD_BT2020;
			break;
		}

		switch (usage & MALI_GRALLOC_USAGE_RANGE_MASK)
		{
		case MALI_GRALLOC_USAGE_RANGE_NARROW:
			range = HAL_DATASPACE_RANGE_LIMITED;
			break;
		case MALI_GRALLOC_USAGE_RANGE_WIDE:
			range = HAL_DATASPACE_RANGE_FULL;
			break;
		}

		*dataspace = static_cast<android_dataspace_t>(color_space | range);

		/* Set deprecated yuv_info field. */
		switch (color_space)
		{
		case HAL_DATASPACE_STANDARD_BT601_625:
			if (range == HAL_DATASPACE_RANGE_LIMITED)
			{
				*yuv_info = MALI_YUV_BT601_NARROW;
			}
			else
			{
				*yuv_info = MALI_YUV_BT601_WIDE;
			}
			break;
		case HAL_DATASPACE_STANDARD_BT709:
			if (range == HAL_DATASPACE_RANGE_LIMITED)
			{
				*yuv_info = MALI_YUV_BT709_NARROW;
			}
			else
			{
				*yuv_info = MALI_YUV_BT709_WIDE;
			}
			break;
		}
	}
	else if (formats[format_idx].is_rgb)
	{
		/* Default RGB dataspace. Expected by Mapper VTS. */
		*dataspace = static_cast<android_dataspace_t>(HAL_DATASPACE_UNKNOWN);
	}
}


bool is_power2(uint8_t n)
{
	return ((n & (n-1)) == 0);
}


bool sanitize_formats(void)
{
	bool fail = false;

	for (int i = 0; i < (int)num_formats; i++)
	{
		const format_info_t * format = &formats[i];

		/* Identify invalid values. */
		if (format->id == 0 ||
		    format->npln == 0 || format->npln > 3 ||
		    format->total_components() == 0 ||
		    format->bps == 0 ||
		    format->align_w == 0 ||
		    format->align_h == 0 ||
		    format->align_w_cpu == 0 ||
		    format->tile_size == 0)
		{
			MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] property zero/out of range (unexpected)", format->id);
			fail = true;
		}

		if (format->is_rgb && format->is_yuv)
		{
			MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] cannot be both RGB and YUV", format->id);
			fail = true;
		}

		if (format->npln > format->total_components())
		{
			MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] planes cannot exceed components", format->id);
			fail = true;
		}

		if (format->linear && (format->bps > format->bpp[0]))
		{
			MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] bpp should be greater than/equal to bps", format->id);
			fail = true;
		}

		if (format->afbc && (format->bps > format->bpp_afbc[0]))
		{
			MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] bpp_afbc should be greater than/equal to bps", format->id);
			fail = true;
		}

		if (!format->linear && format->tile_size > 1)
		{
			MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] tile_size must be set to 1 for formats without linear support", format->id);
			fail = true;
		}

		for (int pln = 0; pln < 3; pln++)
		{
			if (format->linear && (pln < format->npln) && (format->bpp[pln] == 0))
			{
				MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] does not have bpp defined for plane: %d", format->id, pln);
				fail = true;
			}
			else if (format->linear && (pln >= format->npln) && (format->bpp[pln] != 0))
			{
				MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] should not have bpp defined for plane: %d", format->id, pln);
				fail = true;
			}
			else if (!format->linear && (format->bpp[pln] != 0))
			{
				MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] which doesn't support linear should not have bpp defined", format->id);
				fail = true;
			}

			if (format->afbc && (pln < format->npln) && (format->bpp_afbc[pln] == 0))
			{
				MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] does not have bpp_afbc defined for plane: %d", format->id, pln);
				fail = true;
			}
			else if (format->afbc && (pln >= format->npln) && (format->bpp_afbc[pln] != 0))
			{
				MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] should not have bpp_afbc defined for plane: %d", format->id, pln);
				fail = true;
			}
			else if (!format->afbc && (format->bpp_afbc[pln] != 0))
			{
				MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] which doesn't support afbc should not have bpp defined", format->id);
				fail = true;
			}
		}

		if (format->is_yuv)
		{
			if (format->hsub == 0 || format->vsub == 0)
			{
				MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] hsub and vsub should be non-zero (YUV)", format->id);
				fail = true;
			}

			if (!is_power2(format->hsub) || !is_power2(format->vsub))
			{
				MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] hsub and vsub should be powers of 2", format->id);
				fail = true;
			}

			if ((format->align_w % format->hsub) != 0)
			{
				MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] align_w should be a multiple of hsub", format->id);
				fail = true;
			}

			if ((format->align_h % format->vsub) != 0)
			{
				MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] align_h should be a multiple of vsub", format->id);
				fail = true;
			}
		}
		else
		{
			if (format->hsub != 0 || format->vsub != 0)
			{
				MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] hsub and vsub should be zero (non-YUV)", format->id);
				fail = true;
			}
		}

		if (format->align_w == 0 || format->align_h == 0)
		{
			MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] align_w and align_h should be non-zero", format->id);
			fail = true;
		}
		else
		{
			if (!is_power2(format->align_w) || !is_power2(format->align_h))
			{
				MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] align_w and align_h should be powers of 2", format->id);
				fail = true;
			}

			if (!is_power2(format->align_w_cpu))
			{
				MALI_GRALLOC_LOGE("Format [id:0x%" PRIx32 "] align_w_cpu should be a power of 2", format->id);
				fail = true;
			}
		}
	}

	return fail;
}
