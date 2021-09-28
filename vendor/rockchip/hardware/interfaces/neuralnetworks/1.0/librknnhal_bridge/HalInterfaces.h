// Copyright (c) 2021 by Rockchip Electronics Co., Ltd. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ROCKCHIP_FRAMEWORKS_NN_HAL_INTERFACES_H
#define ROCKCHIP_FRAMEWORKS_NN_HAL_INTERFACES_H

#include <rockchip/hardware/neuralnetworks/1.0/IRKNeuralnetworks.h>
#include <android/hidl/allocator/1.0/IAllocator.h>
#include <android/hidl/memory/1.0/IMemory.h>
#include <hidlmemory/mapping.h>
#include <sys/cdefs.h>
__BEGIN_DECLS
#define LOG_TAG "neuralnetworks@ndk_bridge"

namespace rockchip::nn::hal {
    using android::sp;
    using ::android::hardware::hidl_memory;
    using ::android::hardware::Return;
    using ::android::hardware::Void;
    using ::android::hidl::memory::V1_0::IMemory;
    using ::android::hidl::allocator::V1_0::IAllocator;

    namespace V1_0 = rockchip::hardware::neuralnetworks::V1_0;
}
__END_DECLS
#endif
