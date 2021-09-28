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

#define LOG_TAG "Operations"

#include <algorithm>
#include <cmath>
#include <vector>

#include "HalInterfaces.h"
#include "IndexedShapeWrapper.h"
#include "OperationResolver.h"
#include "OperationsUtils.h"
#include "Tracing.h"

namespace android {
namespace nn {
namespace elu {

using namespace hal;

constexpr uint32_t kNumInputs = 2;
constexpr uint32_t kInputTensor = 0;
constexpr uint32_t kAlphaScalar = 1;

constexpr uint32_t kNumOutputs = 1;
constexpr uint32_t kOutputTensor = 0;

namespace {

template <typename T>
bool eluFloat(const T* inputData, const Shape& inputShape, const T alpha, T* outputData,
              const Shape& outputShape) {
    NNTRACE_COMP("ELU");
    int numElements = getNumberOfElements(inputShape);
    for (int i = 0; i < numElements; ++i) {
        float x = static_cast<float>(inputData[i]);
        outputData[i] = static_cast<T>(std::max(0.f, x) + std::min(0.f, alpha * (std::exp(x) - 1)));
    }
    return true;
}

}  // namespace

bool validate(const IOperationValidationContext* context) {
    NN_RET_CHECK_EQ(context->getNumInputs(), kNumInputs);
    NN_RET_CHECK_EQ(context->getNumOutputs(), kNumOutputs);
    auto inputType = context->getInputType(kInputTensor);
    if (inputType == OperandType::TENSOR_FLOAT16 || inputType == OperandType::TENSOR_FLOAT32) {
        NN_RET_CHECK(validateHalVersion(context, HalVersion::V1_3));
    } else {
        NN_RET_CHECK_FAIL() << "Unsupported tensor type for operation ELU";
    }
    auto scalarType =
            inputType == OperandType::TENSOR_FLOAT16 ? OperandType::FLOAT16 : OperandType::FLOAT32;
    return validateInputTypes(context, {inputType, scalarType}) &&
           validateOutputTypes(context, {inputType});
}

bool prepare(IOperationExecutionContext* context) {
    Shape inputShape = context->getInputShape(kInputTensor);
    return context->setOutputShape(kOutputTensor, inputShape);
}

bool execute(IOperationExecutionContext* context) {
    // Bypass execution in the case of zero-sized input.
    if (getNumberOfElements(context->getOutputShape(kOutputTensor)) == 0) return true;
    switch (context->getInputType(kInputTensor)) {
        case OperandType::TENSOR_FLOAT16:
            return eluFloat(context->getInputBuffer<_Float16>(kInputTensor),
                            context->getInputShape(kInputTensor),
                            context->getInputValue<_Float16>(kAlphaScalar),
                            context->getOutputBuffer<_Float16>(kOutputTensor),
                            context->getOutputShape(kOutputTensor));
        case OperandType::TENSOR_FLOAT32:
            return eluFloat(context->getInputBuffer<float>(kInputTensor),
                            context->getInputShape(kInputTensor),
                            context->getInputValue<float>(kAlphaScalar),
                            context->getOutputBuffer<float>(kOutputTensor),
                            context->getOutputShape(kOutputTensor));
        default:
            NN_RET_CHECK_FAIL() << "Unsupported tensor type for operation ELU";
    }
}

}  // namespace elu

NN_REGISTER_OPERATION(ELU, "ELU", elu::validate, elu::prepare, elu::execute);

}  // namespace nn
}  // namespace android
