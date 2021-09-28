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

#pragma once

#include <rockchip/hardware/neuralnetworks/1.0/IRKNeuralnetworks.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

#include "LoadModelCallback.h"
#include "GetResultCallback.h"

namespace rockchip::hardware::neuralnetworks::implementation {

using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;

struct RKNeuralnetworks : public V1_0::IRKNeuralnetworks {
    // Methods from ::rockchip::hardware::neuralnetworks::V1_0::IRKNeuralnetworks follow.
    Return<void> rknnInit(const ::rockchip::hardware::neuralnetworks::V1_0::RKNNModel& model, uint32_t size, uint32_t flag, rknnInit_cb _hidl_cb) override;
    Return<::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus> rknnDestory(uint64_t context) override;
    Return<::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus> rknnQuery(uint64_t context, ::rockchip::hardware::neuralnetworks::V1_0::RKNNQueryCmd cmd, const hidl_memory& info, uint32_t size) override;
    Return<::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus> rknnInputsSet(uint64_t context, const ::rockchip::hardware::neuralnetworks::V1_0::Request& request) override;
    Return<::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus> rknnRun(uint64_t context, const ::rockchip::hardware::neuralnetworks::V1_0::RKNNRunExtend& extend) override;
    Return<::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus> rknnOutputsGet(uint64_t context, const ::rockchip::hardware::neuralnetworks::V1_0::Response& response, const ::rockchip::hardware::neuralnetworks::V1_0::RKNNOutputExtend& extend) override;
    Return<::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus> rknnOutputsRelease(uint64_t context, const ::rockchip::hardware::neuralnetworks::V1_0::Response& response) override;
    Return<::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus> rknnDestoryMemory(uint64_t context, const ::rockchip::hardware::neuralnetworks::V1_0::RKNNTensorMemory& mem) override;
    Return<::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus> rknnSetIOMem(uint64_t context, const ::rockchip::hardware::neuralnetworks::V1_0::RKNNTensorMemory& mem, const ::rockchip::hardware::neuralnetworks::V1_0::RKNNTensorAttr& attr) override;
    Return<void> rknnCreateMem(uint64_t context, uint32_t size, rknnCreateMem_cb _hidl_cb) override;
    Return<void> registerCallback(const sp<::rockchip::hardware::neuralnetworks::V1_0::ILoadModelCallback>& loadCallback, const sp<::rockchip::hardware::neuralnetworks::V1_0::IGetResultCallback>& getCallback) override;

    // Methods from ::android::hidl::base::V1_0::IBase follow.
private:
    LoadModelCallback _load_cb;
    GetResultCallback _get_cb;
    std::map<uint64_t, void*> m_TempTensorMap;
    
#ifdef __arm__
    uint32_t ctx;
#else
    uint64_t ctx;
#endif
};

extern "C" V1_0::IRKNeuralnetworks* HIDL_FETCH_IRKNeuralnetworks(const char* name);

}  // namespace rockchip::hardware::neuralnetworks::implementation
