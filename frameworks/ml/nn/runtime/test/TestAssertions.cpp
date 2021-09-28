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

#include "NeuralNetworks.h"
#include "TestHarness.h"

namespace test_helper {

// Make sure that the NDK enums are compatible with the values defined in
// nn/tools/test_generator/test_harness/include/TestHarness.h.
#define CHECK_TEST_ENUM(EnumType, enumValue) \
    static_assert(static_cast<int32_t>(EnumType::enumValue) == ANEURALNETWORKS_##enumValue)

CHECK_TEST_ENUM(TestOperandType, FLOAT32);
CHECK_TEST_ENUM(TestOperandType, INT32);
CHECK_TEST_ENUM(TestOperandType, UINT32);
CHECK_TEST_ENUM(TestOperandType, TENSOR_FLOAT32);
CHECK_TEST_ENUM(TestOperandType, TENSOR_INT32);
CHECK_TEST_ENUM(TestOperandType, TENSOR_QUANT8_ASYMM);
CHECK_TEST_ENUM(TestOperandType, BOOL);
CHECK_TEST_ENUM(TestOperandType, TENSOR_QUANT16_SYMM);
CHECK_TEST_ENUM(TestOperandType, TENSOR_FLOAT16);
CHECK_TEST_ENUM(TestOperandType, TENSOR_BOOL8);
CHECK_TEST_ENUM(TestOperandType, FLOAT16);
CHECK_TEST_ENUM(TestOperandType, TENSOR_QUANT8_SYMM_PER_CHANNEL);
CHECK_TEST_ENUM(TestOperandType, TENSOR_QUANT16_ASYMM);
CHECK_TEST_ENUM(TestOperandType, TENSOR_QUANT8_SYMM);
CHECK_TEST_ENUM(TestOperandType, TENSOR_QUANT8_ASYMM_SIGNED);

CHECK_TEST_ENUM(TestOperationType, ADD);
CHECK_TEST_ENUM(TestOperationType, AVERAGE_POOL_2D);
CHECK_TEST_ENUM(TestOperationType, CONCATENATION);
CHECK_TEST_ENUM(TestOperationType, CONV_2D);
CHECK_TEST_ENUM(TestOperationType, DEPTHWISE_CONV_2D);
CHECK_TEST_ENUM(TestOperationType, DEPTH_TO_SPACE);
CHECK_TEST_ENUM(TestOperationType, DEQUANTIZE);
CHECK_TEST_ENUM(TestOperationType, EMBEDDING_LOOKUP);
CHECK_TEST_ENUM(TestOperationType, FLOOR);
CHECK_TEST_ENUM(TestOperationType, FULLY_CONNECTED);
CHECK_TEST_ENUM(TestOperationType, HASHTABLE_LOOKUP);
CHECK_TEST_ENUM(TestOperationType, L2_NORMALIZATION);
CHECK_TEST_ENUM(TestOperationType, L2_POOL_2D);
CHECK_TEST_ENUM(TestOperationType, LOCAL_RESPONSE_NORMALIZATION);
CHECK_TEST_ENUM(TestOperationType, LOGISTIC);
CHECK_TEST_ENUM(TestOperationType, LSH_PROJECTION);
CHECK_TEST_ENUM(TestOperationType, LSTM);
CHECK_TEST_ENUM(TestOperationType, MAX_POOL_2D);
CHECK_TEST_ENUM(TestOperationType, MUL);
CHECK_TEST_ENUM(TestOperationType, RELU);
CHECK_TEST_ENUM(TestOperationType, RELU1);
CHECK_TEST_ENUM(TestOperationType, RELU6);
CHECK_TEST_ENUM(TestOperationType, RESHAPE);
CHECK_TEST_ENUM(TestOperationType, RESIZE_BILINEAR);
CHECK_TEST_ENUM(TestOperationType, RNN);
CHECK_TEST_ENUM(TestOperationType, SOFTMAX);
CHECK_TEST_ENUM(TestOperationType, SPACE_TO_DEPTH);
CHECK_TEST_ENUM(TestOperationType, SVDF);
CHECK_TEST_ENUM(TestOperationType, TANH);
CHECK_TEST_ENUM(TestOperationType, BATCH_TO_SPACE_ND);
CHECK_TEST_ENUM(TestOperationType, DIV);
CHECK_TEST_ENUM(TestOperationType, MEAN);
CHECK_TEST_ENUM(TestOperationType, PAD);
CHECK_TEST_ENUM(TestOperationType, SPACE_TO_BATCH_ND);
CHECK_TEST_ENUM(TestOperationType, SQUEEZE);
CHECK_TEST_ENUM(TestOperationType, STRIDED_SLICE);
CHECK_TEST_ENUM(TestOperationType, SUB);
CHECK_TEST_ENUM(TestOperationType, TRANSPOSE);
CHECK_TEST_ENUM(TestOperationType, ABS);
CHECK_TEST_ENUM(TestOperationType, ARGMAX);
CHECK_TEST_ENUM(TestOperationType, ARGMIN);
CHECK_TEST_ENUM(TestOperationType, AXIS_ALIGNED_BBOX_TRANSFORM);
CHECK_TEST_ENUM(TestOperationType, BIDIRECTIONAL_SEQUENCE_LSTM);
CHECK_TEST_ENUM(TestOperationType, BIDIRECTIONAL_SEQUENCE_RNN);
CHECK_TEST_ENUM(TestOperationType, BOX_WITH_NMS_LIMIT);
CHECK_TEST_ENUM(TestOperationType, CAST);
CHECK_TEST_ENUM(TestOperationType, CHANNEL_SHUFFLE);
CHECK_TEST_ENUM(TestOperationType, DETECTION_POSTPROCESSING);
CHECK_TEST_ENUM(TestOperationType, EQUAL);
CHECK_TEST_ENUM(TestOperationType, EXP);
CHECK_TEST_ENUM(TestOperationType, EXPAND_DIMS);
CHECK_TEST_ENUM(TestOperationType, GATHER);
CHECK_TEST_ENUM(TestOperationType, GENERATE_PROPOSALS);
CHECK_TEST_ENUM(TestOperationType, GREATER);
CHECK_TEST_ENUM(TestOperationType, GREATER_EQUAL);
CHECK_TEST_ENUM(TestOperationType, GROUPED_CONV_2D);
CHECK_TEST_ENUM(TestOperationType, HEATMAP_MAX_KEYPOINT);
CHECK_TEST_ENUM(TestOperationType, INSTANCE_NORMALIZATION);
CHECK_TEST_ENUM(TestOperationType, LESS);
CHECK_TEST_ENUM(TestOperationType, LESS_EQUAL);
CHECK_TEST_ENUM(TestOperationType, LOG);
CHECK_TEST_ENUM(TestOperationType, LOGICAL_AND);
CHECK_TEST_ENUM(TestOperationType, LOGICAL_NOT);
CHECK_TEST_ENUM(TestOperationType, LOGICAL_OR);
CHECK_TEST_ENUM(TestOperationType, LOG_SOFTMAX);
CHECK_TEST_ENUM(TestOperationType, MAXIMUM);
CHECK_TEST_ENUM(TestOperationType, MINIMUM);
CHECK_TEST_ENUM(TestOperationType, NEG);
CHECK_TEST_ENUM(TestOperationType, NOT_EQUAL);
CHECK_TEST_ENUM(TestOperationType, PAD_V2);
CHECK_TEST_ENUM(TestOperationType, POW);
CHECK_TEST_ENUM(TestOperationType, PRELU);
CHECK_TEST_ENUM(TestOperationType, QUANTIZE);
CHECK_TEST_ENUM(TestOperationType, QUANTIZED_16BIT_LSTM);
CHECK_TEST_ENUM(TestOperationType, RANDOM_MULTINOMIAL);
CHECK_TEST_ENUM(TestOperationType, REDUCE_ALL);
CHECK_TEST_ENUM(TestOperationType, REDUCE_ANY);
CHECK_TEST_ENUM(TestOperationType, REDUCE_MAX);
CHECK_TEST_ENUM(TestOperationType, REDUCE_MIN);
CHECK_TEST_ENUM(TestOperationType, REDUCE_PROD);
CHECK_TEST_ENUM(TestOperationType, REDUCE_SUM);
CHECK_TEST_ENUM(TestOperationType, ROI_ALIGN);
CHECK_TEST_ENUM(TestOperationType, ROI_POOLING);
CHECK_TEST_ENUM(TestOperationType, RSQRT);
CHECK_TEST_ENUM(TestOperationType, SELECT);
CHECK_TEST_ENUM(TestOperationType, SIN);
CHECK_TEST_ENUM(TestOperationType, SLICE);
CHECK_TEST_ENUM(TestOperationType, SPLIT);
CHECK_TEST_ENUM(TestOperationType, SQRT);
CHECK_TEST_ENUM(TestOperationType, TILE);
CHECK_TEST_ENUM(TestOperationType, TOPK_V2);
CHECK_TEST_ENUM(TestOperationType, TRANSPOSE_CONV_2D);
CHECK_TEST_ENUM(TestOperationType, UNIDIRECTIONAL_SEQUENCE_LSTM);
CHECK_TEST_ENUM(TestOperationType, UNIDIRECTIONAL_SEQUENCE_RNN);
CHECK_TEST_ENUM(TestOperationType, RESIZE_NEAREST_NEIGHBOR);

#undef CHECK_TEST_ENUM

}  // namespace test_helper
