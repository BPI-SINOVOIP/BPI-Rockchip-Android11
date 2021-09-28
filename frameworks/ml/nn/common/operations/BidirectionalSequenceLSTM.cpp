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

#include "BidirectionalSequenceLSTM.h"

#include <algorithm>
#include <vector>

#include "CpuExecutor.h"
#include "CpuOperationUtils.h"
#include "HalInterfaces.h"
#include "OperationsUtils.h"
#include "Tracing.h"

namespace android {
namespace nn {

namespace {

using namespace hal;

template <typename T>
inline T* GetBuffer(RunTimeOperandInfo* operand) {
    return reinterpret_cast<T*>(operand->buffer);
}

template <typename T>
inline const T* GetBuffer(const RunTimeOperandInfo* operand) {
    return reinterpret_cast<const T*>(operand->buffer);
}

template <typename T>
inline const T* GetOptionalBuffer(const RunTimeOperandInfo* operand) {
    return !IsNullInput(operand) ? reinterpret_cast<const T*>(operand->buffer) : nullptr;
}

enum class LinkingMode {
    NO_LINKING,
    PARALLEL_LINKING,
    CROSS_LINKING,
};

bool getLinkingMode(bool hasAuxInput, bool hasAuxWeights, LinkingMode* linkingMode) {
    // Three possible configurations for three possible linking modes:
    // 1) NO_LINKING -- no auxiliary tensors at all
    // 2) PARALLEL_LINKING -- auxiliary input is provided and used as a regular
    //    input to the backward network, so the auxiliary weights are omitted.
    // 3) CROSS_LINKING -- auxiliary input is provided and multiplied by
    //    auxiliary weights.
    if (!hasAuxInput && !hasAuxWeights) {
        *linkingMode = LinkingMode::NO_LINKING;
    } else if (hasAuxInput && !hasAuxWeights) {
        *linkingMode = LinkingMode::PARALLEL_LINKING;
    } else if (hasAuxInput && hasAuxWeights) {
        *linkingMode = LinkingMode::CROSS_LINKING;
    } else {
        NN_RET_CHECK_FAIL()
                << "Unsupported auxiliary tensors configuration for BIDIRECTIONAL_SEQUENCE_RNN.";
    }

    return true;
}

}  // anonymous namespace

BidirectionalSequenceLSTM::BidirectionalSequenceLSTM(const Operation& operation,
                                                     RunTimeOperandInfo* operands) {
    input_ = GetInput(operation, operands, kInputTensor);

    fw_input_to_input_weights_ =
            GetInput(operation, operands, kFwInputToInputWeightsTensor);  // optional
    fw_input_to_forget_weights_ = GetInput(operation, operands, kFwInputToForgetWeightsTensor);
    fw_input_to_cell_weights_ = GetInput(operation, operands, kFwInputToCellWeightsTensor);
    fw_input_to_output_weights_ = GetInput(operation, operands, kFwInputToOutputWeightsTensor);

    fw_recurrent_to_input_weights_ =
            GetInput(operation, operands, kFwRecurrentToInputWeightsTensor);  // optional
    fw_recurrent_to_forget_weights_ =
            GetInput(operation, operands, kFwRecurrentToForgetWeightsTensor);
    fw_recurrent_to_cell_weights_ = GetInput(operation, operands, kFwRecurrentToCellWeightsTensor);
    fw_recurrent_to_output_weights_ =
            GetInput(operation, operands, kFwRecurrentToOutputWeightsTensor);

    fw_cell_to_input_weights_ =
            GetInput(operation, operands, kFwCellToInputWeightsTensor);  // optional
    fw_cell_to_forget_weights_ =
            GetInput(operation, operands, kFwCellToForgetWeightsTensor);  // optional
    fw_cell_to_output_weights_ =
            GetInput(operation, operands, kFwCellToOutputWeightsTensor);  // optional

    fw_input_gate_bias_ = GetInput(operation, operands, kFwInputGateBiasTensor);
    fw_forget_gate_bias_ = GetInput(operation, operands, kFwForgetGateBiasTensor);
    fw_cell_bias_ = GetInput(operation, operands, kFwCellGateBiasTensor);
    fw_output_gate_bias_ = GetInput(operation, operands, kFwOutputGateBiasTensor);

    fw_projection_weights_ = GetInput(operation, operands, kFwProjectionWeightsTensor);  // optional
    fw_projection_bias_ = GetInput(operation, operands, kFwProjectionBiasTensor);        // optional

    fw_activation_state_ = GetInput(operation, operands, kFwInputActivationStateTensor);
    fw_cell_state_ = GetInput(operation, operands, kFwInputCellStateTensor);

    bw_input_to_input_weights_ =
            GetInput(operation, operands, kBwInputToInputWeightsTensor);  // optional
    bw_input_to_forget_weights_ = GetInput(operation, operands, kBwInputToForgetWeightsTensor);
    bw_input_to_cell_weights_ = GetInput(operation, operands, kBwInputToCellWeightsTensor);
    bw_input_to_output_weights_ = GetInput(operation, operands, kBwInputToOutputWeightsTensor);

    bw_recurrent_to_input_weights_ =
            GetInput(operation, operands, kBwRecurrentToInputWeightsTensor);  // optional
    bw_recurrent_to_forget_weights_ =
            GetInput(operation, operands, kBwRecurrentToForgetWeightsTensor);
    bw_recurrent_to_cell_weights_ = GetInput(operation, operands, kBwRecurrentToCellWeightsTensor);
    bw_recurrent_to_output_weights_ =
            GetInput(operation, operands, kBwRecurrentToOutputWeightsTensor);

    bw_cell_to_input_weights_ =
            GetInput(operation, operands, kBwCellToInputWeightsTensor);  // optional
    bw_cell_to_forget_weights_ =
            GetInput(operation, operands, kBwCellToForgetWeightsTensor);  // optional
    bw_cell_to_output_weights_ =
            GetInput(operation, operands, kBwCellToOutputWeightsTensor);  // optional

    bw_input_gate_bias_ = GetInput(operation, operands, kBwInputGateBiasTensor);
    bw_forget_gate_bias_ = GetInput(operation, operands, kBwForgetGateBiasTensor);
    bw_cell_bias_ = GetInput(operation, operands, kBwCellGateBiasTensor);
    bw_output_gate_bias_ = GetInput(operation, operands, kBwOutputGateBiasTensor);

    bw_projection_weights_ = GetInput(operation, operands, kBwProjectionWeightsTensor);  // optional
    bw_projection_bias_ = GetInput(operation, operands, kBwProjectionBiasTensor);        // optional

    bw_activation_state_ = GetInput(operation, operands, kBwInputActivationStateTensor);
    bw_cell_state_ = GetInput(operation, operands, kBwInputCellStateTensor);

    aux_input_ = GetInput(operation, operands, kAuxInputTensor);
    fw_aux_input_to_input_weights_ = GetInput(operation, operands, kFwAuxInputToInputWeightsTensor);
    fw_aux_input_to_forget_weights_ =
            GetInput(operation, operands, kFwAuxInputToForgetWeightsTensor);
    fw_aux_input_to_cell_weights_ = GetInput(operation, operands, kFwAuxInputToCellWeightsTensor);
    fw_aux_input_to_output_weights_ =
            GetInput(operation, operands, kFwAuxInputToOutputWeightsTensor);
    bw_aux_input_to_input_weights_ = GetInput(operation, operands, kBwAuxInputToInputWeightsTensor);
    bw_aux_input_to_forget_weights_ =
            GetInput(operation, operands, kBwAuxInputToForgetWeightsTensor);
    bw_aux_input_to_cell_weights_ = GetInput(operation, operands, kBwAuxInputToCellWeightsTensor);
    bw_aux_input_to_output_weights_ =
            GetInput(operation, operands, kBwAuxInputToOutputWeightsTensor);

    fw_input_layer_norm_weights_ = GetInput(operation, operands, kFwInputLayerNormWeightsTensor);
    fw_forget_layer_norm_weights_ = GetInput(operation, operands, kFwForgetLayerNormWeightsTensor);
    fw_cell_layer_norm_weights_ = GetInput(operation, operands, kFwCellLayerNormWeightsTensor);
    fw_output_layer_norm_weights_ = GetInput(operation, operands, kFwOutputLayerNormWeightsTensor);
    bw_input_layer_norm_weights_ = GetInput(operation, operands, kBwInputLayerNormWeightsTensor);
    bw_forget_layer_norm_weights_ = GetInput(operation, operands, kBwForgetLayerNormWeightsTensor);
    bw_cell_layer_norm_weights_ = GetInput(operation, operands, kBwCellLayerNormWeightsTensor);
    bw_output_layer_norm_weights_ = GetInput(operation, operands, kBwOutputLayerNormWeightsTensor);

    const auto& activationOperand = *GetInput(operation, operands, kActivationParam);
    params_.activation = static_cast<TfLiteFusedActivation>(getScalarDataWithDefault<int32_t>(
            activationOperand, TfLiteFusedActivation::kTfLiteActNone));
    const auto& clipOperand = *GetInput(operation, operands, kCellClipParam);
    const auto& projOperand = *GetInput(operation, operands, kProjClipParam);
    if (input_->type == OperandType::TENSOR_FLOAT32) {
        params_.cell_clip = getScalarDataWithDefault<float>(clipOperand, 0.0f);
        params_.proj_clip = getScalarDataWithDefault<float>(projOperand, 0.0f);
    } else {
        params_.cell_clip =
                static_cast<float>(getScalarDataWithDefault<_Float16>(clipOperand, 0.0f));
        params_.proj_clip =
                static_cast<float>(getScalarDataWithDefault<_Float16>(projOperand, 0.0f));
    }
    const auto& mergeOutputsOperand = *GetInput(operation, operands, kMergeOutputsParam);
    params_.merge_outputs = getScalarDataWithDefault<bool>(mergeOutputsOperand, false);
    const auto& timeMajorOperand = *GetInput(operation, operands, kTimeMajorParam);
    params_.time_major = getScalarDataWithDefault<bool>(timeMajorOperand, false);
    params_.use_layer_norm = !IsNullInput(fw_input_layer_norm_weights_);

    fw_output_ = GetOutput(operation, operands, kFwOutputTensor);
    if (!params_.merge_outputs) {
        bw_output_ = GetOutput(operation, operands, kBwOutputTensor);
    }

    params_.output_state = (operation.outputs.size() == 5 || operation.outputs.size() == 6);
    if (params_.output_state) {
        uint32_t delta = params_.merge_outputs ? 1 : 0;
        fw_output_activation_state_ =
                GetOutput(operation, operands, kFwOutputActivationStateTensor - delta);
        fw_output_cell_state_ = GetOutput(operation, operands, kFwOutputCellStateTensor - delta);
        bw_output_activation_state_ =
                GetOutput(operation, operands, kBwOutputActivationStateTensor - delta);
        bw_output_cell_state_ = GetOutput(operation, operands, kBwOutputCellStateTensor - delta);
    }
}

bool BidirectionalSequenceLSTM::Prepare(const Operation& operation, RunTimeOperandInfo* operands,
                                        Shape* fwOutputShape, Shape* bwOutputShape,
                                        Shape* fwOutputActivationState, Shape* fwOutputCellState,
                                        Shape* bwOutputActivationState, Shape* bwOutputCellState) {
    // Check we have all the inputs and outputs we need.
    constexpr int requiredInputs[] = {
            kInputTensor,
            kFwInputToForgetWeightsTensor,
            kFwInputToCellWeightsTensor,
            kFwInputToOutputWeightsTensor,
            kFwRecurrentToForgetWeightsTensor,
            kFwRecurrentToCellWeightsTensor,
            kFwRecurrentToOutputWeightsTensor,
            kFwForgetGateBiasTensor,
            kFwCellGateBiasTensor,
            kFwOutputGateBiasTensor,
            kBwInputToForgetWeightsTensor,
            kBwInputToCellWeightsTensor,
            kBwInputToOutputWeightsTensor,
            kBwRecurrentToForgetWeightsTensor,
            kBwRecurrentToCellWeightsTensor,
            kBwRecurrentToOutputWeightsTensor,
            kBwForgetGateBiasTensor,
            kBwCellGateBiasTensor,
            kBwOutputGateBiasTensor,
            kFwInputActivationStateTensor,
            kFwInputCellStateTensor,
            kBwInputActivationStateTensor,
            kBwInputCellStateTensor,
            kActivationParam,
            kCellClipParam,
            kProjClipParam,
            kMergeOutputsParam,
            kTimeMajorParam,
    };
    for (const int requiredInput : requiredInputs) {
        NN_RET_CHECK(!IsNullInput(GetInput(operation, operands, requiredInput)))
                << "required input " << requiredInput << " is omitted";
    }

    // Check that the scalar operands' buffers are large enough.
    const auto& activationOperand = *GetInput(operation, operands, kActivationParam);
    NN_RET_CHECK(activationOperand.length >= sizeof(int32_t));
    const auto& cellOperand = *GetInput(operation, operands, kCellClipParam);
    const auto& projOperand = *GetInput(operation, operands, kProjClipParam);
    if (input_->type == OperandType::TENSOR_FLOAT32) {
        NN_RET_CHECK(cellOperand.length >= sizeof(float));
        NN_RET_CHECK(projOperand.length >= sizeof(float));
    } else {
        NN_RET_CHECK(cellOperand.length >= sizeof(_Float16));
        NN_RET_CHECK(projOperand.length >= sizeof(_Float16));
    }
    const auto& mergeOutputsOperand = *GetInput(operation, operands, kMergeOutputsParam);
    NN_RET_CHECK(mergeOutputsOperand.length >= sizeof(bool));
    const auto& timeMajorOperand = *GetInput(operation, operands, kTimeMajorParam);
    NN_RET_CHECK(timeMajorOperand.length >= sizeof(bool));

    // Inferring batch size, number of outputs and number of cells from the
    // input tensors.
    NN_CHECK(NumDimensions(input_) == 3);
    const uint32_t max_time = SizeOfDimension(input_, params_.time_major ? 0 : 1);
    const uint32_t n_batch = SizeOfDimension(input_, params_.time_major ? 1 : 0);
    const uint32_t n_fw_input = SizeOfDimension(input_, 2);

    const uint32_t n_fw_cell = SizeOfDimension(fw_input_to_output_weights_, 0);
    NN_CHECK_EQ(NumDimensions(fw_input_to_output_weights_), 2);
    NN_CHECK_EQ(SizeOfDimension(fw_input_to_output_weights_, 1), n_fw_input);

    NN_CHECK_EQ(NumDimensions(fw_recurrent_to_output_weights_), 2);
    NN_CHECK_EQ(SizeOfDimension(fw_recurrent_to_output_weights_, 0), n_fw_cell);
    const uint32_t n_fw_output = SizeOfDimension(fw_recurrent_to_output_weights_, 1);

    const uint32_t n_bw_cell = SizeOfDimension(bw_input_to_output_weights_, 0);

    NN_CHECK_EQ(NumDimensions(bw_recurrent_to_output_weights_), 2);
    NN_CHECK_EQ(SizeOfDimension(bw_recurrent_to_output_weights_, 0), n_bw_cell);
    const uint32_t n_bw_output = SizeOfDimension(bw_recurrent_to_output_weights_, 1);

    // Check that input tensor dimensions matches with each other.
    if (!LSTMCell::CheckInputTensorDimensions(
                input_, fw_input_to_input_weights_, fw_input_to_forget_weights_,
                fw_input_to_cell_weights_, fw_input_to_output_weights_,
                fw_recurrent_to_input_weights_, fw_recurrent_to_forget_weights_,
                fw_recurrent_to_cell_weights_, fw_recurrent_to_output_weights_,
                fw_cell_to_input_weights_, fw_cell_to_forget_weights_, fw_cell_to_output_weights_,
                fw_input_gate_bias_, fw_forget_gate_bias_, fw_cell_bias_, fw_output_gate_bias_,
                fw_projection_weights_, fw_projection_bias_, fw_input_layer_norm_weights_,
                fw_forget_layer_norm_weights_, fw_cell_layer_norm_weights_,
                fw_output_layer_norm_weights_, n_fw_input, n_fw_output, n_fw_cell, &params_)) {
        return false;
    }

    if (params_.use_cifg) {
        NN_RET_CHECK(IsNullInput(fw_aux_input_to_input_weights_) &&
                     IsNullInput(bw_aux_input_to_input_weights_));
    }

    const bool aux_fw_weights_all_or_none =
            ((params_.use_cifg || !IsNullInput(fw_aux_input_to_input_weights_)) &&
             !IsNullInput(fw_aux_input_to_forget_weights_) &&
             !IsNullInput(fw_aux_input_to_cell_weights_) &&
             !IsNullInput(fw_aux_input_to_output_weights_)) ||
            (IsNullInput(fw_aux_input_to_input_weights_) &&
             IsNullInput(fw_aux_input_to_forget_weights_) &&
             IsNullInput(fw_aux_input_to_cell_weights_) &&
             IsNullInput(fw_aux_input_to_output_weights_));
    const bool aux_bw_weights_all_or_none =
            ((params_.use_cifg || !IsNullInput(bw_aux_input_to_input_weights_)) &&
             !IsNullInput(bw_aux_input_to_forget_weights_) &&
             !IsNullInput(bw_aux_input_to_cell_weights_) &&
             !IsNullInput(bw_aux_input_to_output_weights_)) ||
            (IsNullInput(bw_aux_input_to_input_weights_) &&
             IsNullInput(bw_aux_input_to_forget_weights_) &&
             IsNullInput(bw_aux_input_to_cell_weights_) &&
             IsNullInput(bw_aux_input_to_output_weights_));

    NN_RET_CHECK(aux_fw_weights_all_or_none && aux_bw_weights_all_or_none);
    const bool has_aux_input = !IsNullInput(aux_input_);
    const bool has_fw_aux_weights = !IsNullInput(fw_aux_input_to_forget_weights_);
    const bool has_bw_aux_weights = !IsNullInput(bw_aux_input_to_forget_weights_);

    NN_RET_CHECK(has_fw_aux_weights == has_bw_aux_weights);

    LinkingMode linkingMode;
    NN_RET_CHECK(getLinkingMode(has_aux_input, has_fw_aux_weights, &linkingMode));

    if (has_aux_input) {
        // Check that aux_input has the same dimensions (except last) as the input.
        NN_CHECK_EQ(aux_input_->shape().dimensions[0], input_->shape().dimensions[0]);
        NN_CHECK_EQ(aux_input_->shape().dimensions[1], input_->shape().dimensions[1]);
    }

    if (has_fw_aux_weights) {
        int n_aux_input = SizeOfDimension(input_, 2);

        // Check forward auxiliary input shapes
        {
            NN_RET_CHECK_EQ(NumDimensions(fw_aux_input_to_input_weights_), 2);
            NN_RET_CHECK_EQ(SizeOfDimension(fw_aux_input_to_input_weights_, 0), n_fw_cell);
            NN_RET_CHECK_EQ(SizeOfDimension(fw_aux_input_to_input_weights_, 1), n_aux_input);

            NN_RET_CHECK_EQ(NumDimensions(fw_aux_input_to_forget_weights_), 2);
            NN_RET_CHECK_EQ(SizeOfDimension(fw_aux_input_to_forget_weights_, 0), n_fw_cell);
            NN_RET_CHECK_EQ(SizeOfDimension(fw_aux_input_to_forget_weights_, 1), n_aux_input);

            NN_RET_CHECK_EQ(NumDimensions(fw_aux_input_to_cell_weights_), 2);
            NN_RET_CHECK_EQ(SizeOfDimension(fw_aux_input_to_cell_weights_, 0), n_fw_cell);
            NN_RET_CHECK_EQ(SizeOfDimension(fw_aux_input_to_cell_weights_, 1), n_aux_input);

            NN_RET_CHECK_EQ(NumDimensions(fw_aux_input_to_output_weights_), 2);
            NN_RET_CHECK_EQ(SizeOfDimension(fw_aux_input_to_output_weights_, 0), n_fw_cell);
            NN_RET_CHECK_EQ(SizeOfDimension(fw_aux_input_to_output_weights_, 1), n_aux_input);
        }

        // Check backward auxiliary input shapes
        {
            NN_RET_CHECK_EQ(NumDimensions(bw_aux_input_to_input_weights_), 2);
            NN_RET_CHECK_EQ(SizeOfDimension(bw_aux_input_to_input_weights_, 0), n_bw_cell);
            NN_RET_CHECK_EQ(SizeOfDimension(bw_aux_input_to_input_weights_, 1), n_aux_input);

            NN_RET_CHECK_EQ(NumDimensions(bw_aux_input_to_forget_weights_), 2);
            NN_RET_CHECK_EQ(SizeOfDimension(bw_aux_input_to_forget_weights_, 0), n_bw_cell);
            NN_RET_CHECK_EQ(SizeOfDimension(bw_aux_input_to_forget_weights_, 1), n_aux_input);

            NN_RET_CHECK_EQ(NumDimensions(bw_aux_input_to_cell_weights_), 2);
            NN_RET_CHECK_EQ(SizeOfDimension(bw_aux_input_to_cell_weights_, 0), n_bw_cell);
            NN_RET_CHECK_EQ(SizeOfDimension(bw_aux_input_to_cell_weights_, 1), n_aux_input);

            NN_RET_CHECK_EQ(NumDimensions(bw_aux_input_to_output_weights_), 2);
            NN_RET_CHECK_EQ(SizeOfDimension(bw_aux_input_to_output_weights_, 0), n_bw_cell);
            NN_RET_CHECK_EQ(SizeOfDimension(bw_aux_input_to_output_weights_, 1), n_aux_input);
        }
    }

    const Shape& inputShape = input_->shape();
    fwOutputShape->type = inputShape.type;
    fwOutputShape->offset = inputShape.offset;
    fwOutputShape->scale = inputShape.scale;
    fwOutputShape->dimensions.resize(3);
    fwOutputShape->dimensions[0] = params_.time_major ? max_time : n_batch;
    fwOutputShape->dimensions[1] = params_.time_major ? n_batch : max_time;
    fwOutputShape->dimensions[2] = params_.merge_outputs ? n_fw_output + n_bw_output : n_fw_output;

    const RunTimeOperandInfo* bw_input =
            linkingMode == LinkingMode::PARALLEL_LINKING ? aux_input_ : input_;
    const uint32_t n_bw_input = SizeOfDimension(bw_input, 2);
    // Check that input tensor dimensions matches with each other.
    if (!LSTMCell::CheckInputTensorDimensions(
                bw_input, bw_input_to_input_weights_, bw_input_to_forget_weights_,
                bw_input_to_cell_weights_, bw_input_to_output_weights_,
                bw_recurrent_to_input_weights_, bw_recurrent_to_forget_weights_,
                bw_recurrent_to_cell_weights_, bw_recurrent_to_output_weights_,
                bw_cell_to_input_weights_, bw_cell_to_forget_weights_, bw_cell_to_output_weights_,
                bw_input_gate_bias_, bw_forget_gate_bias_, bw_cell_bias_, bw_output_gate_bias_,
                bw_projection_weights_, bw_projection_bias_, bw_input_layer_norm_weights_,
                bw_forget_layer_norm_weights_, bw_cell_layer_norm_weights_,
                bw_output_layer_norm_weights_, n_bw_input, n_bw_output, n_bw_cell, &params_)) {
        return false;
    }

    if (!params_.merge_outputs) {
        bwOutputShape->type = inputShape.type;
        bwOutputShape->offset = inputShape.offset;
        bwOutputShape->scale = inputShape.scale;
        bwOutputShape->dimensions.resize(3);
        bwOutputShape->dimensions[0] = params_.time_major ? max_time : n_batch;
        bwOutputShape->dimensions[1] = params_.time_major ? n_batch : max_time;
        bwOutputShape->dimensions[2] = n_bw_output;
    }

    if (params_.output_state) {
        *fwOutputActivationState = fw_activation_state_->shape();
        *fwOutputCellState = fw_cell_state_->shape();
        *bwOutputActivationState = bw_activation_state_->shape();
        *bwOutputCellState = bw_cell_state_->shape();
    }

    if (params_.use_cifg) {
        fw_scratch_shape_.dimensions = {n_batch, n_fw_cell * 3};
        bw_scratch_shape_.dimensions = {n_batch, n_bw_cell * 3};
    } else {
        fw_scratch_shape_.dimensions = {n_batch, n_fw_cell * 4};
        bw_scratch_shape_.dimensions = {n_batch, n_bw_cell * 4};
    }
    fw_scratch_shape_.type = bw_scratch_shape_.type = inputShape.type;
    fw_scratch_shape_.offset = bw_scratch_shape_.offset = inputShape.offset;
    fw_scratch_shape_.scale = bw_scratch_shape_.scale = inputShape.scale;

    return true;
}

bool BidirectionalSequenceLSTM::Eval() {
    const uint32_t n_fw_output = SizeOfDimension(fw_recurrent_to_output_weights_, 1);
    const uint32_t n_bw_output = SizeOfDimension(bw_recurrent_to_output_weights_, 1);
    std::vector<uint32_t> fw_output_dims = input_->shape().dimensions;
    fw_output_dims[2] = n_fw_output;
    std::vector<uint32_t> bw_output_dims = fw_output_dims;
    bw_output_dims[2] = n_bw_output;
    const uint32_t n_fw_output_elements = fw_output_dims[0] * fw_output_dims[1] * fw_output_dims[2];
    const uint32_t n_output_elements =
            fw_output_dims[0] * fw_output_dims[1] * (fw_output_dims[2] + bw_output_dims[2]);

    const bool has_aux_input = !IsNullInput(aux_input_);
    const bool has_aux_weights = !IsNullInput(fw_aux_input_to_forget_weights_);

    LinkingMode linkingMode;
    NN_RET_CHECK(getLinkingMode(has_aux_input, has_aux_weights, &linkingMode));

    switch (input_->type) {
        case OperandType::TENSOR_FLOAT32: {
            const float* bwInput = GetBuffer<const float>(input_);
            Shape bwInputShape = input_->shape();
            const float* auxInput = GetOptionalBuffer<const float>(aux_input_);
            if (linkingMode == LinkingMode::PARALLEL_LINKING) {
                bwInput = GetBuffer<const float>(aux_input_);
                bwInputShape = aux_input_->shape();
                auxInput = nullptr;
            }

            float* fw_output_activation_state_buffer = nullptr;
            float* fw_output_cell_state_buffer = nullptr;
            std::vector<float> fw_output_activation_state;
            std::vector<float> fw_output_cell_state;
            if (params_.output_state) {
                fw_output_activation_state_buffer = GetBuffer<float>(fw_output_activation_state_);
                fw_output_cell_state_buffer = GetBuffer<float>(fw_output_cell_state_);
            } else {
                fw_output_activation_state.resize(
                        getNumberOfElements(fw_activation_state_->shape()));
                fw_output_cell_state.resize(getNumberOfElements(fw_cell_state_->shape()));

                fw_output_activation_state_buffer = fw_output_activation_state.data();
                fw_output_cell_state_buffer = fw_output_cell_state.data();
            }
            std::vector<float> fw_scratch_buffer(getNumberOfElements(fw_scratch_shape_));
            const bool kForwardSequence = true;
            LSTMCell::LSTMEvalFloat32(
                    params_, GetBuffer<const float>(input_), input_->shape(),
                    GetBuffer<const float>(fw_input_to_input_weights_),
                    GetBuffer<const float>(fw_input_to_forget_weights_),
                    GetBuffer<const float>(fw_input_to_cell_weights_),
                    GetBuffer<const float>(fw_input_to_output_weights_),
                    fw_input_to_output_weights_->shape(),
                    GetBuffer<const float>(fw_recurrent_to_input_weights_),
                    GetBuffer<const float>(fw_recurrent_to_forget_weights_),
                    GetBuffer<const float>(fw_recurrent_to_cell_weights_),
                    GetBuffer<const float>(fw_recurrent_to_output_weights_),
                    fw_recurrent_to_output_weights_->shape(),
                    GetBuffer<const float>(fw_cell_to_input_weights_),
                    GetBuffer<const float>(fw_cell_to_forget_weights_),
                    GetBuffer<const float>(fw_cell_to_output_weights_), auxInput,
                    GetOptionalBuffer<const float>(fw_aux_input_to_input_weights_),
                    GetOptionalBuffer<const float>(fw_aux_input_to_forget_weights_),
                    GetOptionalBuffer<const float>(fw_aux_input_to_cell_weights_),
                    GetOptionalBuffer<const float>(fw_aux_input_to_output_weights_),
                    GetBuffer<const float>(fw_input_gate_bias_),
                    GetBuffer<const float>(fw_forget_gate_bias_),
                    GetBuffer<const float>(fw_cell_bias_),
                    GetBuffer<const float>(fw_output_gate_bias_),
                    GetBuffer<const float>(fw_projection_weights_),
                    GetBuffer<const float>(fw_projection_bias_),
                    GetBuffer<const float>(fw_activation_state_),
                    GetBuffer<const float>(fw_cell_state_),
                    GetOptionalBuffer<const float>(fw_input_layer_norm_weights_),
                    GetOptionalBuffer<const float>(fw_forget_layer_norm_weights_),
                    GetOptionalBuffer<const float>(fw_cell_layer_norm_weights_),
                    GetOptionalBuffer<const float>(fw_output_layer_norm_weights_),
                    fw_output_activation_state_buffer, fw_output_cell_state_buffer,
                    GetBuffer<float>(fw_output_), fw_scratch_buffer.data(), params_.time_major,
                    kForwardSequence);

            float* bw_output_activation_state_buffer;
            float* bw_output_cell_state_buffer;
            std::vector<float> bw_output_activation_state;
            std::vector<float> bw_output_cell_state;
            if (params_.output_state) {
                bw_output_activation_state_buffer = GetBuffer<float>(bw_output_activation_state_);
                bw_output_cell_state_buffer = GetBuffer<float>(bw_output_cell_state_);
            } else {
                bw_output_activation_state.resize(
                        getNumberOfElements(bw_activation_state_->shape()));
                bw_output_cell_state.resize(getNumberOfElements(bw_cell_state_->shape()));

                bw_output_activation_state_buffer = bw_output_activation_state.data();
                bw_output_cell_state_buffer = bw_output_cell_state.data();
            }
            std::vector<float> bw_scratch_buffer(getNumberOfElements(bw_scratch_shape_));
            const bool kBackwardSequence = false;
            LSTMCell::LSTMEvalFloat32(
                    params_, bwInput, bwInputShape,
                    GetBuffer<const float>(bw_input_to_input_weights_),
                    GetBuffer<const float>(bw_input_to_forget_weights_),
                    GetBuffer<const float>(bw_input_to_cell_weights_),
                    GetBuffer<const float>(bw_input_to_output_weights_),
                    bw_input_to_output_weights_->shape(),
                    GetBuffer<const float>(bw_recurrent_to_input_weights_),
                    GetBuffer<const float>(bw_recurrent_to_forget_weights_),
                    GetBuffer<const float>(bw_recurrent_to_cell_weights_),
                    GetBuffer<const float>(bw_recurrent_to_output_weights_),
                    bw_recurrent_to_output_weights_->shape(),
                    GetBuffer<const float>(bw_cell_to_input_weights_),
                    GetBuffer<const float>(bw_cell_to_forget_weights_),
                    GetBuffer<const float>(bw_cell_to_output_weights_), auxInput,
                    GetOptionalBuffer<const float>(bw_aux_input_to_input_weights_),
                    GetOptionalBuffer<const float>(bw_aux_input_to_forget_weights_),
                    GetOptionalBuffer<const float>(bw_aux_input_to_cell_weights_),
                    GetOptionalBuffer<const float>(bw_aux_input_to_output_weights_),
                    GetBuffer<const float>(bw_input_gate_bias_),
                    GetBuffer<const float>(bw_forget_gate_bias_),
                    GetBuffer<const float>(bw_cell_bias_),
                    GetBuffer<const float>(bw_output_gate_bias_),
                    GetBuffer<const float>(bw_projection_weights_),
                    GetBuffer<const float>(bw_projection_bias_),
                    GetBuffer<const float>(bw_activation_state_),
                    GetBuffer<const float>(bw_cell_state_),
                    GetOptionalBuffer<const float>(bw_input_layer_norm_weights_),
                    GetOptionalBuffer<const float>(bw_forget_layer_norm_weights_),
                    GetOptionalBuffer<const float>(bw_cell_layer_norm_weights_),
                    GetOptionalBuffer<const float>(bw_output_layer_norm_weights_),
                    bw_output_activation_state_buffer, bw_output_cell_state_buffer,
                    params_.merge_outputs ? GetBuffer<float>(fw_output_) + n_fw_output_elements
                                          : GetBuffer<float>(bw_output_),
                    bw_scratch_buffer.data(), params_.time_major, kBackwardSequence);
            if (params_.merge_outputs) {
                std::vector<float> temp(n_output_elements);
                mergeThirdDimension(GetBuffer<float>(fw_output_), fw_output_dims,
                                    GetBuffer<float>(fw_output_) + n_fw_output_elements,
                                    bw_output_dims, temp.data());
                std::copy(temp.data(), temp.data() + n_output_elements,
                          GetBuffer<float>(fw_output_));
            }
        } break;
        case OperandType::TENSOR_FLOAT16: {
            const _Float16* bwInput = GetBuffer<const _Float16>(input_);
            Shape bwInputShape = input_->shape();
            const _Float16* auxInput = GetOptionalBuffer<const _Float16>(aux_input_);
            if (linkingMode == LinkingMode::PARALLEL_LINKING) {
                bwInput = GetBuffer<const _Float16>(aux_input_);
                bwInputShape = aux_input_->shape();
                auxInput = nullptr;
            }

            _Float16* fw_output_activation_state_buffer;
            _Float16* fw_output_cell_state_buffer;
            std::vector<_Float16> fw_output_activation_state;
            std::vector<_Float16> fw_output_cell_state;
            if (params_.output_state) {
                fw_output_activation_state_buffer =
                        GetBuffer<_Float16>(fw_output_activation_state_);
                fw_output_cell_state_buffer = GetBuffer<_Float16>(fw_output_cell_state_);
            } else {
                fw_output_activation_state.resize(
                        getNumberOfElements(fw_activation_state_->shape()));
                fw_output_cell_state.resize(getNumberOfElements(fw_cell_state_->shape()));

                fw_output_activation_state_buffer = fw_output_activation_state.data();
                fw_output_cell_state_buffer = fw_output_cell_state.data();
            }
            std::vector<_Float16> fw_scratch_buffer(getNumberOfElements(fw_scratch_shape_));
            const bool kForwardSequence = true;
            LSTMCell::LSTMEvalFloat16(
                    params_, GetBuffer<const _Float16>(input_), input_->shape(),
                    GetOptionalBuffer<const _Float16>(fw_input_to_input_weights_),
                    GetBuffer<const _Float16>(fw_input_to_forget_weights_),
                    GetBuffer<const _Float16>(fw_input_to_cell_weights_),
                    GetBuffer<const _Float16>(fw_input_to_output_weights_),
                    fw_input_to_output_weights_->shape(),
                    GetOptionalBuffer<const _Float16>(fw_recurrent_to_input_weights_),
                    GetBuffer<const _Float16>(fw_recurrent_to_forget_weights_),
                    GetBuffer<const _Float16>(fw_recurrent_to_cell_weights_),
                    GetBuffer<const _Float16>(fw_recurrent_to_output_weights_),
                    fw_recurrent_to_output_weights_->shape(),
                    GetOptionalBuffer<const _Float16>(fw_cell_to_input_weights_),
                    GetOptionalBuffer<const _Float16>(fw_cell_to_forget_weights_),
                    GetOptionalBuffer<const _Float16>(fw_cell_to_output_weights_), auxInput,
                    GetOptionalBuffer<const _Float16>(fw_aux_input_to_input_weights_),
                    GetOptionalBuffer<const _Float16>(fw_aux_input_to_forget_weights_),
                    GetOptionalBuffer<const _Float16>(fw_aux_input_to_cell_weights_),
                    GetOptionalBuffer<const _Float16>(fw_aux_input_to_output_weights_),
                    GetOptionalBuffer<const _Float16>(fw_input_gate_bias_),
                    GetBuffer<const _Float16>(fw_forget_gate_bias_),
                    GetBuffer<const _Float16>(fw_cell_bias_),
                    GetBuffer<const _Float16>(fw_output_gate_bias_),
                    GetOptionalBuffer<const _Float16>(fw_projection_weights_),
                    GetOptionalBuffer<const _Float16>(fw_projection_bias_),
                    GetBuffer<const _Float16>(fw_activation_state_),
                    GetBuffer<const _Float16>(fw_cell_state_),
                    GetOptionalBuffer<const _Float16>(fw_input_layer_norm_weights_),
                    GetOptionalBuffer<const _Float16>(fw_forget_layer_norm_weights_),
                    GetOptionalBuffer<const _Float16>(fw_cell_layer_norm_weights_),
                    GetOptionalBuffer<const _Float16>(fw_output_layer_norm_weights_),
                    fw_output_activation_state_buffer, fw_output_cell_state_buffer,
                    GetBuffer<_Float16>(fw_output_), fw_scratch_buffer.data(), params_.time_major,
                    kForwardSequence);

            _Float16* bw_output_activation_state_buffer;
            _Float16* bw_output_cell_state_buffer;
            std::vector<_Float16> bw_output_activation_state;
            std::vector<_Float16> bw_output_cell_state;
            if (params_.output_state) {
                bw_output_activation_state_buffer =
                        GetBuffer<_Float16>(bw_output_activation_state_);
                bw_output_cell_state_buffer = GetBuffer<_Float16>(bw_output_cell_state_);
            } else {
                bw_output_activation_state.resize(
                        getNumberOfElements(bw_activation_state_->shape()));
                bw_output_cell_state.resize(getNumberOfElements(bw_cell_state_->shape()));

                bw_output_activation_state_buffer = bw_output_activation_state.data();
                bw_output_cell_state_buffer = bw_output_cell_state.data();
            }
            std::vector<_Float16> bw_scratch_buffer(getNumberOfElements(bw_scratch_shape_));
            const bool kBackwardSequence = false;
            LSTMCell::LSTMEvalFloat16(
                    params_, bwInput, bwInputShape,
                    GetOptionalBuffer<const _Float16>(bw_input_to_input_weights_),
                    GetBuffer<const _Float16>(bw_input_to_forget_weights_),
                    GetBuffer<const _Float16>(bw_input_to_cell_weights_),
                    GetBuffer<const _Float16>(bw_input_to_output_weights_),
                    bw_input_to_output_weights_->shape(),
                    GetOptionalBuffer<const _Float16>(bw_recurrent_to_input_weights_),
                    GetBuffer<const _Float16>(bw_recurrent_to_forget_weights_),
                    GetBuffer<const _Float16>(bw_recurrent_to_cell_weights_),
                    GetBuffer<const _Float16>(bw_recurrent_to_output_weights_),
                    bw_recurrent_to_output_weights_->shape(),
                    GetOptionalBuffer<const _Float16>(bw_cell_to_input_weights_),
                    GetOptionalBuffer<const _Float16>(bw_cell_to_forget_weights_),
                    GetOptionalBuffer<const _Float16>(bw_cell_to_output_weights_), auxInput,
                    GetOptionalBuffer<const _Float16>(bw_aux_input_to_input_weights_),
                    GetOptionalBuffer<const _Float16>(bw_aux_input_to_forget_weights_),
                    GetOptionalBuffer<const _Float16>(bw_aux_input_to_cell_weights_),
                    GetOptionalBuffer<const _Float16>(bw_aux_input_to_output_weights_),
                    GetOptionalBuffer<const _Float16>(bw_input_gate_bias_),
                    GetBuffer<const _Float16>(bw_forget_gate_bias_),
                    GetBuffer<const _Float16>(bw_cell_bias_),
                    GetBuffer<const _Float16>(bw_output_gate_bias_),
                    GetOptionalBuffer<const _Float16>(bw_projection_weights_),
                    GetOptionalBuffer<const _Float16>(bw_projection_bias_),
                    GetBuffer<const _Float16>(bw_activation_state_),
                    GetBuffer<const _Float16>(bw_cell_state_),
                    GetOptionalBuffer<const _Float16>(bw_input_layer_norm_weights_),
                    GetOptionalBuffer<const _Float16>(bw_forget_layer_norm_weights_),
                    GetOptionalBuffer<const _Float16>(bw_cell_layer_norm_weights_),
                    GetOptionalBuffer<const _Float16>(bw_output_layer_norm_weights_),
                    bw_output_activation_state_buffer, bw_output_cell_state_buffer,
                    params_.merge_outputs ? GetBuffer<_Float16>(fw_output_) + n_fw_output_elements
                                          : GetBuffer<_Float16>(bw_output_),
                    bw_scratch_buffer.data(), params_.time_major, kBackwardSequence);
            if (params_.merge_outputs) {
                std::vector<_Float16> temp(n_output_elements);
                mergeThirdDimension(GetBuffer<_Float16>(fw_output_), fw_output_dims,
                                    GetBuffer<_Float16>(fw_output_) + n_fw_output_elements,
                                    bw_output_dims, temp.data());
                std::copy(temp.data(), temp.data() + n_output_elements,
                          GetBuffer<_Float16>(fw_output_));
            }
        } break;
        default: {
            LOG(ERROR) << "Unsupported data type: " << static_cast<int>(input_->type);
            return false;
        }
    }
    return true;
}

}  // namespace nn
}  // namespace android
