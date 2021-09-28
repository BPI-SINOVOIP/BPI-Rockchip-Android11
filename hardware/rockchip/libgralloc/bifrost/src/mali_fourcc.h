/*
 * Copyright (C) 2019-2020 ARM Limited. All rights reserved.
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

#ifndef MALI_FOURCC_H_
#define MALI_FOURCC_H_

extern "C"
{

#include <drm_fourcc.h>
#include <stdint.h>

/* FOURCCs for formats that exist upstream, but may not be in the drm_fourcc.h header included above.
 *
 * Below we define DRM FOURCC formats that are upstreamed, but may not be in the drm_fourcc.h header that we include
 * above, merely because that header is too old. As drm_fourcc.h is an external header that we cannot control, the best
 * we can do is to define here the missing formats.
 */
#ifndef DRM_FORMAT_INVALID
#define DRM_FORMAT_INVALID 0
#endif

#ifndef DRM_FORMAT_P010
#define DRM_FORMAT_P010 fourcc_code('P', '0', '1', '0')
#endif

#ifndef DRM_FORMAT_Y0L2
#define DRM_FORMAT_Y0L2 fourcc_code('Y', '0', 'L', '2')
#endif

#ifndef DRM_FORMAT_P210
#define DRM_FORMAT_P210 fourcc_code('P', '2', '1', '0')
#endif

#ifndef DRM_FORMAT_Y210
#define DRM_FORMAT_Y210 fourcc_code('Y', '2', '1', '0')
#endif

#ifndef DRM_FORMAT_Y410
#define DRM_FORMAT_Y410 fourcc_code('Y', '4', '1', '0')
#endif

#ifndef DRM_FORMAT_YUV420_8BIT
#define DRM_FORMAT_YUV420_8BIT fourcc_code('Y', 'U', '0', '8')
#endif

#ifndef DRM_FORMAT_YUV420_10BIT
#define DRM_FORMAT_YUV420_10BIT fourcc_code('Y', 'U', '1', '0')
#endif

#ifndef DRM_FORMAT_ABGR16161616F
#define DRM_FORMAT_ABGR16161616F fourcc_code('A', 'B', '4', 'H')
#endif

#ifndef DRM_FORMAT_R16
#define DRM_FORMAT_R16 fourcc_code('R', '1', '6', ' ')
#endif

#ifndef DRM_FORMAT_Q410
#define DRM_FORMAT_Q410 fourcc_code('Q', '4', '1', '0')
#endif

#ifndef DRM_FORMAT_Q401
#define DRM_FORMAT_Q401 fourcc_code('Q', '4', '0', '1')
#endif

/* ARM specific modifiers. */
#ifndef DRM_FORMAT_MOD_VENDOR_ARM
#define DRM_FORMAT_MOD_VENDOR_ARM    0x08
#endif

#ifndef fourcc_mod_code
#define fourcc_mod_code(vendor, val) \
         ((((uint64_t)DRM_FORMAT_MOD_VENDOR_## vendor) << 56) | ((val) & 0x00ffffffffffffffULL))
#endif

#ifndef DRM_FORMAT_MOD_ARM_AFBC
/* AFBC modifiers. */

#define DRM_FORMAT_MOD_ARM_AFBC(__afbc_mode)	fourcc_mod_code(ARM, (__afbc_mode))
/* AFBC superblock size. */
#ifndef AFBC_FORMAT_MOD_BLOCK_SIZE_16x16
#define AFBC_FORMAT_MOD_BLOCK_SIZE_16x16     ((uint64_t)0x1)
#endif

#ifndef AFBC_FORMAT_MOD_BLOCK_SIZE_32x8
#define AFBC_FORMAT_MOD_BLOCK_SIZE_32x8      ((uint64_t)0x2)
#endif

#ifndef AFBC_FORMAT_MOD_BLOCK_SIZE_MASK
#define AFBC_FORMAT_MOD_BLOCK_SIZE_MASK      ((uint64_t)0xf)
#endif

/* AFBC lossless transform. */
#ifndef AFBC_FORMAT_MOD_YTR
#define AFBC_FORMAT_MOD_YTR     (((uint64_t)1) <<  4)
#endif

/* AFBC block-split. */
#ifndef AFBC_FORMAT_MOD_SPLIT
#define AFBC_FORMAT_MOD_SPLIT   (((uint64_t)1) <<  5)
#endif

/* AFBC sparse layout. */
#ifndef AFBC_FORMAT_MOD_SPARSE
#define AFBC_FORMAT_MOD_SPARSE  (((uint64_t)1) <<  6)
#endif

/* AFBC tiled layout. */
#ifndef AFBC_FORMAT_MOD_TILED
#define AFBC_FORMAT_MOD_TILED   (((uint64_t)1) <<  8)
#endif

#endif /* DRM_FORMAT_MOD_ARM_AFBC */

/* AFBC 1.3 block sizes. */
#ifndef AFBC_FORMAT_MOD_BLOCK_SIZE_64x4
#define AFBC_FORMAT_MOD_BLOCK_SIZE_64x4      ((uint64_t)0x3)
#endif

#ifndef AFBC_FORMAT_MOD_BLOCK_SIZE_32x8_64x4
#define AFBC_FORMAT_MOD_BLOCK_SIZE_32x8_64x4 ((uint64_t)0x4)
#endif

/* AFBC double-buffer. */
#ifndef AFBC_FORMAT_MOD_DB
#define AFBC_FORMAT_MOD_DB (((uint64_t)1) << 10)
#endif

/* AFBC buffer content hints. */
#ifndef AFBC_FORMAT_MOD_BCH
#define AFBC_FORMAT_MOD_BCH (((uint64_t)1) << 11)
#endif

}

#endif /* MALI_FOURCC_H_ */
