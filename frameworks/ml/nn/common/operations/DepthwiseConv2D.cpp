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

#include <tensorflow/lite/kernels/internal/optimized/depthwiseconv_uint8.h>
#include <tensorflow/lite/kernels/internal/reference/depthwiseconv_float.h>

#include <algorithm>
#include <vector>

#include "CpuOperationUtils.h"
#include "OperationResolver.h"
#include "Operations.h"
#include "Tracing.h"

namespace android {
namespace nn {
namespace depthwise_conv_2d {
constexpr char kOperationName[] = "DEPTHWISE_CONV_2D";

constexpr uint32_t kNumInputsArray[] = {8, 9, 11, 12, 14};
constexpr uint32_t kInputTensor = 0;
constexpr uint32_t kFilterTensor = 1;
constexpr uint32_t kBiasTensor = 2;

constexpr uint32_t kNumOutputs = 1;
constexpr uint32_t kOutputTensor = 0;

namespace {

using namespace hal;

struct DepthwiseConv2dParam {
    int32_t padding_left, padding_right;
    int32_t padding_top, padding_bottom;
    int32_t stride_width, stride_height;
    int32_t dilation_width_factor = 1, dilation_height_factor = 1;
    int32_t depth_multiplier;
    int32_t activation;
    bool useNchw = false;

    bool initialize(const IOperationExecutionContext* context) {
        uint32_t inCount = context->getNumInputs();
        int32_t padding_implicit = 0;
        bool useImplicitPadding = false;
        if ((inCount >= 9 && context->getInputType(8) == OperandType::BOOL) || inCount == 8) {
            padding_implicit = context->getInputValue<int32_t>(3);
            stride_width = context->getInputValue<int32_t>(4);
            stride_height = context->getInputValue<int32_t>(5);
            depth_multiplier = context->getInputValue<int32_t>(6);
            activation = context->getInputValue<int32_t>(7);
            if (inCount >= 9) {
                useNchw = context->getInputValue<bool>(8);
            }
            if (inCount == 11) {
                dilation_width_factor = context->getInputValue<int32_t>(9);
                dilation_height_factor = context->getInputValue<int32_t>(10);
            }
            useImplicitPadding = true;
        } else if (inCount >= 11 && context->getInputType(8) == OperandType::INT32) {
            padding_left = context->getInputValue<int32_t>(3);
            padding_right = context->getInputValue<int32_t>(4);
            padding_top = context->getInputValue<int32_t>(5);
            padding_bottom = context->getInputValue<int32_t>(6);
            stride_width = context->getInputValue<int32_t>(7);
            stride_height = context->getInputValue<int32_t>(8);
            depth_multiplier = context->getInputValue<int32_t>(9);
            activation = context->getInputValue<int32_t>(10);
            if (inCount >= 12) {
                useNchw = context->getInputValue<bool>(11);
            }
            if (inCount == 14) {
                dilation_width_factor = context->getInputValue<int32_t>(12);
                dilation_height_factor = context->getInputValue<int32_t>(13);
            }
        } else {
            NN_RET_CHECK_FAIL() << "Unsupported input spec for operation " << kOperationName;
        }
        if (useImplicitPadding) {
            Shape inputShape = context->getInputShape(kInputTensor);
            Shape filterShape = context->getInputShape(kFilterTensor);
            int32_t input_width = getSizeOfDimension(inputShape, useNchw ? 3 : 2);
            int32_t input_height = getSizeOfDimension(inputShape, useNchw ? 2 : 1);
            int32_t filter_width = getSizeOfDimension(filterShape, 2);
            int32_t filter_height = getSizeOfDimension(filterShape, 1);
            calculateExplicitPadding(input_width, stride_width, dilation_width_factor, filter_width,
                                     padding_implicit, &padding_left, &padding_right);
            calculateExplicitPadding(input_height, stride_height, dilation_height_factor,
                                     filter_height, padding_implicit, &padding_top,
                                     &padding_bottom);
        }
        NN_RET_CHECK_GE(padding_left, 0);
        NN_RET_CHECK_GE(padding_right, 0);
        NN_RET_CHECK_GE(padding_top, 0);
        NN_RET_CHECK_GE(padding_bottom, 0);
        NN_RET_CHECK_GT(stride_width, 0);
        NN_RET_CHECK_GT(stride_height, 0);
        NN_RET_CHECK_GT(dilation_width_factor, 0);
        NN_RET_CHECK_GT(dilation_height_factor, 0);
        NN_RET_CHECK_GT(depth_multiplier, 0);
        NN_RET_CHECK_GE(activation, 0);
        return true;
    }
};

#define ANDROID_NN_DEPTHWISE_CONV_PARAMETERS                    \
    uint32_t height = getSizeOfDimension(inputShape, 1);        \
    uint32_t width = getSizeOfDimension(inputShape, 2);         \
    uint32_t filterHeight = getSizeOfDimension(filterShape, 1); \
    uint32_t filterWidth = getSizeOfDimension(filterShape, 2);  \
    uint32_t outHeight = getSizeOfDimension(outputShape, 1);    \
    uint32_t outWidth = getSizeOfDimension(outputShape, 2);     \
                                                                \
    uint32_t paddingHeight = (uint32_t)paddingTop;              \
    uint32_t paddingWidth = (uint32_t)paddingLeft;

bool depthwiseConvNhwc(const float* inputData, const Shape& inputShape, const float* filterData,
                       const Shape& filterShape, const float* biasData, const Shape& biasShape,
                       int32_t paddingLeft, int32_t paddingRight, int32_t paddingTop,
                       int32_t paddingBottom, int32_t strideWidth, int32_t strideHeight,
                       int32_t dilationWidthFactor, int32_t dilationHeightFactor,
                       int32_t depthMultiplier, int32_t activation, float* outputData,
                       const Shape& outputShape) {
    NNTRACE_TRANS("depthwiseConvFloat32");

    ANDROID_NN_DEPTHWISE_CONV_PARAMETERS

    float output_activation_min, output_activation_max;
    CalculateActivationRangeFloat(activation, &output_activation_min, &output_activation_max);

    tflite::DepthwiseParams params{
            .padding_values = {static_cast<int16>(paddingWidth), static_cast<int16>(paddingHeight),
                               0 /*width_offset*/, 0 /*height_offset*/},
            .stride_width = static_cast<int16>(strideWidth),
            .stride_height = static_cast<int16>(strideHeight),
            .dilation_width_factor = static_cast<int16>(dilationWidthFactor),
            .dilation_height_factor = static_cast<int16>(dilationHeightFactor),
            .depth_multiplier = static_cast<int16>(depthMultiplier),
            .float_activation_min = output_activation_min,
            .float_activation_max = output_activation_max,
    };
    NNTRACE_COMP_SWITCH("optimized_ops::DepthwiseConv");
    tflite::reference_ops::DepthwiseConv(params, convertShapeToTflshape(inputShape), inputData,
                                         convertShapeToTflshape(filterShape), filterData,
                                         convertShapeToTflshape(biasShape), biasData,
                                         convertShapeToTflshape(outputShape), outputData);

    return true;
}

bool depthwiseConvNhwc(const _Float16* inputData, const Shape& inputShape,
                       const _Float16* filterData, const Shape& filterShape,
                       const _Float16* biasData, const Shape& biasShape, int32_t paddingLeft,
                       int32_t paddingRight, int32_t paddingTop, int32_t paddingBottom,
                       int32_t strideWidth, int32_t strideHeight, int32_t dilationWidthFactor,
                       int32_t dilationHeightFactor, int32_t depthMultiplier, int32_t activation,
                       _Float16* outputData, const Shape& outputShape) {
    NNTRACE_TRANS("depthwiseConvFloat16");
    std::vector<float> inputDataFloat32(getNumberOfElements(inputShape));
    convertFloat16ToFloat32(inputData, &inputDataFloat32);
    std::vector<float> filterDataFloat32(getNumberOfElements(filterShape));
    convertFloat16ToFloat32(filterData, &filterDataFloat32);
    std::vector<float> biasDataFloat32(getNumberOfElements(biasShape));
    convertFloat16ToFloat32(biasData, &biasDataFloat32);

    std::vector<float> outputDataFloat32(getNumberOfElements(outputShape));
    depthwiseConvNhwc(inputDataFloat32.data(), inputShape, filterDataFloat32.data(), filterShape,
                      biasDataFloat32.data(), biasShape, paddingLeft, paddingRight, paddingTop,
                      paddingBottom, strideWidth, strideHeight, dilationWidthFactor,
                      dilationHeightFactor, depthMultiplier, activation, outputDataFloat32.data(),
                      outputShape);

    convertFloat32ToFloat16(outputDataFloat32, outputData);
    return true;
}

bool depthwiseConvNhwc(const uint8_t* inputData, const Shape& inputShape, const uint8_t* filterData,
                       const Shape& filterShape, const int32_t* biasData, const Shape& biasShape,
                       int32_t paddingLeft, int32_t paddingRight, int32_t paddingTop,
                       int32_t paddingBottom, int32_t strideWidth, int32_t strideHeight,
                       int32_t dilationWidthFactor, int32_t dilationHeightFactor,
                       int32_t depthMultiplier, int32_t activation, uint8_t* outputData,
                       const Shape& outputShape) {
    NNTRACE_TRANS("depthwiseConvQuant8");

    ANDROID_NN_DEPTHWISE_CONV_PARAMETERS

    double real_multiplier = 0.0;
    int32_t output_multiplier = 0;
    int32_t output_shift = 0;
    int32_t output_activation_min = 0;
    int32_t output_activation_max = 0;

    NN_RET_CHECK(GetQuantizedConvolutionMultipler(inputShape, filterShape, biasShape, outputShape,
                                                  &real_multiplier));
    int exponent;
    NN_RET_CHECK(QuantizeMultiplier(real_multiplier, &output_multiplier, &exponent));
    output_shift = -exponent;
    CalculateActivationRangeUint8(activation, outputShape, &output_activation_min,
                                  &output_activation_max);

    tflite::DepthwiseParams params{
            .padding_values = {static_cast<int16>(paddingWidth), static_cast<int16>(paddingHeight),
                               0 /*width_offset*/, 0 /*height_offset*/},
            .stride_width = static_cast<int16>(strideWidth),
            .stride_height = static_cast<int16>(strideHeight),
            .dilation_width_factor = static_cast<int16>(dilationWidthFactor),
            .dilation_height_factor = static_cast<int16>(dilationHeightFactor),
            .depth_multiplier = static_cast<int16>(depthMultiplier),
            .input_offset = -inputShape.offset,
            .weights_offset = -filterShape.offset,
            .output_offset = outputShape.offset,
            .output_multiplier = output_multiplier,
            .output_shift = -output_shift,
            .quantized_activation_min = output_activation_min,
            .quantized_activation_max = output_activation_max,
    };
    NNTRACE_COMP_SWITCH("optimized_ops::DepthwiseConv");
    tflite::reference_ops::DepthwiseConv(params, convertShapeToTflshape(inputShape), inputData,
                                         convertShapeToTflshape(filterShape), filterData,
                                         convertShapeToTflshape(biasShape), biasData,
                                         convertShapeToTflshape(outputShape), outputData);
    return true;
}

// Passing input, filter and output shapes by value, so that we can change the
// offsets without modifying the actual shapes.
bool depthwiseConvNhwc(const int8_t* inputData, Shape inputShape, const int8_t* filterData,
                       Shape filterShape, const int32_t* biasData, const Shape& biasShape,
                       int32_t paddingLeft, int32_t paddingRight, int32_t paddingTop,
                       int32_t paddingBottom, int32_t strideWidth, int32_t strideHeight,
                       int32_t dilationWidthFactor, int32_t dilationHeightFactor,
                       int32_t depthMultiplier, int32_t activation, int8_t* outputData,
                       Shape outputShape) {
    NNTRACE_TRANS("depthwiseConvQuant8");

    std::vector<uint8_t> unsignedInput(getNumberOfElements(inputShape));
    convertInt8ToUInt8(inputData, &unsignedInput);
    inputShape.offset += 128;

    std::vector<uint8_t> unsignedFilter(getNumberOfElements(filterShape));
    convertInt8ToUInt8(filterData, &unsignedFilter);
    filterShape.offset += 128;

    std::vector<uint8_t> unsignedOutput(getNumberOfElements(outputShape));
    outputShape.offset += 128;

    NN_RET_CHECK(depthwiseConvNhwc(unsignedInput.data(), inputShape, unsignedFilter.data(),
                                   filterShape, biasData, biasShape, paddingLeft, paddingRight,
                                   paddingTop, paddingBottom, strideWidth, strideHeight,
                                   dilationWidthFactor, dilationHeightFactor, depthMultiplier,
                                   activation, unsignedOutput.data(), outputShape));

    convertUInt8ToInt8(unsignedOutput, outputData);

    return true;
}

template <typename T>
bool depthwiseConvQuant8PerChannelNhwc(
        const T* inputData, const Shape& inputShape, const int8_t* filterData,
        const Shape& filterShape, const float* filterScales, const int32_t* biasData,
        const Shape& biasShape, int32_t paddingLeft, int32_t paddingRight, int32_t paddingTop,
        int32_t paddingBottom, int32_t strideWidth, int32_t strideHeight,
        int32_t dilationWidthFactor, int32_t dilationHeightFactor,

        int32_t depthMultiplier, int32_t activation, T* outputData, const Shape& outputShape) {
    NNTRACE_TRANS("depthwiseConvQuant8");

    uint32_t paddingHeight = (uint32_t)paddingTop;
    uint32_t paddingWidth = (uint32_t)paddingLeft;

    uint32_t numBatches = getSizeOfDimension(inputShape, 0);
    uint32_t inputHeight = getSizeOfDimension(inputShape, 1);
    uint32_t inputWidth = getSizeOfDimension(inputShape, 2);
    uint32_t inputDepth = getSizeOfDimension(inputShape, 3);
    uint32_t filterHeight = getSizeOfDimension(filterShape, 1);
    uint32_t filterWidth = getSizeOfDimension(filterShape, 2);
    uint32_t filterDepth = getSizeOfDimension(filterShape, 3);
    uint32_t outputHeight = getSizeOfDimension(outputShape, 1);
    uint32_t outputWidth = getSizeOfDimension(outputShape, 2);
    uint32_t outputDepth = getSizeOfDimension(outputShape, 3);

    int32_t inputOffset = -inputShape.offset;
    int32_t outputOffset = outputShape.offset;

    auto realMultiplier = std::vector<double>(outputDepth, .0f);
    auto outputMultiplier = std::vector<int32_t>(outputDepth, 0);
    auto outputShift = std::vector<int32_t>(outputDepth, .0f);

    for (int i = 0; i < outputDepth; ++i) {
        Shape filterChannelShape = filterShape;
        filterChannelShape.scale = filterScales[i];
        Shape biasChannelShape = biasShape;
        biasChannelShape.scale = filterScales[i] * inputShape.scale;
        NN_RET_CHECK(GetQuantizedConvolutionMultipler(
                inputShape, filterChannelShape, biasChannelShape, outputShape, &realMultiplier[i]));
        int exponent;
        NN_RET_CHECK(QuantizeMultiplier(realMultiplier[i], &outputMultiplier[i], &exponent));
        outputShift[i] = -exponent;
    }

    int32_t output_activation_min = 0, output_activation_max = 0;
    CalculateActivationRange<T>(activation, outputShape, &output_activation_min,
                                &output_activation_max);

    const T* inputBase = inputData;
    T* outPtr = outputData;
    for (uint32_t b = 0; b < numBatches; b++) {
        for (uint32_t h = 0; h < outputHeight; h++) {
            for (uint32_t w = 0; w < outputWidth; w++) {
                for (uint32_t ic = 0; ic < inputDepth; ic++) {
                    for (uint32_t m = 0; m < depthMultiplier; m++) {
                        int32_t wInputOrigin = static_cast<int32_t>(w) * strideWidth - paddingLeft;
                        int32_t hInputOrigin = static_cast<int32_t>(h) * strideHeight - paddingTop;
                        const int oc = m + ic * depthMultiplier;

                        int32_t sum = 0.0f;
                        for (uint32_t i = 0; i < filterHeight; i++) {
                            for (uint32_t j = 0; j < filterWidth; j++) {
                                int32_t hInput = hInputOrigin +
                                                 dilationHeightFactor * static_cast<int32_t>(i);
                                int32_t wInput = wInputOrigin +
                                                 dilationWidthFactor * static_cast<int32_t>(j);

                                if (hInput >= 0 && hInput < static_cast<int32_t>(inputHeight) &&
                                    wInput >= 0 && wInput < static_cast<int32_t>(inputWidth)) {
                                    uint32_t filterIndex =
                                            i * filterWidth * filterDepth + j * filterDepth + oc;
                                    uint32_t inputIndex = hInput * inputWidth * inputDepth +
                                                          wInput * inputDepth + ic;
                                    sum += (static_cast<int32_t>(filterData[filterIndex])) *
                                           (static_cast<int32_t>(inputBase[inputIndex]) +
                                            inputOffset);
                                }
                            }
                        }

                        sum += biasData[oc];
                        sum = tflite::MultiplyByQuantizedMultiplier(sum, outputMultiplier[oc],
                                                                    -outputShift[oc]);
                        sum += outputOffset;
                        sum = std::max(std::min(sum, output_activation_max), output_activation_min);
                        outPtr[m] = static_cast<T>(sum);
                    }
                    outPtr += depthMultiplier;
                }
            }
        }
        inputBase += inputHeight * inputWidth * inputDepth;
    }

    return true;
}

template <typename T_Input, typename T_Filter, typename T_Bias>
bool depthwiseConv(const T_Input* inputData, const Shape& inputShape, const T_Filter* filterData,
                   const Shape& filterShape, const T_Bias* biasData, const Shape& biasShape,
                   int32_t paddingLeft, int32_t paddingRight, int32_t paddingTop,
                   int32_t paddingBottom, int32_t strideWidth, int32_t strideHeight,
                   int32_t dilationWidthFactor, int32_t dilationHeightFactor,
                   int32_t depthMultiplier, int32_t activation, bool useNchw, T_Input* outputData,
                   const Shape& outputShape) {
    InputWithLayout<T_Input> input(useNchw);
    OutputWithLayout<T_Input> output(useNchw);
    NN_RET_CHECK(input.initialize(inputData, inputShape));
    NN_RET_CHECK(output.initialize(outputData, outputShape));
    NN_RET_CHECK(depthwiseConvNhwc(input.getNhwcBuffer(), input.getNhwcShape(), filterData,
                                   filterShape, biasData, biasShape, paddingLeft, paddingRight,
                                   paddingTop, paddingBottom, strideWidth, strideHeight,
                                   dilationWidthFactor, dilationHeightFactor, depthMultiplier,
                                   activation, output.getNhwcBuffer(), output.getNhwcShape()));
    NN_RET_CHECK(output.commit());
    return true;
}

template <typename T>
bool depthwiseConvQuant8PerChannel(const T* inputData, const Shape& inputShape,
                                   const int8_t* filterData, const Shape& filterShape,
                                   const float* filterScales, const int32_t* biasData,
                                   const Shape& biasShape, int32_t paddingLeft,
                                   int32_t paddingRight, int32_t paddingTop, int32_t paddingBottom,
                                   int32_t strideWidth, int32_t strideHeight,
                                   int32_t dilationWidthFactor, int32_t dilationHeightFactor,
                                   int32_t depthMultiplier, int32_t activation, bool useNchw,
                                   T* outputData, const Shape& outputShape) {
    InputWithLayout<T> input(useNchw);
    OutputWithLayout<T> output(useNchw);
    NN_RET_CHECK(input.initialize(inputData, inputShape));
    NN_RET_CHECK(output.initialize(outputData, outputShape));
    NN_RET_CHECK(depthwiseConvQuant8PerChannelNhwc(
            input.getNhwcBuffer(), input.getNhwcShape(), filterData, filterShape, filterScales,
            biasData, biasShape, paddingLeft, paddingRight, paddingTop, paddingBottom, strideWidth,
            strideHeight, dilationWidthFactor, dilationHeightFactor, depthMultiplier, activation,
            output.getNhwcBuffer(), output.getNhwcShape()));
    NN_RET_CHECK(output.commit());
    return true;
}

#undef ANDROID_NN_DEPTHWISE_CONV_PARAMETERS

}  // namespace

bool validate(const IOperationValidationContext* context) {
    const uint32_t numInputs = context->getNumInputs();
    NN_RET_CHECK(
            std::binary_search(std::begin(kNumInputsArray), std::end(kNumInputsArray), numInputs));
    NN_RET_CHECK_EQ(context->getNumOutputs(), kNumOutputs);
    auto inputType = context->getInputType(kInputTensor);
    auto filterType = context->getInputType(kFilterTensor);
    std::vector<OperandType> inExpectedTypes;
    if (inputType == OperandType::TENSOR_FLOAT32) {
        inExpectedTypes = {
                OperandType::TENSOR_FLOAT32, OperandType::TENSOR_FLOAT32,
                OperandType::TENSOR_FLOAT32, OperandType::INT32,
                OperandType::INT32,          OperandType::INT32,
                OperandType::INT32,          OperandType::INT32,
        };
    } else if (inputType == OperandType::TENSOR_FLOAT16) {
        inExpectedTypes = {
                OperandType::TENSOR_FLOAT16, OperandType::TENSOR_FLOAT16,
                OperandType::TENSOR_FLOAT16, OperandType::INT32,
                OperandType::INT32,          OperandType::INT32,
                OperandType::INT32,          OperandType::INT32,
        };
    } else if (inputType == OperandType::TENSOR_QUANT8_ASYMM ||
               inputType == OperandType::TENSOR_QUANT8_ASYMM_SIGNED) {
        NN_RET_CHECK(filterType == OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL ||
                     filterType == inputType)
                << "Unsupported filter tensor type for operation " << kOperationName;
        if (filterType == OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL) {
            NN_RET_CHECK_EQ(context->getInputExtraParams(kFilterTensor).channelQuant().channelDim,
                            3)
                    << "Unsupported filter tensor channel dimension for operation "
                    << kOperationName;
        }
        inExpectedTypes = {
                inputType,          filterType,         OperandType::TENSOR_INT32,
                OperandType::INT32, OperandType::INT32, OperandType::INT32,
                OperandType::INT32, OperandType::INT32,
        };
    } else {
        NN_RET_CHECK_FAIL() << "Unsupported input tensor type for operation " << kOperationName;
    }

    // NeuralNetworks.h specifies that ANEURALNETWORKS_DEPTHWISE_CONV_2D's output must
    // meet "outputScale > inputScale * filterScale" for the operand type
    // ANEURALNETWORKS_TENSOR_QUANT8_ASYMM before API level 29. For other
    // operand types (e.g., ANEURALNETWORKS_TENSOR_FLOAT32), this constraint
    // does not apply, so by default the constraint is met.
    bool meetsQuantizedScaleConstraintBeforeV1_2 = true;
    if (inputType == OperandType::TENSOR_QUANT8_ASYMM) {
        const float inputScale = context->getInputShape(kInputTensor).scale;
        const float filterScale = context->getInputShape(kFilterTensor).scale;
        const float outputScale = context->getInputShape(kOutputTensor).scale;
        meetsQuantizedScaleConstraintBeforeV1_2 = (outputScale > inputScale * filterScale);
    }

    bool withExplicitPadding = false;
    bool withLayout = false;
    bool withDilation = false;
    if (numInputs >= 9) {
        if (context->getInputType(8) == OperandType::INT32 && numInputs >= 11) {
            std::vector<OperandType> explicitScalarTypes(3, OperandType::INT32);
            inExpectedTypes.insert(inExpectedTypes.end(), explicitScalarTypes.begin(),
                                   explicitScalarTypes.end());
            withExplicitPadding = true;
        }
        int inputOffset = withExplicitPadding ? 3 : 0;
        if (numInputs >= 9 + inputOffset) {
            inExpectedTypes.push_back(OperandType::BOOL);
            withLayout = true;
        }
        NN_RET_CHECK_NE(numInputs, 10 + inputOffset)
                << "Provided only one dilation factor value, two values are required for operation "
                << kOperationName;
        if (numInputs == 11 + inputOffset) {
            inExpectedTypes.push_back(OperandType::INT32);
            inExpectedTypes.push_back(OperandType::INT32);
            withDilation = true;
        }
    }

    if (inputType == OperandType::TENSOR_QUANT8_ASYMM_SIGNED) {
        NN_RET_CHECK(validateHalVersion(context, HalVersion::V1_3));
    } else if (inputType == OperandType::TENSOR_FLOAT16 ||
               filterType == OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL || withLayout ||
               withDilation || !meetsQuantizedScaleConstraintBeforeV1_2) {
        NN_RET_CHECK(validateHalVersion(context, HalVersion::V1_2));
    } else {
        NN_RET_CHECK(validateHalVersion(context, HalVersion::V1_0));
    }
    return validateInputTypes(context, inExpectedTypes) &&
           validateOutputTypes(context, {inputType});
}

bool prepare(IOperationExecutionContext* context) {
    Shape input = context->getInputShape(kInputTensor);
    Shape filter = context->getInputShape(kFilterTensor);
    Shape bias = context->getInputShape(kBiasTensor);

    if (filter.type == OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        NN_RET_CHECK(input.type == OperandType::TENSOR_QUANT8_ASYMM ||
                     input.type == OperandType::TENSOR_QUANT8_ASYMM_SIGNED);
    } else {
        NN_RET_CHECK(input.type == filter.type);
    }
    if (input.type == OperandType::TENSOR_QUANT8_ASYMM ||
        input.type == OperandType::TENSOR_QUANT8_ASYMM_SIGNED) {
        NN_RET_CHECK(bias.type == OperandType::TENSOR_INT32);
    } else {
        NN_RET_CHECK(input.type == bias.type);
    }
    NN_RET_CHECK_EQ(getNumberOfDimensions(input), 4);
    NN_RET_CHECK_EQ(getNumberOfDimensions(filter), 4);
    NN_RET_CHECK_EQ(getNumberOfDimensions(bias), 1);
    NN_RET_CHECK_EQ(getSizeOfDimension(filter, 0), 1);
    NN_RET_CHECK_EQ(getSizeOfDimension(filter, 3), getSizeOfDimension(bias, 0));

    DepthwiseConv2dParam param;
    NN_RET_CHECK(param.initialize(context));

    uint32_t batches = getSizeOfDimension(input, 0);
    uint32_t height = getSizeOfDimension(input, param.useNchw ? 2 : 1);
    uint32_t width = getSizeOfDimension(input, param.useNchw ? 3 : 2);
    uint32_t channels_in = getSizeOfDimension(input, param.useNchw ? 1 : 3);
    uint32_t channels_out = getSizeOfDimension(filter, 3);
    uint32_t filterHeight = getSizeOfDimension(filter, 1);
    uint32_t filterWidth = getSizeOfDimension(filter, 2);

    NN_OPS_CHECK(param.depth_multiplier * channels_in == channels_out);
    int32_t effectiveFilterWidth = (filterWidth - 1) * param.dilation_width_factor + 1;
    int32_t effectiveFilterHeight = (filterHeight - 1) * param.dilation_height_factor + 1;
    NN_RET_CHECK_GT(effectiveFilterWidth, param.padding_left);
    NN_RET_CHECK_GT(effectiveFilterWidth, param.padding_right);
    NN_RET_CHECK_GT(effectiveFilterHeight, param.padding_top);
    NN_RET_CHECK_GT(effectiveFilterHeight, param.padding_bottom);

    uint32_t outHeight =
            computeOutSize(height, filterHeight, param.stride_height, param.dilation_height_factor,
                           param.padding_top, param.padding_bottom);
    uint32_t outWidth =
            computeOutSize(width, filterWidth, param.stride_width, param.dilation_width_factor,
                           param.padding_left, param.padding_right);

    Shape output = context->getOutputShape(kOutputTensor);
    output.type = input.type;
    if (param.useNchw) {
        output.dimensions = {batches, channels_out, outHeight, outWidth};
    } else {
        output.dimensions = {batches, outHeight, outWidth, channels_out};
    }
    return context->setOutputShape(kOutputTensor, output);
}

bool execute(IOperationExecutionContext* context) {
    // Bypass execution in the case of zero-sized input.
    if (getNumberOfElements(context->getOutputShape(kOutputTensor)) == 0) return true;
    DepthwiseConv2dParam param;
    NN_RET_CHECK(param.initialize(context));
    switch (context->getInputType(kInputTensor)) {
        case OperandType::TENSOR_FLOAT32:
            return depthwiseConv(context->getInputBuffer<float>(kInputTensor),
                                 context->getInputShape(kInputTensor),
                                 context->getInputBuffer<float>(kFilterTensor),
                                 context->getInputShape(kFilterTensor),
                                 context->getInputBuffer<float>(kBiasTensor),
                                 context->getInputShape(kBiasTensor), param.padding_left,
                                 param.padding_right, param.padding_top, param.padding_bottom,
                                 param.stride_width, param.stride_height,
                                 param.dilation_width_factor, param.dilation_height_factor,
                                 param.depth_multiplier, param.activation, param.useNchw,
                                 context->getOutputBuffer<float>(kOutputTensor),
                                 context->getOutputShape(kOutputTensor));
        case OperandType::TENSOR_FLOAT16:
            return depthwiseConv(context->getInputBuffer<_Float16>(kInputTensor),
                                 context->getInputShape(kInputTensor),
                                 context->getInputBuffer<_Float16>(kFilterTensor),
                                 context->getInputShape(kFilterTensor),
                                 context->getInputBuffer<_Float16>(kBiasTensor),
                                 context->getInputShape(kBiasTensor), param.padding_left,
                                 param.padding_right, param.padding_top, param.padding_bottom,
                                 param.stride_width, param.stride_height,
                                 param.dilation_width_factor, param.dilation_height_factor,
                                 param.depth_multiplier, param.activation, param.useNchw,
                                 context->getOutputBuffer<_Float16>(kOutputTensor),
                                 context->getOutputShape(kOutputTensor));
        case OperandType::TENSOR_QUANT8_ASYMM:
            if (context->getInputType(kFilterTensor) ==
                OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL) {
                return depthwiseConvQuant8PerChannel(
                        context->getInputBuffer<uint8_t>(kInputTensor),
                        context->getInputShape(kInputTensor),
                        context->getInputBuffer<int8_t>(kFilterTensor),
                        context->getInputShape(kFilterTensor),
                        context->getInputExtraParams(kFilterTensor).channelQuant().scales.data(),
                        context->getInputBuffer<int32_t>(kBiasTensor),
                        context->getInputShape(kBiasTensor), param.padding_left,
                        param.padding_right, param.padding_top, param.padding_bottom,
                        param.stride_width, param.stride_height, param.dilation_width_factor,
                        param.dilation_height_factor, param.depth_multiplier, param.activation,
                        param.useNchw, context->getOutputBuffer<uint8_t>(kOutputTensor),
                        context->getOutputShape(kOutputTensor));
            } else if (context->getInputType(kFilterTensor) == OperandType::TENSOR_QUANT8_ASYMM) {
                return depthwiseConv(context->getInputBuffer<uint8_t>(kInputTensor),
                                     context->getInputShape(kInputTensor),
                                     context->getInputBuffer<uint8_t>(kFilterTensor),
                                     context->getInputShape(kFilterTensor),
                                     context->getInputBuffer<int32_t>(kBiasTensor),
                                     context->getInputShape(kBiasTensor), param.padding_left,
                                     param.padding_right, param.padding_top, param.padding_bottom,
                                     param.stride_width, param.stride_height,
                                     param.dilation_width_factor, param.dilation_height_factor,
                                     param.depth_multiplier, param.activation, param.useNchw,
                                     context->getOutputBuffer<uint8_t>(kOutputTensor),
                                     context->getOutputShape(kOutputTensor));
            } else {
                NN_RET_CHECK_FAIL() << "Unsupported filter type for operation " << kOperationName;
            }
        case OperandType::TENSOR_QUANT8_ASYMM_SIGNED:
            if (context->getInputType(kFilterTensor) ==
                OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL) {
                return depthwiseConvQuant8PerChannel(
                        context->getInputBuffer<int8_t>(kInputTensor),
                        context->getInputShape(kInputTensor),
                        context->getInputBuffer<int8_t>(kFilterTensor),
                        context->getInputShape(kFilterTensor),
                        context->getInputExtraParams(kFilterTensor).channelQuant().scales.data(),
                        context->getInputBuffer<int32_t>(kBiasTensor),
                        context->getInputShape(kBiasTensor), param.padding_left,
                        param.padding_right, param.padding_top, param.padding_bottom,
                        param.stride_width, param.stride_height, param.dilation_width_factor,
                        param.dilation_height_factor, param.depth_multiplier, param.activation,
                        param.useNchw, context->getOutputBuffer<int8_t>(kOutputTensor),
                        context->getOutputShape(kOutputTensor));
            } else if (context->getInputType(kFilterTensor) ==
                       OperandType::TENSOR_QUANT8_ASYMM_SIGNED) {
                return depthwiseConv(context->getInputBuffer<int8_t>(kInputTensor),
                                     context->getInputShape(kInputTensor),
                                     context->getInputBuffer<int8_t>(kFilterTensor),
                                     context->getInputShape(kFilterTensor),
                                     context->getInputBuffer<int32_t>(kBiasTensor),
                                     context->getInputShape(kBiasTensor), param.padding_left,
                                     param.padding_right, param.padding_top, param.padding_bottom,
                                     param.stride_width, param.stride_height,
                                     param.dilation_width_factor, param.dilation_height_factor,
                                     param.depth_multiplier, param.activation, param.useNchw,
                                     context->getOutputBuffer<int8_t>(kOutputTensor),
                                     context->getOutputShape(kOutputTensor));
            } else {
                NN_RET_CHECK_FAIL() << "Unsupported filter type for operation " << kOperationName;
            }
        default:
            NN_RET_CHECK_FAIL() << "Unsupported tensor type for operation " << kOperationName;
    }
}

}  // namespace depthwise_conv_2d

NN_REGISTER_OPERATION(DEPTHWISE_CONV_2D, depthwise_conv_2d::kOperationName,
                      depthwise_conv_2d::validate, depthwise_conv_2d::prepare,
                      depthwise_conv_2d::execute, .allowZeroSizedInput = true);

}  // namespace nn
}  // namespace android
