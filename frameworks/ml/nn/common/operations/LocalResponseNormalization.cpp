/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <tensorflow/lite/kernels/internal/optimized/optimized_ops.h>

#include <algorithm>
#include <vector>

#include "CpuOperationUtils.h"
#include "HalInterfaces.h"
#include "OperationResolver.h"
#include "Tracing.h"

namespace android {
namespace nn {
namespace local_response_norm {

constexpr char kOperationName[] = "LOCAL_RESPONSE_NORMALIZATION";

constexpr uint32_t kNumInputs = 6;
constexpr uint32_t kInputTensor = 0;
constexpr uint32_t kRadiusScalar = 1;
constexpr uint32_t kBiasScalar = 2;
constexpr uint32_t kAlphaScalar = 3;
constexpr uint32_t kBetaScalar = 4;
constexpr uint32_t kAxisScalar = 5;

constexpr uint32_t kNumOutputs = 1;
constexpr uint32_t kOutputTensor = 0;

namespace {

using namespace hal;

inline bool localResponseNormFloat32Impl(const float* inputData, const Shape& inputShape,
                                         int32_t radius, float bias, float alpha, float beta,
                                         int32_t axis, float* outputData,
                                         const Shape& outputShape) {
    NNTRACE_TRANS("localResponseNormFloat32");
    const uint32_t outerSize = getNumberOfElements(inputShape, 0, axis);
    const uint32_t axisSize = getSizeOfDimension(inputShape, axis);
    const uint32_t innerSize =
            getNumberOfElements(inputShape, axis + 1, getNumberOfDimensions(inputShape));
    for (uint32_t outer = 0; outer < outerSize; ++outer) {
        const float* inputBase = inputData + outer * axisSize * innerSize;
        float* outputBase = outputData + outer * axisSize * innerSize;
        for (uint32_t inner = 0; inner < innerSize; ++inner, ++inputBase, ++outputBase) {
            for (int32_t i = 0; i < axisSize; i++) {
                const int32_t dBegin = std::max(0, i - radius);
                // Add 1 on dEnd to comply with optimized_ops in TFLite
                const int32_t dEnd = std::min(static_cast<int32_t>(axisSize), i + radius + 1);
                float sum = 0.0f;
                for (int32_t d = dBegin; d < dEnd; d++) {
                    float val = inputBase[d * innerSize];
                    sum += val * val;
                }
                float multiplier = std::pow(bias + alpha * sum, -beta);
                outputBase[i * innerSize] = inputBase[i * innerSize] * multiplier;
            }
        }
    }
    return true;
}

template <typename T>
bool localResponseNorm(const T* inputData, const Shape& inputShape, int32_t radius, T bias, T alpha,
                       T beta, int32_t axis, T* outputData, const Shape& outputShape);

template <>
bool localResponseNorm<float>(const float* inputData, const Shape& inputShape, int32_t radius,
                              float bias, float alpha, float beta, int32_t axis, float* outputData,
                              const Shape& outputShape) {
    int32_t ndim = getNumberOfDimensions(inputShape);
    NN_CHECK(handleNegativeAxis(inputShape, &axis));
    // TFLite optimized implementation only supports computation along the last axis
    if (axis == ndim - 1) {
        NNTRACE_COMP("optimized_ops::LocalResponseNormalization::float");
        tflite::LocalResponseNormalizationParams param = {
                .range = radius, .bias = bias, .alpha = alpha, .beta = beta};
        tflite::optimized_ops::LocalResponseNormalization(
                param, convertShapeToTflshape(inputShape), inputData,
                convertShapeToTflshape(outputShape), outputData);
        return true;
    } else {
        return localResponseNormFloat32Impl(inputData, inputShape, radius, bias, alpha, beta, axis,
                                            outputData, outputShape);
    }
}

template <>
bool localResponseNorm<_Float16>(const _Float16* inputData, const Shape& inputShape, int32_t radius,
                                 _Float16 bias, _Float16 alpha, _Float16 beta, int32_t axis,
                                 _Float16* outputData, const Shape& outputShape) {
    NNTRACE_TRANS("localResponseNormFloat16");
    std::vector<float> inputDataFloat32(getNumberOfElements(inputShape));
    convertFloat16ToFloat32(inputData, &inputDataFloat32);
    std::vector<float> outputDataFloat32(getNumberOfElements(outputShape));

    localResponseNorm<float>(inputDataFloat32.data(), inputShape, radius, bias, alpha, beta, axis,
                             outputDataFloat32.data(), outputShape);
    convertFloat32ToFloat16(outputDataFloat32, outputData);

    return true;
}

template <typename T>
bool executeTyped(IOperationExecutionContext* context) {
    int32_t axis = context->getNumInputs() == kNumInputs
                           ? context->getInputValue<int32_t>(kAxisScalar)
                           : -1;
    NN_RET_CHECK(handleNegativeAxis(context->getInputShape(kInputTensor), &axis));
    return localResponseNorm<T>(
            context->getInputBuffer<T>(kInputTensor), context->getInputShape(kInputTensor),
            context->getInputValue<int32_t>(kRadiusScalar), context->getInputValue<T>(kBiasScalar),
            context->getInputValue<T>(kAlphaScalar), context->getInputValue<T>(kBetaScalar), axis,
            context->getOutputBuffer<T>(kOutputTensor), context->getOutputShape(kOutputTensor));
}

}  // namespace

bool validate(const IOperationValidationContext* context) {
    NN_RET_CHECK(context->getNumInputs() == kNumInputs ||
                 context->getNumInputs() == kNumInputs - 1);
    NN_RET_CHECK_EQ(context->getNumOutputs(), kNumOutputs);

    const OperandType inputType = context->getInputType(kInputTensor);
    std::vector<OperandType> inExpectedTypes;
    std::vector<OperandType> outExpectedTypes;
    if (inputType == OperandType::TENSOR_FLOAT32) {
        NN_RET_CHECK(validateHalVersion(context, HalVersion::V1_0));
        inExpectedTypes = {
                OperandType::TENSOR_FLOAT32, OperandType::INT32,   OperandType::FLOAT32,
                OperandType::FLOAT32,        OperandType::FLOAT32,
        };
        outExpectedTypes = {OperandType::TENSOR_FLOAT32};
    } else if (inputType == OperandType::TENSOR_FLOAT16) {
        NN_RET_CHECK(validateHalVersion(context, HalVersion::V1_2));
        inExpectedTypes = {
                OperandType::TENSOR_FLOAT16, OperandType::INT32,   OperandType::FLOAT16,
                OperandType::FLOAT16,        OperandType::FLOAT16,
        };
        outExpectedTypes = {OperandType::TENSOR_FLOAT16};
    } else {
        NN_RET_CHECK_FAIL() << "Unsupported tensor type for operation " << kOperationName;
    }

    if (context->getNumInputs() == kNumInputs) {
        inExpectedTypes.push_back(OperandType::INT32);
        NN_RET_CHECK(validateHalVersion(context, HalVersion::V1_2));
    } else if (context->getInputShape(kInputTensor).dimensions.size() != 4) {
        NN_RET_CHECK(validateHalVersion(context, HalVersion::V1_2));
    }

    const Shape& input = context->getInputShape(kInputTensor);
    if (hasKnownRank(input)) {
        NN_RET_CHECK_LE(getNumberOfDimensions(input), 4);
    }
    return validateInputTypes(context, inExpectedTypes) &&
           validateOutputTypes(context, {inputType});
}

bool prepare(IOperationExecutionContext* context) {
    const Shape& input = context->getInputShape(kInputTensor);
    int32_t numDimensions = getNumberOfDimensions(input);
    int32_t axis = context->getNumInputs() == kNumInputs
                           ? context->getInputValue<int32_t>(kAxisScalar)
                           : -1;
    NN_RET_CHECK_LE(numDimensions, 4);
    NN_RET_CHECK_GE(axis, -numDimensions);
    NN_RET_CHECK_LT(axis, numDimensions);
    return context->setOutputShape(kOutputTensor, input);
}

bool execute(IOperationExecutionContext* context) {
    switch (context->getInputType(kInputTensor)) {
        case OperandType::TENSOR_FLOAT32:
            return executeTyped<float>(context);
        case OperandType::TENSOR_FLOAT16:
            return executeTyped<_Float16>(context);
        default:
            NN_RET_CHECK_FAIL() << "Unsupported tensor type for operation " << kOperationName;
    }
}

}  // namespace local_response_norm

NN_REGISTER_OPERATION(LOCAL_RESPONSE_NORMALIZATION, local_response_norm::kOperationName,
                      local_response_norm::validate, local_response_norm::prepare,
                      local_response_norm::execute);

}  // namespace nn
}  // namespace android
