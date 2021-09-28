/*
 * Copyright (C) 2018 The Android Open Source Project
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
#include <utility>
#include <vector>

#include "HalInterfaces.h"
#include "OperationResolver.h"
#include "OperationsUtils.h"

namespace android {
namespace nn {
namespace topk_v2 {

constexpr uint32_t kNumInputs = 2;
constexpr uint32_t kInputTensor = 0;
constexpr uint32_t kTopKScalar = 1;

constexpr uint32_t kNumOutputs = 2;
constexpr uint32_t kOutputValuesTensor = 0;
constexpr uint32_t kOutputIndicesTensor = 1;

namespace {

using namespace hal;

template <typename T>
bool evalGeneric(const T* inputData, const Shape& inputShape, const int32_t k, T* valuesData,
                 int32_t* indicesData) {
    const int rowSize = inputShape.dimensions.back();
    const int totalSize = getNumberOfElements(inputShape);
    std::vector<std::pair<T, int32_t>> values(rowSize);
    T* curOutputValue = valuesData;
    int32_t* curOutputIndex = indicesData;
    for (int rowBegin = 0; rowBegin < totalSize; rowBegin += rowSize) {
        for (int i = 0; i < rowSize; ++i) {
            values[i] = std::make_pair(inputData[rowBegin + i], i);
        }
        std::nth_element(values.begin(), values.begin() + (rowSize - k), values.end());
        std::sort(values.begin() + (rowSize - k), values.end());
        std::reverse(values.begin(), values.end());
        for (int i = 0; i < k; ++i) {
            *curOutputValue = values[i].first;
            *curOutputIndex = values[i].second;
            curOutputValue++;
            curOutputIndex++;
        }
    }
    return true;
}

template <typename T>
bool executeTyped(IOperationExecutionContext* context) {
    return evalGeneric(context->getInputBuffer<T>(kInputTensor),
                       context->getInputShape(kInputTensor),
                       context->getInputValue<int32_t>(kTopKScalar),
                       context->getOutputBuffer<T>(kOutputValuesTensor),
                       context->getOutputBuffer<int32_t>(kOutputIndicesTensor));
}

}  // namespace

bool validate(const IOperationValidationContext* context) {
    NN_RET_CHECK_EQ(context->getNumInputs(), kNumInputs);
    NN_RET_CHECK_EQ(context->getNumOutputs(), kNumOutputs);
    OperandType inputType = context->getInputType(kInputTensor);
    NN_RET_CHECK(inputType == OperandType::TENSOR_FLOAT16 ||
                 inputType == OperandType::TENSOR_FLOAT32 ||
                 inputType == OperandType::TENSOR_INT32 ||
                 inputType == OperandType::TENSOR_QUANT8_ASYMM ||
                 inputType == OperandType::TENSOR_QUANT8_ASYMM_SIGNED)
            << "Unsupported input operand type for select op: " << toString(inputType);
    NN_RET_CHECK(validateInputTypes(context, {inputType, OperandType::INT32}));
    NN_RET_CHECK(validateOutputTypes(context, {inputType, OperandType::TENSOR_INT32}));
    HalVersion minSupportedHalVersion = HalVersion::V1_2;
    if (inputType == OperandType::TENSOR_QUANT8_ASYMM_SIGNED) {
        minSupportedHalVersion = HalVersion::V1_3;
    }
    return validateHalVersion(context, minSupportedHalVersion);
}

bool prepare(IOperationExecutionContext* context) {
    const Shape inputShape = context->getInputShape(kInputTensor);
    const int32_t k = context->getInputValue<int32_t>(kTopKScalar);
    NN_RET_CHECK_GT(k, 0);
    NN_RET_CHECK_LE(k, inputShape.dimensions.back());

    // Copy input shape to ensure that quantization parameters for the output
    // values are the same as for the input tensor.
    Shape outputValuesShape = inputShape;
    outputValuesShape.dimensions.back() = k;
    Shape outputIndicesShape;
    outputIndicesShape.type = OperandType::TENSOR_INT32;
    outputIndicesShape.dimensions = inputShape.dimensions;
    outputIndicesShape.dimensions.back() = k;
    return context->setOutputShape(kOutputValuesTensor, outputValuesShape) &&
           context->setOutputShape(kOutputIndicesTensor, outputIndicesShape);
}

bool execute(IOperationExecutionContext* context) {
    const Shape inputShape = context->getInputShape(kInputTensor);
    switch (inputShape.type) {
        case OperandType::TENSOR_FLOAT16: {
            return executeTyped<_Float16>(context);
        } break;
        case OperandType::TENSOR_FLOAT32: {
            return executeTyped<float>(context);
        } break;
        case OperandType::TENSOR_INT32: {
            return executeTyped<int32_t>(context);
        } break;
        case OperandType::TENSOR_QUANT8_ASYMM: {
            return executeTyped<uint8_t>(context);
        } break;
        case OperandType::TENSOR_QUANT8_ASYMM_SIGNED: {
            return executeTyped<int8_t>(context);
        } break;
        default: {
            LOG(ERROR) << "Unsupported data type: " << toString(inputShape.type);
            return false;
        }
    }
}

}  // namespace topk_v2

NN_REGISTER_OPERATION(TOPK_V2, "TOPK_V2", topk_v2::validate, topk_v2::prepare, topk_v2::execute);

}  // namespace nn
}  // namespace android
