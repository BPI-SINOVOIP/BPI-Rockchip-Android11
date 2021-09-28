/*
 * Copyright (C) 2017-2020 ARM Limited. All rights reserved.
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

#ifndef MALI_GRALLOC_USAGES_H_
#define MALI_GRALLOC_USAGES_H_

#if GRALLOC_HOST_BUILD && !defined(GRALLOC_VERSION_MAJOR)
#define GRALLOC_VERSION_MAJOR 1
#endif

#ifndef GRALLOC_VERSION_MAJOR
#error "GRALLOC_VERSION_MAJOR must be defined."
#endif

#define GRALLOC_USAGE_PRIVATE_MASK (0xffff0000f0000000U)

/*
 * Below usage types overlap, this is intentional.
 * The reason is that for Gralloc 0.3 there are very
 * few usage flags we have at our disposal.
 *
 * The overlapping is handled by processing the definitions
 * in a specific order.
 *
 * MALI_GRALLOC_USAGE_PRIVATE_FORMAT and MALI_GRALLOC_USAGE_NO_AFBC
 * don't overlap and are processed first.
 *
 * MALI_GRALLOC_USAGE_YUV_CONF are only for YUV formats and clients
 * using MALI_GRALLOC_USAGE_NO_AFBC must never allocate YUV formats.
 * The latter is strictly enforced and allocations will fail.
 *
 * MALI_GRALLOC_USAGE_AFBC_PADDING is only valid if MALI_GRALLOC_USAGE_NO_AFBC
 * is not present.
 */

/*
 * Gralloc private usage 0-3 are the same in 0.3 and 1.0.
 * We defined based our usages based on what is available.
 */
#if GRALLOC_VERSION_MAJOR == 1 || GRALLOC_HOST_BUILD
#include <hardware/gralloc1.h>

/*
 * Most gralloc code is fairly version agnostic, but certain
 * places still use old usage defines. Make sure it works
 * ok for usages that are backwards compatible.
 */
#define GRALLOC_USAGE_PRIVATE_0 GRALLOC1_CONSUMER_USAGE_PRIVATE_0
#define GRALLOC_USAGE_PRIVATE_1 GRALLOC1_CONSUMER_USAGE_PRIVATE_1
#define GRALLOC_USAGE_PRIVATE_2 GRALLOC1_CONSUMER_USAGE_PRIVATE_2
#define GRALLOC_USAGE_PRIVATE_3 GRALLOC1_CONSUMER_USAGE_PRIVATE_3
#define GRALLOC_USAGE_PRIVATE_11 GRALLOC1_PRODUCER_USAGE_PRIVATE_11
#define GRALLOC_USAGE_PRIVATE_12 GRALLOC1_PRODUCER_USAGE_PRIVATE_12
#define GRALLOC_USAGE_PRIVATE_13 GRALLOC1_PRODUCER_USAGE_PRIVATE_13
#define GRALLOC_USAGE_PRIVATE_14 GRALLOC1_PRODUCER_USAGE_PRIVATE_14
#define GRALLOC_USAGE_PRIVATE_15 GRALLOC1_PRODUCER_USAGE_PRIVATE_15
#define GRALLOC_USAGE_PRIVATE_16 GRALLOC1_PRODUCER_USAGE_PRIVATE_16
#define GRALLOC_USAGE_PRIVATE_17 GRALLOC1_PRODUCER_USAGE_PRIVATE_17
#define GRALLOC_USAGE_PRIVATE_18 GRALLOC1_PRODUCER_USAGE_PRIVATE_18
#define GRALLOC_USAGE_PRIVATE_19 GRALLOC1_PRODUCER_USAGE_PRIVATE_19

#define GRALLOC_USAGE_SW_WRITE_RARELY GRALLOC1_PRODUCER_USAGE_CPU_WRITE
#define GRALLOC_USAGE_SW_WRITE_OFTEN GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN
#define GRALLOC_USAGE_SW_READ_RARELY GRALLOC1_CONSUMER_USAGE_CPU_READ
#define GRALLOC_USAGE_SW_READ_OFTEN GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN
#define GRALLOC_USAGE_RENDERSCRIPT GRALLOC1_CONSUMER_USAGE_RENDERSCRIPT
#define GRALLOC_USAGE_HW_FB GRALLOC1_CONSUMER_USAGE_CLIENT_TARGET
#define GRALLOC_USAGE_HW_2D 0x00000400U

#define GRALLOC_USAGE_SW_WRITE_MASK 0x000000F0U
#define GRALLOC_USAGE_SW_READ_MASK 0x0000000FU
#define GRALLOC_USAGE_PROTECTED GRALLOC1_PRODUCER_USAGE_PROTECTED
#define GRALLOC_USAGE_HW_RENDER GRALLOC1_PRODUCER_USAGE_GPU_RENDER_TARGET
#define GRALLOC_USAGE_HW_CAMERA_WRITE GRALLOC1_PRODUCER_USAGE_CAMERA
#define GRALLOC_USAGE_HW_CAMERA_READ GRALLOC1_CONSUMER_USAGE_CAMERA
#define GRALLOC_USAGE_HW_TEXTURE GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE
#define GRALLOC_USAGE_HW_VIDEO_ENCODER GRALLOC1_CONSUMER_USAGE_VIDEO_ENCODER
#define GRALLOC_USAGE_HW_COMPOSER GRALLOC1_CONSUMER_USAGE_HWCOMPOSER
#define GRALLOC_USAGE_EXTERNAL_DISP 0x00002000U
#define GRALLOC_USAGE_CURSOR GRALLOC1_CONSUMER_USAGE_CURSOR

#define GRALLOC_USAGE_SENSOR_DIRECT_DATA GRALLOC1_PRODUCER_USAGE_SENSOR_DIRECT_DATA
#define GRALLOC_USAGE_GPU_DATA_BUFFER GRALLOC1_CONSUMER_USAGE_GPU_DATA_BUFFER


typedef enum
{
	/*
	 * Allocation will be used as a front-buffer, which
	 * supports concurrent producer-consumer access.
	 *
	 * NOTE: Must not be used with MALI_GRALLOC_USAGE_FORCE_BACKBUFFER
	 */
	MALI_GRALLOC_USAGE_FRONTBUFFER = GRALLOC1_PRODUCER_USAGE_PRIVATE_0,

	/*
	 * Allocation will be used as a back-buffer.
	 * Use when switching from front-buffer as a workaround for Android
	 * buffer queue, which does not re-allocate for a sub-set of
	 * existing usage.
	 *
	 * NOTE: Must not be used with MALI_GRALLOC_USAGE_FRONTBUFFER.
	 */
	MALI_GRALLOC_USAGE_FORCE_BACKBUFFER = GRALLOC1_PRODUCER_USAGE_PRIVATE_13,

	/*
	 * Buffer will not be allocated with AFBC.
	 *
	 * NOTE: Not compatible with MALI_GRALLOC_USAGE_FORCE_BACKBUFFER so cannot be
	 * used when switching from front-buffer to back-buffer.
	 */
	MALI_GRALLOC_USAGE_NO_AFBC = GRALLOC1_PRODUCER_USAGE_PRIVATE_1,

	/* Custom alignment for AFBC headers.
	 *
	 * NOTE: due to usage flag overlap, AFBC_PADDING cannot be used with FORCE_BACKBUFFER.
	 */
	MALI_GRALLOC_USAGE_AFBC_PADDING = GRALLOC1_PRODUCER_USAGE_PRIVATE_14,

	/* Private format usage.
	 * 'format' argument to allocation function will be interpreted in a
	 * private manner and must be constructed via GRALLOC_PRIVATE_FORMAT_WRAPPER_*
	 * macros which pack base format and AFBC format modifiers into 32-bit value.
	 */
	MALI_GRALLOC_USAGE_PRIVATE_FORMAT = GRALLOC1_PRODUCER_USAGE_PRIVATE_15,

	/* YUV only. */
	MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_DEFAULT = 0,
	MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_BT601 = GRALLOC1_PRODUCER_USAGE_PRIVATE_18,
	MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_BT709 = GRALLOC1_PRODUCER_USAGE_PRIVATE_19,
	MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_BT2020 = (GRALLOC1_PRODUCER_USAGE_PRIVATE_18 | GRALLOC1_PRODUCER_USAGE_PRIVATE_19),
	MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_MASK = MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_BT2020,

	MALI_GRALLOC_USAGE_RANGE_DEFAULT = 0,
	MALI_GRALLOC_USAGE_RANGE_NARROW = GRALLOC1_PRODUCER_USAGE_PRIVATE_16,
	MALI_GRALLOC_USAGE_RANGE_WIDE = GRALLOC1_PRODUCER_USAGE_PRIVATE_17,
	MALI_GRALLOC_USAGE_RANGE_MASK = (GRALLOC1_PRODUCER_USAGE_PRIVATE_16 | GRALLOC1_PRODUCER_USAGE_PRIVATE_17),

} mali_gralloc_usage_type;

#elif GRALLOC_VERSION_MAJOR >= 2
#if HIDL_COMMON_VERSION_SCALED == 100
#include <android/hardware/graphics/common/1.0/types.h>
namespace hidl_common = android::hardware::graphics::common::V1_0;
#elif HIDL_COMMON_VERSION_SCALED == 110
#include <android/hardware/graphics/common/1.1/types.h>
namespace hidl_common = android::hardware::graphics::common::V1_1;
#elif HIDL_COMMON_VERSION_SCALED == 120
#include <android/hardware/graphics/common/1.2/types.h>
/* BufferUsage is not defined in 1.2/types.h as there are no changes from previous version */
namespace hidl_common = android::hardware::graphics::common::V1_1;
#endif


/* Local macro definitions to emulate Gralloc 1.0 usage interface */
#define GRALLOC_USAGE_PRIVATE_0 1ULL << 28
#define GRALLOC_USAGE_PRIVATE_1 1ULL << 29
#define GRALLOC_USAGE_PRIVATE_2 1ULL << 30
#define GRALLOC_USAGE_PRIVATE_3 1ULL << 31
#define GRALLOC_USAGE_PRIVATE_11 1ULL << 56
#define GRALLOC_USAGE_PRIVATE_12 1ULL << 55
#define GRALLOC_USAGE_PRIVATE_13 1ULL << 54
#define GRALLOC_USAGE_PRIVATE_14 1ULL << 53
#define GRALLOC_USAGE_PRIVATE_15 1ULL << 52
#define GRALLOC_USAGE_PRIVATE_16 1ULL << 51
#define GRALLOC_USAGE_PRIVATE_17 1ULL << 50
#define GRALLOC_USAGE_PRIVATE_18 1ULL << 49
#define GRALLOC_USAGE_PRIVATE_19 1ULL << 48

typedef enum
{

	/* 表征 "当前 调用 alloc() 的 client 通过 width 指定了其预期的 buffer stride",
	 * 即要求 gralloc 遵循 rk_implicit_alloc_semantic (即 满足 implicit_requirement_for_rk_gralloc_allocate).
	 */
	RK_GRALLOC_USAGE_SPECIFY_STRIDE = GRALLOC_USAGE_PRIVATE_2,

	/* 表征 "调用 alloc() 的 client 要求分配 底层 pages 是物理连续的 buffer".
	 * 目前的实现从 ion_cma_heap 分配.
	 */
	RK_GRALLOC_USAGE_PHY_CONTIG_BUFFER = GRALLOC_USAGE_PRIVATE_3,

	/* 表征 "调用 alloc() 的 client 要求分配的 buffer 的所有物理 page 的地址都在 4G 以内".
	 */
	RK_GRALLOC_USAGE_WITHIN_4G = GRALLOC_USAGE_PRIVATE_11,

	/* See comment for Gralloc 1.0, above. */
	MALI_GRALLOC_USAGE_FRONTBUFFER = GRALLOC_USAGE_PRIVATE_0,

	/* See comment for Gralloc 1.0, above. */
	MALI_GRALLOC_USAGE_FORCE_BACKBUFFER = GRALLOC_USAGE_PRIVATE_13,

	/* See comment for Gralloc 1.0, above. */
	MALI_GRALLOC_USAGE_NO_AFBC = GRALLOC_USAGE_PRIVATE_1,

	/* See comment for Gralloc 1.0, above. */
	MALI_GRALLOC_USAGE_AFBC_PADDING = GRALLOC_USAGE_PRIVATE_14,

	/* See comment for Gralloc 1.0, above. */
	MALI_GRALLOC_USAGE_PRIVATE_FORMAT = GRALLOC_USAGE_PRIVATE_15,

	/* YUV-only. */
	MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_DEFAULT = 0,
	MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_BT601 = GRALLOC_USAGE_PRIVATE_18,
	MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_BT709 = GRALLOC_USAGE_PRIVATE_19,
	MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_BT2020 =  (GRALLOC_USAGE_PRIVATE_18 | GRALLOC_USAGE_PRIVATE_19),
	MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_MASK = MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_BT2020,

	MALI_GRALLOC_USAGE_RANGE_DEFAULT = 0,
	MALI_GRALLOC_USAGE_RANGE_NARROW = GRALLOC_USAGE_PRIVATE_16,
	MALI_GRALLOC_USAGE_RANGE_WIDE = GRALLOC_USAGE_PRIVATE_17,
	MALI_GRALLOC_USAGE_RANGE_MASK = (GRALLOC_USAGE_PRIVATE_16 | GRALLOC_USAGE_PRIVATE_17),
} mali_gralloc_usage_type;

#define GRALLOC_USAGE_SW_WRITE_RARELY static_cast<uint64_t>(hidl_common::BufferUsage::CPU_WRITE_RARELY)
#define GRALLOC_USAGE_SW_WRITE_OFTEN static_cast<uint64_t>(hidl_common::BufferUsage::CPU_WRITE_OFTEN)
#define GRALLOC_USAGE_SW_READ_RARELY static_cast<uint64_t>(hidl_common::BufferUsage::CPU_READ_RARELY)
#define GRALLOC_USAGE_SW_READ_OFTEN static_cast<uint64_t>(hidl_common::BufferUsage::CPU_READ_OFTEN)
#define GRALLOC_USAGE_RENDERSCRIPT static_cast<uint64_t>(hidl_common::BufferUsage::RENDERSCRIPT)
#define GRALLOC_USAGE_HW_FB static_cast<uint64_t>(hidl_common::BufferUsage::COMPOSER_CLIENT_TARGET)

/* Bit 10 must be zero as per Gralloc 2.x interface specification. Used, however, for backward compatibility */
#define GRALLOC_USAGE_HW_2D  static_cast<uint64_t>(0x00000400)

#define GRALLOC_USAGE_SW_WRITE_MASK static_cast<uint64_t>(hidl_common::BufferUsage::CPU_WRITE_MASK)
#define GRALLOC_USAGE_SW_READ_MASK static_cast<uint64_t>(hidl_common::BufferUsage::CPU_READ_MASK)
#define GRALLOC_USAGE_PROTECTED static_cast<uint64_t>(hidl_common::BufferUsage::PROTECTED)
#define GRALLOC_USAGE_CURSOR static_cast<uint64_t>(hidl_common::BufferUsage::COMPOSER_CURSOR)
#define GRALLOC_USAGE_HW_RENDER static_cast<uint64_t>(hidl_common::BufferUsage::GPU_RENDER_TARGET)
#define GRALLOC_USAGE_HW_CAMERA_WRITE static_cast<uint64_t>(hidl_common::BufferUsage::CAMERA_OUTPUT)
#define GRALLOC_USAGE_HW_CAMERA_READ static_cast<uint64_t>(hidl_common::BufferUsage::CAMERA_INPUT)
#define GRALLOC_USAGE_HW_TEXTURE static_cast<uint64_t>(hidl_common::BufferUsage::GPU_TEXTURE)
#define GRALLOC_USAGE_HW_VIDEO_ENCODER static_cast<uint64_t>(hidl_common::BufferUsage::VIDEO_ENCODER)
#define GRALLOC_USAGE_HW_COMPOSER static_cast<uint64_t>(hidl_common::BufferUsage::COMPOSER_OVERLAY)
#define GRALLOC_USAGE_EXTERNAL_DISP  static_cast<uint64_t>(0x00002000)

#define GRALLOC_USAGE_SENSOR_DIRECT_DATA static_cast<uint64_t>(hidl_common::BufferUsage::SENSOR_DIRECT_DATA)
#define GRALLOC_USAGE_GPU_DATA_BUFFER static_cast<uint64_t>(hidl_common::BufferUsage::GPU_DATA_BUFFER)

#endif

/* Originally (Gralloc 0.x), Android did not provide an explicit DECODER usage. This was rectified in Android N-MR1/7.1
 * when Gralloc 1.0 defined GRALLOC1_PRODUCER_USAGE_VIDEO_DECODER. However, libstagefright continues
 * (currently, Android P/9.0) to use this old combination of flags to indicate decoder producer
 */
#define GRALLOC_USAGE_DECODER (GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_EXTERNAL_DISP)

static const uint64_t VALID_USAGE =
    GRALLOC_USAGE_SW_READ_MASK |       /* 0x0FU */
    GRALLOC_USAGE_SW_WRITE_MASK |      /* 0xF0U */
    GRALLOC_USAGE_HW_TEXTURE |         /* 1U << 8 */
    GRALLOC_USAGE_HW_RENDER |          /* 1U << 9 */
    GRALLOC_USAGE_HW_2D |              /* 1U << 10 */
    GRALLOC_USAGE_HW_COMPOSER |        /* 1U << 11 */
    GRALLOC_USAGE_HW_FB |              /* 1U << 12 */
    GRALLOC_USAGE_EXTERNAL_DISP |      /* 1U << 13 */
    GRALLOC_USAGE_PROTECTED |          /* 1U << 14 */
    GRALLOC_USAGE_CURSOR |             /* 1U << 15 */
    GRALLOC_USAGE_HW_VIDEO_ENCODER |   /* 1U << 16 */
    GRALLOC_USAGE_HW_CAMERA_WRITE |    /* 1U << 17 */
    GRALLOC_USAGE_HW_CAMERA_READ |     /* 1U << 18 */
    GRALLOC_USAGE_RENDERSCRIPT |       /* 1U << 20 */

    /* Producer and consumer usage are combined, but on Gralloc version 1 there is no way to differentiate these as they
     * are mapped to the same value (1U << 23). */
    GRALLOC_USAGE_SENSOR_DIRECT_DATA | /* 1U << 23 */
    GRALLOC_USAGE_GPU_DATA_BUFFER |    /* 1U << 24 */

    GRALLOC_USAGE_PRIVATE_19 |         /* 1U << 48 */
    GRALLOC_USAGE_PRIVATE_18 |         /* 1U << 49 */
    GRALLOC_USAGE_PRIVATE_17 |         /* 1U << 50 */
    GRALLOC_USAGE_PRIVATE_16 |         /* 1U << 51 */
    GRALLOC_USAGE_PRIVATE_15 |         /* 1U << 52 */
    GRALLOC_USAGE_PRIVATE_14 |         /* 1U << 53 */
    GRALLOC_USAGE_PRIVATE_13 |         /* 1U << 54 */
    GRALLOC_USAGE_PRIVATE_0 |          /* 1U << 28 */
    GRALLOC_USAGE_PRIVATE_1 |          /* 1U << 29 */
    0;

#endif /*MALI_GRALLOC_USAGES_H_*/
