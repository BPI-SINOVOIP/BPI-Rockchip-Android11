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

// Contains the implementation of the operations.

#define LOG_TAG "Operations"

#include <tensorflow/lite/kernels/internal/reference/legacy_reference_ops.h>

#include <vector>

#include "CpuOperationUtils.h"
#include "HalInterfaces.h"
#include "OperationResolver.h"
#include "Operations.h"
#include "Tracing.h"

namespace android {
namespace nn {
namespace strided_slice {

constexpr uint32_t kNumInputs = 7;
constexpr uint32_t kInputTensor = 0;
constexpr uint32_t kBeginTensor = 1;
constexpr uint32_t kEndTensor = 2;
constexpr uint32_t kStridesTensor = 3;
constexpr uint32_t kBeginMask = 4;
constexpr uint32_t kEndMask = 5;
constexpr uint32_t kShrinkAxisMask = 6;

constexpr uint32_t kNumOutputs = 1;
constexpr uint32_t kOutputTensor = 0;

namespace {

using namespace hal;

template <typename T>
bool compute(const T* inputData, const Shape& inputShape, const int32_t* beginData,
             const int32_t* endData, const int32_t* stridesData, int32_t beginMask, int32_t endMask,
             int32_t shrinkAxisMask, T* outputData, const Shape& outputShape) {
    NNTRACE_TRANS("stridedSlice");
    // This Op only supports 1-4D cases and since we use the reference 4D
    // implementation, the 1-3D tensors are mapped to 4D.
    const int kMaxDim = 4;

    std::vector<int> starts;
    std::vector<int> stops;
    std::vector<int> strides;

    int32_t numInputDims = static_cast<int32_t>(getNumberOfDimensions(inputShape));
    for (int32_t idx = numInputDims - 1; idx >= 0; --idx) {
        starts.emplace_back(beginData[idx]);
        stops.emplace_back(endData[idx]);
        strides.emplace_back(stridesData[idx]);
    }

    for (int i = numInputDims; i < kMaxDim; i++) {
        starts.emplace_back(0);
        stops.emplace_back(1);
        strides.emplace_back(1);
    }

    beginMask = ReverseMaskBits(beginMask, numInputDims);
    endMask = ReverseMaskBits(endMask, numInputDims);
    shrinkAxisMask = ReverseMaskBits(shrinkAxisMask, numInputDims);

    tflite::reference_ops::StridedSlice(inputData, convertShapeToDims(inputShape), beginMask,
                                        endMask, shrinkAxisMask, starts, stops, strides, outputData,
                                        convertShapeToDims(outputShape));

    return true;
}

template <typename T>
bool executeTyped(IOperationExecutionContext* context) {
    return compute<T>(
            context->getInputBuffer<T>(kInputTensor), context->getInputShape(kInputTensor),
            context->getInputBuffer<int32_t>(kBeginTensor),
            context->getInputBuffer<int32_t>(kEndTensor),
            context->getInputBuffer<int32_t>(kStridesTensor),
            context->getInputValue<int32_t>(kBeginMask), context->getInputValue<int32_t>(kEndMask),
            context->getInputValue<int32_t>(kShrinkAxisMask),
            context->getOutputBuffer<T>(kOutputTensor), context->getOutputShape(kOutputTensor));
}

}  // namespace

bool validate(const IOperationValidationContext* context) {
    NN_RET_CHECK_EQ(context->getNumInputs(), kNumInputs);
    NN_RET_CHECK_EQ(context->getNumOutputs(), kNumOutputs);
    OperandType inputType = context->getInputType(kInputTensor);
    NN_RET_CHECK(inputType == OperandType::TENSOR_FLOAT16 ||
                 inputType == OperandType::TENSOR_FLOAT32 ||
                 inputType == OperandType::TENSOR_QUANT8_ASYMM ||
                 inputType == OperandType::TENSOR_QUANT8_ASYMM_SIGNED)
            << "Unsupported input operand type for STRIDED_SLICE op: " << toString(inputType);

    HalVersion minSupportedHalVersion;
    if (inputType == OperandType::TENSOR_QUANT8_ASYMM_SIGNED) {
        minSupportedHalVersion = HalVersion::V1_3;
    } else if (inputType == OperandType::TENSOR_FLOAT16) {
        minSupportedHalVersion = HalVersion::V1_2;
    } else {
        minSupportedHalVersion = HalVersion::V1_1;
    }

    NN_RET_CHECK(validateInputTypes(context, {
                                                     inputType,
                                                     OperandType::TENSOR_INT32,
                                                     OperandType::TENSOR_INT32,
                                                     OperandType::TENSOR_INT32,
                                                     OperandType::INT32,
                                                     OperandType::INT32,
                                                     OperandType::INT32,
                                             }));
    NN_RET_CHECK(validateOutputTypes(context, {inputType}));
    const Shape& input = context->getInputShape(kInputTensor);
    if (hasKnownRank(input)) {
        NN_RET_CHECK_LE(getNumberOfDimensions(input), 4);
    }
    return validateHalVersion(context, minSupportedHalVersion);
}

bool prepare(IOperationExecutionContext* context) {
    // StridedSlice op only supports 1D-4D input arrays.
    const Shape& inputShape = context->getInputShape(kInputTensor);
    uint32_t numInputDims = getNumberOfDimensions(inputShape);
    NN_OPS_CHECK(numInputDims <= 4);

    const Shape& beginShape = context->getInputShape(kBeginTensor);
    const Shape& endShape = context->getInputShape(kEndTensor);
    const Shape& stridesShape = context->getInputShape(kStridesTensor);

    NN_OPS_CHECK(getNumberOfDimensions(beginShape) == 1);
    NN_OPS_CHECK(getNumberOfDimensions(endShape) == 1);
    NN_OPS_CHECK(getNumberOfDimensions(stridesShape) == 1);

    NN_OPS_CHECK(getSizeOfDimension(beginShape, 0) == numInputDims);
    NN_OPS_CHECK(getSizeOfDimension(endShape, 0) == numInputDims);
    NN_OPS_CHECK(getSizeOfDimension(stridesShape, 0) == numInputDims);

    NN_OPS_CHECK(beginShape.type == OperandType::TENSOR_INT32);
    NN_OPS_CHECK(endShape.type == OperandType::TENSOR_INT32);
    NN_OPS_CHECK(stridesShape.type == OperandType::TENSOR_INT32);

    const int32_t* beginData = context->getInputBuffer<int32_t>(kBeginTensor);
    const int32_t* endData = context->getInputBuffer<int32_t>(kEndTensor);
    const int32_t* stridesData = context->getInputBuffer<int32_t>(kStridesTensor);

    const int32_t beginMask = context->getInputValue<int32_t>(kBeginMask);
    const int32_t endMask = context->getInputValue<int32_t>(kEndMask);
    const int32_t shrinkAxisMask = context->getInputValue<int32_t>(kShrinkAxisMask);

    // Determine size of output tensor and map indices
    std::vector<uint32_t> outDims;
    for (int32_t idx = 0; idx < static_cast<int32_t>(numInputDims); idx++) {
        int32_t dim = static_cast<int32_t>(getSizeOfDimension(inputShape, idx));
        int32_t stride = stridesData[idx];
        // stride value has to be non-zero
        NN_OPS_CHECK(stride != 0);
        bool positiveStride = stride > 0;

        int32_t begin = beginMask & (1 << idx) ? positiveStride ? 0 : dim - 1
                                               : ClampedIndex(beginData[idx], dim, positiveStride);
        int32_t end = endMask & (1 << idx) ? positiveStride ? dim : -1
                                           : ClampedIndex(endData[idx], dim, positiveStride);

        // This is valid for both positive and negative strides
        int32_t outDim = ceil((end - begin) / static_cast<float>(stride));
        outDim = outDim < 0 ? 0 : static_cast<uint32_t>(outDim);
        if (!(shrinkAxisMask & (1 << idx))) {
            outDims.push_back(outDim);
        } else {
            // Only positive stride is allowed on non-range indexing (i.e. shrinkMask is set).
            NN_RET_CHECK_GT(stride, 0) << "index = " << idx;
            NN_RET_CHECK_EQ(outDim, 1) << "index = " << idx;
        }
    }

    // Handle the case when all dimensions are removed
    if (outDims.empty()) {
        outDims.push_back(1);
    }

    Shape outputShape = context->getOutputShape(kOutputTensor);
    NN_RET_CHECK(SetShape(inputShape, &outputShape));
    outputShape.dimensions = outDims;
    return context->setOutputShape(kOutputTensor, outputShape);
}

bool execute(IOperationExecutionContext* context) {
    switch (context->getInputType(kInputTensor)) {
        case OperandType::TENSOR_FLOAT16:
            return executeTyped<_Float16>(context);
        case OperandType::TENSOR_FLOAT32:
            return executeTyped<float>(context);
        case OperandType::TENSOR_QUANT8_ASYMM:
            return executeTyped<uint8_t>(context);
        case OperandType::TENSOR_QUANT8_ASYMM_SIGNED:
            return executeTyped<int8_t>(context);
        default:
            NN_RET_CHECK_FAIL() << "Unsupported tensor type for STRIDED_SLICE op.";
    }
}
}  // namespace strided_slice

NN_REGISTER_OPERATION(STRIDED_SLICE, "STRIDED_SLICE", strided_slice::validate,
                      strided_slice::prepare, strided_slice::execute);

}  // namespace nn
}  // namespace android
