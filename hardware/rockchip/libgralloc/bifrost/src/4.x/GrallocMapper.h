/*
 * Copyright (C) 2020 Arm Limited. All rights reserved.
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
#ifndef ANDROID_HARDWARE_GRAPHICS_MAPPER_V4_x_GRALLOC_MAPPER_H
#define ANDROID_HARDWARE_GRAPHICS_MAPPER_V4_x_GRALLOC_MAPPER_H

#include "hidl_common/Mapper.h"

namespace arm
{
namespace mapper
{

using android::hardware::Return;
using android::hardware::hidl_handle;
using android::hardware::hidl_vec;

class GrallocMapper : public IMapper
{
public:
	/**
	 * IMapper constructor. All the state information required for the Gralloc
	 * private module is populated in its default constructor. Gralloc 4.x specific
	 * state information can be populated here.
	 */
	GrallocMapper();

	/**
	 * IMapper destructor. All the resources aquired for Gralloc private module
	 * (in the IMapper context) are released
	 */
	~GrallocMapper();

	/* Override the public IMapper 4.0 interface */
	Return<void> createDescriptor(const BufferDescriptorInfo &descriptorInfo, createDescriptor_cb hidl_cb) override;

	Return<void> importBuffer(const hidl_handle &rawHandle, importBuffer_cb hidl_cb) override;

	Return<Error> freeBuffer(void *buffer) override;

	Return<Error> validateBufferSize(void *buffer, const IMapper::BufferDescriptorInfo &descriptorInfo,
	                                 uint32_t stride) override;

	Return<void> getTransportSize(void *buffer, getTransportSize_cb _hidl_cb) override;

	Return<void> lock(void *buffer, uint64_t cpuUsage, const IMapper::Rect &accessRegion,
	                  const hidl_handle &acquireFence, lock_cb hidl_cb) override;

	Return<void> unlock(void *buffer, unlock_cb hidl_cb) override;

	Return<void> flushLockedBuffer(void *buffer, flushLockedBuffer_cb hidl_cb) override;

	Return<Error> rereadLockedBuffer(void *buffer) override;

	Return<void> isSupported(const IMapper::BufferDescriptorInfo &description, isSupported_cb hidl_cb) override;

	Return<void> get(void *buffer, const MetadataType &metadataType, IMapper::get_cb hidl_cb) override;

	Return<Error> set(void *buffer, const MetadataType &metadataType, const hidl_vec<uint8_t> &metadata) override;

	Return<void> getFromBufferDescriptorInfo(BufferDescriptorInfo const &description, MetadataType const &metadataType,
	                                         getFromBufferDescriptorInfo_cb hidl_cb) override;

	Return<void> listSupportedMetadataTypes(listSupportedMetadataTypes_cb _hidl_cb) override;

	Return<void> dumpBuffer(void *buffer, dumpBuffer_cb _hidl_cb) override;

	Return<void> dumpBuffers(dumpBuffers_cb _hidl_cb) override;

	Return<void> getReservedRegion(void *buffer, getReservedRegion_cb _hidl_cb) override;
};

} // namespace mapper
} // namespace arm

#endif // ANDROID_HARDWARE_GRAPHICS_MAPPER_V4_x_GRALLOC_MAPPER_H
