/*
 * Copyright (C) 2019 The Android Open Source Project
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

#define LOG_TAG "SampleDriverFull"

#include "SampleDriverFull.h"

#include <vector>

#include "Utils.h"
#include "ValidateHal.h"

namespace android {
namespace nn {
namespace sample_driver {

using namespace hal;

Return<void> SampleDriverFull::getCapabilities_1_3(getCapabilities_1_3_cb cb) {
    android::nn::initVLogMask();
    VLOG(DRIVER) << "getCapabilities_1_3()";
    Capabilities capabilities = {
            .relaxedFloat32toFloat16PerformanceScalar = mPerf,
            .relaxedFloat32toFloat16PerformanceTensor = mPerf,
            .operandPerformance = nonExtensionOperandPerformance<HalVersion::V1_3>(mPerf),
            .ifPerformance = mPerf,
            .whilePerformance = mPerf};
    cb(ErrorStatus::NONE, capabilities);
    return Void();
}

Return<void> SampleDriverFull::getSupportedOperations_1_3(const V1_3::Model& model,
                                                          getSupportedOperations_1_3_cb cb) {
    VLOG(DRIVER) << "getSupportedOperations_1_3()";
    if (validateModel(model)) {
        const size_t count = model.main.operations.size();
        std::vector<bool> supported(count, true);
        cb(ErrorStatus::NONE, supported);
    } else {
        std::vector<bool> supported;
        cb(ErrorStatus::INVALID_ARGUMENT, supported);
    }
    return Void();
}

}  // namespace sample_driver
}  // namespace nn
}  // namespace android
