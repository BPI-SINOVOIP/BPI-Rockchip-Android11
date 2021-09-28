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

#include "LoadModelCallback.h"
#include "utils.h"

namespace rockchip::hardware::neuralnetworks::implementation {

// Methods from ::rockchip::hardware::neuralnetworks::V1_0::ILoadModelCallback follow.
Return<void> LoadModelCallback::notify(::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus status, const ::rockchip::hardware::neuralnetworks::V1_0::RKNNDeviceID& pdevs) {
    RECORD_TAG();
    UNUSED(ret);
    UNUSED(status);
    UNUSED(pdevs);
    return Void();
}


// Methods from ::android::hidl::base::V1_0::IBase follow.

//ILoadModelCallback* HIDL_FETCH_ILoadModelCallback(const char* /* name */) {
    //return new LoadModelCallback();
//}
//
}  // namespace rockchip::hardware::neuralnetworks::implementation
