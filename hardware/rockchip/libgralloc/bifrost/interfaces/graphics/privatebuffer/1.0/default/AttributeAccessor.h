/*
 * Copyright (C) 2019-2020 Arm Limited.
 * SPDX-License-Identifier: Apache-2.0
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

#ifndef ARM_GRAPHICS_PRIVATEBUFFER_V1_0_ATTRIBUTE_ACCESSOR_H
#define ARM_GRAPHICS_PRIVATEBUFFER_V1_0_ATTRIBUTE_ACCESSOR_H

#include <arm/graphics/privatebuffer/1.0/IAttributeAccessor.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

#include "mali_gralloc_buffer.h"

namespace arm {
namespace graphics {
namespace privatebuffer {
namespace V1_0 {
namespace implementation {

using ::android::hardware::hidl_array;
using ::android::hardware::hidl_bitfield;
using ::android::hardware::hidl_handle;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;
using ::android::hardware::graphics::common::V1_2::Dataspace;

class AttributeAccessor : public IAttributeAccessor {
    void * attr_base;
    size_t attr_size;
public:
    AttributeAccessor(void* attr_base, size_t attr_size);
    ~AttributeAccessor() override;
    // Methods from ::arm::graphics::privatebuffer::V1_0::IAttributeAccessor follow.
    Return<void> getCropRectangle(getCropRectangle_cb _hidl_cb) override;
    Return<Error> setCropRectangle(const CropRectangle &region) override;
    Return<void> getDataspace(getDataspace_cb _hidl_cb) override;
    Return<Error> setDataspace(Dataspace dataspace) override;
    // Methods from ::android::hidl::base::V1_0::IBase follow.
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace privatebuffer
}  // namespace graphics
}  // namespace arm

#endif  // ARM_GRAPHICS_PRIVATEBUFFER_V1_0_ATTRIBUTE_ACCESSOR_H
