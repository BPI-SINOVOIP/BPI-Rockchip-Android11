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

#include <algorithm>

#include "fuzzing/operation_signatures/OperationSignatureUtils.h"

namespace android {
namespace nn {
namespace fuzzing_test {

static void broadcastOpConstructor(TestOperandType dataType, uint32_t rank, RandomOperation* op) {
    const uint32_t rank2 = getUniform(1u, rank), rankDiff = rank - rank2;
    op->inputs[0]->dimensions.resize(rank);
    op->inputs[1]->dimensions.resize(rank2);
    op->outputs[0]->dimensions.resize(rank);
    for (uint32_t i = 0; i < rank; i++) {
        op->outputs[0]->dimensions[i] = RandomVariableType::FREE;
        if (i < rankDiff) {
            op->inputs[0]->dimensions[i] = op->outputs[0]->dimensions[i];
        } else {
            if (getBernoulli(0.5f)) {
                // No broadcast on this dimension.
                op->inputs[0]->dimensions[i] = op->outputs[0]->dimensions[i];
                op->inputs[1]->dimensions[i - rankDiff] = op->outputs[0]->dimensions[i];
            } else if (getBernoulli(0.5f)) {
                // input0 broadcast on this dimension.
                op->inputs[0]->dimensions[i] = 1;
                op->inputs[1]->dimensions[i - rankDiff] = op->outputs[0]->dimensions[i];
            } else {
                // input1 broadcast on this dimension.
                op->inputs[0]->dimensions[i] = op->outputs[0]->dimensions[i];
                op->inputs[1]->dimensions[i - rankDiff] = 1;
            }
        }
    }
    if (getBernoulli(0.5f)) {
        // Swap the dimensions to test the case that inpuy 1 has a larger rank than input0.
        op->inputs[0]->dimensions.swap(op->inputs[1]->dimensions);
    }

    // MUL requires output.scale > input0.scale * input1.scale.
    if (isQuantizedType(dataType) && op->opType == TestOperationType::MUL) {
        float minScale = op->inputs[0]->scale * op->inputs[1]->scale;
        op->outputs[0]->scale = getUniform(minScale, minScale * 5);
    }

    // DIV and POW may produce Inf output values. We should not connect this output tensor to the
    // input of another operation.
    if (op->opType == TestOperationType::DIV || op->opType == TestOperationType::POW) {
        op->outputs[0]->doNotConnect = true;
    }

    // For ADD/MUL/SUB/DIV with TENSOR_INT32 tensors, the activation must be "NONE".
    if ((op->opType == TestOperationType::ADD || op->opType == TestOperationType::MUL ||
         op->opType == TestOperationType::SUB || op->opType == TestOperationType::DIV) &&
        dataType == TestOperandType::TENSOR_INT32) {
        op->inputs[2]->setScalarValue(0);
    }

    if (op->opType == TestOperationType::DIV) {
        op->inputs[1]->valueProperties = RandomOperand::NON_ZERO;
    }

    if (op->opType == TestOperationType::POW) {
        op->inputs[0]->valueProperties = RandomOperand::NON_NEGATIVE;
    }
}

// For broadcast operations with fused activation.
#define DEFINE_BROADCAST_WITH_ACT_SIGNATURE(op, ver, ...)                     \
    DEFINE_OPERATION_SIGNATURE(op##_##ver){                                   \
            .opType = TestOperationType::op,                                  \
            .supportedDataTypes = {__VA_ARGS__},                              \
            .supportedRanks = {1, 2, 3, 4},                                   \
            .version = TestHalVersion::ver,                                   \
            .inputs = {INPUT_DEFAULT, INPUT_DEFAULT,                          \
                       PARAMETER_CHOICE(TestOperandType::INT32, 0, 1, 2, 3)}, \
            .outputs = {OUTPUT_DEFAULT},                                      \
            .constructor = broadcastOpConstructor};

// Arithmetic with activation.
DEFINE_BROADCAST_WITH_ACT_SIGNATURE(ADD, V1_0, TestOperandType::TENSOR_FLOAT32,
                                    TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_BROADCAST_WITH_ACT_SIGNATURE(MUL, V1_0, TestOperandType::TENSOR_FLOAT32,
                                    TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_BROADCAST_WITH_ACT_SIGNATURE(SUB, V1_1, TestOperandType::TENSOR_FLOAT32);
DEFINE_BROADCAST_WITH_ACT_SIGNATURE(DIV, V1_1, TestOperandType::TENSOR_FLOAT32);
DEFINE_BROADCAST_WITH_ACT_SIGNATURE(ADD, V1_2, TestOperandType::TENSOR_FLOAT16);
DEFINE_BROADCAST_WITH_ACT_SIGNATURE(MUL, V1_2, TestOperandType::TENSOR_FLOAT16);
DEFINE_BROADCAST_WITH_ACT_SIGNATURE(SUB, V1_2, TestOperandType::TENSOR_FLOAT16,
                                    TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_BROADCAST_WITH_ACT_SIGNATURE(DIV, V1_2, TestOperandType::TENSOR_FLOAT16);
DEFINE_BROADCAST_WITH_ACT_SIGNATURE(ADD, V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                                    TestOperandType::TENSOR_INT32);
DEFINE_BROADCAST_WITH_ACT_SIGNATURE(MUL, V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                                    TestOperandType::TENSOR_INT32);
DEFINE_BROADCAST_WITH_ACT_SIGNATURE(SUB, V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                                    TestOperandType::TENSOR_INT32);
DEFINE_BROADCAST_WITH_ACT_SIGNATURE(DIV, V1_3, TestOperandType::TENSOR_INT32);

// For broadcast ops with output of the same data type as inputs.
#define DEFINE_BROADCAST_SIGNATURE(op, ver, ...)                                     \
    DEFINE_OPERATION_SIGNATURE(op##_##ver){.opType = TestOperationType::op,          \
                                           .supportedDataTypes = {__VA_ARGS__},      \
                                           .supportedRanks = {1, 2, 3, 4, 5},        \
                                           .version = TestHalVersion::ver,           \
                                           .inputs = {INPUT_DEFAULT, INPUT_DEFAULT}, \
                                           .outputs = {OUTPUT_DEFAULT},              \
                                           .constructor = broadcastOpConstructor};

// Arithmetic without activation.
DEFINE_BROADCAST_SIGNATURE(POW, V1_2, TestOperandType::TENSOR_FLOAT32,
                           TestOperandType::TENSOR_FLOAT16);
DEFINE_BROADCAST_SIGNATURE(PRELU, V1_2, TestOperandType::TENSOR_FLOAT32,
                           TestOperandType::TENSOR_FLOAT16, TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_BROADCAST_SIGNATURE(MAXIMUM, V1_2, TestOperandType::TENSOR_FLOAT32,
                           TestOperandType::TENSOR_FLOAT16, TestOperandType::TENSOR_QUANT8_ASYMM,
                           TestOperandType::TENSOR_INT32);
DEFINE_BROADCAST_SIGNATURE(MINIMUM, V1_2, TestOperandType::TENSOR_FLOAT32,
                           TestOperandType::TENSOR_FLOAT16, TestOperandType::TENSOR_QUANT8_ASYMM,
                           TestOperandType::TENSOR_INT32);
DEFINE_BROADCAST_SIGNATURE(PRELU, V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);
DEFINE_BROADCAST_SIGNATURE(MAXIMUM, V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);
DEFINE_BROADCAST_SIGNATURE(MINIMUM, V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

// Logical
DEFINE_BROADCAST_SIGNATURE(LOGICAL_AND, V1_2, TestOperandType::TENSOR_BOOL8);
DEFINE_BROADCAST_SIGNATURE(LOGICAL_OR, V1_2, TestOperandType::TENSOR_BOOL8);

// Comparisons
#define DEFINE_COMPARISON_SIGNATURE(op, ver, ...)                     \
    DEFINE_OPERATION_SIGNATURE(op##_##ver){                           \
            .opType = TestOperationType::op,                          \
            .supportedDataTypes = {__VA_ARGS__},                      \
            .supportedRanks = {1, 2, 3, 4},                           \
            .version = TestHalVersion::ver,                           \
            .inputs = {INPUT_DEFAULT, INPUT_DEFAULT},                 \
            .outputs = {OUTPUT_TYPED(TestOperandType::TENSOR_BOOL8)}, \
            .constructor = broadcastOpConstructor};

DEFINE_COMPARISON_SIGNATURE(EQUAL, V1_2, TestOperandType::TENSOR_FLOAT32,
                            TestOperandType::TENSOR_FLOAT16, TestOperandType::TENSOR_INT32,
                            TestOperandType::TENSOR_QUANT8_ASYMM, TestOperandType::TENSOR_BOOL8);
DEFINE_COMPARISON_SIGNATURE(GREATER, V1_2, TestOperandType::TENSOR_FLOAT32,
                            TestOperandType::TENSOR_FLOAT16, TestOperandType::TENSOR_INT32,
                            TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_COMPARISON_SIGNATURE(GREATER_EQUAL, V1_2, TestOperandType::TENSOR_FLOAT32,
                            TestOperandType::TENSOR_FLOAT16, TestOperandType::TENSOR_INT32,
                            TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_COMPARISON_SIGNATURE(LESS, V1_2, TestOperandType::TENSOR_FLOAT32,
                            TestOperandType::TENSOR_FLOAT16, TestOperandType::TENSOR_INT32,
                            TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_COMPARISON_SIGNATURE(LESS_EQUAL, V1_2, TestOperandType::TENSOR_FLOAT32,
                            TestOperandType::TENSOR_FLOAT16, TestOperandType::TENSOR_INT32,
                            TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_COMPARISON_SIGNATURE(NOT_EQUAL, V1_2, TestOperandType::TENSOR_FLOAT32,
                            TestOperandType::TENSOR_FLOAT16, TestOperandType::TENSOR_INT32,
                            TestOperandType::TENSOR_QUANT8_ASYMM, TestOperandType::TENSOR_BOOL8);
DEFINE_COMPARISON_SIGNATURE(EQUAL, V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);
DEFINE_COMPARISON_SIGNATURE(GREATER, V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);
DEFINE_COMPARISON_SIGNATURE(GREATER_EQUAL, V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);
DEFINE_COMPARISON_SIGNATURE(LESS, V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);
DEFINE_COMPARISON_SIGNATURE(LESS_EQUAL, V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);
DEFINE_COMPARISON_SIGNATURE(NOT_EQUAL, V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

}  // namespace fuzzing_test
}  // namespace nn
}  // namespace android
