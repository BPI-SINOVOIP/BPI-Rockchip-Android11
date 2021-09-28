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

#ifndef GRALLOC_COMMON_MAPPER_METADATA_H
#define GRALLOC_COMMON_MAPPER_METADATA_H

#include <inttypes.h>
#include "mali_gralloc_log.h"
#include "core/mali_gralloc_bufferdescriptor.h"
#include "mali_gralloc_buffer.h"

#if GRALLOC_VERSION_MAJOR < 4
#error "Metadata supported only on Gralloc 4.X and later"
#elif GRALLOC_VERSION_MAJOR == 4
#include "4.x/gralloc_mapper_hidl_header.h"
#endif

#include <aidl/arm/graphics/Compression.h>
#include <aidl/arm/graphics/ArmMetadataType.h>


namespace arm
{
namespace mapper
{
namespace common
{
using android::hardware::hidl_vec;
using aidl::android::hardware::graphics::common::ExtendableType;

#define GRALLOC_ARM_COMPRESSION_TYPE_NAME "arm.graphics.Compression"
const static ExtendableType Compression_AFBC{ GRALLOC_ARM_COMPRESSION_TYPE_NAME,
                                                  static_cast<int64_t>(aidl::arm::graphics::Compression::AFBC) };

#define GRALLOC_ARM_METADATA_TYPE_NAME "arm.graphics.ArmMetadataType"
const static IMapper::MetadataType ArmMetadataType_PLANE_FDS{ GRALLOC_ARM_METADATA_TYPE_NAME,
                                                  static_cast<int64_t>(aidl::arm::graphics::ArmMetadataType::PLANE_FDS) };

/**
 * Retrieves a Buffer's metadata value.
 *
 * @param handle       [in] The private handle of the buffer to query for metadata.
 * @param metadataType [in] The type of metadata queried.
 * @param hidl_cb      [in] HIDL callback function generating -
 *                          error: NONE on success.
 *                                 UNSUPPORTED on error when reading or unsupported metadata type.
 *                          metadata: Vector of bytes representing the metadata value.
 */
void get_metadata(const private_handle_t *handle, const IMapper::MetadataType &metadataType, IMapper::get_cb hidl_cb);

/**
 * Sets a Buffer's metadata value.
 *
 * @param handle       [in] The private handle of the buffer for which to modify metadata.
 * @param metadataType [in] The type of metadata to modify.
 * @param metadata     [in] Vector of bytes representing the new value for the metadata associated with the buffer.
 *
 * @return Error::NONE on success.
 *         Error::UNSUPPORTED on error when writing or unsupported metadata type.
 */
Error set_metadata(const private_handle_t *handle, const IMapper::MetadataType &metadataType,
                   const hidl_vec<uint8_t> &metadata);

/**
 * Query basic metadata information about a buffer form its descriptor before allocation.
 *
 * @param description  [in] The buffer descriptor.
 * @param metadataType [in] The type of metadata to query
 * @param hidl_cb      [in] HIDL callback function generating -
 *                          error: NONE on success.
 *                                 UNSUPPORTED on unsupported metadata type.
 *                          metadata: Vector of bytes representing the metadata value.
 */
void getFromBufferDescriptorInfo(IMapper::BufferDescriptorInfo const &description,
                                 IMapper::MetadataType const &metadataType,
                                 IMapper::getFromBufferDescriptorInfo_cb hidl_cb);

} // namespace common
} // namespace mapper
} // namespace arm

#endif
