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
#include <utility>
#include <vector>

#include "fuzzing/operation_signatures/OperationSignatureUtils.h"

namespace android {
namespace nn {
namespace fuzzing_test {

static void embeddingLookupConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    setFreeDimensions(op->inputs[0], /*rank=*/1);
    setFreeDimensions(op->inputs[1], rank);
    op->outputs[0]->dimensions.resize(rank);
    op->outputs[0]->dimensions[0] = op->inputs[0]->dimensions[0];
    for (uint32_t i = 1; i < rank; i++) {
        op->outputs[0]->dimensions[i] = op->inputs[1]->dimensions[i];
    }
    setSameQuantization(op->outputs[0], op->inputs[1]);
}

static void embeddingLookupFinalizer(RandomOperation* op) {
    uint32_t dimValue = op->inputs[1]->dimensions[0].getValue();
    uint32_t numElements = op->inputs[0]->getNumberOfElements();
    for (uint32_t i = 0; i < numElements; i++) {
        // The index values must be in the range of [0, input1_dim0).
        op->inputs[0]->value<int32_t>(i) = getUniform<int32_t>(0, dimValue - 1);
    }
}

#define DEFINE_EMBEDDING_LOOKUP_SIGNATURE(ver, ...)                                   \
    DEFINE_OPERATION_SIGNATURE(EMBEDDING_LOOKUP_##ver){                               \
            .opType = TestOperationType::EMBEDDING_LOOKUP,                            \
            .supportedDataTypes = {__VA_ARGS__},                                      \
            .supportedRanks = {2, 3, 4},                                              \
            .version = TestHalVersion::ver,                                           \
            .inputs = {PARAMETER_NONE(TestOperandType::TENSOR_INT32), INPUT_DEFAULT}, \
            .outputs = {OUTPUT_DEFAULT},                                              \
            .constructor = embeddingLookupConstructor,                                \
            .finalizer = embeddingLookupFinalizer};

DEFINE_EMBEDDING_LOOKUP_SIGNATURE(V1_0, TestOperandType::TENSOR_FLOAT32);
DEFINE_EMBEDDING_LOOKUP_SIGNATURE(V1_2, TestOperandType::TENSOR_INT32,
                                  TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_EMBEDDING_LOOKUP_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                                  TestOperandType::TENSOR_FLOAT16);

static void hashtableLookupConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    op->inputs[0]->dimensions = {RandomVariableType::FREE};
    op->inputs[1]->dimensions = {RandomVariableType::FREE};
    op->inputs[2]->dimensions.resize(rank);
    op->outputs[0]->dimensions.resize(rank);
    op->inputs[2]->dimensions[0] = op->inputs[1]->dimensions[0];
    op->outputs[0]->dimensions[0] = op->inputs[0]->dimensions[0];
    for (uint32_t i = 1; i < rank; i++) {
        op->inputs[2]->dimensions[i] = RandomVariableType::FREE;
        op->outputs[0]->dimensions[i] = op->inputs[2]->dimensions[i];
    }
    setSameQuantization(op->outputs[0], op->inputs[2]);
    op->outputs[1]->dimensions = {op->inputs[0]->dimensions[0]};
}

static void hashtableLookupFinalizer(RandomOperation* op) {
    // Generate values for keys. The keys tensor must be sorted in ascending order.
    uint32_t n = op->inputs[1]->getNumberOfElements();
    int32_t val = 0;
    for (uint32_t i = 0; i < n; i++) {
        op->inputs[1]->value<int32_t>(i) = val;
        val += getUniform<int32_t>(1, 2);
    }
    // Generate values for lookups.
    uint32_t k = op->inputs[0]->getNumberOfElements();
    for (uint32_t i = 0; i < k; i++) {
        op->inputs[0]->value<int32_t>(i) = getUniform<int32_t>(0, val);
    }
}

// The hits tensor in HASHTABLE_LOOKUP.
static const OperandSignature hitsTensor_HASHTABLE_LOOKUP = {
        .type = RandomOperandType::OUTPUT,
        .constructor = [](TestOperandType, uint32_t, RandomOperand* op) {
            op->dataType = TestOperandType::TENSOR_QUANT8_ASYMM;
            op->scale = 1.0f;
            op->zeroPoint = 0;
        }};

DEFINE_OPERATION_SIGNATURE(HASHTABLE_LOOKUP_V1_0){
        .opType = TestOperationType::HASHTABLE_LOOKUP,
        .supportedDataTypes = {TestOperandType::TENSOR_FLOAT32, TestOperandType::TENSOR_INT32,
                               TestOperandType::TENSOR_QUANT8_ASYMM},
        .supportedRanks = {2, 3, 4},
        .version = TestHalVersion::V1_0,
        .inputs = {PARAMETER_NONE(TestOperandType::TENSOR_INT32),
                   PARAMETER_NONE(TestOperandType::TENSOR_INT32), INPUT_DEFAULT},
        .outputs = {OUTPUT_DEFAULT, hitsTensor_HASHTABLE_LOOKUP},
        .constructor = hashtableLookupConstructor,
        .finalizer = hashtableLookupFinalizer};

static void gatherConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    // Generate value for "axis" scalar.
    int32_t axis = getUniform<int32_t>(-rank, rank - 1);
    op->inputs[1]->setScalarValue<int32_t>(axis);
    if (axis < 0) axis += rank;

    // Set dimensions for input and indices tensor.
    uint32_t indRank = getUniform<uint32_t>(1, 5);
    setFreeDimensions(op->inputs[0], rank);
    setFreeDimensions(op->inputs[2], indRank);

    for (uint32_t i = 0; i < static_cast<uint32_t>(axis); i++) {
        op->outputs[0]->dimensions.push_back(op->inputs[0]->dimensions[i]);
    }
    for (uint32_t i = 0; i < indRank; i++) {
        op->outputs[0]->dimensions.push_back(op->inputs[2]->dimensions[i]);
    }
    for (uint32_t i = axis + 1; i < rank; i++) {
        op->outputs[0]->dimensions.push_back(op->inputs[0]->dimensions[i]);
    }
    setSameQuantization(op->outputs[0], op->inputs[0]);
}

static void gatherFinalizer(RandomOperation* op) {
    int32_t axis = op->inputs[1]->value<int32_t>();
    if (axis < 0) axis += op->inputs[0]->dimensions.size();
    uint32_t dimValue = op->inputs[0]->dimensions[axis].getValue();
    uint32_t numElements = op->inputs[2]->getNumberOfElements();
    for (uint32_t i = 0; i < numElements; i++) {
        // The index values must be in the range of [0, dimValue).
        op->inputs[2]->value<int32_t>(i) = getUniform<int32_t>(0, dimValue - 1);
    }
}

#define DEFINE_GATHER_SIGNATURE(ver, ...)                                     \
    DEFINE_OPERATION_SIGNATURE(GATHER_##ver){                                 \
            .opType = TestOperationType::GATHER,                              \
            .supportedDataTypes = {__VA_ARGS__},                              \
            .supportedRanks = {1, 2, 3, 4, 5},                                \
            .version = TestHalVersion::ver,                                   \
            .inputs = {INPUT_DEFAULT, PARAMETER_NONE(TestOperandType::INT32), \
                       PARAMETER_NONE(TestOperandType::TENSOR_INT32)},        \
            .outputs = {OUTPUT_DEFAULT},                                      \
            .constructor = gatherConstructor,                                 \
            .finalizer = gatherFinalizer};

DEFINE_GATHER_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT32, TestOperandType::TENSOR_FLOAT16,
                        TestOperandType::TENSOR_INT32, TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_GATHER_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

static void selectConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    setFreeDimensions(op->inputs[0], rank);
    op->inputs[1]->dimensions = op->inputs[0]->dimensions;
    op->inputs[2]->dimensions = op->inputs[0]->dimensions;
    op->outputs[0]->dimensions = op->inputs[0]->dimensions;
    setSameQuantization(op->inputs[2], op->inputs[1]);
    setSameQuantization(op->outputs[0], op->inputs[1]);
}

#define DEFINE_SELECT_SIGNATURE(ver, ...)                                                         \
    DEFINE_OPERATION_SIGNATURE(SELECT_##ver){                                                     \
            .opType = TestOperationType::SELECT,                                                  \
            .supportedDataTypes = {__VA_ARGS__},                                                  \
            .supportedRanks = {1, 2, 3, 4},                                                       \
            .version = TestHalVersion::ver,                                                       \
            .inputs = {INPUT_TYPED(TestOperandType::TENSOR_BOOL8), INPUT_DEFAULT, INPUT_DEFAULT}, \
            .outputs = {OUTPUT_DEFAULT},                                                          \
            .constructor = selectConstructor};

DEFINE_SELECT_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT32, TestOperandType::TENSOR_FLOAT16,
                        TestOperandType::TENSOR_INT32, TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_SELECT_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

static void topKConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    setFreeDimensions(op->inputs[0], rank);
    op->outputs[0]->dimensions.resize(rank);
    op->outputs[1]->dimensions.resize(rank);
    for (uint32_t i = 0; i < rank - 1; i++) {
        op->outputs[0]->dimensions[i] = op->inputs[0]->dimensions[i];
        op->outputs[1]->dimensions[i] = op->inputs[0]->dimensions[i];
    }

    // K must be in the range of [1, depth].
    auto k = op->inputs[1]->value<RandomVariable>();
    k.setRange(1, kInvalidValue);
    op->inputs[0]->dimensions.back().setGreaterEqual(k);

    op->outputs[0]->dimensions.back() = k;
    op->outputs[1]->dimensions.back() = k;
    setSameQuantization(op->outputs[0], op->inputs[0]);

    // As the sorting is not required to be stable, we should not check the second output (indices).
    op->outputs[1]->doNotCheckAccuracy = true;
    op->outputs[1]->doNotConnect = true;
}

#define DEFINE_TOPK_SIGNATURE(ver, ...)                                               \
    DEFINE_OPERATION_SIGNATURE(TOPK_V2_##ver){                                        \
            .opType = TestOperationType::TOPK_V2,                                     \
            .supportedDataTypes = {__VA_ARGS__},                                      \
            .supportedRanks = {1, 2, 3, 4},                                           \
            .version = TestHalVersion::ver,                                           \
            .inputs = {INPUT_DEFAULT, RANDOM_INT_FREE},                               \
            .outputs = {OUTPUT_DEFAULT, OUTPUT_TYPED(TestOperandType::TENSOR_INT32)}, \
            .constructor = topKConstructor};

DEFINE_TOPK_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT32, TestOperandType::TENSOR_FLOAT16,
                      TestOperandType::TENSOR_INT32, TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_TOPK_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

static void sliceConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    op->inputs[1]->dimensions = {rank};
    op->inputs[2]->dimensions = {rank};
    setFreeDimensions(op->inputs[0], rank);
    setFreeDimensions(op->outputs[0], rank);
    // The axis size of output must be less than or equal to input.
    for (uint32_t i = 0; i < rank; i++) {
        op->inputs[0]->dimensions[i].setGreaterEqual(op->outputs[0]->dimensions[i]);
    }
    setSameQuantization(op->outputs[0], op->inputs[0]);
}

static void sliceFinalizer(RandomOperation* op) {
    uint32_t rank = op->inputs[0]->dimensions.size();
    int32_t* begin = reinterpret_cast<int32_t*>(op->inputs[1]->buffer.data());
    int32_t* size = reinterpret_cast<int32_t*>(op->inputs[2]->buffer.data());
    for (uint32_t i = 0; i < rank; i++) {
        int32_t inputSize = op->inputs[0]->dimensions[i].getValue();
        int32_t outputSize = op->outputs[0]->dimensions[i].getValue();
        // Randomly choose a valid begin index for each axis.
        begin[i] = getUniform<int32_t>(0, inputSize - outputSize);
        size[i] = outputSize;
    }
}

#define DEFINE_SLICE_SIGNATURE(ver, ...)                                             \
    DEFINE_OPERATION_SIGNATURE(SLICE_##ver){                                         \
            .opType = TestOperationType::SLICE,                                      \
            .supportedDataTypes = {__VA_ARGS__},                                     \
            .supportedRanks = {1, 2, 3, 4},                                          \
            .version = TestHalVersion::ver,                                          \
            .inputs = {INPUT_DEFAULT, PARAMETER_NONE(TestOperandType::TENSOR_INT32), \
                       PARAMETER_NONE(TestOperandType::TENSOR_INT32)},               \
            .outputs = {OUTPUT_DEFAULT},                                             \
            .constructor = sliceConstructor,                                         \
            .finalizer = sliceFinalizer};

DEFINE_SLICE_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT32, TestOperandType::TENSOR_FLOAT16,
                       TestOperandType::TENSOR_INT32, TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_SLICE_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

inline int32_t convertToBitMask(const std::vector<bool>& flags) {
    int32_t mask = 0, bit = 1;
    for (bool flag : flags) {
        if (flag) mask |= bit;
        bit <<= 1;
    }
    return mask;
}

static void stridedSliceConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    op->inputs[1]->dimensions = {rank};
    op->inputs[2]->dimensions = {rank};
    op->inputs[3]->dimensions = {rank};
    op->inputs[3]->resizeBuffer<int32_t>(rank);
    setFreeDimensions(op->inputs[0], rank);
    std::vector<bool> shrinkMask(rank, false);
    for (uint32_t i = 0; i < rank; i++) {
        shrinkMask[i] = getBernoulli(0.2f);
        int32_t stride = getUniform<int32_t>(1, 3);
        op->inputs[3]->value<int32_t>(i) = stride;
        if (!shrinkMask[i]) {
            op->outputs[0]->dimensions.push_back(RandomVariableType::FREE);
            auto maxOut = (op->inputs[0]->dimensions[i] + (stride - 1)) / stride;
            maxOut.setGreaterEqual(op->outputs[0]->dimensions.back());
        }
    }
    setSameQuantization(op->outputs[0], op->inputs[0]);
    op->inputs[6]->setScalarValue<int32_t>(convertToBitMask(shrinkMask));
}

static void stridedSliceFinalizer(RandomOperation* op) {
    uint32_t rank = op->inputs[0]->dimensions.size();
    int32_t* begin = reinterpret_cast<int32_t*>(op->inputs[1]->buffer.data());
    int32_t* end = reinterpret_cast<int32_t*>(op->inputs[2]->buffer.data());
    std::vector<bool> beginMask(rank, false), endMask(rank, false);
    int32_t shrinkMask = op->inputs[6]->value<int32_t>();
    for (uint32_t i = 0, o = 0; i < rank; i++) {
        int32_t inputSize = op->inputs[0]->dimensions[i].getValue();
        int32_t stride = op->inputs[3]->value<int32_t>(i);
        bool shrink = shrinkMask & (1 << i);
        if (!shrink) {
            int32_t outputSize = op->outputs[0]->dimensions[o++].getValue();
            int32_t maxStart = inputSize - (outputSize - 1) * stride - 1;
            begin[i] = getUniform<int32_t>(0, maxStart);

            int32_t minEnd = begin[i] + (outputSize - 1) * stride + 1;
            int32_t maxEnd = std::min(begin[i] + outputSize * stride, inputSize);
            end[i] = getUniform<int32_t>(minEnd, maxEnd);

            // Switch to masked begin/end.
            beginMask[i] = (begin[i] == 0 && getBernoulli(0.2f));
            endMask[i] = (end[i] == 0 && getBernoulli(0.2f));

            // When begin or end mask is set, begin[i] or end[i] is ignored and can have any
            // arbitrary value.
            if (beginMask[i]) begin[i] = getUniform<int32_t>(-inputSize, inputSize - 1);
            if (endMask[i]) end[i] = getUniform<int32_t>(-inputSize, inputSize - 1);

            // Switch to negative stride.
            if (getBernoulli(0.2f)) {
                op->inputs[3]->value<int32_t>(i) = -stride;
                std::swap(begin[i], end[i]);
                std::swap(beginMask[i], endMask[i]);
                begin[i]--;
                end[i]--;
                // end = -1 will be interpreted to inputSize - 1 if not setting endMask.
                if (end[i] < 0) endMask[i] = true;
            }
        } else {
            // When shrink mask is set, the begin and end must define a slice of size 1, e.g.
            // begin[i] = x, end[i] = x + 1.
            begin[i] = getUniform<int32_t>(0, inputSize - 1);
            end[i] = begin[i] + 1;
        }
    }
    op->inputs[4]->setScalarValue<int32_t>(convertToBitMask(beginMask));
    op->inputs[5]->setScalarValue<int32_t>(convertToBitMask(endMask));
}

DEFINE_OPERATION_SIGNATURE(STRIDED_SLICE_V1_1){
        .opType = TestOperationType::STRIDED_SLICE,
        .supportedDataTypes = {TestOperandType::TENSOR_FLOAT32,
                               TestOperandType::TENSOR_QUANT8_ASYMM},
        .supportedRanks = {1, 2, 3, 4},
        .version = TestHalVersion::V1_1,
        .inputs = {INPUT_DEFAULT, PARAMETER_NONE(TestOperandType::TENSOR_INT32),
                   PARAMETER_NONE(TestOperandType::TENSOR_INT32),
                   PARAMETER_NONE(TestOperandType::TENSOR_INT32),
                   PARAMETER_CHOICE(TestOperandType::INT32, 0),
                   PARAMETER_CHOICE(TestOperandType::INT32, 0),
                   PARAMETER_CHOICE(TestOperandType::INT32, 0)},
        .outputs = {OUTPUT_DEFAULT},
        .constructor = stridedSliceConstructor,
        .finalizer = stridedSliceFinalizer};

DEFINE_OPERATION_SIGNATURE(STRIDED_SLICE_V1_2){
        .opType = TestOperationType::STRIDED_SLICE,
        .supportedDataTypes = {TestOperandType::TENSOR_FLOAT16},
        .supportedRanks = {1, 2, 3, 4},
        .version = TestHalVersion::V1_2,
        .inputs = {INPUT_DEFAULT, PARAMETER_NONE(TestOperandType::TENSOR_INT32),
                   PARAMETER_NONE(TestOperandType::TENSOR_INT32),
                   PARAMETER_NONE(TestOperandType::TENSOR_INT32),
                   PARAMETER_NONE(TestOperandType::INT32), PARAMETER_NONE(TestOperandType::INT32),
                   PARAMETER_NONE(TestOperandType::INT32)},
        .outputs = {OUTPUT_DEFAULT},
        .constructor = stridedSliceConstructor,
        .finalizer = stridedSliceFinalizer};

DEFINE_OPERATION_SIGNATURE(STRIDED_SLICE_V1_3){
        .opType = TestOperationType::STRIDED_SLICE,
        .supportedDataTypes = {TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED},
        .supportedRanks = {1, 2, 3, 4},
        .version = TestHalVersion::V1_3,
        .inputs = {INPUT_DEFAULT, PARAMETER_NONE(TestOperandType::TENSOR_INT32),
                   PARAMETER_NONE(TestOperandType::TENSOR_INT32),
                   PARAMETER_NONE(TestOperandType::TENSOR_INT32),
                   PARAMETER_NONE(TestOperandType::INT32), PARAMETER_NONE(TestOperandType::INT32),
                   PARAMETER_NONE(TestOperandType::INT32)},
        .outputs = {OUTPUT_DEFAULT},
        .constructor = stridedSliceConstructor,
        .finalizer = stridedSliceFinalizer};

}  // namespace fuzzing_test
}  // namespace nn
}  // namespace android
