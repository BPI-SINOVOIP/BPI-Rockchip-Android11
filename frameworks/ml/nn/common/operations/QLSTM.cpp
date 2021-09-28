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

#include <algorithm>
#include <memory>
#include <vector>

#include "CpuExecutor.h"
#include "OperationsUtils.h"
#include "QuantUtils.h"

namespace android {
namespace nn {
namespace qlstm {

namespace {

// Inputs
constexpr uint32_t kNumInputs = 32;

constexpr uint32_t kInputTensor = 0;
// Input weight tensors of size: [numUnits, inputSize].
constexpr uint32_t kInputToInputWeightsTensor = 1;
constexpr uint32_t kInputToForgetWeightsTensor = 2;
constexpr uint32_t kInputToCellWeightsTensor = 3;
constexpr uint32_t kInputToOutputWeightsTensor = 4;

// Recurrent weight tensors of size [numUnits, outputSize].
constexpr uint32_t kRecurrentToInputWeightsTensor = 5;
constexpr uint32_t kRecurrentToForgetWeightsTensor = 6;
constexpr uint32_t kRecurrentToCellWeightsTensor = 7;
constexpr uint32_t kRecurrentToOutputWeightsTensor = 8;

// For peephole (optional).
// Cell to input/forget/output weights of size [numUnits].
constexpr uint32_t kCellToInputWeightsTensor = 9;
constexpr uint32_t kCellToForgetWeightsTensor = 10;
constexpr uint32_t kCellToOutputWeightsTensor = 11;

// Gates bias tensors of size [numUnits].
constexpr uint32_t kInputGateBiasTensor = 12;
constexpr uint32_t kForgetGateBiasTensor = 13;
constexpr uint32_t kCellGateBiasTensor = 14;
constexpr uint32_t kOutputGateBiasTensor = 15;

// Projection weight tensor of size [outputSize, numUnits].
constexpr uint32_t kProjectionWeightsTensor = 16;
// Projection bias tensor of size [outputSize].
constexpr uint32_t kProjectionBiasTensor = 17;

// Output from the previous time step, as tensor
// of size [numBatches, outputSize].
constexpr uint32_t kPrevOutputTensor = 18;

// Cell state from the previous time step, as tensor
// of size [numBatches, numUnits].
constexpr uint32_t kPrevCellStateTensor = 19;

// Layer normalization tensors of size [numUnits].
constexpr uint32_t kInputLayerNormTensor = 20;
constexpr uint32_t kForgetLayerNormTensor = 21;
constexpr uint32_t kCellLayerNormTensor = 22;
constexpr uint32_t kOutputLayerNormTensor = 23;

// Clipping.
constexpr uint32_t kCellClip = 24;
constexpr uint32_t kProjectionClip = 25;

// Scales of the result of matmul, i.e. input to layer normalization.
constexpr uint32_t kInputIntermediateScale = 26;
constexpr uint32_t kForgetIntermediateScale = 27;
constexpr uint32_t kCellIntermediateScale = 28;
constexpr uint32_t kOutputIntermediateScale = 29;

// Zero point and scale of hidden state.
constexpr uint32_t kHiddenStateZeroPoint = 30;
constexpr uint32_t kHiddenStateScale = 31;

// Outputs:
constexpr uint32_t kNumOutputs = 3;
constexpr uint32_t kOutputStateOutTensor = 0;
constexpr uint32_t kCellStateOutTensor = 1;
constexpr uint32_t kOutputTensor = 2;

inline bool hasTensor(IOperationExecutionContext* context, const uint32_t tensor) {
    return context->getInputBuffer(tensor) != nullptr;
}

}  // namespace

using hal::OperandType;

bool validate(const IOperationValidationContext* context) {
    NN_RET_CHECK_EQ(context->getNumInputs(), kNumInputs);
    NN_RET_CHECK_EQ(context->getNumOutputs(), kNumOutputs);

    std::vector<OperandType> inExpectedTypes;
    // Input.
    inExpectedTypes.push_back(OperandType::TENSOR_QUANT8_ASYMM_SIGNED);
    // Input-to-* and recurrent-to-* weights.
    for (int i = 0; i < 8; ++i) {
        inExpectedTypes.push_back(OperandType::TENSOR_QUANT8_SYMM);
    }
    // Cell-to-* weights.
    for (int i = 0; i < 3; ++i) {
        inExpectedTypes.push_back(OperandType::TENSOR_QUANT16_SYMM);
    }
    // Gate biases.
    for (int i = 0; i < 4; ++i) {
        inExpectedTypes.push_back(OperandType::TENSOR_INT32);
    }
    // Projection.
    inExpectedTypes.push_back(OperandType::TENSOR_QUANT8_SYMM);
    inExpectedTypes.push_back(OperandType::TENSOR_INT32);
    // Previous output.
    inExpectedTypes.push_back(OperandType::TENSOR_QUANT8_ASYMM_SIGNED);
    // Previous cell state.
    inExpectedTypes.push_back(OperandType::TENSOR_QUANT16_SYMM);
    // Layer norm weights
    for (int i = 0; i < 4; ++i) {
        inExpectedTypes.push_back(OperandType::TENSOR_QUANT16_SYMM);
    }
    // Cell/projection clipping and scales of intermediate results at the 4 gates.
    for (int i = 0; i < 6; ++i) {
        inExpectedTypes.push_back(OperandType::FLOAT32);
    }
    // Zero point and scale of the hidden state.
    inExpectedTypes.push_back(OperandType::INT32);
    inExpectedTypes.push_back(OperandType::FLOAT32);
    NN_RET_CHECK(validateInputTypes(context, inExpectedTypes));

    std::vector<OperandType> outExpectedTypes;
    // Output state (out).
    outExpectedTypes.push_back(OperandType::TENSOR_QUANT8_ASYMM_SIGNED);
    // Cell state (out).
    outExpectedTypes.push_back(OperandType::TENSOR_QUANT16_SYMM);
    // Output.
    outExpectedTypes.push_back(OperandType::TENSOR_QUANT8_ASYMM_SIGNED);
    NN_RET_CHECK(validateOutputTypes(context, outExpectedTypes));

    return validateHalVersion(context, HalVersion::V1_3);
}

bool prepare(IOperationExecutionContext* context) {
    // Check that none of the required inputs are omitted
    const std::vector<int> requiredTensorInputs = {
            kInputTensor,
            kInputToForgetWeightsTensor,
            kInputToCellWeightsTensor,
            kInputToOutputWeightsTensor,
            kRecurrentToForgetWeightsTensor,
            kRecurrentToCellWeightsTensor,
            kRecurrentToOutputWeightsTensor,
            kForgetGateBiasTensor,
            kCellGateBiasTensor,
            kOutputGateBiasTensor,
            kPrevOutputTensor,
            kPrevCellStateTensor,
    };
    for (const int tensor : requiredTensorInputs) {
        NN_RET_CHECK(!context->isOmittedInput(tensor))
                << "required input " << tensor << " is omitted";
    }

    const Shape inputShape = context->getInputShape(kInputTensor);
    const uint32_t inputRank = getNumberOfDimensions(inputShape);
    NN_RET_CHECK_EQ(inputRank, 2) << "Invalid input tensor rank: " << inputRank;

    const uint32_t batchSize = getSizeOfDimension(inputShape, 0);
    const uint32_t inputSize = getSizeOfDimension(inputShape, 1);

    const Shape inputToOutputShape = context->getInputShape(kInputToOutputWeightsTensor);
    NN_RET_CHECK_EQ(getNumberOfDimensions(inputToOutputShape), 2);
    NN_RET_CHECK_EQ(getSizeOfDimension(inputToOutputShape, 1), inputSize);
    const uint32_t numUnits = getSizeOfDimension(inputToOutputShape, 0);

    const Shape recurrentToOutputShape = context->getInputShape(kRecurrentToOutputWeightsTensor);
    NN_RET_CHECK_EQ(getNumberOfDimensions(recurrentToOutputShape), 2);
    NN_RET_CHECK_EQ(getSizeOfDimension(recurrentToOutputShape, 0), numUnits);
    const uint32_t outputSize = getSizeOfDimension(recurrentToOutputShape, 1);

    if (hasTensor(context, kInputToInputWeightsTensor)) {
        const Shape inputToInputShape = context->getInputShape(kInputToInputWeightsTensor);
        NN_RET_CHECK_EQ(getNumberOfDimensions(inputToInputShape), 2);
        NN_RET_CHECK_EQ(getSizeOfDimension(inputToInputShape, 0), numUnits);
        NN_RET_CHECK_EQ(getSizeOfDimension(inputToInputShape, 1), inputSize);
    }

    const Shape inputToForgetShape = context->getInputShape(kInputToForgetWeightsTensor);
    NN_RET_CHECK_EQ(getNumberOfDimensions(inputToForgetShape), 2);
    NN_RET_CHECK_EQ(getSizeOfDimension(inputToForgetShape, 0), numUnits);
    NN_RET_CHECK_EQ(getSizeOfDimension(inputToForgetShape, 1), inputSize);
    const Shape inputToCellShape = context->getInputShape(kInputToCellWeightsTensor);
    NN_RET_CHECK_EQ(getNumberOfDimensions(inputToCellShape), 2);
    NN_RET_CHECK_EQ(getSizeOfDimension(inputToCellShape, 0), numUnits);
    NN_RET_CHECK_EQ(getSizeOfDimension(inputToCellShape, 1), inputSize);

    if (hasTensor(context, kRecurrentToInputWeightsTensor)) {
        const Shape recurrentToInputShape = context->getInputShape(kRecurrentToInputWeightsTensor);
        NN_RET_CHECK_EQ(getNumberOfDimensions(recurrentToInputShape), 2);
        NN_RET_CHECK_EQ(getSizeOfDimension(recurrentToInputShape, 0), numUnits);
        NN_RET_CHECK_EQ(getSizeOfDimension(recurrentToInputShape, 1), outputSize);
    }

    const Shape recurrentToForgetShape = context->getInputShape(kRecurrentToForgetWeightsTensor);
    NN_RET_CHECK_EQ(getNumberOfDimensions(recurrentToForgetShape), 2);
    NN_RET_CHECK_EQ(getSizeOfDimension(recurrentToForgetShape, 0), numUnits);
    NN_RET_CHECK_EQ(getSizeOfDimension(recurrentToForgetShape, 1), outputSize);
    const Shape recurrentToCellShape = context->getInputShape(kRecurrentToCellWeightsTensor);
    NN_RET_CHECK_EQ(getNumberOfDimensions(recurrentToCellShape), 2);
    NN_RET_CHECK_EQ(getSizeOfDimension(recurrentToCellShape, 0), numUnits);
    NN_RET_CHECK_EQ(getSizeOfDimension(recurrentToCellShape, 1), outputSize);

    // Make sure the input-gate's parameters are either all present (non-CIFG) or
    // not at all (CIFG).
    const bool cifgWeightsAllOrNone = (hasTensor(context, kInputToInputWeightsTensor) &&
                                       hasTensor(context, kRecurrentToInputWeightsTensor)) ||
                                      (!hasTensor(context, kInputToInputWeightsTensor) &&
                                       !hasTensor(context, kRecurrentToInputWeightsTensor));
    NN_RET_CHECK(cifgWeightsAllOrNone);

    if (hasTensor(context, kCellToInputWeightsTensor)) {
        const Shape cellToInputShape = context->getInputShape(kCellToInputWeightsTensor);
        NN_RET_CHECK_EQ(getNumberOfDimensions(cellToInputShape), 1);
        NN_RET_CHECK_EQ(getSizeOfDimension(cellToInputShape, 0), numUnits);
    }

    if (hasTensor(context, kCellToForgetWeightsTensor)) {
        const Shape cellToForgetShape = context->getInputShape(kCellToForgetWeightsTensor);
        NN_RET_CHECK_EQ(getNumberOfDimensions(cellToForgetShape), 1);
        NN_RET_CHECK_EQ(getSizeOfDimension(cellToForgetShape, 0), numUnits);
    }

    if (hasTensor(context, kCellToOutputWeightsTensor)) {
        const Shape cellToOutputShape = context->getInputShape(kCellToOutputWeightsTensor);
        NN_RET_CHECK_EQ(getNumberOfDimensions(cellToOutputShape), 1);
        NN_RET_CHECK_EQ(getSizeOfDimension(cellToOutputShape, 0), numUnits);
    }

    // Making sure the peephole weights are there all or none.
    const bool cifgUsed = !hasTensor(context, kInputToInputWeightsTensor);
    const bool peepholeWeightsAllOrNone =
            ((hasTensor(context, kCellToInputWeightsTensor) || cifgUsed) &&
             hasTensor(context, kCellToForgetWeightsTensor) &&
             hasTensor(context, kCellToOutputWeightsTensor)) ||
            (!hasTensor(context, kCellToInputWeightsTensor) &&
             !hasTensor(context, kCellToForgetWeightsTensor) &&
             !hasTensor(context, kCellToOutputWeightsTensor));
    NN_RET_CHECK(peepholeWeightsAllOrNone);

    if (!cifgUsed) {
        NN_RET_CHECK(hasTensor(context, kInputGateBiasTensor));
        const Shape inputGateBiasShape = context->getInputShape(kInputGateBiasTensor);
        NN_RET_CHECK_EQ(getNumberOfDimensions(inputGateBiasShape), 1);
        NN_RET_CHECK_EQ(getSizeOfDimension(inputGateBiasShape, 0), numUnits);
    } else {
        NN_RET_CHECK(!hasTensor(context, kInputGateBiasTensor))
                << "Input gate bias tensor is present when CIFG is used";
    }

    const Shape forgetGateBiasShape = context->getInputShape(kForgetGateBiasTensor);
    NN_RET_CHECK_EQ(getNumberOfDimensions(forgetGateBiasShape), 1);
    NN_RET_CHECK_EQ(getSizeOfDimension(forgetGateBiasShape, 0), numUnits);
    const Shape cellGateBiasShape = context->getInputShape(kCellGateBiasTensor);
    NN_RET_CHECK_EQ(getNumberOfDimensions(cellGateBiasShape), 1);
    NN_RET_CHECK_EQ(getSizeOfDimension(cellGateBiasShape, 0), numUnits);
    const Shape outputGateBiasShape = context->getInputShape(kOutputGateBiasTensor);
    NN_RET_CHECK_EQ(getNumberOfDimensions(outputGateBiasShape), 1);
    NN_RET_CHECK_EQ(getSizeOfDimension(outputGateBiasShape, 0), numUnits);

    if (hasTensor(context, kProjectionWeightsTensor)) {
        const Shape projectionShape = context->getInputShape(kProjectionWeightsTensor);
        NN_RET_CHECK_EQ(getNumberOfDimensions(projectionShape), 2);
        NN_RET_CHECK_EQ(getSizeOfDimension(projectionShape, 0), outputSize);
        NN_RET_CHECK_EQ(getSizeOfDimension(projectionShape, 1), numUnits);
    }

    if (hasTensor(context, kProjectionBiasTensor)) {
        const Shape projectionBiasShape = context->getInputShape(kProjectionBiasTensor);
        NN_RET_CHECK_EQ(getNumberOfDimensions(projectionBiasShape), 1);
        NN_RET_CHECK_EQ(getSizeOfDimension(projectionBiasShape, 0), outputSize);
    }

    const Shape outputStateShape = context->getInputShape(kPrevOutputTensor);
    NN_RET_CHECK_EQ(getNumberOfDimensions(outputStateShape), 2);
    NN_RET_CHECK_EQ(getSizeOfDimension(outputStateShape, 0), batchSize);
    NN_RET_CHECK_EQ(getSizeOfDimension(outputStateShape, 1), outputSize);
    const Shape cellStateShape = context->getInputShape(kPrevCellStateTensor);
    NN_RET_CHECK_EQ(getNumberOfDimensions(cellStateShape), 2);
    NN_RET_CHECK_EQ(getSizeOfDimension(cellStateShape, 0), batchSize);
    NN_RET_CHECK_EQ(getSizeOfDimension(cellStateShape, 1), numUnits);

    if (hasTensor(context, kInputLayerNormTensor)) {
        const Shape inputLayerNormShape = context->getInputShape(kInputLayerNormTensor);
        NN_RET_CHECK_EQ(getNumberOfDimensions(inputLayerNormShape), 1);
        NN_RET_CHECK_EQ(getSizeOfDimension(inputLayerNormShape, 0), numUnits);
    }

    if (hasTensor(context, kForgetLayerNormTensor)) {
        const Shape forgetLayerNormShape = context->getInputShape(kForgetLayerNormTensor);
        NN_RET_CHECK_EQ(getNumberOfDimensions(forgetLayerNormShape), 1);
        NN_RET_CHECK_EQ(getSizeOfDimension(forgetLayerNormShape, 0), numUnits);
    }

    if (hasTensor(context, kCellLayerNormTensor)) {
        const Shape cellLayerNormShape = context->getInputShape(kCellLayerNormTensor);
        NN_RET_CHECK_EQ(getNumberOfDimensions(cellLayerNormShape), 1);
        NN_RET_CHECK_EQ(getSizeOfDimension(cellLayerNormShape, 0), numUnits);
    }

    if (hasTensor(context, kOutputLayerNormTensor)) {
        const Shape outputLayerNormShape = context->getInputShape(kOutputLayerNormTensor);
        NN_RET_CHECK_EQ(getNumberOfDimensions(outputLayerNormShape), 1);
        NN_RET_CHECK_EQ(getSizeOfDimension(outputLayerNormShape, 0), numUnits);
    }

    if (cifgUsed) {
        NN_RET_CHECK(!hasTensor(context, kInputLayerNormTensor))
                << "Input layer norm weights tensor is present when CIFG is used";
        const bool layerNormWeightsAllOrNoneCifg = (hasTensor(context, kForgetLayerNormTensor) &&
                                                    hasTensor(context, kCellLayerNormTensor) &&
                                                    hasTensor(context, kOutputLayerNormTensor)) ||
                                                   (!hasTensor(context, kForgetLayerNormTensor) &&
                                                    !hasTensor(context, kCellLayerNormTensor) &&
                                                    !hasTensor(context, kOutputLayerNormTensor));
        NN_RET_CHECK(layerNormWeightsAllOrNoneCifg);
    } else {
        const bool layerNormWeightsAllOrNone = (hasTensor(context, kInputLayerNormTensor) &&
                                                hasTensor(context, kForgetLayerNormTensor) &&
                                                hasTensor(context, kCellLayerNormTensor) &&
                                                hasTensor(context, kOutputLayerNormTensor)) ||
                                               (!hasTensor(context, kInputLayerNormTensor) &&
                                                !hasTensor(context, kForgetLayerNormTensor) &&
                                                !hasTensor(context, kCellLayerNormTensor) &&
                                                !hasTensor(context, kOutputLayerNormTensor));
        NN_RET_CHECK(layerNormWeightsAllOrNone);
    }

    const Shape prevOutputShape = context->getInputShape(kPrevOutputTensor);
    Shape outputShape = context->getOutputShape(kOutputTensor);
    outputShape.dimensions = prevOutputShape.dimensions;

    const Shape prevCellStateShape = context->getInputShape(kPrevCellStateTensor);
    Shape cellStateOutShape = context->getOutputShape(kCellStateOutTensor);
    cellStateOutShape.dimensions = prevCellStateShape.dimensions;

    return context->setOutputShape(kOutputStateOutTensor, outputShape) &&
           context->setOutputShape(kCellStateOutTensor, cellStateOutShape) &&
           context->setOutputShape(kOutputTensor, outputShape);
}

bool execute(IOperationExecutionContext* context) {
    // Gets the inputs.
    const Shape inputShape = context->getInputShape(kInputTensor);
    const Shape inputToInputWeightsShape = context->getInputShape(kInputToInputWeightsTensor);
    const Shape recurrentToInputWeightsShape =
            context->getInputShape(kRecurrentToInputWeightsTensor);
    const Shape cellToInputShape = context->getInputShape(kCellToInputWeightsTensor);
    const Shape inputLayerNormShape = context->getInputShape(kInputLayerNormTensor);
    const Shape inputToForgetWeightsShape = context->getInputShape(kInputToForgetWeightsTensor);
    const Shape recurrentToForgetWeightsShape =
            context->getInputShape(kRecurrentToForgetWeightsTensor);
    const Shape cellToForgetShape = context->getInputShape(kCellToForgetWeightsTensor);
    const Shape forgetLayerNormShape = context->getInputShape(kForgetLayerNormTensor);
    const Shape inputToCellWeightsShape = context->getInputShape(kInputToCellWeightsTensor);
    const Shape recurrentToCellWeightsShape = context->getInputShape(kRecurrentToCellWeightsTensor);
    const Shape cellLayerNormShape = context->getInputShape(kCellLayerNormTensor);
    const Shape inputToOutputWeightsShape = context->getInputShape(kInputToOutputWeightsTensor);
    const Shape recurrentToOutputWeightsShape =
            context->getInputShape(kRecurrentToOutputWeightsTensor);
    const Shape cellToOutputShape = context->getInputShape(kCellToOutputWeightsTensor);
    const Shape outputLayerNormShape = context->getInputShape(kOutputLayerNormTensor);
    const Shape projectionWeightsShape = context->getInputShape(kProjectionWeightsTensor);
    const Shape prevOutputShape = context->getInputShape(kPrevOutputTensor);
    const Shape prevCellStateShape = context->getInputShape(kPrevCellStateTensor);

    const uint32_t batchSize = inputShape.dimensions[0];
    const uint32_t inputSize = inputShape.dimensions[1];
    const uint32_t numUnits = inputToOutputWeightsShape.dimensions[0];
    const uint32_t outputSize = recurrentToOutputWeightsShape.dimensions[1];

    const float cellClip = context->getInputValue<float>(kCellClip);
    const float projectionClip = context->getInputValue<float>(kProjectionClip);
    const float inputIntermediateScale = context->getInputValue<float>(kInputIntermediateScale);
    const float forgetIntermediateScale = context->getInputValue<float>(kForgetIntermediateScale);
    const float cellIntermediateScale = context->getInputValue<float>(kCellIntermediateScale);
    const float outputIntermediateScale = context->getInputValue<float>(kOutputIntermediateScale);
    const int8_t hiddenStateZeroPoint = context->getInputValue<int8_t>(kHiddenStateZeroPoint);
    const float hiddenStateScale = context->getInputValue<float>(kHiddenStateScale);

    const int8_t* inputBuffer =
            reinterpret_cast<const int8_t*>(context->getInputBuffer(kInputTensor));

    const int8_t* inputToInputWeightsBuffer =
            reinterpret_cast<const int8_t*>(context->getInputBuffer(kInputToInputWeightsTensor));
    const bool useCifg = (inputToInputWeightsBuffer == nullptr);
    const int8_t* recurrentToInputWeightsBuffer = reinterpret_cast<const int8_t*>(
            context->getInputBuffer(kRecurrentToInputWeightsTensor));
    const int16_t* cellToInputBuffer =
            reinterpret_cast<const int16_t*>(context->getInputBuffer(kCellToInputWeightsTensor));
    const int16_t* inputLayerNormBuffer =
            reinterpret_cast<const int16_t*>(context->getInputBuffer(kInputLayerNormTensor));
    const int32_t* inputBiasBuffer =
            reinterpret_cast<const int32_t*>(context->getInputBuffer(kInputGateBiasTensor));

    const int8_t* inputToForgetWeightsBuffer =
            reinterpret_cast<const int8_t*>(context->getInputBuffer(kInputToForgetWeightsTensor));
    const int8_t* recurrentToForgetWeightsBuffer = reinterpret_cast<const int8_t*>(
            context->getInputBuffer(kRecurrentToForgetWeightsTensor));
    const int16_t* cellToForgetBuffer =
            reinterpret_cast<const int16_t*>(context->getInputBuffer(kCellToForgetWeightsTensor));
    const int16_t* forgetLayerNormBuffer =
            reinterpret_cast<const int16_t*>(context->getInputBuffer(kForgetLayerNormTensor));
    const int32_t* forgetBiasBuffer =
            reinterpret_cast<const int32_t*>(context->getInputBuffer(kForgetGateBiasTensor));

    const int8_t* inputToCellWeightsBuffer =
            reinterpret_cast<const int8_t*>(context->getInputBuffer(kInputToCellWeightsTensor));
    const int8_t* recurrentToCellWeightsBuffer =
            reinterpret_cast<const int8_t*>(context->getInputBuffer(kRecurrentToCellWeightsTensor));
    const int16_t* cellLayerNormBuffer =
            reinterpret_cast<const int16_t*>(context->getInputBuffer(kCellLayerNormTensor));
    const int32_t* cellBiasBuffer =
            reinterpret_cast<const int32_t*>(context->getInputBuffer(kCellGateBiasTensor));

    const int8_t* inputToOutputWeightsBuffer =
            reinterpret_cast<const int8_t*>(context->getInputBuffer(kInputToOutputWeightsTensor));
    const int8_t* recurrentToOutputWeightsBuffer = reinterpret_cast<const int8_t*>(
            context->getInputBuffer(kRecurrentToOutputWeightsTensor));
    const int16_t* cellToOutputBuffer =
            reinterpret_cast<const int16_t*>(context->getInputBuffer(kCellToOutputWeightsTensor));
    const int16_t* outputLayerNormBuffer =
            reinterpret_cast<const int16_t*>(context->getInputBuffer(kOutputLayerNormTensor));
    const int32_t* outputBiasBuffer =
            reinterpret_cast<const int32_t*>(context->getInputBuffer(kOutputGateBiasTensor));

    const int8_t* projectionWeightsBuffer =
            reinterpret_cast<const int8_t*>(context->getInputBuffer(kProjectionWeightsTensor));
    const int32_t* projectionBiasBuffer =
            reinterpret_cast<const int32_t*>(context->getInputBuffer(kProjectionBiasTensor));

    const int8_t* prevOutputBuffer =
            reinterpret_cast<const int8_t*>(context->getInputBuffer(kPrevOutputTensor));
    const int16_t* prevCellStateBuffer =
            reinterpret_cast<const int16_t*>(context->getInputBuffer(kPrevCellStateTensor));

    uint8_t* outputStateBuffer =
            reinterpret_cast<uint8_t*>(context->getOutputBuffer(kOutputStateOutTensor));
    int16_t* cellStateBuffer =
            reinterpret_cast<int16_t*>(context->getOutputBuffer(kCellStateOutTensor));
    int8_t* outputBuffer = reinterpret_cast<int8_t*>(context->getOutputBuffer(kOutputTensor));

    // Calculates and decomposes effective scales.
    // This is for optimizing the matmul calculation.
    int cellShift;
    NN_RET_CHECK(CheckedLog2(prevCellStateShape.scale, &cellShift));
    NN_RET_CHECK(cellShift <= -9);

    int32_t inputToInputEffectiveScaleA;
    int32_t inputToInputEffectiveScaleB;
    int32_t recurrentToInputEffectiveScaleA;
    int32_t recurrentToInputEffectiveScaleB;
    int32_t cellToInputEffectiveScaleA;
    int32_t cellToInputEffectiveScaleB;
    if (!useCifg) {
        const float inputToInputEffectiveScale =
                inputToInputWeightsShape.scale * inputShape.scale / inputIntermediateScale;
        NN_RET_CHECK(QuantizeMultiplier(inputToInputEffectiveScale, &inputToInputEffectiveScaleA,
                                        &inputToInputEffectiveScaleB));
        const float recurrentToInputEffectiveScale =
                recurrentToInputWeightsShape.scale * prevOutputShape.scale / inputIntermediateScale;
        NN_RET_CHECK(QuantizeMultiplier(recurrentToInputEffectiveScale,
                                        &recurrentToInputEffectiveScaleA,
                                        &recurrentToInputEffectiveScaleB));
        if (cellToInputBuffer != nullptr) {
            const float cellToInputEffectiveScale =
                    std::pow(2, cellShift) * cellToInputShape.scale / inputIntermediateScale;
            NN_RET_CHECK(QuantizeMultiplier(cellToInputEffectiveScale, &cellToInputEffectiveScaleA,
                                            &cellToInputEffectiveScaleB));
        }
    }

    int32_t inputLayerNormScaleA;
    int32_t inputLayerNormScaleB;
    if (inputLayerNormBuffer != nullptr) {
        NN_RET_CHECK(QuantizeMultiplier(inputLayerNormShape.scale, &inputLayerNormScaleA,
                                        &inputLayerNormScaleB));
    }

    const float inputToForgetEffectiveScale =
            inputToForgetWeightsShape.scale * inputShape.scale / forgetIntermediateScale;
    int32_t inputToForgetEffectiveScaleA;
    int32_t inputToForgetEffectiveScaleB;
    NN_RET_CHECK(QuantizeMultiplier(inputToForgetEffectiveScale, &inputToForgetEffectiveScaleA,
                                    &inputToForgetEffectiveScaleB));
    const float recurrentToForgetEffectiveScale =
            recurrentToForgetWeightsShape.scale * prevOutputShape.scale / forgetIntermediateScale;
    int32_t recurrentToForgetEffectiveScaleA;
    int32_t recurrentToForgetEffectiveScaleB;
    NN_RET_CHECK(QuantizeMultiplier(recurrentToForgetEffectiveScale,
                                    &recurrentToForgetEffectiveScaleA,
                                    &recurrentToForgetEffectiveScaleB));
    int32_t cellToForgetEffectiveScaleA;
    int32_t cellToForgetEffectiveScaleB;
    if (cellToForgetBuffer != nullptr) {
        const float cellToForgetEffectiveScale =
                std::pow(2, cellShift) * cellToForgetShape.scale / forgetIntermediateScale;
        NN_RET_CHECK(QuantizeMultiplier(cellToForgetEffectiveScale, &cellToForgetEffectiveScaleA,
                                        &cellToForgetEffectiveScaleB));
    }
    int32_t forgetLayerNormScaleA;
    int32_t forgetLayerNormScaleB;
    if (forgetLayerNormBuffer != nullptr) {
        NN_RET_CHECK(QuantizeMultiplier(forgetLayerNormShape.scale, &forgetLayerNormScaleA,
                                        &forgetLayerNormScaleB));
    }

    const float inputToCellEffectiveScale =
            inputToCellWeightsShape.scale * inputShape.scale / cellIntermediateScale;
    int32_t inputToCellEffectiveScaleA;
    int32_t inputToCellEffectiveScaleB;
    NN_RET_CHECK(QuantizeMultiplier(inputToCellEffectiveScale, &inputToCellEffectiveScaleA,
                                    &inputToCellEffectiveScaleB));
    const float recurrentToCellEffectiveScale =
            recurrentToCellWeightsShape.scale * prevOutputShape.scale / cellIntermediateScale;
    int32_t recurrentToCellEffectiveScaleA;
    int32_t recurrentToCellEffectiveScaleB;
    NN_RET_CHECK(QuantizeMultiplier(recurrentToCellEffectiveScale, &recurrentToCellEffectiveScaleA,
                                    &recurrentToCellEffectiveScaleB));

    int32_t cellLayerNormScaleA;
    int32_t cellLayerNormScaleB;
    if (cellLayerNormBuffer != nullptr) {
        NN_RET_CHECK(QuantizeMultiplier(cellLayerNormShape.scale, &cellLayerNormScaleA,
                                        &cellLayerNormScaleB));
    }

    const float inputToOutputEffectiveScale =
            inputToOutputWeightsShape.scale * inputShape.scale / outputIntermediateScale;
    int32_t inputToOutputEffectiveScaleA;
    int32_t inputToOutputEffectiveScaleB;
    NN_RET_CHECK(QuantizeMultiplier(inputToOutputEffectiveScale, &inputToOutputEffectiveScaleA,
                                    &inputToOutputEffectiveScaleB));
    const float recurrentToOutputEffectiveScale =
            recurrentToOutputWeightsShape.scale * prevOutputShape.scale / outputIntermediateScale;
    int32_t recurrentToOutputEffectiveScaleA;
    int32_t recurrentToOutputEffectiveScaleB;
    NN_RET_CHECK(QuantizeMultiplier(recurrentToOutputEffectiveScale,
                                    &recurrentToOutputEffectiveScaleA,
                                    &recurrentToOutputEffectiveScaleB));
    int32_t cellToOutputEffectiveScaleA;
    int32_t cellToOutputEffectiveScaleB;
    if (cellToOutputBuffer != nullptr) {
        const float cellToOutputEffectiveScale =
                std::pow(2, cellShift) * cellToOutputShape.scale / outputIntermediateScale;
        NN_RET_CHECK(QuantizeMultiplier(cellToOutputEffectiveScale, &cellToOutputEffectiveScaleA,
                                        &cellToOutputEffectiveScaleB));
    }
    int32_t outputLayerNormScaleA;
    int32_t outputLayerNormScaleB;
    if (outputLayerNormBuffer != nullptr) {
        NN_RET_CHECK(QuantizeMultiplier(outputLayerNormShape.scale, &outputLayerNormScaleA,
                                        &outputLayerNormScaleB));
    }

    const float hiddenStateEffectiveScale = std::pow(2, -15) / hiddenStateScale * std::pow(2, -15);
    int32_t hiddenStateEffectiveScaleA;
    int32_t hiddenStateEffectiveScaleB;
    NN_RET_CHECK(QuantizeMultiplier(hiddenStateEffectiveScale, &hiddenStateEffectiveScaleA,
                                    &hiddenStateEffectiveScaleB));

    int32_t projectionEffectiveScaleA;
    int32_t projectionEffectiveScaleB;
    if (projectionWeightsBuffer != nullptr) {
        const float projectionEffectiveScale =
                projectionWeightsShape.scale * hiddenStateScale / prevOutputShape.scale;
        NN_RET_CHECK(QuantizeMultiplier(projectionEffectiveScale, &projectionEffectiveScaleA,
                                        &projectionEffectiveScaleB));
    }

    // Calculates quantized clipping parameters.
    int16_t quantizedCellClip = 0;
    if (cellClip > 0.0) {
        quantizedCellClip = static_cast<int32_t>(
                std::min(std::max(cellClip / prevCellStateShape.scale, -32768.0f), 32767.0f));
    }
    int8_t quantizedProjectionClip = 0;
    if (projectionClip > 0.0) {
        quantizedProjectionClip = static_cast<int32_t>(
                std::min(std::max(projectionClip / projectionWeightsShape.scale, -128.0f), 127.0f));
    }

    // Calculates effective bias.
    // This is for optimizing the matmul calculation.
    std::unique_ptr<int32_t[]> inputToInputEffectiveBias;
    std::unique_ptr<int32_t[]> recurrentToInputEffectiveBias;
    if (!useCifg) {
        NN_RET_CHECK(PrecomputeZeroPointTimesWeightWithBias(
                -inputShape.offset, inputToInputWeightsBuffer, inputToInputWeightsShape,
                /*bias=*/nullptr, &inputToInputEffectiveBias));
        NN_RET_CHECK(PrecomputeZeroPointTimesWeightWithBias(
                -prevOutputShape.offset, recurrentToInputWeightsBuffer,
                recurrentToInputWeightsShape,
                /*bias=*/nullptr, &recurrentToInputEffectiveBias));
    }

    std::unique_ptr<int32_t[]> inputToForgetEffectiveBias;
    NN_RET_CHECK(PrecomputeZeroPointTimesWeightWithBias(
            -inputShape.offset, inputToForgetWeightsBuffer, inputToForgetWeightsShape,
            /*bias=*/nullptr, &inputToForgetEffectiveBias));
    std::unique_ptr<int32_t[]> recurrentToForgetEffectiveBias;
    NN_RET_CHECK(PrecomputeZeroPointTimesWeightWithBias(
            -prevOutputShape.offset, recurrentToForgetWeightsBuffer, recurrentToForgetWeightsShape,
            /*bias=*/nullptr, &recurrentToForgetEffectiveBias));

    std::unique_ptr<int32_t[]> inputToCellEffectiveBias;
    NN_RET_CHECK(PrecomputeZeroPointTimesWeightWithBias(
            -inputShape.offset, inputToCellWeightsBuffer, inputToCellWeightsShape,
            /*bias=*/nullptr, &inputToCellEffectiveBias));
    std::unique_ptr<int32_t[]> recurrentToCellEffectiveBias;
    NN_RET_CHECK(PrecomputeZeroPointTimesWeightWithBias(
            -prevOutputShape.offset, recurrentToCellWeightsBuffer, recurrentToCellWeightsShape,
            /*bias=*/nullptr, &recurrentToCellEffectiveBias));

    std::unique_ptr<int32_t[]> inputToOutputEffectiveBias;
    NN_RET_CHECK(PrecomputeZeroPointTimesWeightWithBias(
            -inputShape.offset, inputToOutputWeightsBuffer, inputToOutputWeightsShape,
            /*bias=*/nullptr, &inputToOutputEffectiveBias));
    std::unique_ptr<int32_t[]> recurrentToOutputEffectiveBias;
    NN_RET_CHECK(PrecomputeZeroPointTimesWeightWithBias(
            -prevOutputShape.offset, recurrentToOutputWeightsBuffer, recurrentToOutputWeightsShape,
            /*bias=*/nullptr, &recurrentToOutputEffectiveBias));

    std::unique_ptr<int32_t[]> projectionEffectiveBias;
    if (projectionBiasBuffer != nullptr) {
        NN_RET_CHECK(PrecomputeZeroPointTimesWeightWithBias(
                hiddenStateZeroPoint, projectionWeightsBuffer, projectionWeightsShape,
                projectionBiasBuffer, &projectionEffectiveBias));
    }

    // Temporary buffers.
    std::vector<int16_t> inputGateBuffer(batchSize * numUnits);
    std::vector<int16_t> forgetGateBuffer(batchSize * numUnits);
    std::vector<int16_t> cellGateBuffer(batchSize * numUnits);
    std::vector<int16_t> outputGateBuffer(batchSize * numUnits);
    std::vector<int8_t> buffer8(batchSize * numUnits);

    // To avoid overflow when calculating layer norm.
    const int32_t inputInvLargeValue =
            std::min(1, static_cast<int32_t>(10000 * inputLayerNormShape.scale));
    const int32_t forgetInvLargeValue =
            std::min(1, static_cast<int32_t>(10000 * forgetLayerNormShape.scale));
    const int32_t cellInvLargeValue =
            std::min(1, static_cast<int32_t>(10000 * cellLayerNormShape.scale));
    const int32_t outputInvLargeValue =
            std::min(1, static_cast<int32_t>(10000 * outputLayerNormShape.scale));

    // Forget gate.
    MatrixBatchVectorMultiplyAccumulate(inputBuffer, inputToForgetEffectiveBias.get(),
                                        inputToForgetWeightsBuffer, inputToForgetEffectiveScaleA,
                                        inputToForgetEffectiveScaleB, batchSize, inputSize,
                                        numUnits,
                                        /*outputZeroPoint=*/0, forgetGateBuffer.data());
    MatrixBatchVectorMultiplyAccumulate(
            prevOutputBuffer, recurrentToForgetEffectiveBias.get(), recurrentToForgetWeightsBuffer,
            recurrentToForgetEffectiveScaleA, recurrentToForgetEffectiveScaleB, batchSize,
            outputSize, numUnits,
            /*outputZeroPoint=*/0, forgetGateBuffer.data());
    if (cellToForgetBuffer != nullptr) {
        VectorBatchVectorCwiseProductAccumulate(
                cellToForgetBuffer, outputSize, cellStateBuffer, batchSize,
                cellToForgetEffectiveScaleA, cellToForgetEffectiveScaleB, forgetGateBuffer.data());
    }
    if (forgetLayerNormBuffer != nullptr) {
        ApplyLayerNorm(forgetGateBuffer.data(), forgetLayerNormBuffer, forgetBiasBuffer,
                       forgetLayerNormScaleA, forgetLayerNormScaleB, forgetInvLargeValue, batchSize,
                       numUnits, forgetGateBuffer.data());
    }
    ApplySigmoid(forgetGateBuffer.data(), batchSize, numUnits, forgetGateBuffer.data());

    // Modulation gate.
    MatrixBatchVectorMultiplyAccumulate(inputBuffer, inputToCellEffectiveBias.get(),
                                        inputToCellWeightsBuffer, inputToCellEffectiveScaleA,
                                        inputToCellEffectiveScaleB, batchSize, inputSize, numUnits,
                                        /*outputZeroPoint=*/0, cellGateBuffer.data());
    MatrixBatchVectorMultiplyAccumulate(
            prevOutputBuffer, recurrentToCellEffectiveBias.get(), recurrentToCellWeightsBuffer,
            recurrentToCellEffectiveScaleA, recurrentToCellEffectiveScaleB, batchSize, outputSize,
            numUnits,
            /*outputZeroPoint=*/0, cellGateBuffer.data());
    if (cellLayerNormBuffer != nullptr) {
        ApplyLayerNorm(cellGateBuffer.data(), cellLayerNormBuffer, cellBiasBuffer,
                       cellLayerNormScaleA, cellLayerNormScaleB, cellInvLargeValue, batchSize,
                       numUnits, cellGateBuffer.data());
    }
    ApplyTanh<3>(cellGateBuffer.data(), batchSize, numUnits, cellGateBuffer.data());

    // Input gate.
    if (useCifg) {
        Sub1Vector(forgetGateBuffer.data(), batchSize * numUnits, inputGateBuffer.data());
    } else {
        MatrixBatchVectorMultiplyAccumulate(inputBuffer, inputToInputEffectiveBias.get(),
                                            inputToInputWeightsBuffer, inputToInputEffectiveScaleA,
                                            inputToInputEffectiveScaleB, batchSize, inputSize,
                                            numUnits,
                                            /*outputZeroPoint=*/0, inputGateBuffer.data());
        MatrixBatchVectorMultiplyAccumulate(
                prevOutputBuffer, recurrentToInputEffectiveBias.get(),
                recurrentToInputWeightsBuffer, recurrentToInputEffectiveScaleA,
                recurrentToInputEffectiveScaleB, batchSize, outputSize, numUnits,
                /*outputZeroPoint=*/0, inputGateBuffer.data());
        if (cellToInputBuffer != nullptr) {
            VectorBatchVectorCwiseProductAccumulate(
                    cellToInputBuffer, outputSize, cellStateBuffer, batchSize,
                    cellToInputEffectiveScaleA, cellToInputEffectiveScaleB, inputGateBuffer.data());
        }
        if (inputLayerNormBuffer != nullptr) {
            ApplyLayerNorm(inputGateBuffer.data(), inputLayerNormBuffer, inputBiasBuffer,
                           inputLayerNormScaleA, inputLayerNormScaleB, inputInvLargeValue,
                           batchSize, numUnits, inputGateBuffer.data());
        }
        ApplySigmoid(inputGateBuffer.data(), batchSize, numUnits, inputGateBuffer.data());
    }

    // Cell.
    CwiseMul(forgetGateBuffer.data(), prevCellStateBuffer, batchSize, numUnits,
             /*shift=*/15, forgetGateBuffer.data());
    CwiseMul(inputGateBuffer.data(), cellGateBuffer.data(), batchSize, numUnits, 30 + cellShift,
             cellGateBuffer.data());
    CwiseAdd(forgetGateBuffer.data(), cellGateBuffer.data(), batchSize, numUnits, cellStateBuffer);
    if (quantizedCellClip > 0) {
        CwiseClipping(cellStateBuffer, quantizedCellClip, batchSize, numUnits);
    }

    // Output gate.
    MatrixBatchVectorMultiplyAccumulate(inputBuffer, inputToOutputEffectiveBias.get(),
                                        inputToOutputWeightsBuffer, inputToOutputEffectiveScaleA,
                                        inputToOutputEffectiveScaleB, batchSize, inputSize,
                                        numUnits,
                                        /*outputZeroPoint=*/0, outputGateBuffer.data());
    MatrixBatchVectorMultiplyAccumulate(
            prevOutputBuffer, recurrentToOutputEffectiveBias.get(), recurrentToOutputWeightsBuffer,
            recurrentToOutputEffectiveScaleA, recurrentToOutputEffectiveScaleB, batchSize,
            outputSize, numUnits,
            /*outputZeroPoint=*/0, outputGateBuffer.data());
    if (cellToOutputBuffer != nullptr) {
        VectorBatchVectorCwiseProductAccumulate(
                cellToOutputBuffer, outputSize, cellStateBuffer, batchSize,
                cellToOutputEffectiveScaleA, cellToOutputEffectiveScaleB, outputGateBuffer.data());
    }
    if (outputLayerNormBuffer != nullptr) {
        ApplyLayerNorm(outputGateBuffer.data(), outputLayerNormBuffer, outputBiasBuffer,
                       outputLayerNormScaleA, outputLayerNormScaleB, outputInvLargeValue, batchSize,
                       numUnits, outputGateBuffer.data());
    }
    ApplySigmoid(outputGateBuffer.data(), batchSize, numUnits, outputGateBuffer.data());

    // Hidden.
    ApplyTanh(cellShift + 15, cellStateBuffer, batchSize, numUnits, inputGateBuffer.data());
    CwiseMul(outputGateBuffer.data(), inputGateBuffer.data(), hiddenStateEffectiveScaleA,
             hiddenStateEffectiveScaleB, batchSize, numUnits, hiddenStateZeroPoint, buffer8.data());

    // Projection.
    if (projectionWeightsBuffer != nullptr) {
        memset(outputBuffer, 0, batchSize * outputSize * sizeof(int8_t));
        MatrixBatchVectorMultiplyAccumulate(buffer8.data(), projectionEffectiveBias.get(),
                                            projectionWeightsBuffer, projectionEffectiveScaleA,
                                            projectionEffectiveScaleB, batchSize, numUnits,
                                            outputSize, prevOutputShape.offset, outputBuffer);
        if (quantizedProjectionClip > 0) {
            CwiseClipping(outputBuffer, quantizedProjectionClip, batchSize, outputSize);
        }
    } else {
        std::copy_n(buffer8.data(), batchSize * outputSize, outputBuffer);
    }

    // Copy output to output state out.
    for (unsigned int i = 0; i < batchSize * outputSize; ++i) {
        outputStateBuffer[i] = outputBuffer[i];
    }

    return true;
}

}  // namespace qlstm

NN_REGISTER_OPERATION(QUANTIZED_LSTM, "QUANTIZED_LSTM", qlstm::validate, qlstm::prepare,
                      qlstm::execute, .allowOmittedOperand = true);

}  // namespace nn
}  // namespace android
