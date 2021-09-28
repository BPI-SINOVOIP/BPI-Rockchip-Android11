/*
 * Copyright (C) 2018-2020 ARM Limited. All rights reserved.
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
#ifndef ANDROID_HARDWARE_GRAPHICS_MAPPER_V2_x_GRALLOC_MAPPER_H
#define ANDROID_HARDWARE_GRAPHICS_MAPPER_V2_x_GRALLOC_MAPPER_H

#if HIDL_MAPPER_VERSION_SCALED == 200
#include <android/hardware/graphics/mapper/2.0/IMapper.h>
#elif HIDL_MAPPER_VERSION_SCALED == 210
#include <android/hardware/graphics/mapper/2.1/IMapper.h>
#endif

#include "hidl_common/Mapper.h"

namespace imapper2 = android::hardware::graphics::mapper::V2_0;
#if HIDL_MAPPER_VERSION_SCALED >= 210
namespace imapper2_1 = android::hardware::graphics::mapper::V2_1;
#endif

namespace arm {
namespace mapper {

using android::hardware::graphics::mapper::V2_0::Error;
using android::hardware::graphics::mapper::V2_0::BufferDescriptor;
using android::hardware::graphics::mapper::V2_0::YCbCrLayout;

using android::hardware::graphics::mapper::V2_1::IMapper;
using android::hardware::Return;
using android::hardware::hidl_handle;

class GrallocMapper : public IMapper
{
public:

	/**
	 * IMapper constructor. All the state information required for the Gralloc
	 * private module is populated in its default constructor. Gralloc 2.0 specific
	 * state information can be populated here.
	 */
	GrallocMapper();

	/*
	 * IMapper destructor. All the resources aquired for Gralloc private module
	 * (in the IMapper context) are released
	 */
	~GrallocMapper();

	/* Override the public IMapper 2.0 interface */
	Return<void> createDescriptor(const imapper2::IMapper::BufferDescriptorInfo& descriptorInfo,
	                              createDescriptor_cb hidl_cb) override;

	Return<void> importBuffer(const hidl_handle& rawHandle,
	                          importBuffer_cb hidl_cb) override;

	Return<Error> freeBuffer(void* buffer) override;

	Return<void> lock(void* buffer, uint64_t cpuUsage,
	                  const IMapper::Rect& accessRegion,
	                  const hidl_handle& acquireFence,
	                  lock_cb hidl_cb) override;

	Return<void> lockYCbCr(void* buffer, uint64_t cpuUsage,
	                       const IMapper::Rect& accessRegion,
	                       const hidl_handle& acquireFence,
	                       lockYCbCr_cb hidl_cb) override;

	Return<void> unlock(void* buffer, unlock_cb hidl_cb) override;

#if HIDL_MAPPER_VERSION_SCALED >= 210
	/* Override the public IMapper 2.1 specific interface */
	Return<Error> validateBufferSize(void* buffer,
	                                 const imapper2_1::IMapper::BufferDescriptorInfo& descriptorInfo,
	                                 uint32_t stride) override;

	Return<void> getTransportSize(void* buffer, getTransportSize_cb _hidl_cb) override;

	Return<void> createDescriptor_2_1(const imapper2_1::IMapper::BufferDescriptorInfo& descriptorInfo,
	                                  createDescriptor_2_1_cb _hidl_cb) override;
#endif

};

} // namespace mapper
} // namespace arm


#endif // ANDROID_HARDWARE_GRAPHICS_MAPPER_V2_x_GRALLOC_MAPPER_H
