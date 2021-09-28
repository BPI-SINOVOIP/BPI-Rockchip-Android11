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

#include "fuzzing/operation_signatures/OperationSignatureUtils.h"

namespace android {
namespace nn {
namespace fuzzing_test {

static void elementwiseOpConstructor(TestOperandType dataType, uint32_t rank, RandomOperation* op) {
    sameShapeOpConstructor(dataType, rank, op);

    switch (op->opType) {
        case TestOperationType::RELU:
        case TestOperationType::RELU6:
            op->outputs[0]->valueProperties = RandomOperand::NON_NEGATIVE;
            break;
        case TestOperationType::LOGISTIC:
            op->outputs[0]->valueProperties = RandomOperand::NON_ZERO | RandomOperand::NON_NEGATIVE;
            break;
        case TestOperationType::ABS:
            op->outputs[0]->valueProperties = RandomOperand::NON_NEGATIVE;
            break;
        case TestOperationType::EXP:
            op->outputs[0]->valueProperties = RandomOperand::NON_ZERO | RandomOperand::NON_NEGATIVE;
            break;
        case TestOperationType::LOG:
            op->inputs[0]->valueProperties = RandomOperand::NON_ZERO | RandomOperand::NON_NEGATIVE;
            break;
        case TestOperationType::RSQRT:
            op->inputs[0]->valueProperties = RandomOperand::NON_ZERO | RandomOperand::NON_NEGATIVE;
            op->outputs[0]->valueProperties = RandomOperand::NON_ZERO | RandomOperand::NON_NEGATIVE;
            break;
        case TestOperationType::SQRT:
            op->inputs[0]->valueProperties = RandomOperand::NON_NEGATIVE;
            op->outputs[0]->valueProperties = RandomOperand::NON_NEGATIVE;
            break;
        default:
            break;
    }
}

#define DEFINE_ELEMENTWISE_SIGNATURE(op, ver, ...)                              \
    DEFINE_OPERATION_SIGNATURE(op##_##ver){.opType = TestOperationType::op,     \
                                           .supportedDataTypes = {__VA_ARGS__}, \
                                           .supportedRanks = {1, 2, 3, 4},      \
                                           .version = TestHalVersion::ver,      \
                                           .inputs = {INPUT_DEFAULT},           \
                                           .outputs = {OUTPUT_DEFAULT},         \
                                           .constructor = elementwiseOpConstructor};

DEFINE_ELEMENTWISE_SIGNATURE(FLOOR, V1_0, TestOperandType::TENSOR_FLOAT32);
DEFINE_ELEMENTWISE_SIGNATURE(RELU, V1_0, TestOperandType::TENSOR_FLOAT32,
                             TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_ELEMENTWISE_SIGNATURE(RELU1, V1_0, TestOperandType::TENSOR_FLOAT32,
                             TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_ELEMENTWISE_SIGNATURE(RELU6, V1_0, TestOperandType::TENSOR_FLOAT32,
                             TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_ELEMENTWISE_SIGNATURE(TANH, V1_0, TestOperandType::TENSOR_FLOAT32);
DEFINE_ELEMENTWISE_SIGNATURE(FLOOR, V1_2, TestOperandType::TENSOR_FLOAT16);
DEFINE_ELEMENTWISE_SIGNATURE(LOGISTIC, V1_2, TestOperandType::TENSOR_FLOAT16);
DEFINE_ELEMENTWISE_SIGNATURE(RELU, V1_2, TestOperandType::TENSOR_FLOAT16);
DEFINE_ELEMENTWISE_SIGNATURE(RELU1, V1_2, TestOperandType::TENSOR_FLOAT16);
DEFINE_ELEMENTWISE_SIGNATURE(RELU6, V1_2, TestOperandType::TENSOR_FLOAT16);
DEFINE_ELEMENTWISE_SIGNATURE(RELU, V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);
DEFINE_ELEMENTWISE_SIGNATURE(RELU1, V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);
DEFINE_ELEMENTWISE_SIGNATURE(RELU6, V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);
DEFINE_ELEMENTWISE_SIGNATURE(HARD_SWISH, V1_3, TestOperandType::TENSOR_FLOAT32,
                             TestOperandType::TENSOR_FLOAT16, TestOperandType::TENSOR_QUANT8_ASYMM);

#define DEFINE_ELEMENTWISE_SIGNATURE_WITH_RANK5(op, ver, ...)                   \
    DEFINE_OPERATION_SIGNATURE(op##_##ver){.opType = TestOperationType::op,     \
                                           .supportedDataTypes = {__VA_ARGS__}, \
                                           .supportedRanks = {1, 2, 3, 4, 5},   \
                                           .version = TestHalVersion::ver,      \
                                           .inputs = {INPUT_DEFAULT},           \
                                           .outputs = {OUTPUT_DEFAULT},         \
                                           .constructor = elementwiseOpConstructor};

DEFINE_ELEMENTWISE_SIGNATURE_WITH_RANK5(ABS, V1_2, TestOperandType::TENSOR_FLOAT32,
                                        TestOperandType::TENSOR_FLOAT16);
DEFINE_ELEMENTWISE_SIGNATURE_WITH_RANK5(EXP, V1_2, TestOperandType::TENSOR_FLOAT32,
                                        TestOperandType::TENSOR_FLOAT16);
DEFINE_ELEMENTWISE_SIGNATURE_WITH_RANK5(NEG, V1_2, TestOperandType::TENSOR_FLOAT32,
                                        TestOperandType::TENSOR_FLOAT16,
                                        TestOperandType::TENSOR_INT32);
DEFINE_ELEMENTWISE_SIGNATURE_WITH_RANK5(SIN, V1_2, TestOperandType::TENSOR_FLOAT32,
                                        TestOperandType::TENSOR_FLOAT16);
DEFINE_ELEMENTWISE_SIGNATURE_WITH_RANK5(LOGICAL_NOT, V1_2, TestOperandType::TENSOR_BOOL8);
DEFINE_ELEMENTWISE_SIGNATURE_WITH_RANK5(ABS, V1_3, TestOperandType::TENSOR_INT32);

DEFINE_ELEMENTWISE_SIGNATURE_WITH_RANK5(LOG, V1_2, TestOperandType::TENSOR_FLOAT32,
                                        TestOperandType::TENSOR_FLOAT16);
DEFINE_ELEMENTWISE_SIGNATURE_WITH_RANK5(RSQRT, V1_2, TestOperandType::TENSOR_FLOAT32,
                                        TestOperandType::TENSOR_FLOAT16);
DEFINE_ELEMENTWISE_SIGNATURE_WITH_RANK5(SQRT, V1_2, TestOperandType::TENSOR_FLOAT32,
                                        TestOperandType::TENSOR_FLOAT16);

// Quantized operations with special output quantization parameters.
#define DEFINE_ELEMENTWISE_WITH_QUANT_OUTPUT_SIGNATURE(op, ver, s, z, ...)      \
    DEFINE_OPERATION_SIGNATURE(op##_##ver){.opType = TestOperationType::op,     \
                                           .supportedDataTypes = {__VA_ARGS__}, \
                                           .supportedRanks = {1, 2, 3, 4},      \
                                           .version = TestHalVersion::ver,      \
                                           .inputs = {INPUT_DEFAULT},           \
                                           .outputs = {OUTPUT_QUANT((s), (z))}, \
                                           .constructor = sameDimensionOpConstructor};

DEFINE_ELEMENTWISE_WITH_QUANT_OUTPUT_SIGNATURE(LOGISTIC, V1_0, /*scale=*/1.f / 256, /*zeroPoint=*/0,
                                               TestOperandType::TENSOR_FLOAT32,
                                               TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_ELEMENTWISE_WITH_QUANT_OUTPUT_SIGNATURE(TANH, V1_2, /*scale=*/1.f / 128, /*zeroPoint=*/128,
                                               TestOperandType::TENSOR_FLOAT16,
                                               TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_ELEMENTWISE_WITH_QUANT_OUTPUT_SIGNATURE(LOGISTIC, V1_3, /*scale=*/1.f / 256,
                                               /*zeroPoint=*/-128,
                                               TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);
DEFINE_ELEMENTWISE_WITH_QUANT_OUTPUT_SIGNATURE(TANH, V1_3, /*scale=*/1.f / 128, /*zeroPoint=*/0,
                                               TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

static void castingOpConstructor(TestOperandType dataType, uint32_t rank, RandomOperation* op) {
    sameDimensionOpConstructor(dataType, rank, op);

    // If it is casting to/from a FP16 data type, the source/destination should have a scale
    // representable in FP16 to avoid precision loss.
    if (op->inputs[0]->dataType == TestOperandType::TENSOR_FLOAT16) {
        op->outputs[0]->scale = static_cast<_Float16>(op->outputs[0]->scale);
    } else if (op->outputs[0]->dataType == TestOperandType::TENSOR_FLOAT16) {
        op->inputs[0]->scale = static_cast<_Float16>(op->inputs[0]->scale);
    }
}

// Operations with output data type different from input.
#define DEFINE_QUANTIZATION_OP_SIGNATURE(op, ver, outType, ...)  \
    DEFINE_OPERATION_SIGNATURE(op##_##outType##_##ver){          \
            .opType = TestOperationType::op,                     \
            .supportedDataTypes = {__VA_ARGS__},                 \
            .supportedRanks = {1, 2, 3, 4},                      \
            .version = TestHalVersion::ver,                      \
            .inputs = {INPUT_DEFAULT},                           \
            .outputs = {OUTPUT_TYPED(TestOperandType::outType)}, \
            .constructor = castingOpConstructor};

DEFINE_QUANTIZATION_OP_SIGNATURE(DEQUANTIZE, V1_0, /*outType=*/TENSOR_FLOAT32,
                                 TestOperandType::TENSOR_QUANT8_ASYMM);

DEFINE_QUANTIZATION_OP_SIGNATURE(DEQUANTIZE, V1_2, /*outType=*/TENSOR_FLOAT32,
                                 TestOperandType::TENSOR_QUANT8_SYMM);

DEFINE_QUANTIZATION_OP_SIGNATURE(DEQUANTIZE, V1_2, /*outType=*/TENSOR_FLOAT16,
                                 TestOperandType::TENSOR_QUANT8_ASYMM,
                                 TestOperandType::TENSOR_QUANT8_SYMM);

DEFINE_QUANTIZATION_OP_SIGNATURE(DEQUANTIZE, V1_3, /*outType=*/TENSOR_FLOAT32,
                                 TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

DEFINE_QUANTIZATION_OP_SIGNATURE(DEQUANTIZE, V1_3, /*outType=*/TENSOR_FLOAT16,
                                 TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

DEFINE_QUANTIZATION_OP_SIGNATURE(QUANTIZE, V1_2, /*outType=*/TENSOR_QUANT8_ASYMM,
                                 TestOperandType::TENSOR_FLOAT32, TestOperandType::TENSOR_FLOAT16);

DEFINE_QUANTIZATION_OP_SIGNATURE(QUANTIZE, V1_3,
                                 /*outType=*/TENSOR_QUANT8_ASYMM_SIGNED,
                                 TestOperandType::TENSOR_FLOAT32, TestOperandType::TENSOR_FLOAT16);

#define DEFINE_CAST_SIGNATURE(ver, outType, ...)                 \
    DEFINE_OPERATION_SIGNATURE(CAST_##outType##_##ver){          \
            .opType = TestOperationType::CAST,                   \
            .supportedDataTypes = {__VA_ARGS__},                 \
            .supportedRanks = {1, 2, 3, 4, 5},                   \
            .version = TestHalVersion::ver,                      \
            .inputs = {INPUT_DEFAULT},                           \
            .outputs = {OUTPUT_TYPED(TestOperandType::outType)}, \
            .constructor = castingOpConstructor};

DEFINE_CAST_SIGNATURE(V1_2, /*outType=*/TENSOR_FLOAT32, TestOperandType::TENSOR_FLOAT32,
                      TestOperandType::TENSOR_FLOAT16, TestOperandType::TENSOR_QUANT8_ASYMM,
                      TestOperandType::TENSOR_INT32);

DEFINE_CAST_SIGNATURE(V1_2, /*outType=*/TENSOR_FLOAT16, TestOperandType::TENSOR_FLOAT32,
                      TestOperandType::TENSOR_FLOAT16, TestOperandType::TENSOR_QUANT8_ASYMM,
                      TestOperandType::TENSOR_INT32);

DEFINE_CAST_SIGNATURE(V1_2, /*outType=*/TENSOR_QUANT8_ASYMM, TestOperandType::TENSOR_FLOAT32,
                      TestOperandType::TENSOR_FLOAT16, TestOperandType::TENSOR_QUANT8_ASYMM,
                      TestOperandType::TENSOR_INT32);

DEFINE_CAST_SIGNATURE(V1_2, /*outType=*/TENSOR_INT32, TestOperandType::TENSOR_FLOAT32,
                      TestOperandType::TENSOR_FLOAT16, TestOperandType::TENSOR_QUANT8_ASYMM,
                      TestOperandType::TENSOR_INT32);

DEFINE_CAST_SIGNATURE(V1_3, /*outType=*/TENSOR_BOOL8, TestOperandType::TENSOR_BOOL8);
DEFINE_CAST_SIGNATURE(V1_3, /*outType=*/TENSOR_INT32, TestOperandType::TENSOR_INT32);
DEFINE_CAST_SIGNATURE(V1_3, /*outType=*/TENSOR_QUANT16_ASYMM,
                      TestOperandType::TENSOR_QUANT16_ASYMM);
DEFINE_CAST_SIGNATURE(V1_3, /*outType=*/TENSOR_QUANT16_SYMM, TestOperandType::TENSOR_QUANT16_SYMM);
DEFINE_CAST_SIGNATURE(V1_3, /*outType=*/TENSOR_QUANT8_ASYMM_SIGNED,
                      TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);
DEFINE_CAST_SIGNATURE(V1_3, /*outType=*/TENSOR_QUANT8_SYMM, TestOperandType::TENSOR_QUANT8_SYMM);

DEFINE_OPERATION_SIGNATURE(ELU_V1_3){
        .opType = TestOperationType::ELU,
        .supportedDataTypes = {TestOperandType::TENSOR_FLOAT32, TestOperandType::TENSOR_FLOAT16},
        .supportedRanks = {1, 2, 3, 4, 5},
        .version = TestHalVersion::V1_3,
        .inputs = {INPUT_DEFAULT, PARAMETER_FLOAT_RANGE(0.0f, 10.0f)},
        .outputs = {OUTPUT_DEFAULT},
        .constructor = sameDimensionOpConstructor};

}  // namespace fuzzing_test
}  // namespace nn
}  // namespace android
