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
#include <vector>

#include "fuzzing/operation_signatures/OperationSignatureUtils.h"

namespace android {
namespace nn {
namespace fuzzing_test {

static void spaceToDepthConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    NN_FUZZER_CHECK(rank == 4);

    bool useNchw = false;
    if (op->inputs.size() > 2) useNchw = op->inputs[2]->value<bool8>();
    int heightIndex = useNchw ? 2 : 1;
    int widthIndex = useNchw ? 3 : 2;
    int depthIndex = useNchw ? 1 : 3;

    op->inputs[0]->dimensions = {RandomVariableType::FREE, RandomVariableType::FREE,
                                 RandomVariableType::FREE, RandomVariableType::FREE};
    int32_t blockSize = op->inputs[1]->value<int32_t>();
    auto outHeight = op->inputs[0]->dimensions[heightIndex].exactDiv(blockSize);
    auto outWidth = op->inputs[0]->dimensions[widthIndex].exactDiv(blockSize);
    auto outDepth = op->inputs[0]->dimensions[depthIndex] * (blockSize * blockSize);

    if (useNchw) {
        op->outputs[0]->dimensions = {op->inputs[0]->dimensions[0], outDepth, outHeight, outWidth};
    } else {
        op->outputs[0]->dimensions = {op->inputs[0]->dimensions[0], outHeight, outWidth, outDepth};
    }
    setSameQuantization(op->outputs[0], op->inputs[0]);
}

#define DEFINE_SPACE_TO_DEPTH_SIGNATURE(ver, ...)                                     \
    DEFINE_OPERATION_SIGNATURE(SPACE_TO_DEPTH_##ver){                                 \
            .opType = TestOperationType::SPACE_TO_DEPTH,                              \
            .supportedDataTypes = {__VA_ARGS__},                                      \
            .supportedRanks = {4},                                                    \
            .version = TestHalVersion::ver,                                           \
            .inputs = {INPUT_DEFAULT, PARAMETER_RANGE(TestOperandType::INT32, 1, 5)}, \
            .outputs = {OUTPUT_DEFAULT},                                              \
            .constructor = spaceToDepthConstructor};

DEFINE_SPACE_TO_DEPTH_SIGNATURE(V1_0, TestOperandType::TENSOR_FLOAT32,
                                TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_SPACE_TO_DEPTH_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT16);
DEFINE_SPACE_TO_DEPTH_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

#define DEFINE_SPACE_TO_DEPTH_WITH_LAYOUT_SIGNATURE(ver, ...)                        \
    DEFINE_OPERATION_SIGNATURE(SPACE_TO_DEPTH_layout_##ver){                         \
            .opType = TestOperationType::SPACE_TO_DEPTH,                             \
            .supportedDataTypes = {__VA_ARGS__},                                     \
            .supportedRanks = {4},                                                   \
            .version = TestHalVersion::ver,                                          \
            .inputs = {INPUT_DEFAULT, PARAMETER_RANGE(TestOperandType::INT32, 1, 5), \
                       PARAMETER_CHOICE(TestOperandType::BOOL, true, false)},        \
            .outputs = {OUTPUT_DEFAULT},                                             \
            .constructor = spaceToDepthConstructor};

DEFINE_SPACE_TO_DEPTH_WITH_LAYOUT_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT32,
                                            TestOperandType::TENSOR_QUANT8_ASYMM,
                                            TestOperandType::TENSOR_FLOAT16);
DEFINE_SPACE_TO_DEPTH_WITH_LAYOUT_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

static void depthToSpaceConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    NN_FUZZER_CHECK(rank == 4);

    bool useNchw = false;
    if (op->inputs.size() > 2) useNchw = op->inputs[2]->value<bool8>();
    int heightIndex = useNchw ? 2 : 1;
    int widthIndex = useNchw ? 3 : 2;
    int depthIndex = useNchw ? 1 : 3;

    op->inputs[0]->dimensions = {RandomVariableType::FREE, RandomVariableType::FREE,
                                 RandomVariableType::FREE, RandomVariableType::FREE};
    int32_t blockSize = op->inputs[1]->value<int32_t>();
    auto outHeight = op->inputs[0]->dimensions[heightIndex] * blockSize;
    auto outWidth = op->inputs[0]->dimensions[widthIndex] * blockSize;
    auto outDepth = op->inputs[0]->dimensions[depthIndex].exactDiv(blockSize * blockSize);

    if (useNchw) {
        op->outputs[0]->dimensions = {op->inputs[0]->dimensions[0], outDepth, outHeight, outWidth};
    } else {
        op->outputs[0]->dimensions = {op->inputs[0]->dimensions[0], outHeight, outWidth, outDepth};
    }
    setSameQuantization(op->outputs[0], op->inputs[0]);
}

#define DEFINE_DEPTH_TO_SPACE_SIGNATURE(ver, ...)                                     \
    DEFINE_OPERATION_SIGNATURE(DEPTH_TO_SPACE_##ver){                                 \
            .opType = TestOperationType::DEPTH_TO_SPACE,                              \
            .supportedDataTypes = {TestOperandType::TENSOR_FLOAT32,                   \
                                   TestOperandType::TENSOR_QUANT8_ASYMM},             \
            .supportedRanks = {4},                                                    \
            .version = TestHalVersion::ver,                                           \
            .inputs = {INPUT_DEFAULT, PARAMETER_RANGE(TestOperandType::INT32, 1, 3)}, \
            .outputs = {OUTPUT_DEFAULT},                                              \
            .constructor = depthToSpaceConstructor};

DEFINE_DEPTH_TO_SPACE_SIGNATURE(V1_0, TestOperandType::TENSOR_FLOAT32,
                                TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_DEPTH_TO_SPACE_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT16);
DEFINE_DEPTH_TO_SPACE_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

#define DEFINE_DEPTH_TO_SPACE_WITH_LAYOUT_SIGNATURE(ver, ...)                        \
    DEFINE_OPERATION_SIGNATURE(DEPTH_TO_SPACE_layout_##ver){                         \
            .opType = TestOperationType::DEPTH_TO_SPACE,                             \
            .supportedDataTypes = {__VA_ARGS__},                                     \
            .supportedRanks = {4},                                                   \
            .version = TestHalVersion::ver,                                          \
            .inputs = {INPUT_DEFAULT, PARAMETER_RANGE(TestOperandType::INT32, 1, 3), \
                       PARAMETER_CHOICE(TestOperandType::BOOL, true, false)},        \
            .outputs = {OUTPUT_DEFAULT},                                             \
            .constructor = depthToSpaceConstructor};

DEFINE_DEPTH_TO_SPACE_WITH_LAYOUT_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT32,
                                            TestOperandType::TENSOR_QUANT8_ASYMM,
                                            TestOperandType::TENSOR_FLOAT16);
DEFINE_DEPTH_TO_SPACE_WITH_LAYOUT_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

static void reshapeConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    setFreeDimensions(op->inputs[0], rank);
    op->inputs[1]->dimensions = {rank};
    op->inputs[1]->randomBuffer.resize(rank);
    RandomVariable numInputElements = 1;
    RandomVariable numOutputElements = 1;
    for (uint32_t i = 0; i < rank; i++) {
        op->inputs[1]->randomBuffer[i] = RandomVariableType::FREE;
        numInputElements = numInputElements * op->inputs[0]->dimensions[i];
        numOutputElements = numOutputElements * op->inputs[1]->randomBuffer[i];
    }
    numInputElements.setEqual(numOutputElements);
    op->outputs[0]->dimensions = op->inputs[1]->randomBuffer;
    setSameQuantization(op->outputs[0], op->inputs[0]);
}

#define DEFINE_RESHAPE_SIGNATURE(ver, ...)                                            \
    DEFINE_OPERATION_SIGNATURE(RESHAPE_##ver){                                        \
            .opType = TestOperationType::RESHAPE,                                     \
            .supportedDataTypes = {TestOperandType::TENSOR_FLOAT32,                   \
                                   TestOperandType::TENSOR_QUANT8_ASYMM},             \
            .supportedRanks = {1, 2, 3, 4},                                           \
            .version = TestHalVersion::ver,                                           \
            .inputs = {INPUT_DEFAULT, PARAMETER_NONE(TestOperandType::TENSOR_INT32)}, \
            .outputs = {OUTPUT_DEFAULT},                                              \
            .constructor = reshapeConstructor};

DEFINE_RESHAPE_SIGNATURE(V1_0, TestOperandType::TENSOR_FLOAT32,
                         TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_RESHAPE_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT16);
DEFINE_RESHAPE_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

static void batchToSpaceConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    NN_FUZZER_CHECK(rank == 4);

    bool useNchw = false;
    if (op->inputs.size() > 2) useNchw = op->inputs[2]->value<bool8>();
    int heightIndex = useNchw ? 2 : 1;
    int widthIndex = useNchw ? 3 : 2;

    op->inputs[0]->dimensions = {RandomVariableType::FREE, RandomVariableType::FREE,
                                 RandomVariableType::FREE, RandomVariableType::FREE};
    int32_t blockHeight = op->inputs[1]->value<int32_t>(0);
    int32_t blockWidth = op->inputs[1]->value<int32_t>(1);
    auto outBatch = op->inputs[0]->dimensions[0].exactDiv(blockHeight * blockWidth);
    auto outHeight = op->inputs[0]->dimensions[heightIndex] * blockHeight;
    auto outWidth = op->inputs[0]->dimensions[widthIndex] * blockWidth;

    if (useNchw) {
        op->outputs[0]->dimensions = {outBatch, op->inputs[0]->dimensions[1], outHeight, outWidth};
    } else {
        op->outputs[0]->dimensions = {outBatch, outHeight, outWidth, op->inputs[0]->dimensions[3]};
    }
    setSameQuantization(op->outputs[0], op->inputs[0]);
}

#define DEFINE_BATCH_TO_SPACE_ND_SIGNATURE(ver, ...)                                     \
    DEFINE_OPERATION_SIGNATURE(BATCH_TO_SPACE_ND_##ver){                                 \
            .opType = TestOperationType::BATCH_TO_SPACE_ND,                              \
            .supportedDataTypes = {__VA_ARGS__},                                         \
            .supportedRanks = {4},                                                       \
            .version = TestHalVersion::ver,                                              \
            .inputs = {INPUT_DEFAULT, PARAMETER_VEC_RANGE(TestOperandType::TENSOR_INT32, \
                                                          /*len=*/2, /*range=*/1, 3)},   \
            .outputs = {OUTPUT_DEFAULT},                                                 \
            .constructor = batchToSpaceConstructor};

DEFINE_BATCH_TO_SPACE_ND_SIGNATURE(V1_1, TestOperandType::TENSOR_FLOAT32,
                                   TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_BATCH_TO_SPACE_ND_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT16);
DEFINE_BATCH_TO_SPACE_ND_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

#define DEFINE_BATCH_TO_SPACE_ND_WITH_LAYOUT_SIGNATURE(ver, ...)                                  \
    DEFINE_OPERATION_SIGNATURE(BATCH_TO_SPACE_ND_layout_##ver){                                   \
            .opType = TestOperationType::BATCH_TO_SPACE_ND,                                       \
            .supportedDataTypes = {__VA_ARGS__},                                                  \
            .supportedRanks = {4},                                                                \
            .version = TestHalVersion::ver,                                                       \
            .inputs = {INPUT_DEFAULT,                                                             \
                       PARAMETER_VEC_RANGE(TestOperandType::TENSOR_INT32, /*len=*/2, /*range=*/1, \
                                           3),                                                    \
                       PARAMETER_CHOICE(TestOperandType::BOOL, true, false)},                     \
            .outputs = {OUTPUT_DEFAULT},                                                          \
            .constructor = batchToSpaceConstructor};

DEFINE_BATCH_TO_SPACE_ND_WITH_LAYOUT_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT32,
                                               TestOperandType::TENSOR_QUANT8_ASYMM,
                                               TestOperandType::TENSOR_FLOAT16);
DEFINE_BATCH_TO_SPACE_ND_WITH_LAYOUT_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

static void spaceToBatchConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    NN_FUZZER_CHECK(rank == 4);

    bool useNchw = false;
    if (op->inputs.size() > 3) useNchw = op->inputs[3]->value<bool8>();
    int heightIndex = useNchw ? 2 : 1;
    int widthIndex = useNchw ? 3 : 2;

    op->inputs[0]->dimensions = {RandomVariableType::FREE, RandomVariableType::FREE,
                                 RandomVariableType::FREE, RandomVariableType::FREE};

    // Compute padded height and width.
    auto paddedHeight = op->inputs[0]->dimensions[heightIndex] +
                        (op->inputs[2]->value<int32_t>(0) + op->inputs[2]->value<int32_t>(1));
    auto paddedWidth = op->inputs[0]->dimensions[widthIndex] +
                       (op->inputs[2]->value<int32_t>(2) + op->inputs[2]->value<int32_t>(3));

    // blockHeight/blockWidth must be a divisor of padded height/width
    int32_t blockHeight = op->inputs[1]->value<int32_t>(0);
    int32_t blockWidth = op->inputs[1]->value<int32_t>(1);
    auto outBatch = op->inputs[0]->dimensions[0] * (blockHeight * blockWidth);
    auto outHeight = paddedHeight.exactDiv(blockHeight);
    auto outWidth = paddedWidth.exactDiv(blockWidth);

    if (useNchw) {
        op->outputs[0]->dimensions = {outBatch, op->inputs[0]->dimensions[1], outHeight, outWidth};
    } else {
        op->outputs[0]->dimensions = {outBatch, outHeight, outWidth, op->inputs[0]->dimensions[3]};
    }
    setSameQuantization(op->outputs[0], op->inputs[0]);
}

// The paddings tensor in SPACE_TOBATCH_ND, a [2, 2] tensor with value selected from [0, 10].
static const OperandSignature paddingTensor_SPACE_TO_BATCH_ND = {
        .type = RandomOperandType::CONST,
        .constructor = [](TestOperandType, uint32_t, RandomOperand* op) {
            op->dataType = TestOperandType::TENSOR_INT32;
            op->dimensions = {2, 2};
            op->resizeBuffer<int32_t>(4);
            for (int i = 0; i < 4; i++) op->value<int32_t>(i) = getUniform<int32_t>(0, 10);
        }};

#define DEFINE_SPACE_TO_BATCH_SIGNATURE(ver, ...)                                                 \
    DEFINE_OPERATION_SIGNATURE(SPACE_TO_BATCH_ND_##ver){                                          \
            .opType = TestOperationType::SPACE_TO_BATCH_ND,                                       \
            .supportedDataTypes = {__VA_ARGS__},                                                  \
            .supportedRanks = {4},                                                                \
            .version = TestHalVersion::ver,                                                       \
            .inputs = {INPUT_DEFAULT,                                                             \
                       PARAMETER_VEC_RANGE(TestOperandType::TENSOR_INT32, /*len=*/2, /*range=*/1, \
                                           5),                                                    \
                       paddingTensor_SPACE_TO_BATCH_ND},                                          \
            .outputs = {OUTPUT_DEFAULT},                                                          \
            .constructor = spaceToBatchConstructor};

DEFINE_SPACE_TO_BATCH_SIGNATURE(V1_1, TestOperandType::TENSOR_FLOAT32,
                                TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_SPACE_TO_BATCH_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT16);
DEFINE_SPACE_TO_BATCH_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

#define DEFINE_SPACE_TO_BATCH_WITH_LAYOUT_SIGNATURE(ver, ...)                                     \
    DEFINE_OPERATION_SIGNATURE(SPACE_TO_BATCH_ND_layout_##ver){                                   \
            .opType = TestOperationType::SPACE_TO_BATCH_ND,                                       \
            .supportedDataTypes = {__VA_ARGS__},                                                  \
            .supportedRanks = {4},                                                                \
            .version = TestHalVersion::ver,                                                       \
            .inputs = {INPUT_DEFAULT,                                                             \
                       PARAMETER_VEC_RANGE(TestOperandType::TENSOR_INT32, /*len=*/2, /*range=*/1, \
                                           5),                                                    \
                       paddingTensor_SPACE_TO_BATCH_ND,                                           \
                       PARAMETER_CHOICE(TestOperandType::BOOL, true, false)},                     \
            .outputs = {OUTPUT_DEFAULT},                                                          \
            .constructor = spaceToBatchConstructor};

DEFINE_SPACE_TO_BATCH_WITH_LAYOUT_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT32,
                                            TestOperandType::TENSOR_QUANT8_ASYMM,
                                            TestOperandType::TENSOR_FLOAT16);
DEFINE_SPACE_TO_BATCH_WITH_LAYOUT_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

static void padConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    setFreeDimensions(op->inputs[0], rank);
    op->inputs[1]->dimensions = {rank, 2};
    op->inputs[1]->resizeBuffer<int32_t>(rank * 2);
    op->outputs[0]->dimensions.resize(rank);
    for (uint32_t i = 0; i < rank; i++) {
        int32_t left = getUniform<int32_t>(0, 5), right = getUniform<int32_t>(0, 5);
        op->inputs[1]->value<int32_t>(i * 2) = left;
        op->inputs[1]->value<int32_t>(i * 2 + 1) = right;
        op->outputs[0]->dimensions[i] = op->inputs[0]->dimensions[i] + (left + right);
    }
    setSameQuantization(op->outputs[0], op->inputs[0]);
}

static const OperandSignature paddingScalar_PAD_V2 = {
        .type = RandomOperandType::CONST,
        .constructor = [](TestOperandType dataType, uint32_t, RandomOperand* op) {
            switch (dataType) {
                case TestOperandType::TENSOR_FLOAT32:
                    op->dataType = TestOperandType::FLOAT32;
                    op->setScalarValue<float>(getUniform<float>(-10.0f, 10.0f));
                    break;
                case TestOperandType::TENSOR_FLOAT16:
                    op->dataType = TestOperandType::FLOAT16;
                    op->setScalarValue<_Float16>(getUniform<_Float16>(-10.0f, 10.0f));
                    break;
                case TestOperandType::TENSOR_QUANT8_ASYMM:
                    op->dataType = TestOperandType::INT32;
                    op->setScalarValue<int32_t>(getUniform<int32_t>(0, 255));
                    break;
                case TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED:
                    op->dataType = TestOperandType::INT32;
                    op->setScalarValue<int32_t>(getUniform<int32_t>(-128, 127));
                    break;
                default:
                    NN_FUZZER_CHECK(false) << "Unsupported data type for PAD_V2";
            }
        }};

#define DEFINE_PAD_SIGNATURE(ver, ...)                                                \
    DEFINE_OPERATION_SIGNATURE(PAD_##ver){                                            \
            .opType = TestOperationType::PAD,                                         \
            .supportedDataTypes = {TestOperandType::TENSOR_FLOAT32,                   \
                                   TestOperandType::TENSOR_QUANT8_ASYMM},             \
            .supportedRanks = {1, 2, 3, 4},                                           \
            .version = TestHalVersion::ver,                                           \
            .inputs = {INPUT_DEFAULT, PARAMETER_NONE(TestOperandType::TENSOR_INT32)}, \
            .outputs = {OUTPUT_DEFAULT},                                              \
            .constructor = padConstructor};

DEFINE_PAD_SIGNATURE(V1_1, TestOperandType::TENSOR_FLOAT32, TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_PAD_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT16);
DEFINE_PAD_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

#define DEFINE_PAD_V2_SIGNATURE(ver, ...)                                            \
    DEFINE_OPERATION_SIGNATURE(PAD_V2_##ver){                                        \
            .opType = TestOperationType::PAD_V2,                                     \
            .supportedDataTypes = {__VA_ARGS__},                                     \
            .supportedRanks = {1, 2, 3, 4},                                          \
            .version = TestHalVersion::ver,                                          \
            .inputs = {INPUT_DEFAULT, PARAMETER_NONE(TestOperandType::TENSOR_INT32), \
                       paddingScalar_PAD_V2},                                        \
            .outputs = {OUTPUT_DEFAULT},                                             \
            .constructor = padConstructor};

DEFINE_PAD_V2_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT32, TestOperandType::TENSOR_QUANT8_ASYMM,
                        TestOperandType::TENSOR_FLOAT16);
DEFINE_PAD_V2_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

static void transposeConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    // Create the permutation value by randomly shuffling a sequential array.
    std::vector<int32_t> permutation(rank);
    std::iota(permutation.begin(), permutation.end(), 0);
    randomShuffle(&permutation);
    op->inputs[1]->resizeBuffer<int32_t>(rank);
    std::copy(permutation.begin(), permutation.end(),
              reinterpret_cast<int32_t*>(op->inputs[1]->buffer.data()));

    setFreeDimensions(op->inputs[0], rank);
    op->inputs[1]->dimensions = {rank};
    op->outputs[0]->dimensions.resize(rank);
    for (uint32_t i = 0; i < rank; i++) {
        op->outputs[0]->dimensions[i] = op->inputs[0]->dimensions[permutation[i]];
    }
    setSameQuantization(op->outputs[0], op->inputs[0]);
}

static void transposeOmittedConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    NN_FUZZER_CHECK(rank == 2);
    op->inputs[0]->dimensions = {RandomVariableType::FREE, RandomVariableType::FREE};
    op->inputs[1]->dimensions = {2};
    op->outputs[0]->dimensions = {op->inputs[0]->dimensions[1], op->inputs[0]->dimensions[0]};
    setSameQuantization(op->outputs[0], op->inputs[0]);
}

#define DEFINE_TRANSPOSE_SIGNATURE(ver, ...)                                              \
    DEFINE_OPERATION_SIGNATURE(TRANSPOSE_##ver){                                          \
            .opType = TestOperationType::TRANSPOSE,                                       \
            .supportedDataTypes = {__VA_ARGS__},                                          \
            .supportedRanks = {1, 2, 3, 4},                                               \
            .version = TestHalVersion::ver,                                               \
            .inputs = {INPUT_DEFAULT, PARAMETER_NONE(TestOperandType::TENSOR_INT32)},     \
            .outputs = {OUTPUT_DEFAULT},                                                  \
            .constructor = transposeConstructor};                                         \
    DEFINE_OPERATION_SIGNATURE(TRANSPOSE_omitted_##ver){                                  \
            .opType = TestOperationType::TRANSPOSE,                                       \
            .supportedDataTypes = {__VA_ARGS__},                                          \
            .supportedRanks = {2},                                                        \
            .version = TestHalVersion::ver,                                               \
            .inputs = {INPUT_DEFAULT, PARAMETER_NO_VALUE(TestOperandType::TENSOR_INT32)}, \
            .outputs = {OUTPUT_DEFAULT},                                                  \
            .constructor = transposeOmittedConstructor};

DEFINE_TRANSPOSE_SIGNATURE(V1_1, TestOperandType::TENSOR_FLOAT32,
                           TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_TRANSPOSE_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT16);
DEFINE_TRANSPOSE_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

static void channelShuffleConstructor(TestOperandType dataType, uint32_t rank,
                                      RandomOperation* op) {
    sameShapeOpConstructor(dataType, rank, op);
    // The number of groups must be a divisor of the target axis size.
    int32_t axis = getUniform<int32_t>(-rank, rank - 1);
    op->inputs[2]->setScalarValue<int32_t>(axis);
    int32_t numGroups = op->inputs[1]->value<int32_t>();
    if (axis < 0) axis += rank;
    (op->inputs[0]->dimensions[axis] % numGroups).setEqual(0);
}

#define DEFINE_CHANNEL_SHUFFLE_SIGNATURE(ver, ...)                                   \
    DEFINE_OPERATION_SIGNATURE(CHANNEL_SHUFFLE_##ver){                               \
            .opType = TestOperationType::CHANNEL_SHUFFLE,                            \
            .supportedDataTypes = {__VA_ARGS__},                                     \
            .supportedRanks = {1, 2, 3, 4},                                          \
            .version = TestHalVersion::ver,                                          \
            .inputs = {INPUT_DEFAULT, PARAMETER_RANGE(TestOperandType::INT32, 1, 5), \
                       PARAMETER_NONE(TestOperandType::INT32)},                      \
            .outputs = {OUTPUT_DEFAULT},                                             \
            .constructor = channelShuffleConstructor};

DEFINE_CHANNEL_SHUFFLE_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT32,
                                 TestOperandType::TENSOR_QUANT8_ASYMM,
                                 TestOperandType::TENSOR_FLOAT16);
DEFINE_CHANNEL_SHUFFLE_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

static void squeezeConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    // A boolean array indicating whether each dimension is selected to be squeezed.
    bool squeeze[4] = {false, false, false, false};
    uint32_t numAxis = getUniform<int32_t>(1, 10);
    op->inputs[1]->dimensions = {numAxis};
    op->inputs[1]->resizeBuffer<int32_t>(numAxis);
    for (uint32_t i = 0; i < numAxis; i++) {
        // Generate values for the "axis" tensor.
        int32_t dim = getUniform<int32_t>(0, rank - 1);
        op->inputs[1]->value<int32_t>(i) = dim;
        squeeze[dim] = true;
    }

    op->inputs[0]->dimensions.resize(rank);
    for (uint32_t i = 0; i < rank; i++) {
        if (squeeze[i]) {
            op->inputs[0]->dimensions[i] = 1;
        } else {
            op->inputs[0]->dimensions[i] = RandomVariableType::FREE;
            op->outputs[0]->dimensions.emplace_back(op->inputs[0]->dimensions[i]);
        }
    }
    setSameQuantization(op->outputs[0], op->inputs[0]);
}

static void squeezeOmittedConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    // A boolean array indicating whether each dimension is selected to be squeezed.
    std::vector<bool> squeeze(rank, false);
    for (uint32_t i = 0; i < rank; i++) {
        squeeze[i] = getBernoulli(0.5f);
    }
    op->inputs[0]->dimensions.resize(rank);
    op->inputs[1]->dimensions = {0};
    for (uint32_t i = 0; i < rank; i++) {
        if (squeeze[i]) {
            op->inputs[0]->dimensions[i] = 1;
        } else {
            // Set the dimension to any value greater than 1 to prevent from getting sqeezed.
            op->inputs[0]->dimensions[i] = RandomVariableType::FREE;
            op->inputs[0]->dimensions[i].setGreaterThan(1);
            op->outputs[0]->dimensions.emplace_back(op->inputs[0]->dimensions[i]);
        }
    }
    setSameQuantization(op->outputs[0], op->inputs[0]);
}

#define DEFINE_SQUEEZE_SIGNATURE(ver, ...)                                                \
    DEFINE_OPERATION_SIGNATURE(SQUEEZE_##ver){                                            \
            .opType = TestOperationType::SQUEEZE,                                         \
            .supportedDataTypes = {__VA_ARGS__},                                          \
            .supportedRanks = {1, 2, 3, 4},                                               \
            .version = TestHalVersion::ver,                                               \
            .inputs = {INPUT_DEFAULT, PARAMETER_NONE(TestOperandType::TENSOR_INT32)},     \
            .outputs = {OUTPUT_DEFAULT},                                                  \
            .constructor = squeezeConstructor};                                           \
    DEFINE_OPERATION_SIGNATURE(SQUEEZE_omitted_##ver){                                    \
            .opType = TestOperationType::SQUEEZE,                                         \
            .supportedDataTypes = {__VA_ARGS__},                                          \
            .supportedRanks = {1, 2, 3, 4},                                               \
            .version = TestHalVersion::ver,                                               \
            .inputs = {INPUT_DEFAULT, PARAMETER_NO_VALUE(TestOperandType::TENSOR_INT32)}, \
            .outputs = {OUTPUT_DEFAULT},                                                  \
            .constructor = squeezeOmittedConstructor};

DEFINE_SQUEEZE_SIGNATURE(V1_1, TestOperandType::TENSOR_FLOAT32,
                         TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_SQUEEZE_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT16);
DEFINE_SQUEEZE_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

static void expandDimsConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    // Generate values for the "axis" tensor.
    int32_t axis = getUniform<int32_t>(-rank - 1, rank);
    op->inputs[1]->setScalarValue<int32_t>(axis);
    if (axis < 0) axis += rank + 1;

    setFreeDimensions(op->inputs[0], rank);
    for (uint32_t i = 0; i < rank; i++) {
        if (i == static_cast<uint32_t>(axis)) {
            op->outputs[0]->dimensions.push_back(1);
        }
        op->outputs[0]->dimensions.push_back(op->inputs[0]->dimensions[i]);
    }
    if (rank == static_cast<uint32_t>(axis)) op->outputs[0]->dimensions.push_back(1);
    setSameQuantization(op->outputs[0], op->inputs[0]);
}

#define DEFINE_EXPAND_DIMS_SIGNATURE(ver, ...)                                 \
    DEFINE_OPERATION_SIGNATURE(EXPAND_DIMS_##ver){                             \
            .opType = TestOperationType::EXPAND_DIMS,                          \
            .supportedDataTypes = {__VA_ARGS__},                               \
            .supportedRanks = {1, 2, 3, 4, 5},                                 \
            .version = TestHalVersion::ver,                                    \
            .inputs = {INPUT_DEFAULT, PARAMETER_NONE(TestOperandType::INT32)}, \
            .outputs = {OUTPUT_DEFAULT},                                       \
            .constructor = expandDimsConstructor};

DEFINE_EXPAND_DIMS_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT32, TestOperandType::TENSOR_FLOAT16,
                             TestOperandType::TENSOR_INT32, TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_EXPAND_DIMS_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

static void tileConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    setFreeDimensions(op->inputs[0], rank);
    op->outputs[0]->dimensions.resize(rank);
    op->inputs[1]->dimensions = {rank};
    op->inputs[1]->resizeBuffer<int32_t>(rank);
    for (uint32_t i = 0; i < rank; i++) {
        int32_t multiple = getUniform<int32_t>(1, 5);
        op->inputs[1]->value<int32_t>(i) = multiple;
        op->outputs[0]->dimensions[i] = op->inputs[0]->dimensions[i] * multiple;
    }
    setSameQuantization(op->outputs[0], op->inputs[0]);
}

#define DEFINE_TILE_SIGNATURE(ver, ...)                                               \
    DEFINE_OPERATION_SIGNATURE(TILE_##ver){                                           \
            .opType = TestOperationType::TILE,                                        \
            .supportedDataTypes = {__VA_ARGS__},                                      \
            .supportedRanks = {1, 2, 3, 4, 5},                                        \
            .version = TestHalVersion::ver,                                           \
            .inputs = {INPUT_DEFAULT, PARAMETER_NONE(TestOperandType::TENSOR_INT32)}, \
            .outputs = {OUTPUT_DEFAULT},                                              \
            .constructor = tileConstructor};

DEFINE_TILE_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT32, TestOperandType::TENSOR_FLOAT16,
                      TestOperandType::TENSOR_INT32, TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_TILE_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

static void fillConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    op->inputs[0]->dimensions = {rank};
    setFreeDimensions(op->outputs[0], rank);
    op->inputs[0]->randomBuffer = op->outputs[0]->dimensions;
}

DEFINE_OPERATION_SIGNATURE(FILL_V1_3){
        .opType = TestOperationType::FILL,
        .supportedDataTypes = {TestOperandType::TENSOR_FLOAT32, TestOperandType::TENSOR_FLOAT16,
                               TestOperandType::TENSOR_INT32},
        .supportedRanks = {1, 2, 3, 4, 5},
        .version = TestHalVersion::V1_3,
        .inputs = {PARAMETER_NONE(TestOperandType::TENSOR_INT32), INPUT_SCALAR},
        .outputs = {OUTPUT_DEFAULT},
        .constructor = fillConstructor};

static void rankConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    setFreeDimensions(op->inputs[0], rank);
}

DEFINE_OPERATION_SIGNATURE(RANK_V1_3){
        .opType = TestOperationType::RANK,
        .supportedDataTypes = {TestOperandType::TENSOR_FLOAT32, TestOperandType::TENSOR_FLOAT16,
                               TestOperandType::TENSOR_INT32, TestOperandType::TENSOR_QUANT8_ASYMM,
                               TestOperandType::TENSOR_BOOL8},
        .supportedRanks = {1, 2, 3, 4, 5},
        .version = TestHalVersion::V1_3,
        .inputs = {INPUT_DEFAULT},
        .outputs = {OUTPUT_TYPED(TestOperandType::INT32)},
        .constructor = rankConstructor};

}  // namespace fuzzing_test
}  // namespace nn
}  // namespace android
