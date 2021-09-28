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

#include "Model.pb.h"
#include "TestHarness.h"

namespace android::nn::fuzz {
namespace {

using namespace test_helper;
using namespace android_nn_fuzz;

static_assert(static_cast<TestOperandType>(FLOAT32) == TestOperandType::FLOAT32);
static_assert(static_cast<TestOperandType>(INT32) == TestOperandType::INT32);
static_assert(static_cast<TestOperandType>(UINT32) == TestOperandType::UINT32);
static_assert(static_cast<TestOperandType>(TENSOR_FLOAT32) == TestOperandType::TENSOR_FLOAT32);
static_assert(static_cast<TestOperandType>(TENSOR_INT32) == TestOperandType::TENSOR_INT32);
static_assert(static_cast<TestOperandType>(TENSOR_QUANT8_ASYMM) ==
              TestOperandType::TENSOR_QUANT8_ASYMM);
static_assert(static_cast<TestOperandType>(BOOL) == TestOperandType::BOOL);
static_assert(static_cast<TestOperandType>(TENSOR_QUANT16_SYMM) ==
              TestOperandType::TENSOR_QUANT16_SYMM);
static_assert(static_cast<TestOperandType>(TENSOR_FLOAT16) == TestOperandType::TENSOR_FLOAT16);
static_assert(static_cast<TestOperandType>(TENSOR_BOOL8) == TestOperandType::TENSOR_BOOL8);
static_assert(static_cast<TestOperandType>(FLOAT16) == TestOperandType::FLOAT16);
static_assert(static_cast<TestOperandType>(TENSOR_QUANT8_SYMM_PER_CHANNEL) ==
              TestOperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL);
static_assert(static_cast<TestOperandType>(TENSOR_QUANT16_ASYMM) ==
              TestOperandType::TENSOR_QUANT16_ASYMM);
static_assert(static_cast<TestOperandType>(TENSOR_QUANT8_SYMM) ==
              TestOperandType::TENSOR_QUANT8_SYMM);
static_assert(static_cast<TestOperandType>(TENSOR_QUANT8_ASYMM_SIGNED) ==
              TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

static_assert(static_cast<TestOperationType>(ADD) == TestOperationType::ADD);
static_assert(static_cast<TestOperationType>(AVERAGE_POOL_2D) ==
              TestOperationType::AVERAGE_POOL_2D);
static_assert(static_cast<TestOperationType>(CONCATENATION) == TestOperationType::CONCATENATION);
static_assert(static_cast<TestOperationType>(CONV_2D) == TestOperationType::CONV_2D);
static_assert(static_cast<TestOperationType>(DEPTHWISE_CONV_2D) ==
              TestOperationType::DEPTHWISE_CONV_2D);
static_assert(static_cast<TestOperationType>(DEPTH_TO_SPACE) == TestOperationType::DEPTH_TO_SPACE);
static_assert(static_cast<TestOperationType>(DEQUANTIZE) == TestOperationType::DEQUANTIZE);
static_assert(static_cast<TestOperationType>(EMBEDDING_LOOKUP) ==
              TestOperationType::EMBEDDING_LOOKUP);
static_assert(static_cast<TestOperationType>(FLOOR) == TestOperationType::FLOOR);
static_assert(static_cast<TestOperationType>(FULLY_CONNECTED) ==
              TestOperationType::FULLY_CONNECTED);
static_assert(static_cast<TestOperationType>(HASHTABLE_LOOKUP) ==
              TestOperationType::HASHTABLE_LOOKUP);
static_assert(static_cast<TestOperationType>(L2_NORMALIZATION) ==
              TestOperationType::L2_NORMALIZATION);
static_assert(static_cast<TestOperationType>(L2_POOL_2D) == TestOperationType::L2_POOL_2D);
static_assert(static_cast<TestOperationType>(LOCAL_RESPONSE_NORMALIZATION) ==
              TestOperationType::LOCAL_RESPONSE_NORMALIZATION);
static_assert(static_cast<TestOperationType>(LOGISTIC) == TestOperationType::LOGISTIC);
static_assert(static_cast<TestOperationType>(LSH_PROJECTION) == TestOperationType::LSH_PROJECTION);
static_assert(static_cast<TestOperationType>(LSTM) == TestOperationType::LSTM);
static_assert(static_cast<TestOperationType>(MAX_POOL_2D) == TestOperationType::MAX_POOL_2D);
static_assert(static_cast<TestOperationType>(MUL) == TestOperationType::MUL);
static_assert(static_cast<TestOperationType>(RELU) == TestOperationType::RELU);
static_assert(static_cast<TestOperationType>(RELU1) == TestOperationType::RELU1);
static_assert(static_cast<TestOperationType>(RELU6) == TestOperationType::RELU6);
static_assert(static_cast<TestOperationType>(RESHAPE) == TestOperationType::RESHAPE);
static_assert(static_cast<TestOperationType>(RESIZE_BILINEAR) ==
              TestOperationType::RESIZE_BILINEAR);
static_assert(static_cast<TestOperationType>(RNN) == TestOperationType::RNN);
static_assert(static_cast<TestOperationType>(SOFTMAX) == TestOperationType::SOFTMAX);
static_assert(static_cast<TestOperationType>(SPACE_TO_DEPTH) == TestOperationType::SPACE_TO_DEPTH);
static_assert(static_cast<TestOperationType>(SVDF) == TestOperationType::SVDF);
static_assert(static_cast<TestOperationType>(TANH) == TestOperationType::TANH);
static_assert(static_cast<TestOperationType>(BATCH_TO_SPACE_ND) ==
              TestOperationType::BATCH_TO_SPACE_ND);
static_assert(static_cast<TestOperationType>(DIV) == TestOperationType::DIV);
static_assert(static_cast<TestOperationType>(MEAN) == TestOperationType::MEAN);
static_assert(static_cast<TestOperationType>(PAD) == TestOperationType::PAD);
static_assert(static_cast<TestOperationType>(SPACE_TO_BATCH_ND) ==
              TestOperationType::SPACE_TO_BATCH_ND);
static_assert(static_cast<TestOperationType>(SQUEEZE) == TestOperationType::SQUEEZE);
static_assert(static_cast<TestOperationType>(STRIDED_SLICE) == TestOperationType::STRIDED_SLICE);
static_assert(static_cast<TestOperationType>(SUB) == TestOperationType::SUB);
static_assert(static_cast<TestOperationType>(TRANSPOSE) == TestOperationType::TRANSPOSE);
static_assert(static_cast<TestOperationType>(ABS) == TestOperationType::ABS);
static_assert(static_cast<TestOperationType>(ARGMAX) == TestOperationType::ARGMAX);
static_assert(static_cast<TestOperationType>(ARGMIN) == TestOperationType::ARGMIN);
static_assert(static_cast<TestOperationType>(AXIS_ALIGNED_BBOX_TRANSFORM) ==
              TestOperationType::AXIS_ALIGNED_BBOX_TRANSFORM);
static_assert(static_cast<TestOperationType>(BIDIRECTIONAL_SEQUENCE_LSTM) ==
              TestOperationType::BIDIRECTIONAL_SEQUENCE_LSTM);
static_assert(static_cast<TestOperationType>(BIDIRECTIONAL_SEQUENCE_RNN) ==
              TestOperationType::BIDIRECTIONAL_SEQUENCE_RNN);
static_assert(static_cast<TestOperationType>(BOX_WITH_NMS_LIMIT) ==
              TestOperationType::BOX_WITH_NMS_LIMIT);
static_assert(static_cast<TestOperationType>(CAST) == TestOperationType::CAST);
static_assert(static_cast<TestOperationType>(CHANNEL_SHUFFLE) ==
              TestOperationType::CHANNEL_SHUFFLE);
static_assert(static_cast<TestOperationType>(DETECTION_POSTPROCESSING) ==
              TestOperationType::DETECTION_POSTPROCESSING);
static_assert(static_cast<TestOperationType>(EQUAL) == TestOperationType::EQUAL);
static_assert(static_cast<TestOperationType>(EXP) == TestOperationType::EXP);
static_assert(static_cast<TestOperationType>(EXPAND_DIMS) == TestOperationType::EXPAND_DIMS);
static_assert(static_cast<TestOperationType>(GATHER) == TestOperationType::GATHER);
static_assert(static_cast<TestOperationType>(GENERATE_PROPOSALS) ==
              TestOperationType::GENERATE_PROPOSALS);
static_assert(static_cast<TestOperationType>(GREATER) == TestOperationType::GREATER);
static_assert(static_cast<TestOperationType>(GREATER_EQUAL) == TestOperationType::GREATER_EQUAL);
static_assert(static_cast<TestOperationType>(GROUPED_CONV_2D) ==
              TestOperationType::GROUPED_CONV_2D);
static_assert(static_cast<TestOperationType>(HEATMAP_MAX_KEYPOINT) ==
              TestOperationType::HEATMAP_MAX_KEYPOINT);
static_assert(static_cast<TestOperationType>(INSTANCE_NORMALIZATION) ==
              TestOperationType::INSTANCE_NORMALIZATION);
static_assert(static_cast<TestOperationType>(LESS) == TestOperationType::LESS);
static_assert(static_cast<TestOperationType>(LESS_EQUAL) == TestOperationType::LESS_EQUAL);
static_assert(static_cast<TestOperationType>(LOG) == TestOperationType::LOG);
static_assert(static_cast<TestOperationType>(LOGICAL_AND) == TestOperationType::LOGICAL_AND);
static_assert(static_cast<TestOperationType>(LOGICAL_NOT) == TestOperationType::LOGICAL_NOT);
static_assert(static_cast<TestOperationType>(LOGICAL_OR) == TestOperationType::LOGICAL_OR);
static_assert(static_cast<TestOperationType>(LOG_SOFTMAX) == TestOperationType::LOG_SOFTMAX);
static_assert(static_cast<TestOperationType>(MAXIMUM) == TestOperationType::MAXIMUM);
static_assert(static_cast<TestOperationType>(MINIMUM) == TestOperationType::MINIMUM);
static_assert(static_cast<TestOperationType>(NEG) == TestOperationType::NEG);
static_assert(static_cast<TestOperationType>(NOT_EQUAL) == TestOperationType::NOT_EQUAL);
static_assert(static_cast<TestOperationType>(PAD_V2) == TestOperationType::PAD_V2);
static_assert(static_cast<TestOperationType>(POW) == TestOperationType::POW);
static_assert(static_cast<TestOperationType>(PRELU) == TestOperationType::PRELU);
static_assert(static_cast<TestOperationType>(QUANTIZE) == TestOperationType::QUANTIZE);
static_assert(static_cast<TestOperationType>(QUANTIZED_16BIT_LSTM) ==
              TestOperationType::QUANTIZED_16BIT_LSTM);
static_assert(static_cast<TestOperationType>(RANDOM_MULTINOMIAL) ==
              TestOperationType::RANDOM_MULTINOMIAL);
static_assert(static_cast<TestOperationType>(REDUCE_ALL) == TestOperationType::REDUCE_ALL);
static_assert(static_cast<TestOperationType>(REDUCE_ANY) == TestOperationType::REDUCE_ANY);
static_assert(static_cast<TestOperationType>(REDUCE_MAX) == TestOperationType::REDUCE_MAX);
static_assert(static_cast<TestOperationType>(REDUCE_MIN) == TestOperationType::REDUCE_MIN);
static_assert(static_cast<TestOperationType>(REDUCE_PROD) == TestOperationType::REDUCE_PROD);
static_assert(static_cast<TestOperationType>(REDUCE_SUM) == TestOperationType::REDUCE_SUM);
static_assert(static_cast<TestOperationType>(ROI_ALIGN) == TestOperationType::ROI_ALIGN);
static_assert(static_cast<TestOperationType>(ROI_POOLING) == TestOperationType::ROI_POOLING);
static_assert(static_cast<TestOperationType>(RSQRT) == TestOperationType::RSQRT);
static_assert(static_cast<TestOperationType>(SELECT) == TestOperationType::SELECT);
static_assert(static_cast<TestOperationType>(SIN) == TestOperationType::SIN);
static_assert(static_cast<TestOperationType>(SLICE) == TestOperationType::SLICE);
static_assert(static_cast<TestOperationType>(SPLIT) == TestOperationType::SPLIT);
static_assert(static_cast<TestOperationType>(SQRT) == TestOperationType::SQRT);
static_assert(static_cast<TestOperationType>(TILE) == TestOperationType::TILE);
static_assert(static_cast<TestOperationType>(TOPK_V2) == TestOperationType::TOPK_V2);
static_assert(static_cast<TestOperationType>(TRANSPOSE_CONV_2D) ==
              TestOperationType::TRANSPOSE_CONV_2D);
static_assert(static_cast<TestOperationType>(UNIDIRECTIONAL_SEQUENCE_LSTM) ==
              TestOperationType::UNIDIRECTIONAL_SEQUENCE_LSTM);
static_assert(static_cast<TestOperationType>(UNIDIRECTIONAL_SEQUENCE_RNN) ==
              TestOperationType::UNIDIRECTIONAL_SEQUENCE_RNN);
static_assert(static_cast<TestOperationType>(RESIZE_NEAREST_NEIGHBOR) ==
              TestOperationType::RESIZE_NEAREST_NEIGHBOR);

static_assert(static_cast<TestOperandLifeTime>(TEMPORARY_VARIABLE) ==
              TestOperandLifeTime::TEMPORARY_VARIABLE);
static_assert(static_cast<TestOperandLifeTime>(SUBGRAPH_INPUT) ==
              TestOperandLifeTime::SUBGRAPH_INPUT);
static_assert(static_cast<TestOperandLifeTime>(SUBGRAPH_OUTPUT) ==
              TestOperandLifeTime::SUBGRAPH_OUTPUT);
static_assert(static_cast<TestOperandLifeTime>(CONSTANT_COPY) ==
              TestOperandLifeTime::CONSTANT_COPY);
static_assert(static_cast<TestOperandLifeTime>(CONSTANT_REFERENCE) ==
              TestOperandLifeTime::CONSTANT_REFERENCE);
static_assert(static_cast<TestOperandLifeTime>(NO_VALUE) == TestOperandLifeTime::NO_VALUE);

}  // anonymous namespace
}  // namespace android::nn::fuzz
