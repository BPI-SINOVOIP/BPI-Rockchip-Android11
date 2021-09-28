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

#include "GetResultCallback.h"
#include "utils.h"
namespace rockchip::hardware::neuralnetworks::implementation {

// Methods from ::rockchip::hardware::neuralnetworks::V1_0::IGetResultCallback follow.
Return<void> GetResultCallback::notify(::rockchip::hardware::neuralnetworks::V1_0::ErrorStatus status, const ::rockchip::hardware::neuralnetworks::V1_0::Response& response) {
    RECORD_TAG();
    UNUSED(status);
    UNUSED(response);
    UNUSED(ret);
    return Void();
}


// Methods from ::android::hidl::base::V1_0::IBase follow.

//IGetResultCallback* HIDL_FETCH_IGetResultCallback(const char* /* name */) {
    //return new GetResultCallback();
//}
//
}  // namespace rockchip::hardware::neuralnetworks::implementation
