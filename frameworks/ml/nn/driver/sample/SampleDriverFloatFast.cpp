/*
 * Copyright (C) 2017 The Android Open Source Project
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

#define LOG_TAG "SampleDriverFloatFast"

#include <android-base/logging.h>
#include <hidl/LegacySupport.h>

#include <thread>
#include <vector>

#include "HalInterfaces.h"
#include "SampleDriverPartial.h"
#include "Utils.h"
#include "ValidateHal.h"

namespace android {
namespace nn {
namespace sample_driver {

using namespace hal;

class SampleDriverFloatFast : public SampleDriverPartial {
   public:
    SampleDriverFloatFast() : SampleDriverPartial("nnapi-sample_float_fast") {}
    Return<void> getCapabilities_1_3(getCapabilities_1_3_cb cb) override;

   private:
    std::vector<bool> getSupportedOperationsImpl(const V1_3::Model& model) const override;
};

Return<void> SampleDriverFloatFast::getCapabilities_1_3(getCapabilities_1_3_cb cb) {
    android::nn::initVLogMask();
    VLOG(DRIVER) << "getCapabilities()";

    Capabilities capabilities = {
            .relaxedFloat32toFloat16PerformanceScalar = {.execTime = 0.7f, .powerUsage = 1.1f},
            .relaxedFloat32toFloat16PerformanceTensor = {.execTime = 0.7f, .powerUsage = 1.1f},
            .operandPerformance = nonExtensionOperandPerformance<HalVersion::V1_3>({1.0f, 1.0f}),
            .ifPerformance = {.execTime = 1.0f, .powerUsage = 1.0f},
            .whilePerformance = {.execTime = 1.0f, .powerUsage = 1.0f}};
    update(&capabilities.operandPerformance, OperandType::TENSOR_FLOAT32,
           {.execTime = 0.8f, .powerUsage = 1.2f});
    update(&capabilities.operandPerformance, OperandType::FLOAT32,
           {.execTime = 0.8f, .powerUsage = 1.2f});

    cb(ErrorStatus::NONE, capabilities);
    return Void();
}

std::vector<bool> SampleDriverFloatFast::getSupportedOperationsImpl(
        const V1_3::Model& model) const {
    const size_t count = model.main.operations.size();
    std::vector<bool> supported(count);
    for (size_t i = 0; i < count; i++) {
        const Operation& operation = model.main.operations[i];
        if (operation.inputs.size() > 0) {
            const Operand& firstOperand = model.main.operands[operation.inputs[0]];
            supported[i] = firstOperand.type == OperandType::TENSOR_FLOAT32;
        }
    }
    return supported;
}

}  // namespace sample_driver
}  // namespace nn
}  // namespace android

using android::sp;
using android::nn::sample_driver::SampleDriverFloatFast;

int main() {
    sp<SampleDriverFloatFast> driver(new SampleDriverFloatFast());
    return driver->run();
}
