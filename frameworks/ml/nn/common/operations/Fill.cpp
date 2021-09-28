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

#include "OperationsUtils.h"
#define LOG_TAG "Operations"

#include "HalInterfaces.h"
#include "OperationResolver.h"

namespace android {
namespace nn {
namespace fill_op {

constexpr uint32_t kNumInputs = 2;
constexpr uint32_t kDimsTensor = 0;
constexpr uint32_t kValueScalar = 1;

constexpr uint32_t kNumOutputs = 1;
constexpr uint32_t kOutputTensor = 0;

namespace {

using namespace hal;

template <typename T>
bool executeTyped(IOperationExecutionContext* context) {
    T* output = context->getOutputBuffer<T>(kOutputTensor);
    const int numElements = getNumberOfElements(context->getOutputShape(kOutputTensor));
    const T value = context->getInputValue<T>(kValueScalar);
    for (int i = 0; i < numElements; ++i) {
        output[i] = value;
    }
    return true;
}

bool getValueType(OperandType outputType, OperandType* valueType) {
    switch (outputType) {
        case OperandType::TENSOR_FLOAT16:
            *valueType = OperandType::FLOAT16;
            return true;
        case OperandType::TENSOR_FLOAT32:
            *valueType = OperandType::FLOAT32;
            return true;
        case OperandType::TENSOR_INT32:
            *valueType = OperandType::INT32;
            return true;
        default:
            NN_RET_CHECK_FAIL() << "Unsupported value type for fill op: " << toString(outputType);
    }
}

}  // namespace

bool validate(const IOperationValidationContext* context) {
    NN_RET_CHECK_EQ(context->getNumInputs(), kNumInputs);
    NN_RET_CHECK_EQ(context->getNumOutputs(), kNumOutputs);
    // Check output type first because input value type is dependent on the
    // output type.
    OperandType outputType = context->getOutputType(kOutputTensor);
    NN_RET_CHECK(outputType == OperandType::TENSOR_FLOAT16 ||
                 outputType == OperandType::TENSOR_FLOAT32 ||
                 outputType == OperandType::TENSOR_INT32)
            << "Unsupported output type for fill op: " << toString(outputType);
    NN_RET_CHECK(validateOutputTypes(context, {outputType}));

    OperandType valueType;
    NN_RET_CHECK(getValueType(outputType, &valueType));
    NN_RET_CHECK(validateInputTypes(context, {OperandType::TENSOR_INT32, valueType}));

    return validateHalVersion(context, HalVersion::V1_3);
}

bool prepare(IOperationExecutionContext* context) {
    Shape dimsShape = context->getInputShape(kDimsTensor);
    NN_RET_CHECK_EQ(getNumberOfDimensions(dimsShape), 1);

    Shape outputShape = context->getOutputShape(kOutputTensor);
    outputShape.dimensions.resize(dimsShape.dimensions[0]);
    const int32_t* dims = context->getInputBuffer<int32_t>(kDimsTensor);
    for (int i = 0; i < dimsShape.dimensions[0]; ++i) {
        outputShape.dimensions[i] = dims[i];
    }
    return context->setOutputShape(kOutputTensor, outputShape);
}

bool execute(IOperationExecutionContext* context) {
    switch (context->getInputType(kValueScalar)) {
        case OperandType::FLOAT16:
            return executeTyped<_Float16>(context);
        case OperandType::FLOAT32:
            return executeTyped<float>(context);
        case OperandType::INT32:
            return executeTyped<int32_t>(context);
        default:
            NN_RET_CHECK_FAIL() << "Unsupported value type for fill op.";
    }
}

}  // namespace fill_op

NN_REGISTER_OPERATION(FILL, "FILL", fill_op::validate, fill_op::prepare, fill_op::execute);

}  // namespace nn
}  // namespace android
