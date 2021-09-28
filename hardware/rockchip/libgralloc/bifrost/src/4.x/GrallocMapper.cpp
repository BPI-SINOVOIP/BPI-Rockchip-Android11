/*
 * Copyright (C) 2020 Arm Limited. All rights reserved.
 *
 * Copyright 2016 The Android Open Source Project
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

#include "GrallocMapper.h"
#include "hidl_common/BufferDescriptor.h"
#include "hidl_common/MapperMetadata.h"

#include "allocator/mali_gralloc_ion.h"

namespace arm
{
namespace mapper
{

using android::hardware::graphics::mapper::V4_0::Error;
using android::hardware::graphics::mapper::V4_0::BufferDescriptor;
using android::hardware::graphics::mapper::V4_0::IMapper;
using android::hardware::Return;
using android::hardware::hidl_handle;
using android::hardware::hidl_vec;
using android::hardware::Void;


GrallocMapper::GrallocMapper()
{
}

GrallocMapper::~GrallocMapper()
{
	mali_gralloc_ion_close();
}

Return<void> GrallocMapper::createDescriptor(const BufferDescriptorInfo &descriptorInfo, createDescriptor_cb hidl_cb)
{
	if (common::validateDescriptorInfo(descriptorInfo))
	{
		hidl_cb(Error::NONE, common::grallocEncodeBufferDescriptor<uint8_t>(descriptorInfo));
	}
	else
	{
		MALI_GRALLOC_LOGE("Invalid attributes to create descriptor for Mapper 3.0");
		hidl_cb(Error::BAD_VALUE, BufferDescriptor());
	}

	return Void();
}

Return<void> GrallocMapper::importBuffer(const hidl_handle &rawHandle, importBuffer_cb hidl_cb)
{
	common::importBuffer(rawHandle, hidl_cb);
	return Void();
}

Return<Error> GrallocMapper::freeBuffer(void *buffer)
{
	return common::freeBuffer(buffer);
}

Return<Error> GrallocMapper::validateBufferSize(void *buffer, const BufferDescriptorInfo &descriptorInfo,
                                                uint32_t in_stride)
{
	/* All Gralloc allocated buffers must be conform to local descriptor validation */
	if (!common::validateDescriptorInfo<BufferDescriptorInfo>(descriptorInfo))
	{
		MALI_GRALLOC_LOGE("Invalid descriptor attributes for validating buffer size");
		return Error::BAD_VALUE;
	}
	return common::validateBufferSize(buffer, descriptorInfo, in_stride);
}

Return<void> GrallocMapper::lock(void *buffer, uint64_t cpuUsage, const IMapper::Rect &accessRegion,
                                 const hidl_handle &acquireFence, lock_cb hidl_cb)
{
	common::lock(buffer, cpuUsage, accessRegion, acquireFence, hidl_cb);
	return Void();
}

Return<void> GrallocMapper::unlock(void *buffer, unlock_cb hidl_cb)
{
	common::unlock(buffer, hidl_cb);
	return Void();
}

Return<void> GrallocMapper::flushLockedBuffer(void *buffer, flushLockedBuffer_cb hidl_cb)
{
	common::flushLockedBuffer(buffer, hidl_cb);
	return Void();
}

Return<Error> GrallocMapper::rereadLockedBuffer(void *buffer)
{
	return common::rereadLockedBuffer(buffer);
}

Return<void> GrallocMapper::get(void *buffer, const MetadataType &metadataType, IMapper::get_cb hidl_cb)
{
	common::get(buffer, metadataType, hidl_cb);
	return Void();
}

Return<Error> GrallocMapper::set(void *buffer, const MetadataType &metadataType, const hidl_vec<uint8_t> &metadata)
{
	return common::set(buffer, metadataType, metadata);
}

Return<void> GrallocMapper::getFromBufferDescriptorInfo(const BufferDescriptorInfo &description,
                                                        const MetadataType &metadataType,
                                                        getFromBufferDescriptorInfo_cb hidl_cb)
{
	common::getFromBufferDescriptorInfo(description, metadataType, hidl_cb);
	return Void();
}

Return<void> GrallocMapper::getTransportSize(void *buffer, getTransportSize_cb hidl_cb)
{
	common::getTransportSize(buffer, hidl_cb);
	return Void();
}

Return<void> GrallocMapper::isSupported(const IMapper::BufferDescriptorInfo &description, isSupported_cb hidl_cb)
{
	if (!common::validateDescriptorInfo<BufferDescriptorInfo>(description))
	{
		MALI_GRALLOC_LOGE("Invalid descriptor attributes for validating buffer size");
		hidl_cb(Error::BAD_VALUE, false);
	}
	common::isSupported(description, hidl_cb);
	return Void();
}

Return<void> GrallocMapper::listSupportedMetadataTypes(listSupportedMetadataTypes_cb hidl_cb)
{
	common::listSupportedMetadataTypes(hidl_cb);
	return Void();
}

Return<void> GrallocMapper::dumpBuffer(void *buffer, dumpBuffer_cb hidl_cb)
{
	common::dumpBuffer(buffer, hidl_cb);
	return Void();
}

Return<void> GrallocMapper::dumpBuffers(dumpBuffers_cb hidl_cb)
{
	common::dumpBuffers(hidl_cb);
	return Void();
}

Return<void> GrallocMapper::getReservedRegion(void *buffer, getReservedRegion_cb hidl_cb)
{
	common::getReservedRegion(buffer, hidl_cb);
	return Void();
}

} // namespace mapper
} // namespace arm

extern "C" IMapper *HIDL_FETCH_IMapper(const char * /* name */)
{
	MALI_GRALLOC_LOGV("Arm Module IMapper %d.%d , pid = %d ppid = %d ", GRALLOC_VERSION_MAJOR,
	                  (HIDL_MAPPER_VERSION_SCALED - (GRALLOC_VERSION_MAJOR * 100)) / 10, getpid(), getppid());

	return new arm::mapper::GrallocMapper();
}
