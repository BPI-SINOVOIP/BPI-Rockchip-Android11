/*
 * Copyright (C) 2020 ARM Limited. All rights reserved.
 *
 * Copyright 2016 The Android Open Source Project
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

/* Legacy shared attribute region is deprecated from Android 11.
 * Use the new shared metadata region defined for Gralloc 4.
 */
#define GRALLOC_USE_SHARED_METADATA (GRALLOC_VERSION_MAJOR > 3)

#include "Allocator.h"

#if GRALLOC_USE_SHARED_METADATA
#include "SharedMetadata.h"
#else
#include "gralloc_buffer_priv.h"
#endif

#include "core/mali_gralloc_bufferallocation.h"
#include "core/mali_gralloc_bufferdescriptor.h"
#include "core/format_info.h"
#include "allocator/mali_gralloc_ion.h"
#include "allocator/mali_gralloc_shared_memory.h"
#include "gralloc_priv.h"

namespace arm
{
namespace allocator
{
namespace common
{

void allocate(const buffer_descriptor_t &bufferDescriptor, uint32_t count, IAllocator::allocate_cb hidl_cb,
              std::function<int(const buffer_descriptor_t *, buffer_handle_t *)> fb_allocator)
{
#if DISABLE_FRAMEBUFFER_HAL
	GRALLOC_UNUSED(fb_allocator);
#endif

	Error error = Error::NONE;
	int stride = 0;
	std::vector<hidl_handle> grallocBuffers;
	gralloc_buffer_descriptor_t grallocBufferDescriptor[1];

	grallocBufferDescriptor[0] = (gralloc_buffer_descriptor_t)(&bufferDescriptor);
	grallocBuffers.reserve(count);

	for (uint32_t i = 0; i < count; i++)
	{
		buffer_handle_t tmpBuffer = nullptr;

		int allocResult;
#if (DISABLE_FRAMEBUFFER_HAL != 1)
		if (((bufferDescriptor.producer_usage & GRALLOC_USAGE_HW_FB) ||
		     (bufferDescriptor.consumer_usage & GRALLOC_USAGE_HW_FB)) &&
		    fb_allocator)
		{
			allocResult = fb_allocator(&bufferDescriptor, &tmpBuffer);
		}
		else
#endif
		{
			allocResult = mali_gralloc_buffer_allocate(grallocBufferDescriptor, 1, &tmpBuffer, nullptr);
			if (allocResult != 0)
			{
				MALI_GRALLOC_LOGE("%s, buffer allocation failed with %d", __func__, allocResult);
				error = Error::NO_RESOURCES;
				break;
			}
			auto hnd = const_cast<private_handle_t *>(reinterpret_cast<const private_handle_t *>(tmpBuffer));
			hnd->imapper_version = HIDL_MAPPER_VERSION_SCALED;

#if GRALLOC_USE_SHARED_METADATA
			hnd->reserved_region_size = bufferDescriptor.reserved_size;
			hnd->attr_size = mapper::common::shared_metadata_size() + hnd->reserved_region_size;
#else
			hnd->attr_size = sizeof(attr_region);
#endif
			std::tie(hnd->share_attr_fd, hnd->attr_base) =
			    gralloc_shared_memory_allocate("gralloc_shared_memory", hnd->attr_size);
			if (hnd->share_attr_fd < 0 || hnd->attr_base == MAP_FAILED)
			{
				MALI_GRALLOC_LOGE("%s, shared memory allocation failed with errno %d", __func__, errno);
				mali_gralloc_buffer_free(tmpBuffer);
				error = Error::UNSUPPORTED;
				break;
			}

#if GRALLOC_USE_SHARED_METADATA
			mapper::common::shared_metadata_init(hnd->attr_base, bufferDescriptor.name);
#else
			new(hnd->attr_base) attr_region;
#endif
			const uint32_t base_format = bufferDescriptor.alloc_format & MALI_GRALLOC_INTFMT_FMT_MASK;
			const uint64_t usage = bufferDescriptor.consumer_usage | bufferDescriptor.producer_usage;
			android_dataspace_t dataspace;
			get_format_dataspace(base_format, usage, hnd->width, hnd->height, &dataspace,
			                     &hnd->yuv_info);

#if GRALLOC_USE_SHARED_METADATA
			mapper::common::set_dataspace(hnd, static_cast<mapper::common::Dataspace>(dataspace));
#else
			int temp_dataspace = static_cast<int>(dataspace);
			gralloc_buffer_attr_write(hnd, GRALLOC_ARM_BUFFER_ATTR_DATASPACE, &temp_dataspace);
#endif
			/*
			 * We need to set attr_base to MAP_FAILED before the HIDL callback
			 * to avoid sending an invalid pointer to the client process.
			 *
			 * hnd->attr_base = mmap(...);
			 * hidl_callback(hnd); // client receives hnd->attr_base = <dangling pointer>
			 */
			munmap(hnd->attr_base, hnd->attr_size);
			hnd->attr_base = MAP_FAILED;

			buffer_descriptor_t * const bufDescriptor = (buffer_descriptor_t *)(grallocBufferDescriptor[0]);
			D("got new private_handle_t instance @%p for buffer '%s'. share_fd : %d, share_attr_fd : %d, "
				"flags : 0x%x, width : %d, height : %d, "
				"req_format : 0x%x, producer_usage : 0x%" PRIx64 ", consumer_usage : 0x%" PRIx64 ", "
				"internal_format : 0x%" PRIx64 ", stride : %d, byte_stride : %d, "
				"internalWidth : %d, internalHeight : %d, "
				"alloc_format : 0x%" PRIx64 ", size : %d, layer_count : %u, backing_store_size : %d, "
				"backing_store_id : %" PRIu64 ", "
				"allocating_pid : %d, ref_count : %d, yuv_info : %d",
				hnd, (bufDescriptor->name).c_str() == nullptr ? "unset" : (bufDescriptor->name).c_str(),
			  hnd->share_fd, hnd->share_attr_fd,
			  hnd->flags, hnd->width, hnd->height,
			  hnd->req_format, hnd->producer_usage, hnd->consumer_usage,
			  hnd->internal_format, hnd->stride, hnd->byte_stride,
			  hnd->internalWidth, hnd->internalHeight,
			  hnd->alloc_format, hnd->size, hnd->layer_count, hnd->backing_store_size,
			  hnd->backing_store_id,
			  hnd->allocating_pid, hnd->ref_count, hnd->yuv_info);
			ALOGD("plane_info[0]: offset : %u, byte_stride : %u, alloc_width : %u, alloc_height : %u",
					(hnd->plane_info)[0].offset,
					(hnd->plane_info)[0].byte_stride,
					(hnd->plane_info)[0].alloc_width,
					(hnd->plane_info)[0].alloc_height);
			ALOGD("plane_info[1]: offset : %u, byte_stride : %u, alloc_width : %u, alloc_height : %u",
					(hnd->plane_info)[1].offset,
					(hnd->plane_info)[1].byte_stride,
					(hnd->plane_info)[1].alloc_width,
					(hnd->plane_info)[1].alloc_height);
		}

		int tmpStride = 0;
		if (GRALLOC_USE_LEGACY_CALCS)
		{
			const private_handle_t *hnd = static_cast<const private_handle_t *>(tmpBuffer);
			tmpStride = hnd->stride;
		}
		else
		{
			tmpStride = bufferDescriptor.pixel_stride;
		}

		if (stride == 0)
		{
			stride = tmpStride;
		}
		else if (stride != tmpStride)
		{
			/* Stride must be the same for all allocations */
			mali_gralloc_buffer_free(tmpBuffer);
			stride = 0;
			error = Error::UNSUPPORTED;
			break;
		}

		grallocBuffers.emplace_back(hidl_handle(tmpBuffer));
	}

	/* Populate the array of buffers for application consumption */
	hidl_vec<hidl_handle> hidlBuffers;
	if (error == Error::NONE)
	{
		hidlBuffers.setToExternal(grallocBuffers.data(), grallocBuffers.size());
	}
	hidl_cb(error, stride, hidlBuffers);

	/* The application should import the Gralloc buffers using IMapper for
	 * further usage. Free the allocated buffers in IAllocator context
	 */
	for (const auto &buffer : grallocBuffers)
	{
		mali_gralloc_buffer_free(buffer.getNativeHandle());
		native_handle_delete(const_cast<native_handle_t *>(buffer.getNativeHandle()));
	}
}

} // namespace common
} // namespace allocator
} // namespace arm
