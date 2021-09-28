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

#ifndef ANDROID_FRAMEWORKS_ML_NN_DRIVER_SAMPLE_SAMPLE_DRIVER_FULL_H
#define ANDROID_FRAMEWORKS_ML_NN_DRIVER_SAMPLE_SAMPLE_DRIVER_FULL_H

#include "HalInterfaces.h"
#include "SampleDriver.h"

namespace android {
namespace nn {
namespace sample_driver {

class SampleDriverFull : public SampleDriver {
   public:
    SampleDriverFull(const char* name, hal::PerformanceInfo perf)
        : SampleDriver(name), mPerf(perf) {}
    hal::Return<void> getCapabilities_1_3(getCapabilities_1_3_cb cb) override;
    hal::Return<void> getSupportedOperations_1_3(const hal::V1_3::Model& model,
                                                 getSupportedOperations_1_3_cb cb) override;

   private:
    hal::PerformanceInfo mPerf;
};

}  // namespace sample_driver
}  // namespace nn
}  // namespace android

#endif  // ANDROID_FRAMEWORKS_ML_NN_DRIVER_SAMPLE_SAMPLE_DRIVER_FULL_H
