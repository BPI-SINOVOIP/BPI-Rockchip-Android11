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

#define LOG_TAG "SampleDriverQuant"

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

class SampleDriverQuant : public SampleDriverPartial {
   public:
    SampleDriverQuant() : SampleDriverPartial("nnapi-sample_quant") {}
    Return<void> getCapabilities_1_3(getCapabilities_1_3_cb cb) override;

   private:
    std::vector<bool> getSupportedOperationsImpl(const V1_3::Model& model) const override;
};

Return<void> SampleDriverQuant::getCapabilities_1_3(getCapabilities_1_3_cb cb) {
    android::nn::initVLogMask();
    VLOG(DRIVER) << "getCapabilities()";

    Capabilities capabilities = {
            .relaxedFloat32toFloat16PerformanceScalar = {.execTime = 50.0f, .powerUsage = 1.0f},
            .relaxedFloat32toFloat16PerformanceTensor = {.execTime = 50.0f, .powerUsage = 1.0f},
            .operandPerformance = nonExtensionOperandPerformance<HalVersion::V1_3>({50.0f, 1.0f}),
            .ifPerformance = {.execTime = 50.0f, .powerUsage = 1.0f},
            .whilePerformance = {.execTime = 50.0f, .powerUsage = 1.0f}};

    cb(ErrorStatus::NONE, capabilities);
    return Void();
}

static bool isQuantized(OperandType opType) {
    return opType == OperandType::TENSOR_QUANT8_ASYMM ||
           opType == OperandType::TENSOR_QUANT8_ASYMM_SIGNED;
}

std::vector<bool> SampleDriverQuant::getSupportedOperationsImpl(const V1_3::Model& model) const {
    const size_t count = model.main.operations.size();
    std::vector<bool> supported(count);
    for (size_t i = 0; i < count; i++) {
        const Operation& operation = model.main.operations[i];
        if (operation.inputs.size() > 0) {
            const Operand& firstOperand = model.main.operands[operation.inputs[0]];
            supported[i] = isQuantized(firstOperand.type);
            if (operation.type == OperationType::SELECT) {
                const Operand& secondOperand = model.main.operands[operation.inputs[1]];
                supported[i] = isQuantized(secondOperand.type);
            }
        }
    }
    return supported;
}

}  // namespace sample_driver
}  // namespace nn
}  // namespace android

using android::sp;
using android::nn::sample_driver::SampleDriverQuant;

int main() {
    sp<SampleDriverQuant> driver(new SampleDriverQuant());
    return driver->run();
}
