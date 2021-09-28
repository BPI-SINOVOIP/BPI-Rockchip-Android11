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

using namespace std::placeholders;

namespace android {
namespace nn {
namespace fuzzing_test {

static void conv2DExplicitConstructor(TestOperandType, uint32_t rank, TestHalVersion ver,
                                      RandomOperation* op) {
    NN_FUZZER_CHECK(rank == 4);

    // Parameters
    int32_t paddingLeft = op->inputs[3]->value<int32_t>();
    int32_t paddingRight = op->inputs[4]->value<int32_t>();
    int32_t paddingTop = op->inputs[5]->value<int32_t>();
    int32_t paddingBottom = op->inputs[6]->value<int32_t>();
    int32_t strideWidth = op->inputs[7]->value<int32_t>();
    int32_t strideHeight = op->inputs[8]->value<int32_t>();
    bool useNchw = false;
    int32_t dilationWidth = 1, dilationHeight = 1;
    if (op->inputs.size() > 10) {
        useNchw = op->inputs[10]->value<bool8>();
        if (op->inputs.size() > 11) {
            dilationWidth = op->inputs[11]->value<int32_t>();
            dilationHeight = op->inputs[12]->value<int32_t>();
        }
    }
    int heightIndex = useNchw ? 2 : 1;
    int widthIndex = useNchw ? 3 : 2;
    int channelIndex = useNchw ? 1 : 3;

    // Input, [batch, height_in, width_in, channel_in]
    op->inputs[0]->dimensions = {RandomVariableType::FREE, RandomVariableType::FREE,
                                 RandomVariableType::FREE, RandomVariableType::FREE};

    // Filter, [channel_out, height_flt, width_flt, channel_in]
    op->inputs[1]->dimensions = {RandomVariableType::FREE, RandomVariableType::FREE,
                                 RandomVariableType::FREE, op->inputs[0]->dimensions[channelIndex]};

    // Bias, [channel_out]
    op->inputs[2]->dimensions = {op->inputs[1]->dimensions[0]};

    // Output, [batch, height_out, width_out, channel_out]
    op->outputs[0]->dimensions.resize(4);

    // batch and channel
    op->outputs[0]->dimensions[0] = op->inputs[0]->dimensions[0];
    op->outputs[0]->dimensions[channelIndex] = op->inputs[1]->dimensions[0];

    // height
    explicitPadding(op->inputs[0]->dimensions[heightIndex], op->inputs[1]->dimensions[1],
                    strideHeight, dilationHeight, paddingTop, paddingBottom,
                    &op->outputs[0]->dimensions[heightIndex]);

    // width
    explicitPadding(op->inputs[0]->dimensions[widthIndex], op->inputs[1]->dimensions[2],
                    strideWidth, dilationWidth, paddingLeft, paddingRight,
                    &op->outputs[0]->dimensions[widthIndex]);

    setConvFCScale(/*applyOutputScaleBound=*/ver == TestHalVersion::V1_0, op);
}

static void conv2DImplicitConstructor(TestOperandType, uint32_t rank, TestHalVersion ver,
                                      RandomOperation* op) {
    NN_FUZZER_CHECK(rank == 4);

    // Parameters
    int32_t paddingScheme = op->inputs[3]->value<int32_t>();
    int32_t strideWidth = op->inputs[4]->value<int32_t>();
    int32_t strideHeight = op->inputs[5]->value<int32_t>();
    bool useNchw = false;
    int32_t dilationWidth = 1, dilationHeight = 1;
    if (op->inputs.size() > 7) {
        useNchw = op->inputs[7]->value<bool8>();
        if (op->inputs.size() > 8) {
            dilationWidth = op->inputs[8]->value<int32_t>();
            dilationHeight = op->inputs[9]->value<int32_t>();
        }
    }
    int heightIndex = useNchw ? 2 : 1;
    int widthIndex = useNchw ? 3 : 2;
    int channelIndex = useNchw ? 1 : 3;

    // Input, [batch, height_in, width_in, channel_in]
    op->inputs[0]->dimensions = {RandomVariableType::FREE, RandomVariableType::FREE,
                                 RandomVariableType::FREE, RandomVariableType::FREE};

    // Filter, [channel_out, height_flt, width_flt, channel_in]
    op->inputs[1]->dimensions = {RandomVariableType::FREE, RandomVariableType::FREE,
                                 RandomVariableType::FREE, op->inputs[0]->dimensions[channelIndex]};

    // Bias, [channel_out]
    op->inputs[2]->dimensions = {op->inputs[1]->dimensions[0]};

    // Output, [batch, height_out, width_out, channel_out]
    op->outputs[0]->dimensions.resize(4);

    // batch and channel
    op->outputs[0]->dimensions[0] = op->inputs[0]->dimensions[0];
    op->outputs[0]->dimensions[channelIndex] = op->inputs[1]->dimensions[0];

    // height and width
    implicitPadding(op->inputs[0]->dimensions[heightIndex], op->inputs[1]->dimensions[1],
                    strideHeight, dilationHeight, paddingScheme,
                    &op->outputs[0]->dimensions[heightIndex]);
    implicitPadding(op->inputs[0]->dimensions[widthIndex], op->inputs[1]->dimensions[2],
                    strideWidth, dilationWidth, paddingScheme,
                    &op->outputs[0]->dimensions[widthIndex]);

    setConvFCScale(/*applyOutputScaleBound=*/ver == TestHalVersion::V1_0, op);
}

#define DEFINE_CONV_2D_SIGNATURE(ver, ...)                                                         \
    DEFINE_OPERATION_SIGNATURE(CONV_2D_explicit_##ver){                                            \
            .opType = TestOperationType::CONV_2D,                                                  \
            .supportedDataTypes = {__VA_ARGS__},                                                   \
            .supportedRanks = {4},                                                                 \
            .version = TestHalVersion::ver,                                                        \
            .inputs =                                                                              \
                    {                                                                              \
                            INPUT_DEFAULT,                                                         \
                            INPUT_DEFAULT,                                                         \
                            INPUT_BIAS,                                                            \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_CHOICE(TestOperandType::INT32, 0, 1, 2, 3),                  \
                    },                                                                             \
            .outputs = {OUTPUT_DEFAULT},                                                           \
            .constructor = std::bind(conv2DExplicitConstructor, _1, _2, TestHalVersion::ver, _3)}; \
    DEFINE_OPERATION_SIGNATURE(CONV_2D_implicit_##ver){                                            \
            .opType = TestOperationType::CONV_2D,                                                  \
            .supportedDataTypes = {__VA_ARGS__},                                                   \
            .supportedRanks = {4},                                                                 \
            .version = TestHalVersion::ver,                                                        \
            .inputs =                                                                              \
                    {                                                                              \
                            INPUT_DEFAULT,                                                         \
                            INPUT_DEFAULT,                                                         \
                            INPUT_BIAS,                                                            \
                            PARAMETER_CHOICE(TestOperandType::INT32, 1, 2),                        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_CHOICE(TestOperandType::INT32, 0, 1, 2, 3),                  \
                    },                                                                             \
            .outputs = {OUTPUT_DEFAULT},                                                           \
            .constructor = std::bind(conv2DImplicitConstructor, _1, _2, TestHalVersion::ver, _3)};

DEFINE_CONV_2D_SIGNATURE(V1_0, TestOperandType::TENSOR_FLOAT32,
                         TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_CONV_2D_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT16,
                         TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_CONV_2D_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

#define DEFINE_CONV_2D_WITH_LAYOUT_OR_DILATION_SIGNATURE(ver, ...)                                 \
    DEFINE_OPERATION_SIGNATURE(CONV_2D_explicit_layout_##ver){                                     \
            .opType = TestOperationType::CONV_2D,                                                  \
            .supportedDataTypes = {__VA_ARGS__},                                                   \
            .supportedRanks = {4},                                                                 \
            .version = TestHalVersion::ver,                                                        \
            .inputs =                                                                              \
                    {                                                                              \
                            INPUT_DEFAULT,                                                         \
                            INPUT_DEFAULT,                                                         \
                            INPUT_BIAS,                                                            \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_CHOICE(TestOperandType::INT32, 0, 1, 2, 3),                  \
                            PARAMETER_CHOICE(TestOperandType::BOOL, true, false),                  \
                    },                                                                             \
            .outputs = {OUTPUT_DEFAULT},                                                           \
            .constructor = std::bind(conv2DExplicitConstructor, _1, _2, TestHalVersion::ver, _3)}; \
    DEFINE_OPERATION_SIGNATURE(CONV_2D_implicit_layout_##ver){                                     \
            .opType = TestOperationType::CONV_2D,                                                  \
            .supportedDataTypes = {__VA_ARGS__},                                                   \
            .supportedRanks = {4},                                                                 \
            .version = TestHalVersion::ver,                                                        \
            .inputs =                                                                              \
                    {                                                                              \
                            INPUT_DEFAULT,                                                         \
                            INPUT_DEFAULT,                                                         \
                            INPUT_BIAS,                                                            \
                            PARAMETER_CHOICE(TestOperandType::INT32, 1, 2),                        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_CHOICE(TestOperandType::INT32, 0, 1, 2, 3),                  \
                            PARAMETER_CHOICE(TestOperandType::BOOL, true, false),                  \
                    },                                                                             \
            .outputs = {OUTPUT_DEFAULT},                                                           \
            .constructor = std::bind(conv2DImplicitConstructor, _1, _2, TestHalVersion::ver, _3)}; \
    DEFINE_OPERATION_SIGNATURE(CONV_2D_explicit_dilation_##ver){                                   \
            .opType = TestOperationType::CONV_2D,                                                  \
            .supportedDataTypes = {__VA_ARGS__},                                                   \
            .supportedRanks = {4},                                                                 \
            .version = TestHalVersion::ver,                                                        \
            .inputs =                                                                              \
                    {                                                                              \
                            INPUT_DEFAULT,                                                         \
                            INPUT_DEFAULT,                                                         \
                            INPUT_BIAS,                                                            \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_CHOICE(TestOperandType::INT32, 0, 1, 2, 3),                  \
                            PARAMETER_CHOICE(TestOperandType::BOOL, true, false),                  \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                    },                                                                             \
            .outputs = {OUTPUT_DEFAULT},                                                           \
            .constructor = std::bind(conv2DExplicitConstructor, _1, _2, TestHalVersion::ver, _3)}; \
    DEFINE_OPERATION_SIGNATURE(CONV_2D_implicit_dilation_##ver){                                   \
            .opType = TestOperationType::CONV_2D,                                                  \
            .supportedDataTypes = {__VA_ARGS__},                                                   \
            .supportedRanks = {4},                                                                 \
            .version = TestHalVersion::ver,                                                        \
            .inputs =                                                                              \
                    {                                                                              \
                            INPUT_DEFAULT,                                                         \
                            INPUT_DEFAULT,                                                         \
                            INPUT_BIAS,                                                            \
                            PARAMETER_CHOICE(TestOperandType::INT32, 1, 2),                        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_CHOICE(TestOperandType::INT32, 0, 1, 2, 3),                  \
                            PARAMETER_CHOICE(TestOperandType::BOOL, true, false),                  \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),                         \
                    },                                                                             \
            .outputs = {OUTPUT_DEFAULT},                                                           \
            .constructor = std::bind(conv2DImplicitConstructor, _1, _2, TestHalVersion::ver, _3)};

DEFINE_CONV_2D_WITH_LAYOUT_OR_DILATION_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT32,
                                                 TestOperandType::TENSOR_QUANT8_ASYMM,
                                                 TestOperandType::TENSOR_FLOAT16);
DEFINE_CONV_2D_WITH_LAYOUT_OR_DILATION_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

static void depthwiseConv2DExplicitConstructor(TestOperandType, uint32_t rank, TestHalVersion ver,
                                               RandomOperation* op) {
    NN_FUZZER_CHECK(rank == 4);

    // Parameters
    int32_t paddingLeft = op->inputs[3]->value<int32_t>();
    int32_t paddingRight = op->inputs[4]->value<int32_t>();
    int32_t paddingTop = op->inputs[5]->value<int32_t>();
    int32_t paddingBottom = op->inputs[6]->value<int32_t>();
    int32_t strideWidth = op->inputs[7]->value<int32_t>();
    int32_t strideHeight = op->inputs[8]->value<int32_t>();
    bool useNchw = false;
    int32_t dilationWidth = 1, dilationHeight = 1;
    if (op->inputs.size() > 11) {
        useNchw = op->inputs[11]->value<bool8>();
        if (op->inputs.size() > 12) {
            dilationWidth = op->inputs[12]->value<int32_t>();
            dilationHeight = op->inputs[13]->value<int32_t>();
        }
    }
    int heightIndex = useNchw ? 2 : 1;
    int widthIndex = useNchw ? 3 : 2;
    int channelIndex = useNchw ? 1 : 3;

    // Input, [batch, height_in, width_in, channel_in]
    op->inputs[0]->dimensions = {RandomVariableType::FREE, RandomVariableType::FREE,
                                 RandomVariableType::FREE, RandomVariableType::FREE};

    // Filter, [1, height_flt, width_flt, channel_out]
    RandomVariable channelOut =
            op->inputs[9]->value<RandomVariable>() * op->inputs[0]->dimensions[channelIndex];
    op->inputs[1]->dimensions = {1, RandomVariableType::FREE, RandomVariableType::FREE, channelOut};

    // Bias, [channel_out]
    op->inputs[2]->dimensions = {channelOut};

    // Output, [batch, height_out, width_out, channel_out]
    op->outputs[0]->dimensions.resize(4);

    // batch and channel
    op->outputs[0]->dimensions[0] = op->inputs[0]->dimensions[0];
    op->outputs[0]->dimensions[channelIndex] = channelOut;

    // height
    explicitPadding(op->inputs[0]->dimensions[heightIndex], op->inputs[1]->dimensions[1],
                    strideHeight, dilationHeight, paddingTop, paddingBottom,
                    &op->outputs[0]->dimensions[heightIndex]);

    // width
    explicitPadding(op->inputs[0]->dimensions[widthIndex], op->inputs[1]->dimensions[2],
                    strideWidth, dilationWidth, paddingLeft, paddingRight,
                    &op->outputs[0]->dimensions[widthIndex]);

    setConvFCScale(/*applyOutputScaleBound=*/ver == TestHalVersion::V1_0, op);
}

static void depthwiseConv2DImplicitConstructor(TestOperandType, uint32_t rank, TestHalVersion ver,
                                               RandomOperation* op) {
    NN_FUZZER_CHECK(rank == 4);

    // Parameters
    int32_t paddingScheme = op->inputs[3]->value<int32_t>();
    int32_t strideWidth = op->inputs[4]->value<int32_t>();
    int32_t strideHeight = op->inputs[5]->value<int32_t>();
    bool useNchw = false;
    int32_t dilationWidth = 1, dilationHeight = 1;
    if (op->inputs.size() > 8) {
        useNchw = op->inputs[8]->value<bool8>();
        if (op->inputs.size() > 9) {
            dilationWidth = op->inputs[9]->value<int32_t>();
            dilationHeight = op->inputs[10]->value<int32_t>();
        }
    }
    int heightIndex = useNchw ? 2 : 1;
    int widthIndex = useNchw ? 3 : 2;
    int channelIndex = useNchw ? 1 : 3;

    // Input, [batch, height_in, width_in, channel_in]
    op->inputs[0]->dimensions = {RandomVariableType::FREE, RandomVariableType::FREE,
                                 RandomVariableType::FREE, RandomVariableType::FREE};

    // Filter, [1, height_flt, width_flt, channel_out]
    RandomVariable channelOut =
            op->inputs[6]->value<RandomVariable>() * op->inputs[0]->dimensions[channelIndex];
    op->inputs[1]->dimensions = {1, RandomVariableType::FREE, RandomVariableType::FREE, channelOut};

    // Bias, [channel_out]
    op->inputs[2]->dimensions = {channelOut};

    // Output, [batch, height_out, width_out, channel_out]
    op->outputs[0]->dimensions.resize(4);

    // batch and channel
    op->outputs[0]->dimensions[0] = op->inputs[0]->dimensions[0];
    op->outputs[0]->dimensions[channelIndex] = channelOut;

    // height and width
    implicitPadding(op->inputs[0]->dimensions[heightIndex], op->inputs[1]->dimensions[1],
                    strideHeight, dilationHeight, paddingScheme,
                    &op->outputs[0]->dimensions[heightIndex]);
    implicitPadding(op->inputs[0]->dimensions[widthIndex], op->inputs[1]->dimensions[2],
                    strideWidth, dilationWidth, paddingScheme,
                    &op->outputs[0]->dimensions[widthIndex]);

    setConvFCScale(/*applyOutputScaleBound=*/ver == TestHalVersion::V1_0, op);
}

#define DEFINE_DEPTHWISE_CONV_2D_SIGNATURE(ver, ...)                              \
    DEFINE_OPERATION_SIGNATURE(DEPTHWISE_CONV_2D_explicit_##ver){                 \
            .opType = TestOperationType::DEPTHWISE_CONV_2D,                       \
            .supportedDataTypes = {__VA_ARGS__},                                  \
            .supportedRanks = {4},                                                \
            .version = TestHalVersion::ver,                                       \
            .inputs =                                                             \
                    {                                                             \
                            INPUT_DEFAULT,                                        \
                            INPUT_DEFAULT,                                        \
                            INPUT_BIAS,                                           \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            RANDOM_INT_RANGE(1, 5),                               \
                            PARAMETER_CHOICE(TestOperandType::INT32, 0, 1, 2, 3), \
                    },                                                            \
            .outputs = {OUTPUT_DEFAULT},                                          \
            .constructor = std::bind(depthwiseConv2DExplicitConstructor, _1, _2,  \
                                     TestHalVersion::ver, _3)};                   \
    DEFINE_OPERATION_SIGNATURE(DEPTHWISE_CONV_2D_implicit_##ver){                 \
            .opType = TestOperationType::DEPTHWISE_CONV_2D,                       \
            .supportedDataTypes = {__VA_ARGS__},                                  \
            .supportedRanks = {4},                                                \
            .version = TestHalVersion::ver,                                       \
            .inputs =                                                             \
                    {                                                             \
                            INPUT_DEFAULT,                                        \
                            INPUT_DEFAULT,                                        \
                            INPUT_BIAS,                                           \
                            PARAMETER_CHOICE(TestOperandType::INT32, 1, 2),       \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            RANDOM_INT_RANGE(1, 5),                               \
                            PARAMETER_CHOICE(TestOperandType::INT32, 0, 1, 2, 3), \
                    },                                                            \
            .outputs = {OUTPUT_DEFAULT},                                          \
            .constructor = std::bind(depthwiseConv2DImplicitConstructor, _1, _2,  \
                                     TestHalVersion::ver, _3)};

DEFINE_DEPTHWISE_CONV_2D_SIGNATURE(V1_0, TestOperandType::TENSOR_FLOAT32,
                                   TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_DEPTHWISE_CONV_2D_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT16,
                                   TestOperandType::TENSOR_QUANT8_ASYMM);
DEFINE_DEPTHWISE_CONV_2D_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

#define DEFINE_DEPTHWISE_CONV_2D_WITH_LAYOUT_OR_DILATION_SIGNATURE(ver, ...)      \
    DEFINE_OPERATION_SIGNATURE(DEPTHWISE_CONV_2D_explicit_layout_##ver){          \
            .opType = TestOperationType::DEPTHWISE_CONV_2D,                       \
            .supportedDataTypes = {__VA_ARGS__},                                  \
            .supportedRanks = {4},                                                \
            .version = TestHalVersion::ver,                                       \
            .inputs =                                                             \
                    {                                                             \
                            INPUT_DEFAULT,                                        \
                            INPUT_DEFAULT,                                        \
                            INPUT_BIAS,                                           \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            RANDOM_INT_RANGE(1, 5),                               \
                            PARAMETER_CHOICE(TestOperandType::INT32, 0, 1, 2, 3), \
                            PARAMETER_CHOICE(TestOperandType::BOOL, true, false), \
                    },                                                            \
            .outputs = {OUTPUT_DEFAULT},                                          \
            .constructor = std::bind(depthwiseConv2DExplicitConstructor, _1, _2,  \
                                     TestHalVersion::ver, _3)};                   \
    DEFINE_OPERATION_SIGNATURE(DEPTHWISE_CONV_2D_implicit_layout_##ver){          \
            .opType = TestOperationType::DEPTHWISE_CONV_2D,                       \
            .supportedDataTypes = {__VA_ARGS__},                                  \
            .supportedRanks = {4},                                                \
            .version = TestHalVersion::ver,                                       \
            .inputs =                                                             \
                    {                                                             \
                            INPUT_DEFAULT,                                        \
                            INPUT_DEFAULT,                                        \
                            INPUT_BIAS,                                           \
                            PARAMETER_CHOICE(TestOperandType::INT32, 1, 2),       \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            RANDOM_INT_RANGE(1, 5),                               \
                            PARAMETER_CHOICE(TestOperandType::INT32, 0, 1, 2, 3), \
                            PARAMETER_CHOICE(TestOperandType::BOOL, true, false), \
                    },                                                            \
            .outputs = {OUTPUT_DEFAULT},                                          \
            .constructor = std::bind(depthwiseConv2DImplicitConstructor, _1, _2,  \
                                     TestHalVersion::ver, _3)};                   \
    DEFINE_OPERATION_SIGNATURE(DEPTHWISE_CONV_2D_explicit_dilation_##ver){        \
            .opType = TestOperationType::DEPTHWISE_CONV_2D,                       \
            .supportedDataTypes = {__VA_ARGS__},                                  \
            .supportedRanks = {4},                                                \
            .version = TestHalVersion::ver,                                       \
            .inputs =                                                             \
                    {                                                             \
                            INPUT_DEFAULT,                                        \
                            INPUT_DEFAULT,                                        \
                            INPUT_BIAS,                                           \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            RANDOM_INT_RANGE(1, 5),                               \
                            PARAMETER_CHOICE(TestOperandType::INT32, 0, 1, 2, 3), \
                            PARAMETER_CHOICE(TestOperandType::BOOL, true, false), \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                    },                                                            \
            .outputs = {OUTPUT_DEFAULT},                                          \
            .constructor = std::bind(depthwiseConv2DExplicitConstructor, _1, _2,  \
                                     TestHalVersion::ver, _3)};                   \
    DEFINE_OPERATION_SIGNATURE(DEPTHWISE_CONV_2D_implicit_dilation_##ver){        \
            .opType = TestOperationType::DEPTHWISE_CONV_2D,                       \
            .supportedDataTypes = {__VA_ARGS__},                                  \
            .supportedRanks = {4},                                                \
            .version = TestHalVersion::ver,                                       \
            .inputs =                                                             \
                    {                                                             \
                            INPUT_DEFAULT,                                        \
                            INPUT_DEFAULT,                                        \
                            INPUT_BIAS,                                           \
                            PARAMETER_CHOICE(TestOperandType::INT32, 1, 2),       \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            RANDOM_INT_RANGE(1, 5),                               \
                            PARAMETER_CHOICE(TestOperandType::INT32, 0, 1, 2, 3), \
                            PARAMETER_CHOICE(TestOperandType::BOOL, true, false), \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                    },                                                            \
            .outputs = {OUTPUT_DEFAULT},                                          \
            .constructor = std::bind(depthwiseConv2DImplicitConstructor, _1, _2,  \
                                     TestHalVersion::ver, _3)};

DEFINE_DEPTHWISE_CONV_2D_WITH_LAYOUT_OR_DILATION_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT32,
                                                           TestOperandType::TENSOR_QUANT8_ASYMM,
                                                           TestOperandType::TENSOR_FLOAT16);
DEFINE_DEPTHWISE_CONV_2D_WITH_LAYOUT_OR_DILATION_SIGNATURE(
        V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

static void groupedConv2DExplicitConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    NN_FUZZER_CHECK(rank == 4);

    // Parameters
    int32_t paddingLeft = op->inputs[3]->value<int32_t>();
    int32_t paddingRight = op->inputs[4]->value<int32_t>();
    int32_t paddingTop = op->inputs[5]->value<int32_t>();
    int32_t paddingBottom = op->inputs[6]->value<int32_t>();
    int32_t strideWidth = op->inputs[7]->value<int32_t>();
    int32_t strideHeight = op->inputs[8]->value<int32_t>();
    bool useNchw = op->inputs[11]->value<bool8>();
    int heightIndex = useNchw ? 2 : 1;
    int widthIndex = useNchw ? 3 : 2;
    int channelIndex = useNchw ? 1 : 3;

    // Input, [batch, height_in, width_in, channel_in]
    RandomVariable numGroups = op->inputs[9]->value<RandomVariable>();
    RandomVariable channelGroup = RandomVariableType::FREE;
    if (useNchw) {
        op->inputs[0]->dimensions = {RandomVariableType::FREE, numGroups * channelGroup,
                                     RandomVariableType::FREE, RandomVariableType::FREE};
    } else {
        op->inputs[0]->dimensions = {RandomVariableType::FREE, RandomVariableType::FREE,
                                     RandomVariableType::FREE, numGroups * channelGroup};
    }

    // Filter, [channel_out, height_flt, width_flt, channel_group]
    op->inputs[1]->dimensions = {RandomVariableType::FREE, RandomVariableType::FREE,
                                 RandomVariableType::FREE, channelGroup};
    // channel_out must be divisible by num_groups.
    (op->inputs[1]->dimensions[0] % numGroups).setEqual(0);

    // Bias, [channel_out]
    op->inputs[2]->dimensions = {op->inputs[1]->dimensions[0]};

    // Output, [batch, height_out, width_out, channel_out]
    op->outputs[0]->dimensions.resize(4);

    // batch and channel
    op->outputs[0]->dimensions[0] = op->inputs[0]->dimensions[0];
    op->outputs[0]->dimensions[channelIndex] = op->inputs[1]->dimensions[0];

    // height
    explicitPadding(op->inputs[0]->dimensions[heightIndex], op->inputs[1]->dimensions[1],
                    strideHeight, /*dilation=*/1, paddingTop, paddingBottom,
                    &op->outputs[0]->dimensions[heightIndex]);

    // width
    explicitPadding(op->inputs[0]->dimensions[widthIndex], op->inputs[1]->dimensions[2],
                    strideWidth, /*dilation=*/1, paddingLeft, paddingRight,
                    &op->outputs[0]->dimensions[widthIndex]);

    setConvFCScale(/*applyOutputScaleBound=*/false, op);
}

static void groupedConv2DImplicitConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    NN_FUZZER_CHECK(rank == 4);

    // Parameters
    int32_t paddingScheme = op->inputs[3]->value<int32_t>();
    int32_t strideWidth = op->inputs[4]->value<int32_t>();
    int32_t strideHeight = op->inputs[5]->value<int32_t>();
    bool useNchw = op->inputs[8]->value<bool8>();
    int heightIndex = useNchw ? 2 : 1;
    int widthIndex = useNchw ? 3 : 2;
    int channelIndex = useNchw ? 1 : 3;

    // Input, [batch, height_in, width_in, channel_in]
    RandomVariable numGroups = op->inputs[6]->value<RandomVariable>();
    RandomVariable channelGroup = RandomVariableType::FREE;
    if (useNchw) {
        op->inputs[0]->dimensions = {RandomVariableType::FREE, numGroups * channelGroup,
                                     RandomVariableType::FREE, RandomVariableType::FREE};
    } else {
        op->inputs[0]->dimensions = {RandomVariableType::FREE, RandomVariableType::FREE,
                                     RandomVariableType::FREE, numGroups * channelGroup};
    }

    // Filter, [channel_out, height_flt, width_flt, channel_group]
    op->inputs[1]->dimensions = {RandomVariableType::FREE, RandomVariableType::FREE,
                                 RandomVariableType::FREE, channelGroup};
    // channel_out must be divisible by num_groups.
    (op->inputs[1]->dimensions[0] % numGroups).setEqual(0);

    // Bias, [channel_out]
    op->inputs[2]->dimensions = {op->inputs[1]->dimensions[0]};

    // Output, [batch, height_out, width_out, channel_out]
    op->outputs[0]->dimensions.resize(4);

    // batch and channel
    op->outputs[0]->dimensions[0] = op->inputs[0]->dimensions[0];
    op->outputs[0]->dimensions[channelIndex] = op->inputs[1]->dimensions[0];

    // height and width
    implicitPadding(op->inputs[0]->dimensions[heightIndex], op->inputs[1]->dimensions[1],
                    strideHeight, /*dilation=*/1, paddingScheme,
                    &op->outputs[0]->dimensions[heightIndex]);
    implicitPadding(op->inputs[0]->dimensions[widthIndex], op->inputs[1]->dimensions[2],
                    strideWidth, /*dilation=*/1, paddingScheme,
                    &op->outputs[0]->dimensions[widthIndex]);

    setConvFCScale(/*applyOutputScaleBound=*/false, op);
}

#define DEFINE_GROUPED_CONV_2D_SIGNATURE(ver, ...)                                \
    DEFINE_OPERATION_SIGNATURE(GROUPED_CONV_2D_explicit_##ver){                   \
            .opType = TestOperationType::GROUPED_CONV_2D,                         \
            .supportedDataTypes = {__VA_ARGS__},                                  \
            .supportedRanks = {4},                                                \
            .version = TestHalVersion::ver,                                       \
            .inputs =                                                             \
                    {                                                             \
                            INPUT_DEFAULT,                                        \
                            INPUT_DEFAULT,                                        \
                            INPUT_BIAS,                                           \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            RANDOM_INT_RANGE(1, 5),                               \
                            PARAMETER_CHOICE(TestOperandType::INT32, 0, 1, 2, 3), \
                            PARAMETER_CHOICE(TestOperandType::BOOL, true, false), \
                    },                                                            \
            .outputs = {OUTPUT_DEFAULT},                                          \
            .constructor = groupedConv2DExplicitConstructor};                     \
    DEFINE_OPERATION_SIGNATURE(GROUPED_CONV_2D_implicit_##ver){                   \
            .opType = TestOperationType::GROUPED_CONV_2D,                         \
            .supportedDataTypes = {__VA_ARGS__},                                  \
            .supportedRanks = {4},                                                \
            .version = TestHalVersion::ver,                                       \
            .inputs =                                                             \
                    {                                                             \
                            INPUT_DEFAULT,                                        \
                            INPUT_DEFAULT,                                        \
                            INPUT_BIAS,                                           \
                            PARAMETER_CHOICE(TestOperandType::INT32, 1, 2),       \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            RANDOM_INT_RANGE(1, 5),                               \
                            PARAMETER_CHOICE(TestOperandType::INT32, 0, 1, 2, 3), \
                            PARAMETER_CHOICE(TestOperandType::BOOL, true, false), \
                    },                                                            \
            .outputs = {OUTPUT_DEFAULT},                                          \
            .constructor = groupedConv2DImplicitConstructor};

DEFINE_GROUPED_CONV_2D_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT32,
                                 TestOperandType::TENSOR_QUANT8_ASYMM,
                                 TestOperandType::TENSOR_FLOAT16);
DEFINE_GROUPED_CONV_2D_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

static void transposeConv2DExplicitConstructor(TestOperandType, uint32_t rank,
                                               RandomOperation* op) {
    NN_FUZZER_CHECK(rank == 4);

    // Parameters
    int32_t paddingLeft = op->inputs[3]->value<int32_t>();
    int32_t paddingRight = op->inputs[4]->value<int32_t>();
    int32_t paddingTop = op->inputs[5]->value<int32_t>();
    int32_t paddingBottom = op->inputs[6]->value<int32_t>();
    int32_t strideWidth = op->inputs[7]->value<int32_t>();
    int32_t strideHeight = op->inputs[8]->value<int32_t>();
    bool useNchw = op->inputs[10]->value<bool8>();
    int heightIndex = useNchw ? 2 : 1;
    int widthIndex = useNchw ? 3 : 2;
    int channelIndex = useNchw ? 1 : 3;

    // Input, [batch, height_in, width_in, channel_in]
    op->inputs[0]->dimensions = {RandomVariableType::FREE, RandomVariableType::FREE,
                                 RandomVariableType::FREE, RandomVariableType::FREE};

    // Filter, [channel_out, height_flt, width_flt, channel_in]
    op->inputs[1]->dimensions = {RandomVariableType::FREE, RandomVariableType::FREE,
                                 RandomVariableType::FREE, op->inputs[0]->dimensions[channelIndex]};

    // Bias, [channel_out]
    op->inputs[2]->dimensions = {op->inputs[1]->dimensions[0]};

    // Output, [batch, height_out, width_out, channel_out]
    op->outputs[0]->dimensions.resize(4);

    // batch and channel
    op->outputs[0]->dimensions[0] = op->inputs[0]->dimensions[0];
    op->outputs[0]->dimensions[channelIndex] = op->inputs[1]->dimensions[0];

    // height
    explicitPaddingTranspose(op->inputs[0]->dimensions[heightIndex], op->inputs[1]->dimensions[1],
                             strideHeight, paddingTop, paddingBottom,
                             &op->outputs[0]->dimensions[heightIndex]);

    // width
    explicitPaddingTranspose(op->inputs[0]->dimensions[widthIndex], op->inputs[1]->dimensions[2],
                             strideWidth, paddingLeft, paddingRight,
                             &op->outputs[0]->dimensions[widthIndex]);

    setConvFCScale(/*applyOutputScaleBound=*/false, op);
}

static void transposeConv2DImplicitConstructor(TestOperandType, uint32_t rank,
                                               RandomOperation* op) {
    NN_FUZZER_CHECK(rank == 4);

    // Parameters
    int32_t paddingScheme = op->inputs[4]->value<int32_t>();
    int32_t strideWidth = op->inputs[5]->value<int32_t>();
    int32_t strideHeight = op->inputs[6]->value<int32_t>();
    bool useNchw = op->inputs[8]->value<bool8>();
    int heightIndex = useNchw ? 2 : 1;
    int widthIndex = useNchw ? 3 : 2;
    int channelIndex = useNchw ? 1 : 3;

    // Input, [batch, height_in, width_in, channel_in]
    op->inputs[0]->dimensions = {RandomVariableType::FREE, RandomVariableType::FREE,
                                 RandomVariableType::FREE, RandomVariableType::FREE};

    // Filter, [channel_out, height_flt, width_flt, channel_in]
    op->inputs[1]->dimensions = {RandomVariableType::FREE, RandomVariableType::FREE,
                                 RandomVariableType::FREE, op->inputs[0]->dimensions[channelIndex]};

    // Bias, [channel_out]
    op->inputs[2]->dimensions = {op->inputs[1]->dimensions[0]};

    // Output, [batch, height_out, width_out, channel_out]
    op->outputs[0]->dimensions.resize(4);

    // batch and channel
    op->outputs[0]->dimensions[0] = op->inputs[0]->dimensions[0];
    op->outputs[0]->dimensions[channelIndex] = op->inputs[1]->dimensions[0];

    // height and width
    implicitPaddingTranspose(op->inputs[0]->dimensions[heightIndex], op->inputs[1]->dimensions[1],
                             strideHeight, paddingScheme, &op->outputs[0]->dimensions[heightIndex]);
    implicitPaddingTranspose(op->inputs[0]->dimensions[widthIndex], op->inputs[1]->dimensions[2],
                             strideWidth, paddingScheme, &op->outputs[0]->dimensions[widthIndex]);
    op->inputs[3]->dimensions = {4};
    op->inputs[3]->randomBuffer = op->outputs[0]->dimensions;

    setConvFCScale(/*applyOutputScaleBound=*/false, op);
}

#define DEFINE_TRANSPOSE_CONV_2D_SIGNATURE(ver, ...)                              \
    DEFINE_OPERATION_SIGNATURE(TRANSPOSE_CONV_2D_explicit_##ver){                 \
            .opType = TestOperationType::TRANSPOSE_CONV_2D,                       \
            .supportedDataTypes = {__VA_ARGS__},                                  \
            .supportedRanks = {4},                                                \
            .version = TestHalVersion::ver,                                       \
            .inputs =                                                             \
                    {                                                             \
                            INPUT_DEFAULT,                                        \
                            INPUT_DEFAULT,                                        \
                            INPUT_BIAS,                                           \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_CHOICE(TestOperandType::INT32, 0, 1, 2, 3), \
                            PARAMETER_CHOICE(TestOperandType::BOOL, true, false), \
                    },                                                            \
            .outputs = {OUTPUT_DEFAULT},                                          \
            .constructor = transposeConv2DExplicitConstructor};                   \
    DEFINE_OPERATION_SIGNATURE(TRANSPOSE_CONV_2D_implicit_##ver){                 \
            .opType = TestOperationType::TRANSPOSE_CONV_2D,                       \
            .supportedDataTypes = {__VA_ARGS__},                                  \
            .supportedRanks = {4},                                                \
            .version = TestHalVersion::ver,                                       \
            .inputs =                                                             \
                    {                                                             \
                            INPUT_DEFAULT,                                        \
                            INPUT_DEFAULT,                                        \
                            INPUT_BIAS,                                           \
                            PARAMETER_NONE(TestOperandType::TENSOR_INT32),        \
                            PARAMETER_CHOICE(TestOperandType::INT32, 1, 2),       \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_RANGE(TestOperandType::INT32, 1, 3),        \
                            PARAMETER_CHOICE(TestOperandType::INT32, 0, 1, 2, 3), \
                            PARAMETER_CHOICE(TestOperandType::BOOL, true, false), \
                    },                                                            \
            .outputs = {OUTPUT_DEFAULT},                                          \
            .constructor = transposeConv2DImplicitConstructor};

DEFINE_TRANSPOSE_CONV_2D_SIGNATURE(V1_2, TestOperandType::TENSOR_FLOAT32,
                                   TestOperandType::TENSOR_QUANT8_ASYMM,
                                   TestOperandType::TENSOR_FLOAT16);
DEFINE_TRANSPOSE_CONV_2D_SIGNATURE(V1_3, TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED);

}  // namespace fuzzing_test
}  // namespace nn
}  // namespace android
