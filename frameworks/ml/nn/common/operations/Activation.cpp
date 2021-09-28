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

#define LOG_TAG "Operations"

#include <tensorflow/lite/kernels/internal/optimized/legacy_optimized_ops.h>
#include <tensorflow/lite/kernels/internal/optimized/optimized_ops.h>
#include <tensorflow/lite/kernels/internal/reference/integer_ops/logistic.h>
#include <tensorflow/lite/kernels/internal/reference/integer_ops/tanh.h>
#include <tensorflow/lite/kernels/internal/reference/reference_ops.h>

#include <algorithm>
#include <limits>
#include <vector>

#include "ActivationFunctor.h"
#include "CpuOperationUtils.h"
#include "HalInterfaces.h"
#include "OperationResolver.h"
#include "OperationsUtils.h"
#include "Tracing.h"

namespace android {
namespace nn {

using namespace hal;

namespace activation {

constexpr uint32_t kNumInputs = 1;
constexpr uint32_t kInputTensor = 0;

constexpr uint32_t kNumOutputs = 1;
constexpr uint32_t kOutputTensor = 0;

namespace {

template <typename T>
bool reluFloat(const T* inputData, const Shape& inputShape, T* outputData, const Shape& outputShape,
               float reluMin = 0.f, float reluMax = std::numeric_limits<float>::max()) {
    NNTRACE_COMP("reluX");
    int numElements = getNumberOfElements(inputShape);
    for (int i = 0; i < numElements; i++, inputData++, outputData++) {
        *outputData = static_cast<T>(
                std::min(std::max(reluMin, static_cast<float>(*inputData)), reluMax));
    }
    return true;
}
template bool reluFloat<float>(const float* inputData, const Shape& inputShape, float* outputData,
                               const Shape& outputShape, float reluMin, float reluMax);
template bool reluFloat<_Float16>(const _Float16* inputData, const Shape& inputShape,
                                  _Float16* outputData, const Shape& outputShape, float reluMin,
                                  float reluMax);

template <typename T>
bool relu1Float(const T* inputData, const Shape& inputShape, T* outputData,
                const Shape& outputShape) {
    return reluFloat(inputData, inputShape, outputData, outputShape, -1.f, 1.f);
}
template bool relu1Float<float>(const float* inputData, const Shape& inputShape, float* outputData,
                                const Shape& outputShape);
template bool relu1Float<_Float16>(const _Float16* inputData, const Shape& inputShape,
                                   _Float16* outputData, const Shape& outputShape);

template <typename T>
bool relu6Float(const T* inputData, const Shape& inputShape, T* outputData,
                const Shape& outputShape) {
    return reluFloat(inputData, inputShape, outputData, outputShape, 0.f, 6.f);
}
template bool relu6Float<float>(const float* inputData, const Shape& inputShape, float* outputData,
                                const Shape& outputShape);
template bool relu6Float<_Float16>(const _Float16* inputData, const Shape& inputShape,
                                   _Float16* outputData, const Shape& outputShape);

bool tanhFloat16(const _Float16* inputData, const Shape& inputShape, _Float16* outputData,
                 const Shape& outputShape) {
    NNTRACE_COMP("tanhFloat16");
    int numElements = getNumberOfElements(inputShape);
    for (int i = 0; i < numElements; i++, inputData++, outputData++) {
        *outputData = static_cast<_Float16>(std::tanh(static_cast<float>(*inputData)));
    }
    return true;
}

bool tanhFloat32(const float* inputData, const Shape& inputShape, float* outputData,
                 const Shape& outputShape) {
    NNTRACE_COMP("tanhFloat32");
    int numElements = getNumberOfElements(inputShape);
    for (int i = 0; i < numElements; i++, inputData++, outputData++) {
        *outputData = std::tanh(*inputData);
    }
    return true;
}

template <typename T>
bool logisticFloat(const T* inputData, const Shape& inputShape, T* outputData,
                   const Shape& outputShape) {
    NNTRACE_COMP("logisticFloat");
    int numElements = getNumberOfElements(inputShape);
    for (int i = 0; i < numElements; i++, inputData++, outputData++) {
        *outputData = static_cast<T>(1.f / (1.f + std::exp(static_cast<float>(-*inputData))));
    }
    return true;
}
template bool logisticFloat<float>(const float* inputData, const Shape& inputShape,
                                   float* outputData, const Shape& outputShape);
template bool logisticFloat<_Float16>(const _Float16* inputData, const Shape& inputShape,
                                      _Float16* outputData, const Shape& outputShape);

template <ActivationFn activation>
inline bool reluXQuant8(const uint8_t* inputData, const Shape& inputShape, uint8_t* outputData,
                        const Shape& outputShape) {
    int numElements = getNumberOfElements(inputShape);
    int32_t output_activation_min = 0;
    int32_t output_activation_max = 0;

    CalculateActivationRangeUint8(activation, inputShape, &output_activation_min,
                                  &output_activation_max);

    for (int i = 0; i < numElements; i++, inputData++, outputData++) {
        *outputData = std::min((uint8_t)output_activation_max,
                               std::max((uint8_t)output_activation_min, *inputData));
    }
    return true;
}

bool reluQuant8(const uint8_t* inputData, const Shape& inputShape, uint8_t* outputData,
                const Shape& outputShape) {
    NNTRACE_COMP("reluQuant8");
    return reluXQuant8<kActivationRelu>(inputData, inputShape, outputData, outputShape);
}

bool relu1Quant8(const uint8_t* inputData, const Shape& inputShape, uint8_t* outputData,
                 const Shape& outputShape) {
    NNTRACE_COMP("relu1Quant8");
    return reluXQuant8<kActivationRelu1>(inputData, inputShape, outputData, outputShape);
}

bool relu6Quant8(const uint8_t* inputData, const Shape& inputShape, uint8_t* outputData,
                 const Shape& outputShape) {
    NNTRACE_COMP("relu6Quant8");
    return reluXQuant8<kActivationRelu6>(inputData, inputShape, outputData, outputShape);
}

bool tanhQuant8(const uint8_t* inputData, const Shape& inputShape, uint8_t* outputData,
                const Shape& outputShape) {
    NNTRACE_TRANS("tanhQuant8");
    if (outputShape.offset != 128 || outputShape.scale != 1.f / 128) {
        LOG(ERROR) << "incorrect scale or offset for TANH output";
        return false;
    }

    int numElements = getNumberOfElements(inputShape);
    static constexpr int kInputIntegerBits = 4;

    const double input_real_multiplier =
            inputShape.scale * static_cast<double>(1 << (31 - kInputIntegerBits));

    int32_t input_multiplier = 0;
    int32_t input_left_shift = 0;
    if (!QuantizeMultiplierGreaterThanOne(input_real_multiplier, &input_multiplier,
                                          &input_left_shift)) {
        return false;
    }
    int32_t input_range_radius = CalculateInputRadius(kInputIntegerBits, input_left_shift);

    NNTRACE_COMP_SWITCH("optimized_ops::Tanh");
    tflite::optimized_ops::Tanh(inputData, convertShapeToTflshape(inputShape), inputShape.offset,
                                input_range_radius, input_multiplier, input_left_shift, outputData,
                                convertShapeToTflshape(outputShape));

    return true;
}

bool logisticQuant8(const uint8_t* inputData, const Shape& inputShape, uint8_t* outputData,
                    const Shape& outputShape) {
    NNTRACE_TRANS("logisticQuant8");
    if (outputShape.offset != 0 || outputShape.scale != 1.f / 256) {
        LOG(ERROR) << "incorrect scale / offset for output";
        return false;
    }

    int numElements = getNumberOfElements(inputShape);
    static constexpr int kInputIntegerBits = 4;

    const double input_real_multiplier =
            inputShape.scale * static_cast<double>(1 << (31 - kInputIntegerBits));

    int32_t input_multiplier = 0;
    int32_t input_left_shift = 0;
    if (!QuantizeMultiplierGreaterThanOne(input_real_multiplier, &input_multiplier,
                                          &input_left_shift)) {
        return false;
    }
    int32_t input_range_radius = CalculateInputRadius(kInputIntegerBits, input_left_shift);

    NNTRACE_COMP_SWITCH("optimized_ops::Logistic");
    tflite::optimized_ops::Logistic(
            inputData, convertShapeToTflshape(inputShape), inputShape.offset, input_range_radius,
            input_multiplier, input_left_shift, outputData, convertShapeToTflshape(outputShape));

    return true;
}

template <ActivationFn activation>
inline bool reluXQuant8Signed(const int8_t* inputData, const Shape& inputShape, int8_t* outputData,
                              const Shape& outputShape) {
    int numElements = getNumberOfElements(inputShape);
    int32_t output_activation_min = 0;
    int32_t output_activation_max = 0;

    CalculateActivationRangeInt8(activation, inputShape, &output_activation_min,
                                 &output_activation_max);

    for (int i = 0; i < numElements; i++, inputData++, outputData++) {
        *outputData = std::min((int8_t)output_activation_max,
                               std::max((int8_t)output_activation_min, *inputData));
    }
    return true;
}

bool reluQuant8Signed(const int8_t* inputData, const Shape& inputShape, int8_t* outputData,
                      const Shape& outputShape) {
    NNTRACE_COMP("reluQuant8");
    return reluXQuant8Signed<kActivationRelu>(inputData, inputShape, outputData, outputShape);
}

bool relu1Quant8Signed(const int8_t* inputData, const Shape& inputShape, int8_t* outputData,
                       const Shape& outputShape) {
    NNTRACE_COMP("relu1Quant8");
    return reluXQuant8Signed<kActivationRelu1>(inputData, inputShape, outputData, outputShape);
}

bool relu6Quant8Signed(const int8_t* inputData, const Shape& inputShape, int8_t* outputData,
                       const Shape& outputShape) {
    NNTRACE_COMP("relu6Quant8");
    return reluXQuant8Signed<kActivationRelu6>(inputData, inputShape, outputData, outputShape);
}

bool tanhQuant8Signed(const int8_t* inputData, const Shape& inputShape, int8_t* outputData,
                      const Shape& outputShape) {
    NNTRACE_TRANS("tanhQuant8Signed");
    if (outputShape.offset != 0 || outputShape.scale != 1.f / 128) {
        LOG(ERROR) << "incorrect scale or offset for TANH output";
        return false;
    }

    int numElements = getNumberOfElements(inputShape);
    static constexpr int kInputIntegerBits = 4;

    const double input_real_multiplier =
            inputShape.scale * static_cast<double>(1 << (31 - kInputIntegerBits));

    int32_t input_multiplier = 0;
    int32_t input_left_shift = 0;
    if (!QuantizeMultiplierGreaterThanOne(input_real_multiplier, &input_multiplier,
                                          &input_left_shift)) {
        return false;
    }
    int32_t input_range_radius = CalculateInputRadius(kInputIntegerBits, input_left_shift);

    NNTRACE_COMP_SWITCH("reference_integer_ops::Tanh");
    tflite::reference_integer_ops::Tanh(inputShape.offset, input_range_radius, input_multiplier,
                                        input_left_shift, numElements, inputData, outputData);

    return true;
}

bool logisticQuant8Signed(const int8_t* inputData, const Shape& inputShape, int8_t* outputData,
                          const Shape& outputShape) {
    NNTRACE_TRANS("logisticQuant8Signed");
    if (outputShape.offset != -128 || outputShape.scale != 1.f / 256) {
        LOG(ERROR) << "incorrect scale / offset for output";
        return false;
    }

    int numElements = getNumberOfElements(inputShape);
    static constexpr int kInputIntegerBits = 4;

    const double input_real_multiplier =
            inputShape.scale * static_cast<double>(1 << (31 - kInputIntegerBits));

    int32_t input_multiplier = 0;
    int32_t input_left_shift = 0;
    if (!QuantizeMultiplierGreaterThanOne(input_real_multiplier, &input_multiplier,
                                          &input_left_shift)) {
        return false;
    }
    int32_t input_range_radius = CalculateInputRadius(kInputIntegerBits, input_left_shift);

    NNTRACE_COMP_SWITCH("reference_integer_ops::Logistic");
    tflite::reference_integer_ops::Logistic(inputShape.offset, input_range_radius, input_multiplier,
                                            input_left_shift, numElements, inputData, outputData);

    return true;
}

void DownScaleInt32ToInt16Multiplier(int32_t multiplier_int32, int16_t* multiplier_int16) {
    TFLITE_DCHECK_GE(multiplier_int32, 0);
    static constexpr int32_t kRoundingOffset = 1 << 15;
    if (multiplier_int32 >= std::numeric_limits<int32_t>::max() - kRoundingOffset) {
        *multiplier_int16 = std::numeric_limits<int16_t>::max();
        return;
    }
    const int32_t result = (multiplier_int32 + kRoundingOffset) >> 16;
    TFLITE_DCHECK_LE(result << 16, multiplier_int32 + kRoundingOffset);
    TFLITE_DCHECK_GT(result << 16, multiplier_int32 - kRoundingOffset);
    *multiplier_int16 = result;
    TFLITE_DCHECK_EQ(*multiplier_int16, result);
}

template <typename T>
bool hardSwishQuant(const T* inputData, const Shape& inputShape, T* outputData,
                    const Shape& outputShape) {
    tflite::HardSwishParams params;
    params.input_zero_point = inputShape.offset;
    params.output_zero_point = outputShape.offset;
    const float input_scale = inputShape.scale;
    const float hires_input_scale = (1.0f / 128.0f) * input_scale;
    const float reluish_scale = 3.0f / 32768.0f;
    const float output_scale = outputShape.scale;

    const float output_multiplier = hires_input_scale / output_scale;

    int32_t output_multiplier_fixedpoint_int32;
    NN_RET_CHECK(QuantizeMultiplier(output_multiplier, &output_multiplier_fixedpoint_int32,
                                    &params.output_multiplier_exponent));
    DownScaleInt32ToInt16Multiplier(output_multiplier_fixedpoint_int32,
                                    &params.output_multiplier_fixedpoint_int16);
    NN_RET_CHECK(params.output_multiplier_exponent <= 0);

    const float reluish_multiplier = hires_input_scale / reluish_scale;
    int32_t reluish_multiplier_fixedpoint_int32;
    NN_RET_CHECK(QuantizeMultiplier(reluish_multiplier, &reluish_multiplier_fixedpoint_int32,
                                    &params.reluish_multiplier_exponent));
    DownScaleInt32ToInt16Multiplier(reluish_multiplier_fixedpoint_int32,
                                    &params.reluish_multiplier_fixedpoint_int16);

    tflite::reference_ops::HardSwish(params, convertShapeToTflshape(inputShape), inputData,
                                     convertShapeToTflshape(outputShape), outputData);
    return true;
}

}  // namespace

bool validate(OperationType opType, const IOperationValidationContext* context) {
    NN_RET_CHECK_EQ(context->getNumInputs(), kNumInputs);
    NN_RET_CHECK_EQ(context->getNumOutputs(), kNumOutputs);
    auto inputType = context->getInputType(kInputTensor);
    if (inputType == OperandType::TENSOR_FLOAT32) {
        NN_RET_CHECK(validateHalVersion(context, HalVersion::V1_0));
    } else if (inputType == OperandType::TENSOR_FLOAT16) {
        NN_RET_CHECK(validateHalVersion(context, HalVersion::V1_2));
    } else if (inputType == OperandType::TENSOR_QUANT8_ASYMM) {
        if (opType == OperationType::TANH) {
            NN_RET_CHECK(validateHalVersion(context, HalVersion::V1_2));
        } else {
            NN_RET_CHECK(validateHalVersion(context, HalVersion::V1_0));
        }
    } else if (inputType == OperandType::TENSOR_QUANT8_ASYMM_SIGNED) {
        NN_RET_CHECK(validateHalVersion(context, HalVersion::V1_3));
    } else {
        NN_RET_CHECK_FAIL() << "Unsupported tensor type for operation " << getOperationName(opType);
    }
    const Shape& input = context->getInputShape(kInputTensor);
    if (hasKnownRank(input)) {
        NN_RET_CHECK_LE(getNumberOfDimensions(input), 4);
    }
    return validateInputTypes(context, {inputType}) && validateOutputTypes(context, {inputType});
}

bool validateHardSwish(const IOperationValidationContext* context) {
    NN_RET_CHECK_EQ(context->getNumInputs(), kNumInputs);
    NN_RET_CHECK_EQ(context->getNumOutputs(), kNumOutputs);
    auto inputType = context->getInputType(kInputTensor);
    if (inputType == OperandType::TENSOR_FLOAT16 || inputType == OperandType::TENSOR_FLOAT32 ||
        inputType == OperandType::TENSOR_QUANT8_ASYMM ||
        inputType == OperandType::TENSOR_QUANT8_ASYMM_SIGNED) {
        NN_RET_CHECK(validateHalVersion(context, HalVersion::V1_3));
    } else {
        NN_RET_CHECK_FAIL() << "Unsupported tensor type for operation ELU";
    }
    return validateInputTypes(context, {inputType}) && validateOutputTypes(context, {inputType});
}

bool prepare(OperationType opType, IOperationExecutionContext* context) {
    Shape input = context->getInputShape(kInputTensor);
    if (opType != OperationType::HARD_SWISH) {
        NN_RET_CHECK_LE(getNumberOfDimensions(input), 4);
    }
    Shape output = input;
    if (input.type == OperandType::TENSOR_QUANT8_ASYMM ||
        input.type == OperandType::TENSOR_QUANT8_ASYMM_SIGNED) {
        bool isSigned = input.type == OperandType::TENSOR_QUANT8_ASYMM_SIGNED;
        switch (opType) {
            case OperationType::HARD_SWISH: {
                auto outputShape = context->getOutputShape(kOutputTensor);
                output.scale = outputShape.scale;
                output.offset = outputShape.offset;
            } break;
            case OperationType::RELU:
            case OperationType::RELU1:
            case OperationType::RELU6:
                break;
            case OperationType::LOGISTIC:
                output.scale = 1.f / 256;
                output.offset = isSigned ? -128 : 0;
                break;
            case OperationType::TANH:
                output.scale = 1.f / 128;
                output.offset = isSigned ? 0 : 128;
                break;
            default:
                NN_RET_CHECK_FAIL() << "Unsupported operation type";
        }
    }
    return context->setOutputShape(kOutputTensor, output);
}

bool executeRelu(IOperationExecutionContext* context) {
    // Bypass execution in the case of zero-sized input.
    if (getNumberOfElements(context->getOutputShape(kOutputTensor)) == 0) return true;
    switch (context->getInputType(kInputTensor)) {
        case OperandType::TENSOR_FLOAT16:
            return reluFloat(context->getInputBuffer<_Float16>(kInputTensor),
                             context->getInputShape(kInputTensor),
                             context->getOutputBuffer<_Float16>(kOutputTensor),
                             context->getOutputShape(kOutputTensor));
        case OperandType::TENSOR_FLOAT32:
            return reluFloat(context->getInputBuffer<float>(kInputTensor),
                             context->getInputShape(kInputTensor),
                             context->getOutputBuffer<float>(kOutputTensor),
                             context->getOutputShape(kOutputTensor));
        case OperandType::TENSOR_QUANT8_ASYMM:
            return reluQuant8(context->getInputBuffer<uint8_t>(kInputTensor),
                              context->getInputShape(kInputTensor),
                              context->getOutputBuffer<uint8_t>(kOutputTensor),
                              context->getOutputShape(kOutputTensor));
        case OperandType::TENSOR_QUANT8_ASYMM_SIGNED:
            return reluQuant8Signed(context->getInputBuffer<int8_t>(kInputTensor),
                                    context->getInputShape(kInputTensor),
                                    context->getOutputBuffer<int8_t>(kOutputTensor),
                                    context->getOutputShape(kOutputTensor));
        default:
            NN_RET_CHECK_FAIL() << "Unsupported tensor type for operation RELU";
    }
}

bool executeRelu1(IOperationExecutionContext* context) {
    // Bypass execution in the case of zero-sized input.
    if (getNumberOfElements(context->getOutputShape(kOutputTensor)) == 0) return true;
    switch (context->getInputType(kInputTensor)) {
        case OperandType::TENSOR_FLOAT16:
            return relu1Float(context->getInputBuffer<_Float16>(kInputTensor),
                              context->getInputShape(kInputTensor),
                              context->getOutputBuffer<_Float16>(kOutputTensor),
                              context->getOutputShape(kOutputTensor));
        case OperandType::TENSOR_FLOAT32:
            return relu1Float(context->getInputBuffer<float>(kInputTensor),
                              context->getInputShape(kInputTensor),
                              context->getOutputBuffer<float>(kOutputTensor),
                              context->getOutputShape(kOutputTensor));
        case OperandType::TENSOR_QUANT8_ASYMM:
            return relu1Quant8(context->getInputBuffer<uint8_t>(kInputTensor),
                               context->getInputShape(kInputTensor),
                               context->getOutputBuffer<uint8_t>(kOutputTensor),
                               context->getOutputShape(kOutputTensor));
        case OperandType::TENSOR_QUANT8_ASYMM_SIGNED:
            return relu1Quant8Signed(context->getInputBuffer<int8_t>(kInputTensor),
                                     context->getInputShape(kInputTensor),
                                     context->getOutputBuffer<int8_t>(kOutputTensor),
                                     context->getOutputShape(kOutputTensor));
        default:
            NN_RET_CHECK_FAIL() << "Unsupported tensor type for operation RELU1";
    }
}

bool executeRelu6(IOperationExecutionContext* context) {
    // Bypass execution in the case of zero-sized input.
    if (getNumberOfElements(context->getOutputShape(kOutputTensor)) == 0) return true;
    switch (context->getInputType(kInputTensor)) {
        case OperandType::TENSOR_FLOAT16:
            return relu6Float(context->getInputBuffer<_Float16>(kInputTensor),
                              context->getInputShape(kInputTensor),
                              context->getOutputBuffer<_Float16>(kOutputTensor),
                              context->getOutputShape(kOutputTensor));
        case OperandType::TENSOR_FLOAT32:
            return relu6Float(context->getInputBuffer<float>(kInputTensor),
                              context->getInputShape(kInputTensor),
                              context->getOutputBuffer<float>(kOutputTensor),
                              context->getOutputShape(kOutputTensor));
        case OperandType::TENSOR_QUANT8_ASYMM:
            return relu6Quant8(context->getInputBuffer<uint8_t>(kInputTensor),
                               context->getInputShape(kInputTensor),
                               context->getOutputBuffer<uint8_t>(kOutputTensor),
                               context->getOutputShape(kOutputTensor));
        case OperandType::TENSOR_QUANT8_ASYMM_SIGNED:
            return relu6Quant8Signed(context->getInputBuffer<int8_t>(kInputTensor),
                                     context->getInputShape(kInputTensor),
                                     context->getOutputBuffer<int8_t>(kOutputTensor),
                                     context->getOutputShape(kOutputTensor));
        default:
            NN_RET_CHECK_FAIL() << "Unsupported tensor type for operation RELU6";
    }
}

bool executeLogistic(IOperationExecutionContext* context) {
    // Bypass execution in the case of zero-sized input.
    if (getNumberOfElements(context->getOutputShape(kOutputTensor)) == 0) return true;
    switch (context->getInputType(kInputTensor)) {
        case OperandType::TENSOR_FLOAT16:
            return logisticFloat(context->getInputBuffer<_Float16>(kInputTensor),
                                 context->getInputShape(kInputTensor),
                                 context->getOutputBuffer<_Float16>(kOutputTensor),
                                 context->getOutputShape(kOutputTensor));
        case OperandType::TENSOR_FLOAT32:
            return logisticFloat(context->getInputBuffer<float>(kInputTensor),
                                 context->getInputShape(kInputTensor),
                                 context->getOutputBuffer<float>(kOutputTensor),
                                 context->getOutputShape(kOutputTensor));
        case OperandType::TENSOR_QUANT8_ASYMM:
            return logisticQuant8(context->getInputBuffer<uint8_t>(kInputTensor),
                                  context->getInputShape(kInputTensor),
                                  context->getOutputBuffer<uint8_t>(kOutputTensor),
                                  context->getOutputShape(kOutputTensor));
        case OperandType::TENSOR_QUANT8_ASYMM_SIGNED:
            return logisticQuant8Signed(context->getInputBuffer<int8_t>(kInputTensor),
                                        context->getInputShape(kInputTensor),
                                        context->getOutputBuffer<int8_t>(kOutputTensor),
                                        context->getOutputShape(kOutputTensor));
        default:
            NN_RET_CHECK_FAIL() << "Unsupported tensor type for operation LOGISTIC";
    }
}

bool executeTanh(IOperationExecutionContext* context) {
    // Bypass execution in the case of zero-sized input.
    if (getNumberOfElements(context->getOutputShape(kOutputTensor)) == 0) return true;
    switch (context->getInputType(kInputTensor)) {
        case OperandType::TENSOR_FLOAT16:
            return tanhFloat16(context->getInputBuffer<_Float16>(kInputTensor),
                               context->getInputShape(kInputTensor),
                               context->getOutputBuffer<_Float16>(kOutputTensor),
                               context->getOutputShape(kOutputTensor));
        case OperandType::TENSOR_FLOAT32:
            return tanhFloat32(context->getInputBuffer<float>(kInputTensor),
                               context->getInputShape(kInputTensor),
                               context->getOutputBuffer<float>(kOutputTensor),
                               context->getOutputShape(kOutputTensor));
        case OperandType::TENSOR_QUANT8_ASYMM:
            return tanhQuant8(context->getInputBuffer<uint8_t>(kInputTensor),
                              context->getInputShape(kInputTensor),
                              context->getOutputBuffer<uint8_t>(kOutputTensor),
                              context->getOutputShape(kOutputTensor));
        case OperandType::TENSOR_QUANT8_ASYMM_SIGNED:
            return tanhQuant8Signed(context->getInputBuffer<int8_t>(kInputTensor),
                                    context->getInputShape(kInputTensor),
                                    context->getOutputBuffer<int8_t>(kOutputTensor),
                                    context->getOutputShape(kOutputTensor));
        default:
            NN_RET_CHECK_FAIL() << "Unsupported tensor type for operation TANH";
    }
}

bool executeHardSwish(IOperationExecutionContext* context) {
    // Bypass execution in the case of zero-sized input.
    if (getNumberOfElements(context->getOutputShape(kOutputTensor)) == 0) return true;
    switch (context->getInputType(kInputTensor)) {
        case OperandType::TENSOR_FLOAT16: {
            const Shape& inputShape = context->getInputShape(kInputTensor);
            const Shape& outputShape = context->getOutputShape(kOutputTensor);
            std::vector<float> inputFloat(getNumberOfElements(inputShape));
            std::vector<float> outputFloat(getNumberOfElements(outputShape));
            convertFloat16ToFloat32(context->getInputBuffer<_Float16>(kInputTensor), &inputFloat);
            tflite::reference_ops::HardSwish(convertShapeToTflshape(inputShape), inputFloat.data(),
                                             convertShapeToTflshape(outputShape),
                                             outputFloat.data());
            convertFloat32ToFloat16(outputFloat, context->getOutputBuffer<_Float16>(kOutputTensor));
            return true;
        }
        case OperandType::TENSOR_FLOAT32: {
            tflite::reference_ops::HardSwish(
                    convertShapeToTflshape(context->getInputShape(kInputTensor)),
                    context->getInputBuffer<float>(kInputTensor),
                    convertShapeToTflshape(context->getOutputShape(kOutputTensor)),
                    context->getOutputBuffer<float>(kOutputTensor));
            return true;
        }
        case OperandType::TENSOR_QUANT8_ASYMM:
            return hardSwishQuant(context->getInputBuffer<uint8_t>(kInputTensor),
                                  context->getInputShape(kInputTensor),
                                  context->getOutputBuffer<uint8_t>(kOutputTensor),
                                  context->getOutputShape(kOutputTensor));
        case OperandType::TENSOR_QUANT8_ASYMM_SIGNED:
            return hardSwishQuant(context->getInputBuffer<int8_t>(kInputTensor),
                                  context->getInputShape(kInputTensor),
                                  context->getOutputBuffer<int8_t>(kOutputTensor),
                                  context->getOutputShape(kOutputTensor));
        default:
            NN_RET_CHECK_FAIL() << "Unsupported tensor type for operation TANH";
    }
}

}  // namespace activation

using std::placeholders::_1;
NN_REGISTER_OPERATION(RELU, "RELU", std::bind(activation::validate, OperationType::RELU, _1),
                      std::bind(activation::prepare, OperationType::RELU, _1),
                      activation::executeRelu, .allowZeroSizedInput = true);
NN_REGISTER_OPERATION(RELU1, "RELU1", std::bind(activation::validate, OperationType::RELU1, _1),
                      std::bind(activation::prepare, OperationType::RELU1, _1),
                      activation::executeRelu1, .allowZeroSizedInput = true);
NN_REGISTER_OPERATION(RELU6, "RELU6", std::bind(activation::validate, OperationType::RELU6, _1),
                      std::bind(activation::prepare, OperationType::RELU6, _1),
                      activation::executeRelu6, .allowZeroSizedInput = true);
NN_REGISTER_OPERATION(LOGISTIC, "LOGISTIC",
                      std::bind(activation::validate, OperationType::LOGISTIC, _1),
                      std::bind(activation::prepare, OperationType::LOGISTIC, _1),
                      activation::executeLogistic, .allowZeroSizedInput = true);
NN_REGISTER_OPERATION(TANH, "TANH", std::bind(activation::validate, OperationType::TANH, _1),
                      std::bind(activation::prepare, OperationType::TANH, _1),
                      activation::executeTanh, .allowZeroSizedInput = true);
NN_REGISTER_OPERATION(HARD_SWISH, "HARD_SWISH", activation::validateHardSwish,
                      std::bind(activation::prepare, OperationType::HARD_SWISH, _1),
                      activation::executeHardSwish, .allowZeroSizedInput = true);

}  // namespace nn
}  // namespace android
