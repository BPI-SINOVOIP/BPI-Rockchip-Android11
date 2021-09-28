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
#ifndef MALI_GRALLOC_BUFFERALLOCATION_H_
#define MALI_GRALLOC_BUFFERALLOCATION_H_

#include <hardware/hardware.h>
#include "mali_gralloc_private_interface_types.h"
#include "mali_gralloc_buffer.h"
#include "core/mali_gralloc_bufferdescriptor.h"

/* Compression scheme */
enum class AllocBaseType
{
	/*
	 * No compression scheme
         */
	UNCOMPRESSED,

	/*
	 * Arm Framebuffer Compression
         */
	AFBC,              /* 16 x 16 block size */
	AFBC_WIDEBLK,      /* 32 x 8 block size */
	AFBC_EXTRAWIDEBLK, /* 64 x 4 block size */

};

/*
 * Allocation type.
 *
 * Allocation-specific properties of format modifiers
 * described by MALI_GRALLOC_INTFMT_*.
 */
struct AllocType
{
	/*
	 * The compression scheme in use
	 *
	 * For AFBC formats, this describes:
	 * - the block size for single plane base formats, or
	 * - the block size of the first/luma plane for multi-plane base formats.
	 */
	AllocBaseType primary_type{AllocBaseType::UNCOMPRESSED};

	/*
	 * Multi-plane AFBC format. AFBC chroma-only plane(s) are
	 * always compressed with superblock type 'AFBC_EXTRAWIDEBLK'.
	 */
	bool is_multi_plane{};

	/*
	 * Allocate tiled AFBC headers.
	 */
	bool is_tiled{};

	/*
	 * Pad AFBC header stride to 64-byte alignment
	 * (multiple of 4x16B headers).
	 */
	bool is_padded{};

	/*
	 * Front-buffer rendering safe AFBC allocations include an
	 * additional 4kB-aligned body buffer.
	 */
	bool is_frontbuffer_safe{};

	bool is_afbc() const
	{
		switch (primary_type)
		{
		case AllocBaseType::AFBC:
		case AllocBaseType::AFBC_WIDEBLK:
		case AllocBaseType::AFBC_EXTRAWIDEBLK:
			return true;
		default:
			return false;
		}
	}

};

using alloc_type_t = AllocType;

int mali_gralloc_derive_format_and_size(buffer_descriptor_t * const bufDescriptor);

int mali_gralloc_buffer_allocate(const gralloc_buffer_descriptor_t *descriptors,
                                 uint32_t numDescriptors, buffer_handle_t *pHandle, bool *shared_backend);

int mali_gralloc_buffer_free(buffer_handle_t pHandle);

void init_afbc(uint8_t *buf, uint64_t internal_format, const bool is_multi_plane, int w, int h);

uint32_t lcm(uint32_t a, uint32_t b);

bool get_alloc_type(const uint64_t format_ext,
                    const uint32_t format_idx,
                    const uint64_t usage,
                    alloc_type_t * const alloc_type);

#endif /* MALI_GRALLOC_BUFFERALLOCATION_H_ */
