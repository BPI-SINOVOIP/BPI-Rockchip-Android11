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

#include <android-base/logging.h>
#include <hidl/LegacySupport.h>

#include <thread>
#include <vector>

#include "HalInterfaces.h"
#include "SampleDriver.h"
#include "Utils.h"
#include "ValidateHal.h"

namespace android {
namespace nn {
namespace sample_driver {

// A base class for sample drivers that support only a subset of NNAPI
// operations. Classes of such drivers should inherit from this class and
// implement getSupportedOperationsImpl function which is used for filtering out
// unsupported ops.
class SampleDriverPartial : public SampleDriver {
   public:
    SampleDriverPartial(const char* name, const IOperationResolver* operationResolver =
                                                  BuiltinOperationResolver::get())
        : SampleDriver(name, operationResolver) {}
    hal::Return<void> getSupportedOperations_1_3(const hal::V1_3::Model& model,
                                                 getSupportedOperations_1_3_cb cb) override;
    hal::Return<hal::ErrorStatus> prepareModel_1_3(
            const hal::V1_3::Model& model, hal::ExecutionPreference preference,
            hal::Priority priority, const hal::OptionalTimePoint& deadline,
            const hal::hidl_vec<hal::hidl_handle>& modelCache,
            const hal::hidl_vec<hal::hidl_handle>& dataCache, const hal::CacheToken& token,
            const sp<hal::V1_3::IPreparedModelCallback>& callback) override;

   protected:
    // Given a valid NNAPI Model returns a boolean vector that indicates which
    // ops in the model are supported by a driver.
    virtual std::vector<bool> getSupportedOperationsImpl(const hal::V1_3::Model& model) const = 0;
};

}  // namespace sample_driver
}  // namespace nn
}  // namespace android
