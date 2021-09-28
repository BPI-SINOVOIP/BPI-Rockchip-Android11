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

#ifndef ANDROID_FRAMEWORKS_ML_NN_COMMON_OPERATIONS_QUANTIZED_LSTM_H
#define ANDROID_FRAMEWORKS_ML_NN_COMMON_OPERATIONS_QUANTIZED_LSTM_H

#include <vector>

#include "OperationsUtils.h"

namespace android {
namespace nn {

struct RunTimeOperandInfo;

class QuantizedLSTMCell {
   public:
    QuantizedLSTMCell(const hal::Operation& operation, RunTimeOperandInfo* operands);

    static bool prepare(const hal::Operation& operation, RunTimeOperandInfo* operands,
                        Shape* cellStateShape, Shape* outputShape);
    bool eval();

    // Inputs:
    static constexpr int kInputTensor = 0;
    // Input weight tensors of size: {n_cell, n_input}
    static constexpr int kInputToInputWeightsTensor = 1;
    static constexpr int kInputToForgetWeightsTensor = 2;
    static constexpr int kInputToCellWeightsTensor = 3;
    static constexpr int kInputToOutputWeightsTensor = 4;

    // Recurrent weight tensors of size {n_cell, n_output}
    static constexpr int kRecurrentToInputWeightsTensor = 5;
    static constexpr int kRecurrentToForgetWeightsTensor = 6;
    static constexpr int kRecurrentToCellWeightsTensor = 7;
    static constexpr int kRecurrentToOutputWeightsTensor = 8;

    // Gates bias tensors of size {n_cell}
    static constexpr int kInputGateBiasTensor = 9;
    static constexpr int kForgetGateBiasTensor = 10;
    static constexpr int kCellGateBiasTensor = 11;
    static constexpr int kOutputGateBiasTensor = 12;

    static constexpr int kPrevCellStateTensor = 13;
    static constexpr int kPrevOutputTensor = 14;

    // Outputs:
    static constexpr int kCellStateOutTensor = 0;
    static constexpr int kOutputTensor = 1;

   private:
    const RunTimeOperandInfo* input_;

    const RunTimeOperandInfo* inputToInputWeights_;
    const RunTimeOperandInfo* inputToForgetWeights_;
    const RunTimeOperandInfo* inputToCellWeights_;
    const RunTimeOperandInfo* inputToOutputWeights_;

    const RunTimeOperandInfo* recurrentToInputWeights_;
    const RunTimeOperandInfo* recurrentToForgetWeights_;
    const RunTimeOperandInfo* recurrentToCellWeights_;
    const RunTimeOperandInfo* recurrentToOutputWeights_;

    const RunTimeOperandInfo* inputGateBias_;
    const RunTimeOperandInfo* forgetGateBias_;
    const RunTimeOperandInfo* cellGateBias_;
    const RunTimeOperandInfo* outputGateBias_;

    const RunTimeOperandInfo* prevCellState_;
    const RunTimeOperandInfo* prevOutput_;

    RunTimeOperandInfo* cellStateOut_;
    RunTimeOperandInfo* output_;

    void concatenateWeights(const std::vector<uint32_t>& weightsDims, uint8_t* weights);
    void concatenateBiases(uint32_t outputSize, int32_t* bias);
};

}  // namespace nn
}  // namespace android

#endif  // ANDROID_FRAMEWORKS_ML_NN_COMMON_OPERATIONS_QUANTIZED_LSTM_H
