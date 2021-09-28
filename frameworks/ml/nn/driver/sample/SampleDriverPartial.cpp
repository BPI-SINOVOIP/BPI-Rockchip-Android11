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

#define LOG_TAG "SampleDriverPartial"

#include "SampleDriverPartial.h"

#include <android-base/logging.h>
#include <hidl/LegacySupport.h>

#include <thread>
#include <vector>

#include "HalInterfaces.h"
#include "SampleDriverUtils.h"
#include "Utils.h"
#include "ValidateHal.h"

namespace android {
namespace nn {
namespace sample_driver {

using namespace hal;

Return<void> SampleDriverPartial::getSupportedOperations_1_3(const V1_3::Model& model,
                                                             getSupportedOperations_1_3_cb cb) {
    VLOG(DRIVER) << "getSupportedOperations()";
    if (validateModel(model)) {
        std::vector<bool> supported = getSupportedOperationsImpl(model);
        cb(ErrorStatus::NONE, supported);
    } else {
        std::vector<bool> supported;
        cb(ErrorStatus::INVALID_ARGUMENT, supported);
    }
    return Void();
}

Return<ErrorStatus> SampleDriverPartial::prepareModel_1_3(
        const V1_3::Model& model, ExecutionPreference preference, Priority priority,
        const OptionalTimePoint& deadline, const hidl_vec<hidl_handle>&,
        const hidl_vec<hidl_handle>&, const CacheToken&,
        const sp<V1_3::IPreparedModelCallback>& callback) {
    std::vector<bool> supported = getSupportedOperationsImpl(model);
    bool isModelFullySupported =
            std::all_of(supported.begin(), supported.end(), [](bool v) { return v; });
    return prepareModelBase(model, this, preference, priority, deadline, callback,
                            isModelFullySupported);
}

}  // namespace sample_driver
}  // namespace nn
}  // namespace android
