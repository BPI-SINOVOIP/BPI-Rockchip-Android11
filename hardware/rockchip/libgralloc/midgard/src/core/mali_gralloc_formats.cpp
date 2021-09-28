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

#define ENABLE_DEBUG_LOG
#include "../custom_log.h"

#include <string.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <log/log.h>
#include <assert.h>
#include <vector>

#include <cutils/properties.h>

#include <hardware/gralloc1.h>

#include "gralloc_priv.h"
#include "mali_gralloc_bufferallocation.h"
#include "format_info.h"
#include "capabilities/gralloc_capabilities.h"

#if GRALLOC_USE_LEGACY_CALCS == 1
#include "legacy/buffer_alloc.h"
#endif

/* Producer/consumer definitions.
 * CPU: Software access
 * GPU: Graphics processor
 * DPU: Display processor
 * DPU_AEU: AFBC encoder (input to DPU)
 * VPU: Video processor
 * CAM: Camera ISP
 */
#define MALI_GRALLOC_PRODUCER_CPU     ((uint16_t)1 << 0)
#define MALI_GRALLOC_PRODUCER_GPU     ((uint16_t)1 << 1)
#define MALI_GRALLOC_PRODUCER_DPU     ((uint16_t)1 << 2)
#define MALI_GRALLOC_PRODUCER_DPU_AEU ((uint16_t)1 << 3)
#define MALI_GRALLOC_PRODUCER_VPU     ((uint16_t)1 << 4)
#define MALI_GRALLOC_PRODUCER_CAM     ((uint16_t)1 << 5)

#define MALI_GRALLOC_CONSUMER_CPU     ((uint16_t)1 << 0)
#define MALI_GRALLOC_CONSUMER_GPU     ((uint16_t)1 << 1)
#define MALI_GRALLOC_CONSUMER_DPU     ((uint16_t)1 << 2)
#define MALI_GRALLOC_CONSUMER_VPU     ((uint16_t)1 << 3)

typedef struct
{
	uint32_t base_format;
	uint64_t format_ext;
	format_support_flags f_flags;
} fmt_props;

/*
 * Determines all IP consumers included by the requested buffer usage.
 * Private usage flags are excluded from this process.
 *
 * @param usage   [in]    Buffer usage.
 *
 * @return flags word of all enabled consumers;
 *         0, if no consumers are enabled
 */
static uint16_t get_consumers(uint64_t usage)
{
	uint16_t consumers = 0;

	/* Private usage is not applicable to consumer derivation */
	usage &= ~GRALLOC_USAGE_PRIVATE_MASK;
	/* Exclude usages also not applicable to consumer derivation */
	usage &= ~GRALLOC_USAGE_PROTECTED;

	get_ip_capabilities();

	if (usage == GRALLOC_USAGE_HW_COMPOSER)
	{
		consumers = MALI_GRALLOC_CONSUMER_DPU;
	}
	else
	{
		if (usage & GRALLOC_USAGE_SW_READ_MASK)
		{
			consumers |= MALI_GRALLOC_CONSUMER_CPU;
		}

		/* GRALLOC_USAGE_HW_FB describes a framebuffer which contains a
		 * pre-composited scene that is scanned-out to a display. This buffer
		 * can be consumed by even the most basic display processor which does
		 * not support multi-layer composition.
		 */
		if (usage & GRALLOC_USAGE_HW_FB)
		{
			consumers |= MALI_GRALLOC_CONSUMER_DPU;
		}

		if (usage & GRALLOC_USAGE_HW_VIDEO_ENCODER)
		{
			consumers |= MALI_GRALLOC_CONSUMER_VPU;
		}

		/* GRALLOC_USAGE_HW_COMPOSER does not explicitly define whether the
		 * display processor is producer or consumer. When used in combination
		 * with GRALLOC_USAGE_HW_TEXTURE, it is assumed to be consumer since the
		 * GPU and DPU both act as compositors.
		 */
		if ((usage & (GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_COMPOSER)) ==
		    (GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_COMPOSER))
		{
			consumers |= MALI_GRALLOC_CONSUMER_DPU;
		}

		if (usage & (GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_GPU_DATA_BUFFER))
		{
			consumers |= MALI_GRALLOC_CONSUMER_GPU;
		}
	}

	return consumers;
}


/*
 * Determines all IP producers included by the requested buffer usage.
 * Private usage flags are excluded from this process.
 *
 * @param usage   [in]    Buffer usage.
 *
 * @return flags word of all enabled producers;
 *         0, if no producers are enabled
 */
static uint16_t get_producers(uint64_t usage)
{
	uint16_t producers = 0;

	/* Private usage is not applicable to producer derivation */
	usage &= ~GRALLOC_USAGE_PRIVATE_MASK;
	/* Exclude usages also not applicable to producer derivation */
	usage &= ~GRALLOC_USAGE_PROTECTED;

	get_ip_capabilities();

	if (usage == GRALLOC_USAGE_HW_COMPOSER)
	{
		producers = MALI_GRALLOC_PRODUCER_DPU_AEU;
	}
	else
	{
		if (usage & GRALLOC_USAGE_SW_WRITE_MASK)
		{
			producers |= MALI_GRALLOC_PRODUCER_CPU;
		}

		/* DPU is normally consumer however, when there is an alternative
		 * consumer (VPU) and no other producer (e.g. VPU), it acts as a producer.
		 */
		if ((usage & GRALLOC_USAGE_DECODER) != GRALLOC_USAGE_DECODER &&
		    (usage & (GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_VIDEO_ENCODER)) ==
		    (GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_VIDEO_ENCODER))
		{
			producers |= MALI_GRALLOC_PRODUCER_DPU;
		}

		if (usage & (GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_GPU_DATA_BUFFER))
		{
			producers |= MALI_GRALLOC_PRODUCER_GPU;
		}

		if (usage & GRALLOC_USAGE_HW_CAMERA_WRITE)
		{
			producers |= MALI_GRALLOC_PRODUCER_CAM;
		}

		/* Video decoder producer is signalled by a combination of usage flags
		 * (see definition of GRALLOC_USAGE_DECODER).
		 */
		if ((usage & GRALLOC_USAGE_DECODER) == GRALLOC_USAGE_DECODER)
		{
			producers |= MALI_GRALLOC_PRODUCER_VPU;
		}
	}

	return producers;
}


/*
 * Determines the intersection of all IP consumers capability sets. Since all
 * capabiltiies are positive, the intersection can be expressed via a logical
 * AND operation. Capabilities must be defined (OPTIONS_PRESENT) to indicate
 * that an IP is part of the media system (otherwise it will be ignored).
 * See definition of MALI_GRALLOC_FORMAT_CAPABILITY_* for more information.
 *
 * @param consumers   [in]    Buffer consumers.
 *
 * @return flags word of common capabilities shared by *all* consumers;
 *         0, if no capabilities are shared
 */
static uint64_t get_consumer_caps(const uint16_t consumers)
{
	uint64_t consumer_caps = ~0;

	get_ip_capabilities();

	/* Consumers can't write */
	consumer_caps &= ~MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_YUV_WRITE;

	if (consumers & MALI_GRALLOC_CONSUMER_CPU)
	{
		consumer_caps &= cpu_runtime_caps.caps_mask;
	}

	if (consumers & MALI_GRALLOC_CONSUMER_GPU &&
	    gpu_runtime_caps.caps_mask & MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT)
	{
		consumer_caps &= gpu_runtime_caps.caps_mask;
	}

	if (consumers & MALI_GRALLOC_CONSUMER_DPU &&
	    dpu_runtime_caps.caps_mask & MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT)
	{
		consumer_caps &= dpu_runtime_caps.caps_mask;
	}

	if (consumers & MALI_GRALLOC_CONSUMER_VPU &&
	    vpu_runtime_caps.caps_mask & MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT)
	{
		consumer_caps &= vpu_runtime_caps.caps_mask;
	}

	return consumer_caps;
}


/*
 * Determines the intersection of all IP producers capability sets. Since all
 * capabiltiies are positive, the intersection can be expressed via a logical
 * AND operation. Capabilities must be defined (OPTIONS_PRESENT) to indicate
 * that an IP is part of the media system (otherwise it will be ignored).
 * See definition of MALI_GRALLOC_FORMAT_CAPABILITY_* for more information.
 *
 * @param producers   [in]    Buffer producers.
 *
 * @return flags word of common capabilities shared by *all* producers;
 *         0, if no capabilities are shared
 */
static uint64_t get_producer_caps(const uint16_t producers)
{
	uint64_t producer_caps = ~0;

	if (producers == 0)
	{
		/* When no producer is specified assume no capabilities. */
		return 0;
	}

	get_ip_capabilities();

	/* Producers can't read */
	producer_caps &= ~MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_YUV_READ;

	if (producers & MALI_GRALLOC_PRODUCER_CPU)
	{
		producer_caps &= cpu_runtime_caps.caps_mask;
	}

	if (producers & MALI_GRALLOC_PRODUCER_GPU &&
	    gpu_runtime_caps.caps_mask & MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT)
	{
		producer_caps &= gpu_runtime_caps.caps_mask;
	}

	if (producers & MALI_GRALLOC_PRODUCER_DPU &&
	    dpu_runtime_caps.caps_mask & MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT)
	{
		producer_caps &= dpu_runtime_caps.caps_mask;
	}

	if (producers & MALI_GRALLOC_PRODUCER_DPU_AEU &&
	    dpu_aeu_runtime_caps.caps_mask & MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT)
	{
		producer_caps &= dpu_aeu_runtime_caps.caps_mask;
	}

	if (producers & MALI_GRALLOC_PRODUCER_CAM &&
	    cam_runtime_caps.caps_mask & MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT)
	{
		producer_caps &= cam_runtime_caps.caps_mask;
	}

	if (producers & MALI_GRALLOC_PRODUCER_VPU &&
	    vpu_runtime_caps.caps_mask & MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT)
	{
		producer_caps &= vpu_runtime_caps.caps_mask;
	}

	return producer_caps;
}


#if GRALLOC_USE_LEGACY_CALCS == 1
namespace legacy
{

void mali_gralloc_adjust_dimensions(const uint64_t internal_format,
                                    const uint64_t usage,
                                    const alloc_type_t type,
                                    const uint32_t width,
                                    const uint32_t height,
                                    int * const internal_width,
                                    int * const internal_height)
{
	/* Determine producers and consumers. */
	const uint16_t producers = get_producers(usage);
	const uint16_t consumers = get_consumers(usage);

	/*
	 * Default: define internal dimensions the same as public.
	 */
	*internal_width = width;
	*internal_height = height;

	/*
	 * Video producer requires additional height padding of AFBC buffers (whole
	 * rows of 16x16 superblocks). Cropping will be applied to internal
	 * dimensions to fit the public size.
	 */
	if (producers & MALI_GRALLOC_PRODUCER_VPU)
	{
		if (internal_format & MALI_GRALLOC_INTFMT_AFBC_BASIC)
		{
			switch (internal_format & MALI_GRALLOC_INTFMT_FMT_MASK)
			{
			/* 8-bit/10-bit YUV420 formats. */
			case MALI_GRALLOC_FORMAT_INTERNAL_YV12:
			case MALI_GRALLOC_FORMAT_INTERNAL_NV12:
			case MALI_GRALLOC_FORMAT_INTERNAL_NV21:
			case MALI_GRALLOC_FORMAT_INTERNAL_Y0L2:
				*internal_height += (internal_format & MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS) ? 16 : 32;
				break;

			default:
				break;
			}
		}
	}

	get_afbc_alignment(*internal_width, *internal_height, type,
	                   internal_width, internal_height);

out:
	MALI_GRALLOC_LOGV("%s: internal_format=0x%" PRIx64 " usage=0x%" PRIx64
	      " width=%u, height=%u, internal_width=%d, internal_height=%d",
	      __FUNCTION__, internal_format, usage, width, height, *internal_width, *internal_height);
}

}
#endif /* end of legacy */

#define AFBC_BUFFERS_HORIZONTAL_PIXEL_STRIDE_ALIGNMENT_REQUIRED_BY_356X_VOP	(64)
#define AFBC_BUFFERS_VERTICAL_PIXEL_STRIDE_ALIGNMENT_REQUIRED_BY_356X_VOP	(16)

/*
 * Update buffer dimensions for producer/consumer constraints. This process is
 * not valid with CPU producer/consumer since the new resolution cannot be
 * communicated to generic clients through the public APIs. Adjustments are
 * likely to be related to AFBC.
 *
 * @param alloc_format   [in]    Format (inc. modifiers) to be allocated.
 * @param usage          [in]    Buffer usage.
 * @param width          [inout] Buffer width (in pixels).
 * @param height         [inout] Buffer height (in pixels).
 *
 * @return none.
 */
void mali_gralloc_adjust_dimensions(const uint64_t alloc_format,
                                    const uint64_t usage,
                                    int* const width,
                                    int* const height)
{
	/* Determine producers and consumers. */
	const uint16_t producers = get_producers(usage);
	const uint16_t consumers = get_consumers(usage);

	/*-------------------------------------------------------*/

	/* 若当前 buffer 是 AFBC 格式, 且 VOP 是 consumer, 且 VPU 是 producer, 则 ... */
	if ( (alloc_format & MALI_GRALLOC_INTFMT_AFBC_BASIC)
		&& (consumers & MALI_GRALLOC_CONSUMER_DPU)	// "DPU" : Display processor, 就是 RK 的 VOP.
		&& (producers & MALI_GRALLOC_PRODUCER_VPU) )
	{
		const uint32_t base_format = alloc_format & MALI_GRALLOC_INTFMT_FMT_MASK;

		/* 若 base_format "是" 被 的 rk_video 使用 格式, 则 ... */
		if ( is_base_format_used_by_rk_video(base_format) )
		{
			const int pixel_stride = *width; // pixel_stride_ask_by_rk_video

			/* 若 'pixel_stride' "没有" 按照 VOP 的要求对齐. */
			if ( pixel_stride % AFBC_BUFFERS_HORIZONTAL_PIXEL_STRIDE_ALIGNMENT_REQUIRED_BY_356X_VOP != 0 )
			{
				W("pixel_stride_ask_by_rk_video(%d) is not %d aligned required by 356x VOP",
						 pixel_stride,
						 AFBC_BUFFERS_HORIZONTAL_PIXEL_STRIDE_ALIGNMENT_REQUIRED_BY_356X_VOP);
			}
		}
	}

	/*-------------------------------------------------------*/

#if 0	// 这段逻辑不 适用于 RK 的 VPU.
	/*
	 * Video producer requires additional height padding of AFBC buffers (whole
	 * rows of 16x16 superblocks). Cropping will be applied to internal
	 * dimensions to fit the public size.
	 */
	if ((producers & MALI_GRALLOC_PRODUCER_VPU) &&
		(alloc_format & MALI_GRALLOC_INTFMT_AFBC_BASIC))
	{
		const int32_t idx = get_format_index(alloc_format & MALI_GRALLOC_INTFMT_FMT_MASK);
		if (idx != -1)
		{
			/* 8-bit/10-bit YUV420 formats. */
			if (formats[idx].is_yuv && formats[idx].hsub == 2 && formats[idx].vsub == 2)
			{
				*height += (alloc_format & MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS) ? 16 : 32;
			}
		}
	}
#endif

	if (producers & MALI_GRALLOC_PRODUCER_GPU)
	{
		/* Pad all AFBC allocations to multiple of GPU tile size. */
		if (alloc_format & MALI_GRALLOC_INTFMT_AFBC_BASIC)
		{
			*width = GRALLOC_ALIGN(*width, 16);
			*height = GRALLOC_ALIGN(*height, 16);
		}
	}

out:
	MALI_GRALLOC_LOGV("%s: alloc_format=0x%" PRIx64 " usage=0x%" PRIx64
	      " alloc_width=%u, alloc_height=%u",
	      __FUNCTION__, alloc_format, usage, *width, *height);
}


/*
 * Obtain level of support for base format across all producers and consumers as
 * defined by IP support table. This support is defined for the most capable IP -
 * specific IP might have reduced support based on specific capabilities.
 *
 * @param producers      [in]    Producers (flags).
 * @param consumers      [in]    Consumers (flags).
 * @param format         [in]    Format entry in IP support table.
 *
 * @return format support flags.
 */
static format_support_flags ip_supports_base_format(const uint16_t producers,
                                                    const uint16_t consumers,
                                                    const format_ip_support_t * const format)
{
	format_support_flags support = ~0;

	/* Determine producer support for base format. */
	if (producers & MALI_GRALLOC_PRODUCER_CPU)
	{
		support &= format->cpu_wr;
	}
	if (producers & MALI_GRALLOC_PRODUCER_GPU)
	{
		support &= format->gpu_wr;
	}
	if (producers & MALI_GRALLOC_PRODUCER_DPU)
	{
		support &= format->dpu_wr;
	}
	if (producers & MALI_GRALLOC_PRODUCER_DPU_AEU)
	{
		support &= format->dpu_aeu_wr;
	}
	if (producers & MALI_GRALLOC_PRODUCER_CAM)
	{
		support &= format->cam_wr;
	}
	if (producers & MALI_GRALLOC_PRODUCER_VPU)
	{
		support &= format->vpu_wr;
	}

	/* Determine producer support for base format. */
	if (consumers & MALI_GRALLOC_CONSUMER_CPU)
	{
		support &= format->cpu_rd;
	}
	if (consumers & MALI_GRALLOC_CONSUMER_GPU)
	{
		support &= format->gpu_rd;
	}
	if (consumers & MALI_GRALLOC_CONSUMER_DPU)
	{
		support &= format->dpu_rd;
	}
	if (consumers & MALI_GRALLOC_CONSUMER_VPU)
	{
		support &= format->vpu_rd;
	}

	return support;
}


/*
 * Determines whether a base format is subsampled YUV, where each
 * chroma channel has fewer samples than the luma channel. The
 * sub-sampling is always a power of 2.
 *
 * @param base_format   [in]    Base format (internal).
 *
 * @return 1, where format is subsampled YUV;
 *         0, otherwise
 */
bool is_subsampled_yuv(const uint32_t base_format)
{
	unsigned long i;

	for (i = 0; i < num_formats; i++)
	{
		if (formats[i].id == (base_format & MALI_GRALLOC_INTFMT_FMT_MASK))
		{
			if (formats[i].is_yuv == true &&
			    (formats[i].hsub > 1 || formats[i].vsub > 1))
			{
				return true;
			}
		}
	}
	return false;
}

bool is_base_format_used_by_rk_video(const uint32_t base_format)
{
	if ( MALI_GRALLOC_FORMAT_INTERNAL_NV12 == base_format
		|| MALI_GRALLOC_FORMAT_INTERNAL_NV16 == base_format
		|| MALI_GRALLOC_FORMAT_INTERNAL_YUV420_8BIT_I == base_format
		|| MALI_GRALLOC_FORMAT_INTERNAL_YUV420_10BIT_I == base_format
		|| MALI_GRALLOC_FORMAT_INTERNAL_YUV422_8BIT == base_format
		|| MALI_GRALLOC_FORMAT_INTERNAL_Y210 == base_format )
	{
		return true;
	}
	else
	{
		return false;
	}
}

/*---------------------------------------------------------------------------*/

/*
 * Determines whether multi-plane AFBC (requires specific IP capabiltiies) is
 * supported across all producers and consumers.
 *
 * @param producers      [in]    Producers (flags).
 * @param consumers      [in]    Consumers (flags).
 * @param producer_caps  [in]    Producer capabilities (flags).
 * @param consumer_caps  [in]    Consumer capabilities (flags).
 *
 * @return 1, multiplane AFBC is supported
 *         0, otherwise
 */
static inline bool is_afbc_multiplane_supported(const uint16_t producers,
                                                const uint16_t consumers,
                                                const uint64_t producer_caps,
                                                const uint64_t consumer_caps)
{
	GRALLOC_UNUSED(consumers);

	return (producer_caps & consumer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_BASIC &&
	        producer_caps & consumer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_TILED_HEADERS &&
	        producer_caps & consumer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_EXTRAWIDEBLK &&
	        /*no_producer*/ consumer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_MULTIPLANE_READ &&
	        producers == 0) ? true : false;
}


/*
 * Determines whether a given base format is supported by all producers and
 * consumers. After checking broad support across producer/consumer IP, this
 * function uses capabilities to disable features (base formats and AFBC
 * modifiers) that are not supported by specific versions of each IP.
 *
 * @param fmt_idx        [in]    Index into format properties table (base format).
 * @param ip_fmt_idx     [in]    Index into format IP support table (base format).
 * @param usage          [in]    Buffer usage.
 * @param producers      [in]    Producers (flags).
 * @param consumers      [in]    Consumers (flags).
 * @param producer_caps  [in]    Producer capabilities (flags).
 * @param consumer_caps  [in]    Consumer capabilities (flags).
 *
 * @return format support flags.
 */
static format_support_flags is_format_supported(const int32_t fmt_idx,
                                                const int32_t ip_fmt_idx,
                                                const uint64_t usage,
                                                const uint16_t producers,
                                                const uint16_t consumers,
                                                const uint64_t producer_caps,
                                                const uint64_t consumer_caps)
{
	/* Determine format support from table. */
	format_support_flags f_flags = ip_supports_base_format(producers, consumers,
	                                                       &formats_ip_support[ip_fmt_idx]);

	/* Determine whether producers/consumers support required AFBC features. */
	if (f_flags & F_AFBC)
	{
		if (!formats[fmt_idx].afbc ||
		    (producer_caps & consumer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_BASIC) == 0)
		{
			f_flags &= ~F_AFBC;
		}

		/* Check that multi-plane format supported by producers/consumers. */
		if (formats[fmt_idx].npln > 1 &&
		    !is_afbc_multiplane_supported(producers, consumers, producer_caps, consumer_caps))
		{
			f_flags &= ~F_AFBC;
		}

		/* Apply some additional restrictions from producer_caps and consumer_caps */
		/* Some modifiers affect base format support */
		if (formats[fmt_idx].is_yuv)
		{
			if ((producer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_YUV_WRITE) == 0)
			{
				f_flags &= ~F_AFBC;
			}

			if ((consumer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_YUV_READ) == 0)
			{
				f_flags &= ~F_AFBC;
			}
		}

		if (usage & MALI_GRALLOC_USAGE_FRONTBUFFER)
		{
			if ((producer_caps & consumer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_DOUBLE_BODY) == 0)
			{
				f_flags &= ~F_AFBC;
			}
		}
	}
	if (f_flags != F_NONE)
	{
		if (formats[fmt_idx].id == MALI_GRALLOC_FORMAT_INTERNAL_RGBA_1010102 &&
		    (producer_caps & consumer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_PIXFMT_RGBA1010102) == 0)
		{
			f_flags = F_NONE;
		}
		else if (formats[fmt_idx].id == MALI_GRALLOC_FORMAT_INTERNAL_RGBA_16161616)
		{
			if ((producer_caps & consumer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_PIXFMT_RGBA16161616) == 0)
			{
				f_flags = F_NONE;
			}
			else if ((producer_caps & consumer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_RGBA16161616) == 0)
			{
				f_flags = F_LIN;
			}
		}
	}

	return f_flags;
}


/*
 * Ensures that the allocation format conforms to the AFBC specification and is
 * supported by producers and consumers. Format modifiers are (in most cases)
 * disabled as required to make valid. It is important to first resolve invalid
 * combinations which are not dependent upon others to reduce the possibility of
 * circular dependency.
 *
 * @param alloc_format          [in]    Allocation format (base + modifiers).
 * @param producer_active_caps  [in]    Producer capabilities (flags).
 * @param consumer_active_caps  [in]    Consumer capabilities (flags).
 *
 * @return valid alloc_format with AFBC possibly disabled (if required)
 */
static uint64_t validate_afbc_format(uint64_t alloc_format,
                                     const uint64_t producer_active_caps,
                                     const uint64_t consumer_active_caps)
{
	const uint32_t base_format = alloc_format & MALI_GRALLOC_INTFMT_FMT_MASK;

	/*
	 * AFBC with tiled-headers must be enabled for AFBC front-buffer-safe allocations.
	 * NOTE: format selection algorithm will always try and enable AFBC with
	 * tiled-headers where supported by producer(s) and consumer(s).
	 */
	if (alloc_format & MALI_GRALLOC_INTFMT_AFBC_DOUBLE_BODY)
	{
		/*
		 * Disable (extra-) wide-block which is unsupported with front-buffer safe AFBC.
		 */
		alloc_format &= ~MALI_GRALLOC_INTFMT_AFBC_WIDEBLK;
		alloc_format &= ~MALI_GRALLOC_INTFMT_AFBC_EXTRAWIDEBLK;
	}

	/*
	 * AFBC specification: Split-block is not supported for
	 * subsampled formats (YUV) when wide-block is enabled.
	 */
	if (alloc_format & MALI_GRALLOC_INTFMT_AFBC_WIDEBLK &&
	    alloc_format & MALI_GRALLOC_INTFMT_AFBC_SPLITBLK &&
	    is_subsampled_yuv(base_format))
	{
		/* Disable split-block instead of wide-block because because
		 * wide-block has greater impact on display performance.
		 */
		alloc_format &= ~MALI_GRALLOC_INTFMT_AFBC_SPLITBLK;
	}

	/* AFBC specification: Split-block must be enabled for
	 * non-subsampled formats > 16 bpp, where wide-block is enabled.
	 */
	if (alloc_format & MALI_GRALLOC_INTFMT_AFBC_WIDEBLK &&
	   (alloc_format & MALI_GRALLOC_INTFMT_AFBC_SPLITBLK) == 0 &&
	   !is_subsampled_yuv(base_format) &&
	   base_format != MALI_GRALLOC_FORMAT_INTERNAL_RGB_565)
	{
		/* Enable split-block if supported by producer(s) & consumer(s),
		 * otherwise disable wide-block.
		 */
		if (producer_active_caps & consumer_active_caps &
			MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_SPLITBLK)
		{
			alloc_format |= MALI_GRALLOC_INTFMT_AFBC_SPLITBLK;
		}
		else
		{
			alloc_format &= ~MALI_GRALLOC_INTFMT_AFBC_WIDEBLK;
		}
	}

	/* Some RGB formats don't support split block. */
	if (base_format == MALI_GRALLOC_FORMAT_INTERNAL_RGB_565)
	{
		alloc_format &= ~MALI_GRALLOC_INTFMT_AFBC_SPLITBLK;
	}

	/* Ensure that AFBC features are supported by producers/consumers. */
	if ((alloc_format & MALI_GRALLOC_INTFMT_AFBC_BASIC) &&
	    (producer_active_caps & consumer_active_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_BASIC) == 0)
	{
		MALI_GRALLOC_LOGE("AFBC basic selected but not supported by producer/consumer. Disabling "
		       "MALI_GRALLOC_INTFMT_AFBC_BASIC");
		alloc_format &= ~MALI_GRALLOC_INTFMT_AFBC_BASIC;
	}

	if ((alloc_format & MALI_GRALLOC_INTFMT_AFBC_SPLITBLK) &&
	    (producer_active_caps & consumer_active_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_SPLITBLK) == 0)
	{
		MALI_GRALLOC_LOGE("AFBC split-block selected but not supported by producer/consumer. Disabling "
		       "MALI_GRALLOC_INTFMT_AFBC_SPLITBLK");
		alloc_format &= ~MALI_GRALLOC_INTFMT_AFBC_SPLITBLK;
	}

	if ((alloc_format & MALI_GRALLOC_INTFMT_AFBC_WIDEBLK) &&
	    (producer_active_caps & consumer_active_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WIDEBLK) == 0)
	{
		MALI_GRALLOC_LOGE("AFBC wide-block selected but not supported by producer/consumer. Disabling "
		       "MALI_GRALLOC_INTFMT_AFBC_WIDEBLK");
		alloc_format &= ~MALI_GRALLOC_INTFMT_AFBC_WIDEBLK;
	}

	if ((alloc_format & MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS) &&
	    (producer_active_caps & consumer_active_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_TILED_HEADERS) == 0)
	{
		MALI_GRALLOC_LOGE("AFBC tiled-headers selected but not supported by producer/consumer. Disabling "
		       "MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS");
		alloc_format &= ~MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS;
	}

	if (!((alloc_format & MALI_GRALLOC_INTFMT_AFBC_SPARSE) ||
	      (producer_active_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WRITE_NON_SPARSE)))
	{
		MALI_GRALLOC_LOGE("AFBC sparse not selected while producer cannot write non-sparse. Enabling "
		      "MALI_GRALLOC_INTFMT_AFBC_SPARSE");
		alloc_format |= MALI_GRALLOC_INTFMT_AFBC_SPARSE;
	}

	return alloc_format;
}


/*
 * Derives a valid AFBC format (via modifiers) for all producers and consumers.
 * Formats are validated after enabling the largest feature set supported (and
 * desirable) for the IP usage. Some format modifier combinations are not
 * compatible. See MALI_GRALLOC_INTFMT_* modifiers for more information.
 *
 * @param base_format    [in]    Base format (internal).
 * @param usage          [in]    Buffer usage.
 * @param producer       [in]    Buffer producers (write).
 * @param consumer       [in]    Buffer consumers (read).
 * @param producer_caps  [in]    Buffer producer capabilities (intersection).
 * @param consumer_caps  [in]    Buffer consumer capabilities (intersection).
 *
 * @return valid AFBC format, where modifiers are enabled (supported/preferred);
 *         base format without modifers, otherwise
 */
static uint64_t get_afbc_format(const uint32_t base_format,
                                const uint64_t usage,
                                const uint16_t producer,
                                const uint16_t consumer,
                                const uint64_t producer_caps,
                                const uint64_t consumer_caps)
{
	uint64_t alloc_format = base_format;

	/*
	 * Determine AFBC modifiers where capabilities are defined for all producers
	 * and consumers. NOTE: AFBC is not supported for video transcode (VPU --> VPU).
	 */
	if (producer_caps & consumer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT &&
	    ((producer & MALI_GRALLOC_PRODUCER_VPU) == 0 || (consumer & MALI_GRALLOC_CONSUMER_VPU) == 0))
	{
		if (producer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_BASIC &&
		    consumer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_BASIC)
		{
			alloc_format |= MALI_GRALLOC_INTFMT_AFBC_BASIC;

			const int format_idx = get_format_index(base_format);
			if (format_idx != -1)
			{
				if (formats[format_idx].yuv_transform == true)
				{
					alloc_format |= MALI_GRALLOC_INTFMT_AFBC_YUV_TRANSFORM;
				}
			}

			if (!(producer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WRITE_NON_SPARSE))
			{
				alloc_format |= MALI_GRALLOC_INTFMT_AFBC_SPARSE;
			}

			if (producer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_TILED_HEADERS &&
			    consumer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_TILED_HEADERS)
			{
				alloc_format |= MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS;

				if (usage & MALI_GRALLOC_USAGE_FRONTBUFFER &&
				    producer_caps & consumer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_DOUBLE_BODY)
				{
					alloc_format |= MALI_GRALLOC_INTFMT_AFBC_DOUBLE_BODY;
				}
			}

			/*
			 * Specific producer/consumer combinations benefit from additional
			 * AFBC features (e.g. GPU --> DPU).
			 */
			if (producer & MALI_GRALLOC_PRODUCER_GPU && consumer & MALI_GRALLOC_CONSUMER_DPU &&
			    dpu_runtime_caps.caps_mask & MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT)
			{
				if (producer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_SPLITBLK &&
				    consumer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_SPLITBLK)
				{
					alloc_format |= MALI_GRALLOC_INTFMT_AFBC_SPLITBLK;
				}

				/*
				 * NOTE: assume that all AFBC layers are pre-rotated. 16x16 SB
				 * must be used with DPU consumer when rotation is required.
				 */
				if (producer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WIDEBLK &&
				    consumer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WIDEBLK)
				{
					alloc_format |= MALI_GRALLOC_INTFMT_AFBC_WIDEBLK;
				}
			}
		}
	}

	alloc_format = validate_afbc_format(alloc_format, producer_caps, consumer_caps);

	return alloc_format;
}

/*
 * Obtains the 'active' capabilities (for producers/consumers) by applying
 * additional constraints to the capabilities declared for each IP. Some rules
 * are based on format, others specific to producer/consumer. This function must
 * be careful not to make any assumptions about the base format properties since
 * fallback might still occur. It is safe to use any properties which are common
 * across all compatible formats as defined by is_format_compatible().
 *
 * @param format                 [in]    Base format requested.
 * @param producers              [in]    Producers (flags).
 * @param consumers              [in]    Consumers (flags).
 * @param producer_active_caps   [out]   Active producer capabilities (flags).
 * @param consumer_active_caps   [out]   Active consumer capabilities (flags).
 * @param buffer_size            [in]    Buffer resolution (w x h, in pixels).
 *
 * @return none.
 */
static void get_active_caps(const format_info_t format,
                            const uint16_t producers,
                            const uint16_t consumers,
                            uint64_t * const producer_active_caps,
                            uint64_t * const consumer_active_caps,
                            const int buffer_size)
{
	const uint64_t producer_caps = (producer_active_caps) ? *producer_active_caps : 0;
	const uint64_t consumer_caps = (consumer_active_caps) ? *consumer_active_caps : 0;
	uint64_t producer_mask = ~0;
	uint64_t consumer_mask = ~0;

	if (format.is_yuv)
	{
		/* AFBC wide-block is not supported across IP for YUV formats. */
		producer_mask &= ~MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WIDEBLK;
		consumer_mask &= ~MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WIDEBLK;

		if ((producer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_YUV_WRITE) == 0)
		{
			producer_mask &= ~MALI_GRALLOC_FORMAT_CAPABILITY_AFBCENABLE_MASK;
		}
		else if (producers & MALI_GRALLOC_PRODUCER_GPU)
		{
			/* All GPUs that can write YUV AFBC can only do it in 16x16,
			 * optionally with tiled headers.
			 */
			producer_mask &=
				~(MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_SPLITBLK |
				  MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WIDEBLK);
		}

		if ((consumer_caps & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_YUV_READ) == 0)
		{
			consumer_mask &= ~MALI_GRALLOC_FORMAT_CAPABILITY_AFBCENABLE_MASK;
		}
	}

	if (producers & MALI_GRALLOC_PRODUCER_DPU_AEU ||
	    consumers & MALI_GRALLOC_CONSUMER_DPU)
	{
		/* DPU does not support split-block other than RGB(A) 24/32-bit */
		if (!format.is_rgb || format.bpp[0] < 24)
		{
			if (producers & MALI_GRALLOC_PRODUCER_DPU_AEU)
			{
				producer_mask &= ~MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_SPLITBLK;
			}
			if (consumers & MALI_GRALLOC_CONSUMER_DPU)
			{
				consumer_mask &= ~MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_SPLITBLK;
			}
		}
	}

	if (consumers & MALI_GRALLOC_CONSUMER_DPU)
	{
		bool afbc_allowed = false;

        GRALLOC_UNUSED(buffer_size);

#if MALI_DISPLAY_VERSION == 550 || MALI_DISPLAY_VERSION == 650
#if GRALLOC_DISP_W != 0 && GRALLOC_DISP_H != 0
#define GRALLOC_AFBC_MIN_SIZE 75
		/* Disable AFBC based on buffer dimensions */
		afbc_allowed = ((buffer_size * 100) / (GRALLOC_DISP_W * GRALLOC_DISP_H)) >= GRALLOC_AFBC_MIN_SIZE;
#else /* If display size is not valid then always allow AFBC */
		afbc_allowed = true;
#endif
#else /* For cetus, always allow AFBC */
		afbc_allowed = true;
#endif
		if (!afbc_allowed)
		{
			consumer_mask &= ~MALI_GRALLOC_FORMAT_CAPABILITY_AFBCENABLE_MASK;
		}
	}

	if (producer_active_caps)
	{
		*producer_active_caps &= producer_mask;
	}
	if (consumer_active_caps)
	{
		*consumer_active_caps &= consumer_mask;
	}
}


/*
 * Obtains support flags and modifiers for base format.
 *
 * @param base_format           [in]    Base format for which to deduce support.
 * @param usage                 [in]    Buffer usage.
 * @param producers             [in]    Producers (flags).
 * @param consumers             [in]    Consumers (flags).
 * @param producer_active_caps  [in]    Producer capabilities (flags).
 * @param consumer_active_caps  [in]    Consumer capabilities (flags).
 * @param fmt_supported         [out]   Format (base + modifiers) and support (flags).
 *
 * @return 1, base format supported
 *         0, otherwise
 */
bool get_supported_format(const uint32_t base_format,
                          const uint64_t usage,
                          const uint16_t producers,
                          const uint16_t consumers,
                          const uint64_t producer_active_caps,
                          const uint64_t consumer_active_caps,
                          fmt_props * const fmt_supported)
{
	const int32_t fmt_idx = get_format_index(base_format);
	const int32_t ip_fmt_idx = get_ip_format_index(base_format);
	assert(fmt_idx >= 0);
	if (ip_fmt_idx == -1)
	{
		/* Return undefined base format. */
		MALI_GRALLOC_LOGE("Failed to find IP support info for format id: 0x%" PRIx32,
		      base_format);
		return false;
	}

	fmt_supported->f_flags = is_format_supported(fmt_idx,
	                                             ip_fmt_idx,
	                                             usage,
	                                             producers,
	                                             consumers,
	                                             producer_active_caps,
	                                             consumer_active_caps);
	MALI_GRALLOC_LOGV("IP support: 0x%" PRIx16, fmt_supported->f_flags);
	if (fmt_supported->f_flags == F_NONE &&
	    consumers & MALI_GRALLOC_CONSUMER_GPU &&
	    consumers & MALI_GRALLOC_CONSUMER_DPU)
	{
		/* Determine alternative caps for formats when GPU/DPU consumer.
		 * Although we normally combine capabilities for multiple consumers with "AND",
		 * in some situations (e.g. formats) we make best effort and say that fallback
		 * to GPU is acceptable and perferred over rejecting allocation. GPU composition
		 * must always be supported in case of fallback from DPU.
		 */
		const uint16_t consumers_nodpu = consumers & ~MALI_GRALLOC_CONSUMER_DPU;
		uint64_t consumer_nodpu_caps = consumer_active_caps;

		/* Set consumer caps to GPU-only (assume superset of DPU). */
		consumer_nodpu_caps = get_consumer_caps(consumers_nodpu);
		get_active_caps(formats[fmt_idx],
		                producers, consumers_nodpu,
		                NULL, &consumer_nodpu_caps,
		                0 /* N/A without DPU consumer */);

		fmt_supported->f_flags = is_format_supported(fmt_idx,
	                                                 ip_fmt_idx,
		                                             usage,
	                                                 producers,
	                                                 consumers_nodpu,
	                                                 producer_active_caps,
	                                                 consumer_nodpu_caps);
	}

	fmt_supported->base_format = base_format;
	if (fmt_supported->f_flags & F_AFBC)
	{
		const uint64_t afbc_format = get_afbc_format(base_format,
		                                             usage,
		                                             producers,
		                                             consumers,
		                                             producer_active_caps,
		                                             consumer_active_caps);

		MALI_GRALLOC_LOGV("AFBC format: 0x%" PRIx64, afbc_format);

		/* Disable AFBC when forced by usage or no format modifiers selected. */
		if ((usage & MALI_GRALLOC_USAGE_NO_AFBC) == MALI_GRALLOC_USAGE_NO_AFBC ||
		    afbc_format == fmt_supported->base_format)
		{
			fmt_supported->f_flags &= ~F_AFBC;
		}

		/* Check that AFBC features are correct for multiplane format. */
		alloc_type_t alloc_type{};
		get_alloc_type(afbc_format & MALI_GRALLOC_INTFMT_EXT_MASK,
					   fmt_idx,
					   usage,
					   &alloc_type);
		if (formats[fmt_idx].npln > 1 && alloc_type.is_multi_plane == false)
		{
			fmt_supported->f_flags &= ~F_AFBC;
		}

		/* Store any format modifiers */
		fmt_supported->format_ext = afbc_format & MALI_GRALLOC_INTFMT_EXT_MASK;
	}
	if ((fmt_supported->f_flags & F_AFBC) == 0)
	{
		fmt_supported->format_ext = 0;
	}

	MALI_GRALLOC_LOGV("Ext format: 0x%" PRIx64, fmt_supported->format_ext);

	return (fmt_supported->f_flags == F_NONE) ? false : true;
}


/*
 * Determines whether two base formats have comparable 'color' components. Alpha
 * is considered unimportant for YUV formats.
 *
 * @param f_old     [in]    Format properties (old format).
 * @param f_new     [in]    Format properties (new format).
 *
 * @return 1, format components are equivalent
 *         0, otherwise
 */
static bool comparable_components(const format_info_t * const f_old,
                                  const format_info_t * const f_new)
{
	if (f_old->is_yuv && f_new->bps == f_old->bps)
	{
		/* Formats have the same number of components. */
		if (f_new->total_components() == f_old->total_components())
		{
			return true;
		}

		/* Alpha component can be dropped for yuv formats.
		 * This assumption is required for mapping Y0L2 to
		 * single plane 10-bit YUV420 AFBC.
		 */
		if (f_old->has_alpha)
		{
			if (f_new->total_components() == 3 &&
			    f_new->is_yuv &&
			    !f_new->has_alpha)
			{
				return true;
			}
		}
	}
	else if (f_old->is_rgb)
	{
		if (f_new->total_components() == f_old->total_components())
		{
			if (f_new->bpp[0] == f_old->bpp[0] && f_new->bps == f_old->bps)
			{
				return true;
			}
		}
	}
	else
	{
		if (f_new->id == f_old->id)
		{
			return true;
		}
	}

	return false;
}


/*
 * Determines whether two base formats are compatible such that data from one
 * format could be accurately represented/interpreted in the other format.
 *
 * @param f_old     [in]    Format properties (old format).
 * @param f_new     [in]    Format properties (new format).
 *
 * @return 1, formats are equivalent
 *         0, otherwise
 */
static bool is_format_compatible(const format_info_t * const f_old,
                                 const format_info_t * const f_new)
{
	if (f_new->hsub == f_old->hsub &&
	    f_new->vsub == f_old->vsub &&
	    f_new->is_rgb == f_old->is_rgb &&
	    f_new->is_yuv == f_old->is_yuv &&
	    comparable_components(f_old, f_new))
	{
		return true;
	}
	else
	{
		return false;
	}
}

/**
 * Provide a grade for the compatible format with respect to the requested format. Used to find the best compatible
 * format.
 *
 * @param fmt[in]    Compatible format properties.
 * @param req_format Requested base format.
 *
 * @return The grade of the compatible format. Higher is better. Returns 0 if format extensions are incompatible with
 * requested format.
 */
uint64_t grade_format(const fmt_props &fmt, uint32_t req_format)
{
	uint64_t grade = 1;

	GRALLOC_UNUSED(req_format);

	static const struct {
		uint64_t fmt_ext;
		uint64_t value;
	} fmt_ext_values[] {
		{ MALI_GRALLOC_INTFMT_AFBC_BASIC, 1 },
		{ MALI_GRALLOC_INTFMT_AFBC_SPLITBLK, 1 },
		{ MALI_GRALLOC_INTFMT_AFBC_WIDEBLK, 1 },
		{ MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS, 1 },
		{ MALI_GRALLOC_INTFMT_AFBC_EXTRAWIDEBLK, 1 },
		{ MALI_GRALLOC_INTFMT_AFBC_DOUBLE_BODY, 1 },
		{ MALI_GRALLOC_INTFMT_AFBC_BCH, 1 },
		{ MALI_GRALLOC_INTFMT_AFBC_YUV_TRANSFORM, 1 },
		{ MALI_GRALLOC_INTFMT_AFBC_SPARSE, 1 },
	};
	for (auto& ext : fmt_ext_values)
	{
		if (fmt.format_ext & ext.fmt_ext)
		{
			grade += ext.value;
		}
	}

	return grade;
}

/*
 * Obtains the 'best' allocation format for requested format and usage:
 * 1. Find compatible base formats (based on format properties alone)
 * 2. Find base formats supported by producers/consumers
 * 3. Find best modifiers from supported base formats
 * 4. Select allocation format from "best" base format with "best" modifiers
 *
 * NOTE: Base format re-mapping should not take place when CPU usage is
 * requested.
 *
 * @param req_base_format       [in]    Base format requested by client.
 * @param usage                 [in]    Buffer usage.
 * @param producers             [in]    Producers (flags).
 * @param consumers             [in]    Consumers (flags).
 * @param producer_active_caps  [in]    Producer capabilities (flags).
 * @param consumer_active_caps  [in]    Consumer capabilities (flags).
 *
 * @return alloc_format, supported for usage;
 *         MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED, otherwise
 */
static uint64_t get_best_format(const uint32_t req_base_format,
                                const uint64_t usage,
                                const uint16_t producers,
                                const uint16_t consumers,
                                const uint64_t producer_active_caps,
                                const uint64_t consumer_active_caps)
{
	uint64_t alloc_format = MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED;

	assert(req_base_format != MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED);
	const int32_t req_fmt_idx = get_format_index(req_base_format);
	MALI_GRALLOC_LOGV("req_base_format: 0x%" PRIx32, req_base_format);
	MALI_GRALLOC_LOGV("req_fmt_idx: %d", req_fmt_idx);
	assert(req_fmt_idx >= 0);

	/* 1. Find compatible base formats. */
	std::vector<fmt_props> f_compat;
	for (uint16_t i = 0; i < num_formats; i++)
	{
		if (is_format_compatible(&formats[req_fmt_idx], &formats[i]))
		{
			fmt_props fmt = {0, 0, 0};
			fmt.base_format = formats[i].id;
			MALI_GRALLOC_LOGV("Compatible: Base-format: 0x%" PRIx32, fmt.base_format);
			f_compat.push_back(fmt);
		}
	}
	assert(f_compat.size() > 0);

	/* 2. Find base formats supported by IP and among them, find the highest
	 * number of modifier enabled format and check if requested format is present
	 */

	int32_t num_supported_formats = 0;
	uint64_t req_format_grade = 0;
	uint64_t best_fmt_grade = 0;
	uint64_t first_of_best_formats = MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED;
	uint64_t req_format = MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED;

	for (uint16_t i = 0; i < f_compat.size(); i++)
	{
		MALI_GRALLOC_LOGV("Compatible: Base-format: 0x%" PRIx32, f_compat[i].base_format);
		fmt_props fmt = {0, 0, 0};
		bool supported = get_supported_format(f_compat[i].base_format,
		                                      usage,
		                                      producers,
		                                      consumers,
		                                      producer_active_caps,
		                                      consumer_active_caps,
		                                      &fmt);
		if (supported)
		{
			const uint64_t sup_fmt_grade = grade_format(fmt, req_base_format);
			if (sup_fmt_grade)
			{
				num_supported_formats++;
				MALI_GRALLOC_LOGV("Supported: Base-format: 0x%" PRIx32 ", Modifiers: 0x%" PRIx64 ", Flags: 0x%" PRIx16,
					fmt.base_format, fmt.format_ext, fmt.f_flags);

				/* 3. Find best modifiers from supported base formats */
				if (sup_fmt_grade > best_fmt_grade)
				{
					best_fmt_grade = sup_fmt_grade;
					first_of_best_formats = fmt.base_format | fmt.format_ext;
				}

				/* Check if current supported format is same as requested format */
				if (fmt.base_format == req_base_format)
				{
					req_format_grade = sup_fmt_grade;
					req_format = fmt.base_format | fmt.format_ext;
				}
			}
		}
	}

	/* 4. Select allocation format from "best" base format with "best" modifiers */
	if (num_supported_formats > 0)
	{
		/* Select first/one of best format when requested format is either not
		* supported or requested format is not the best format.
		*/
		if ((req_format_grade != best_fmt_grade) &&
			(((producers & MALI_GRALLOC_PRODUCER_CPU) == 0) &&
			((consumers & MALI_GRALLOC_CONSUMER_CPU) == 0)))
		{
			alloc_format = first_of_best_formats;
		}
		else if (req_format_grade != 0)
		{
			alloc_format = req_format;
		}
	}

	MALI_GRALLOC_LOGV("Selected format: 0x%" PRIx64, alloc_format);
	return alloc_format;
}

/* Returns true if the format modifier specifies no compression scheme. */
static bool is_uncompressed(uint64_t format_ext)
{
	return format_ext == 0;
}


/* Returns true if the format modifier specifies AFBC. */
static bool is_afbc(uint64_t format_ext)
{
	return format_ext & MALI_GRALLOC_INTFMT_AFBC_BASIC;
}

/* Returns true if the format modifier specifies multiplane AFBC. */
static bool is_multiplane_afbc(uint64_t format_ext)
{
	return is_afbc(format_ext) &&
	       (format_ext & MALI_GRALLOC_INTFMT_AFBC_EXTRAWIDEBLK) &&
	       (format_ext & MALI_GRALLOC_INTFMT_AFBC_TILED_HEADERS);
}

/* Returns true if the format modifier specifies single plane AFBC. */
static bool is_single_plane_afbc(uint64_t format_ext)
{
	return is_afbc(format_ext) && !is_multiplane_afbc(format_ext);
}

/*
 * Determines the base format suitable for requested allocation format (base +
 * modifiers). Going forward, the base format requested MUST be compatible with
 * the format modifiers. In legacy mode, more leeway is given such that fallback
 * to a supported base format for multi-plane AFBC formats is handled here
 * within the gralloc implementation.
 *
 * @param fmt_idx        [in]    Index into format properties table (base format).
 * @param format_ext     [in]    Format modifiers (extension bits).
 *
 * @return base_format, suitable for modifiers;
 *         MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED, otherwise
 */
static uint32_t get_base_format_for_modifiers(const int32_t fmt_idx,
                                              const uint64_t format_ext)
{
	uint32_t base_format = MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED;
	if (is_uncompressed(format_ext))
	{
		/* Uncompressed formats have no forced fallback. */
		base_format = formats[fmt_idx].id;
	}
	else if (is_afbc(format_ext))
	{
		if (formats[fmt_idx].afbc &&
		    (formats[fmt_idx].npln == 1 || is_multiplane_afbc(format_ext)))
		{
			/* Requested format modifiers are suitable for base format. */
			base_format = formats[fmt_idx].id;
		}
		else if (GRALLOC_USE_LEGACY_CALCS)
		{
			/*
			 * For legacy clients *only*, allow fall-back to 'compatible' base format.
			 * Multi-plane AFBC format requeset would not be intentional and therefore
			 * fallback to single-plane should happen automatically internally.
			 */
			for (uint16_t i = 0; i < num_formats; i++)
			{
				if (is_format_compatible(&formats[fmt_idx], &formats[i]))
				{
					if (formats[i].afbc &&
					    (formats[i].npln == 1 || is_multiplane_afbc(format_ext)))
					{
						base_format = formats[i].id;
						break;
					}
				}
			}
		}
	}

	return base_format;
}


/*
 * Obtain format modifiers from requested format.
 *
 * @param req_format       [in]    Requested format (base + optional modifiers).
 * @param usage            [in]    Buffer usage.
 *
 * @return format modifiers, where extracted from requested format;
 *         0, otherwise
 */
uint64_t get_format_ext(const uint64_t req_format, const uint64_t usage)
{
	uint64_t format_ext = 0;

	if (usage & MALI_GRALLOC_USAGE_PRIVATE_FORMAT)
	{
		format_ext = (req_format & MALI_GRALLOC_INTFMT_EXT_WRAP_MASK) << MALI_GRALLOC_INTFMT_EXT_WRAP_SHIFT;
	}
	else
	{
		format_ext = req_format & MALI_GRALLOC_INTFMT_EXT_MASK;
	}

	return format_ext;
}


/*
 * Obtain base format from requested format. There are two primary ways in which
 * the client can specify requested format:
 * - Public API:
 *   - Normal usage, with HAL_PIXEL_FORMAT_* / MALI_GRALLOC_FORMAT_INTERNAL_*
 *   - Private usage, (as normal usage) with additional format modifiers (MALI_GRALLOC_INTFMT_*)
 * - Private API: allows private usage to be provided explicitly
 *                (type == MALI_GRALLOC_FORMAT_TYPE_INTERNAL)
 *
 * @param req_format       [in]    Requested format (base + optional modifiers).
 * @param usage            [in]    Buffer usage.
 * @param type             [in]    Format type (public usage or internal).
 * @param map_to_internal  [in]    Base format (if public HAL_PIXEL_FORMAT_*)
 *                                 should be mapped to internal representation.
 *
 * @return base format, where identified/translated from requested format;
 *         MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED, otherwise
 */
uint32_t get_base_format(const uint64_t req_format,
                         const uint64_t usage,
                         const mali_gralloc_format_type type,
                         const bool map_to_internal)
{
	GRALLOC_UNUSED(type);

	uint32_t base_format = MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED;

	if (usage & MALI_GRALLOC_USAGE_PRIVATE_FORMAT)
	{
		base_format = req_format & MALI_GRALLOC_INTFMT_FMT_WRAP_MASK;

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
	}
	else
	{
		/*
		 * Internal format (NV12) overlaps with HAL format (JPEG). To disambiguate,
		 * reject HAL_PIXEL_FORMAT_JPEG when provided through the public interface.
		 * All formats requested through private interface (type ==
		 * MALI_GRALLOC_FORMAT_TYPE_INTERNAL) should be accepted, including
		 * MALI_GRALLOC_FORMAT_INTERNAL_NV12 (same value as HAL_PIXEL_FORMAT_JPEG).
		 */
		{
			/* Mask out extension bits which could be present with type 'internal'. */
			base_format = req_format & MALI_GRALLOC_INTFMT_FMT_MASK;
		}
	}

	/* Obtain a valid base format, optionally mapped to internal. Flex formats
	 * are always mapped to internal base format.
	 * NOTE: Overlap between HAL_PIXEL_FORMAT_* and MALI_GRALLOC_FORMAT_INTERNAL_*
	 * is intentional. See enumerations for more information.
	 */
	return get_internal_format(base_format, map_to_internal);
}

typedef enum rk_board_platform_t
{
	RK3326,
	RK356X,
	RK3399,
	RK3288,
	RK_BOARD_PLATFORM_UNKNOWN,
} rk_board_platform_t;

static rk_board_platform_t s_platform = RK_BOARD_PLATFORM_UNKNOWN;

static rk_board_platform_t get_rk_board_platform()
{
	/* 若 's_platform' 尚未初始化, 则... */
	if ( RK_BOARD_PLATFORM_UNKNOWN == s_platform )
	{
		char value[PROPERTY_VALUE_MAX];

		property_get("ro.board.platform", value, "0");

		if (0 == strcmp("rk3326", value) )
		{
			s_platform = RK3326;
		}
		else if (0 == strcmp("rk356x", value) )
		{
			s_platform = RK356X;
		}
		else if (0 == strcmp("rk3399", value) )
		{
			s_platform = RK3399;
		}
		else if (0 == strcmp("rk3288", value) )
		{
			s_platform = RK3288;
		}
		else
		{
			W("unexpected 'value' : %s", value);
			return RK_BOARD_PLATFORM_UNKNOWN;
		}
	}

	return s_platform;
}

static bool is_rk_ext_hal_format(const uint64_t hal_format)
{
	if ( HAL_PIXEL_FORMAT_YCrCb_NV12 == hal_format
		|| HAL_PIXEL_FORMAT_YCrCb_NV12_10 == hal_format )
	{
		return true;
	}
	else
	{
		return false;
	}
}

static bool is_no_afbc_for_sf_client_layer_required_via_prop()
{
	char value[PROPERTY_VALUE_MAX];

	property_get("vendor.gralloc.no_afbc_for_sf_client_layer", value, "0");

	return (0 == strcmp("1", value) );
}

static bool is_no_afbc_for_fb_target_layer_required_via_prop()
{
	char value[PROPERTY_VALUE_MAX];

	property_get("vendor.gralloc.no_afbc_for_fb_target_layer", value, "0");

	return (0 == strcmp("1", value) );
}

static uint64_t rk_gralloc_select_format(const int width,
					 const int height,
					 const uint64_t req_format,
					 const uint64_t usage)
{
	GRALLOC_UNUSED(width);
	uint64_t internal_format = req_format;

	/*-------------------------------------------------------*/
	/* rk 定义的 从 'req_format' 到 'internal_format' 的映射. */

	if ( HAL_PIXEL_FORMAT_YCrCb_NV12 == req_format )
	{
		I("to use 'MALI_GRALLOC_FORMAT_INTERNAL_NV12' as internal_format for req_format of 'HAL_PIXEL_FORMAT_YCrCb_NV12'");
		internal_format = MALI_GRALLOC_FORMAT_INTERNAL_NV12;
	}
	else if ( HAL_PIXEL_FORMAT_YCbCr_422_SP == req_format )
	{
		I("to use MALI_GRALLOC_FORMAT_INTERNAL_NV16 as internal_format for HAL_PIXEL_FORMAT_YCbCr_422_SP.");
		internal_format = MALI_GRALLOC_FORMAT_INTERNAL_NV16;
	}
	else if ( HAL_PIXEL_FORMAT_YCrCb_NV12_10 == req_format )
	{
		I("to use 'MALI_GRALLOC_FORMAT_INTERNAL_P010' as internal_format for req_format of 'HAL_PIXEL_FORMAT_YCrCb_NV12_10'");
		internal_format = MALI_GRALLOC_FORMAT_INTERNAL_P010; // 但实际上 这两种格式在 buffer layout 上 并不相同.
	}
        else if ( req_format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED )
	{
		if ( GRALLOC_USAGE_HW_VIDEO_ENCODER == (usage & GRALLOC_USAGE_HW_VIDEO_ENCODER)
			|| GRALLOC_USAGE_HW_CAMERA_WRITE == (usage & GRALLOC_USAGE_HW_CAMERA_WRITE) )
		{
			I("to select NV12 for HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED for usage : 0x%" PRIx64 ".", usage);
			internal_format = MALI_GRALLOC_FORMAT_INTERNAL_NV12;
		}
		else
		{
			I("to select RGBX_8888 for HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED for usage : 0x%" PRIx64 ".", usage);
			internal_format = HAL_PIXEL_FORMAT_RGBX_8888;
		}
	}
	else if ( req_format == HAL_PIXEL_FORMAT_YCbCr_420_888 )
	{
		I("to use NV12 for  %" PRIu64, req_format);
		internal_format = MALI_GRALLOC_FORMAT_INTERNAL_NV12;
	}
	else if ( HAL_PIXEL_FORMAT_YUV420_8BIT_I == req_format )
	{
		I("to use MALI_GRALLOC_FORMAT_INTERNAL_YUV420_8BIT_I as internal_format for HAL_PIXEL_FORMAT_YUV420_8BIT_I.");
		internal_format = MALI_GRALLOC_FORMAT_INTERNAL_YUV420_8BIT_I;
	}
	else if ( HAL_PIXEL_FORMAT_YUV420_10BIT_I == req_format )
	{
		I("to use MALI_GRALLOC_FORMAT_INTERNAL_YUV420_10BIT_I as internal_format for HAL_PIXEL_FORMAT_YUV420_10BIT_I.");
		internal_format = MALI_GRALLOC_FORMAT_INTERNAL_YUV420_10BIT_I;
	}
	else if ( HAL_PIXEL_FORMAT_YCbCr_422_I == req_format )
	{
		I("to use MALI_GRALLOC_FORMAT_INTERNAL_YUV422_8BIT as internal_format for HAL_PIXEL_FORMAT_YCbCr_422_I.");
		internal_format = MALI_GRALLOC_FORMAT_INTERNAL_YUV422_8BIT;
	}
	else if ( HAL_PIXEL_FORMAT_Y210 == req_format )
	{
		I("to use MALI_GRALLOC_FORMAT_INTERNAL_Y210 as internal_format for HAL_PIXEL_FORMAT_Y210.");
		internal_format = MALI_GRALLOC_FORMAT_INTERNAL_Y210;
	}
	else if ( req_format == HAL_PIXEL_FORMAT_YCRCB_420_SP)
	{
		I("to use NV21 for  %" PRIu64, req_format)
		internal_format = MALI_GRALLOC_FORMAT_INTERNAL_NV21;
	}

	/*-------------------------------------------------------*/

	/* 若 'req_format' "不是" rk_ext_hal_format 且 rk "未" 定义 map 方式, 则... */
	if ( !(is_rk_ext_hal_format(req_format) )
		&& internal_format == req_format )
	{
		/* 用 ARM 定义的规则, 从 'req_format' 得到 'internal_format'. */
		internal_format = get_internal_format(req_format,
						      true);	// 'map_to_internal'
		if ( MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED == internal_format )
		{
			internal_format = req_format;
		}
	}

	/*-------------------------------------------------------*/
	/* 处理可能的 AFBC 配置. */

	/* 若当前 buffer "是" 用于 fb_target_layer, 则... */
	if ( GRALLOC_USAGE_HW_FB == (usage & GRALLOC_USAGE_HW_FB) )
	{
		if ( !is_no_afbc_for_fb_target_layer_required_via_prop() )
		{
			rk_board_platform_t platform = get_rk_board_platform();
			switch ( platform )
			{
			case RK3326:
				I("to allocate AFBC buffer for fb_target_layer on rk3326.");
				internal_format = 
					MALI_GRALLOC_FORMAT_INTERNAL_RGBA_8888
					| MALI_GRALLOC_INTFMT_AFBC_BASIC
					| MALI_GRALLOC_INTFMT_AFBC_YUV_TRANSFORM;

				break;

			case RK356X:
				I("to allocate AFBC buffer for fb_target_layer on rk356x.");
				internal_format = 
					MALI_GRALLOC_FORMAT_INTERNAL_RGBA_8888
					| MALI_GRALLOC_INTFMT_AFBC_BASIC;
				break;

			case RK3399:
				/* 若 height < 2160, 且 buffer 将 "不是" 用于 external_display, */
				if ( (height < 2160)
					&& (RK_GRALLOC_USAGE_EXTERNAL_DISP != (usage & RK_GRALLOC_USAGE_EXTERNAL_DISP) ) )
				{
					/* 使用 AFBC format */
					I("to allocate AFBC buffer for fb_target_layer on 3399.");
					internal_format = 
						MALI_GRALLOC_FORMAT_INTERNAL_RGBA_8888
						| MALI_GRALLOC_INTFMT_AFBC_BASIC;
					break;
				}

				/* 使用 非 AFBC format */
				internal_format = req_format;
				break;

			case RK3288:
				I("to allocate non AFBC buffer for fb_target_layer on rk3288.");
				/* 使用 非 AFBC format */
				internal_format = req_format;
				break;

			default:
				W("unexpected 'platform' : %d", platform);
				break;
			}

			property_set("vendor.gmali.fbdc_target", "1"); // 继续遵循 rk_drm_gralloc 和 rk_drm_hwc 的约定.
		}
		else	// if ( !should_disable_afbc_in_fb_target_layer() )
		{
			I("AFBC IS disabled for fb_target_layer.");
			internal_format = req_format;
		}

		return internal_format;
	}
	/* 否则, 即 当前 buffer 用于 sf_client_layer, 则... */
	else
	{
		/* 若 "没有" '通过属性要求 对 sf_client_layer "不" 使用 AFBC 格式', 即可以使用 AFBC 格式, 则... */
		if ( !is_no_afbc_for_sf_client_layer_required_via_prop() )
		{
			/* 若 client "没有" 在 'usage' 显式要求 "不" 使用 AFBC, 则 ... */
			if ( 0 == (usage & MALI_GRALLOC_USAGE_NO_AFBC) )
			{
				/* 若当前 platform 是 356x, 则... */
				if ( RK356X == get_rk_board_platform() )
				{
					/* 尽可能对 buffers of sf_client_layer 使用 AFBC 格式. */

					/* 若 CPU "不会" 读写 buffer,
					 * 且 VPU "不会" 读 buffer (to encode),
					 * 且 camera "不会" 读写 buffer,
					 * 则... */
					if ( 0 == (usage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK) )
							&& 0 == (usage & GRALLOC_USAGE_HW_VIDEO_ENCODER)
							&& 0 == (usage & GRALLOC_USAGE_HW_CAMERA_WRITE)
							&& 0 == (usage & GRALLOC_USAGE_HW_CAMERA_READ) )
					{
						/* 若 internal_format 不是 nv12,
						   且 不是 MALI_GRALLOC_FORMAT_INTERNAL_P010,
						   且 不是 MALI_GRALLOC_FORMAT_INTERNAL_NV16,
						   则... */
						if ( internal_format != MALI_GRALLOC_FORMAT_INTERNAL_NV12
							&& internal_format != MALI_GRALLOC_FORMAT_INTERNAL_P010
							&& internal_format != MALI_GRALLOC_FORMAT_INTERNAL_RGBA_16161616
							&& internal_format != MALI_GRALLOC_FORMAT_INTERNAL_NV16 )
						{
							/* 强制将 'internal_format' 设置为对应的 AFBC 格式. */
							internal_format = internal_format | MALI_GRALLOC_INTFMT_AFBC_BASIC;
							I("use_afbc_layer: force to set 'internal_format' to 0x%" PRIx64 " for usage '0x%" PRIx64,
									internal_format, usage);
						}
					}
				}
			}
		}
		else	// if ( !is_no_afbc_for_sf_client_layer_required_via_prop() )
		{
			I("no_afbc_for_sf_client_layer is requested via prop");
		}
	}

	/*-------------------------------------------------------*/

	return internal_format;
}

/*
 * Select pixel format (base + modifier) for allocation.
 *
 * @param req_format       [in]   Format (base + optional modifiers) requested by client.
 * @param type             [in]   Format type (public usage or internal).
 * @param usage            [in]   Buffer usage.
 * @param buffer_size      [in]   Buffer resolution (w x h, in pixels).
 * @param internal_format  [out]  Legacy format (base format as requested).
 *
 * @return alloc_format, format to be used in allocation;
 *         MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED, where no suitable
 *         format could be found.
 */
uint64_t mali_gralloc_select_format(const int width,
				    const int height,
				    const uint64_t req_format,
                                    const mali_gralloc_format_type type,
                                    const uint64_t usage,
                                    const int buffer_size,
                                    uint64_t * const internal_format)
{
/* < 若 USE_RK_SELECTING_FORMAT_MANNER 为 1, 则将使用 rk 的方式来选择 alloc_format 和 internal_format.> */
#if USE_RK_SELECTING_FORMAT_MANNER
// #error

	GRALLOC_UNUSED(type);
	GRALLOC_UNUSED(buffer_size);
	uint64_t alloc_format;

	*internal_format = rk_gralloc_select_format(width, height, req_format, usage);

	alloc_format = *internal_format;

	return alloc_format;
#else
	uint64_t alloc_format = MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED;

	/*
	 * Obtain base_format (no extension bits) and indexes into format tables.
	 */
	const uint32_t req_base_format = get_base_format(req_format, usage, type, true);
	const int32_t req_fmt_idx = get_format_index(req_base_format);
	if (req_base_format == MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED ||
	    req_fmt_idx == -1)
	{
		MALI_GRALLOC_LOGE("Invalid base format! req_base_format = 0x%" PRIx32
		      ", req_format = 0x%" PRIx64 ", type = 0x%" PRIx32,
		      req_base_format, req_format, type);
		goto out;
	}

	/* Reject if usage specified is outside white list of valid usages. */
	if (type != MALI_GRALLOC_FORMAT_TYPE_INTERNAL && (usage & (~VALID_USAGE)) != 0)
	{
		MALI_GRALLOC_LOGE("Invalid usage specified: 0x%" PRIx64, usage);
		goto out;
	}

	/*
	 * Construct format as requested (using AFBC modifiers) ensuring that
	 * base format is compatible with modifiers. Otherwise, reject allocation with UNDEFINED.
	 * NOTE: IP support is not considered and modifiers are not adjusted.
	 */
	if (usage & MALI_GRALLOC_USAGE_PRIVATE_FORMAT || type == MALI_GRALLOC_FORMAT_TYPE_INTERNAL)
	{
		const uint64_t format_ext = get_format_ext(req_format, usage);
		const uint32_t base_format = get_base_format_for_modifiers(req_fmt_idx, format_ext);
		if (base_format != MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED)
		{
			alloc_format = format_ext | base_format;
		}
	}
	else
	{
		/* Determine producers and consumers. */
		const uint16_t producers = get_producers(usage);
		const uint16_t consumers = get_consumers(usage);

		MALI_GRALLOC_LOGV("Producers: 0x%" PRIx16 ", Consumers: 0x%" PRIx16,
		      producers, consumers);

		/* Obtain producer and consumer capabilities. */
		const uint64_t producer_caps = get_producer_caps(producers);

		uint64_t consumer_caps = 0;
#ifdef GRALLOC_HWC_FB_DISABLE_AFBC
		if (GRALLOC_HWC_FB_DISABLE_AFBC && DISABLE_FRAMEBUFFER_HAL && (usage & GRALLOC_USAGE_HW_FB))
		{
			/* Override capabilities to disable AFBC for DRM HWC framebuffer surfaces. */
			consumer_caps = MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT;
		}
		else
#endif
		{
			consumer_caps = get_consumer_caps(consumers);
		}

		MALI_GRALLOC_LOGV("Producer caps: 0x%" PRIx64 ", Consumer caps: 0x%" PRIx64,
		      producer_caps, consumer_caps);

		if (producers == 0 && consumers == 0)
		{
			MALI_GRALLOC_LOGE("Producer and consumer not identified.");
			goto out;
		}
		else if (producers == 0 || consumers == 0)
		{
			MALI_GRALLOC_LOGV("Producer or consumer not identified.");
		}

		if ((usage & MALI_GRALLOC_USAGE_NO_AFBC) == MALI_GRALLOC_USAGE_NO_AFBC &&
		    formats[req_fmt_idx].is_yuv)
		{
			MALI_GRALLOC_LOGE("ERROR: Invalid usage 'MALI_GRALLOC_USAGE_NO_AFBC' when allocating YUV formats");
			goto out;
		}

		uint64_t producer_active_caps = producer_caps;
		uint64_t consumer_active_caps = consumer_caps;
		uint64_t consumer_caps_mask;

		get_active_caps(formats[req_fmt_idx],
		                producers, consumers,
		                &producer_active_caps, &consumer_active_caps,
		                buffer_size);

		MALI_GRALLOC_LOGV("Producer caps (active): 0x%" PRIx64 ", Consumer caps (active): 0x%" PRIx64,
		      producer_active_caps, consumer_active_caps);

		alloc_format = get_best_format(formats[req_fmt_idx].id,
		                               usage,
		                               producers,
		                               consumers,
		                               producer_active_caps,
		                               consumer_active_caps);

		/* Some display controllers expect the framebuffer to be in BGRX format, hence we force the format to avoid colour swap issues. */
#if defined(GRALLOC_HWC_FORCE_BGRA_8888) && defined(DISABLE_FRAMEBUFFER_HAL)
		if (GRALLOC_HWC_FORCE_BGRA_8888 && DISABLE_FRAMEBUFFER_HAL && (usage & GRALLOC_USAGE_HW_FB))
		{
			if (alloc_format != HAL_PIXEL_FORMAT_BGRA_8888 &&
			    usage & (GRALLOC_USAGE_SW_WRITE_MASK | GRALLOC_USAGE_SW_READ_MASK))
			{
				MALI_GRALLOC_LOGE("Format unsuitable for both framebuffer usage and CPU access. Failing allocation.");
				alloc_format = MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED;
				goto out;
			}
			alloc_format = HAL_PIXEL_FORMAT_BGRA_8888;
		}
#endif
	}

out:
	/*
	 * Reconstruct internal format (legacy).
	 * In order to retain backwards-compatiblity, private_handle_t member,
	 * 'internal_format' will *not* be updated with single-plane format. Clients with
	 * support for multi-plane AFBC should use a combination of 'internal_format' and
	 * 'is_multi_plane()'' to determine whether the allocated format is multi-plane.
	 */
	if (alloc_format == MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED)
	{
		*internal_format = MALI_GRALLOC_FORMAT_INTERNAL_UNDEFINED;
	}
	else
	{
		*internal_format = get_base_format(req_format, usage, type, false);
		*internal_format |= (alloc_format & MALI_GRALLOC_INTFMT_EXT_MASK);
	}

	MALI_GRALLOC_LOGV("mali_gralloc_select_format: req_format=0x%08" PRIx64 ", usage=0x%" PRIx64
	      ", req_base_format=0x%" PRIx32 ", alloc_format=0x%" PRIx64 ", internal_format=0x%" PRIx64,
	      req_format, usage, req_base_format, alloc_format, *internal_format);

	return alloc_format;
#endif
}

