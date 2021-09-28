// Generated from generate_proposals_quant8_signed.mod.py
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;

namespace generated_tests::generate_proposals_quant8_signed {

const TestModel& get_test_model_nhwc_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2, 3},
                .operands = {{ // scores
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({52, 62, 57, 57, 47, 52, 62, 67}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -28
                        }, { // bboxDeltas
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({10, 2, 2, 2, 10, 2, 10, 2, -5, 2, -2, -2, -5, 2, 4, 2, 8, -2, -4, 4, 8, -2, -4, 4, -4, -4, 4, 4, -4, -4, 4, 4}),
                            .dimensions = {1, 2, 2, 8},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // anchors
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({0, 8, 32, 24, 8, 0, 24, 32}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // imageInfo
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({256, 256}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // scoresOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({67, 62, 57, 52}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -28
                        }, { // roiOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({35, 22, 55, 61, 11, 2, 37, 37, 26, 10, 54, 25, 16, 25, 29, 64}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
                            .outputs = {11, 12, 13},
                            .type = TestOperationType::GENERATE_PROPOSALS
                        }},
                .outputIndexes = {11, 12, 13}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_nhwc_quant8_signed = TestModelManager::get().add("generate_proposals_quant8_signed_nhwc_quant8_signed", get_test_model_nhwc_quant8_signed());

}  // namespace generated_tests::generate_proposals_quant8_signed

namespace generated_tests::generate_proposals_quant8_signed {

const TestModel& get_test_model_nhwc_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2, 3, 14, 17},
                .operands = {{ // scores
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -28
                        }, { // bboxDeltas
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {1, 2, 2, 8},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // anchors
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({0, 8, 32, 24, 8, 0, 24, 32}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // imageInfo
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({256, 256}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // scoresOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({67, 62, 57, 52}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -28
                        }, { // roiOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({35, 22, 55, 61, 11, 2, 37, 37, 26, 10, 54, 25, 16, 25, 29, 64}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // scores_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({52, 62, 57, 57, 47, 52, 62, 67}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -28
                        }, { // dummy
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-28}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -28
                        }, { // param12
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // bboxDeltas_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({10, 2, 2, 2, 10, 2, 10, 2, -5, 2, -2, -2, -5, 2, 4, 2, 8, -2, -4, 4, 8, -2, -4, 4, -4, -4, 4, 4, -4, -4, 4, 4}),
                            .dimensions = {1, 2, 2, 8},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // dummy1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // param13
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {14, 15, 16},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {17, 18, 19},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
                            .outputs = {11, 12, 13},
                            .type = TestOperationType::GENERATE_PROPOSALS
                        }},
                .outputIndexes = {11, 12, 13}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_nhwc_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("generate_proposals_quant8_signed_nhwc_quant8_signed_all_inputs_as_internal", get_test_model_nhwc_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::generate_proposals_quant8_signed

namespace generated_tests::generate_proposals_quant8_signed {

const TestModel& get_test_model_nchw_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2, 3},
                .operands = {{ // scores
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({52, 57, 47, 62, 62, 57, 52, 67}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -28
                        }, { // bboxDeltas
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({10, -5, 8, -4, 2, 2, -2, -4, 2, -2, -4, 4, 2, -2, 4, 4, 10, -5, 8, -4, 2, 2, -2, -4, 10, 4, -4, 4, 2, 2, 4, 4}),
                            .dimensions = {1, 8, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // anchors
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({0, 8, 32, 24, 8, 0, 24, 32}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // imageInfo
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({256, 256}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // scoresOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({67, 62, 57, 52}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -28
                        }, { // roiOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({35, 22, 55, 61, 11, 2, 37, 37, 26, 10, 54, 25, 16, 25, 29, 64}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
                            .outputs = {11, 12, 13},
                            .type = TestOperationType::GENERATE_PROPOSALS
                        }},
                .outputIndexes = {11, 12, 13}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_nchw_quant8_signed = TestModelManager::get().add("generate_proposals_quant8_signed_nchw_quant8_signed", get_test_model_nchw_quant8_signed());

}  // namespace generated_tests::generate_proposals_quant8_signed

namespace generated_tests::generate_proposals_quant8_signed {

const TestModel& get_test_model_nchw_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2, 3, 14, 17},
                .operands = {{ // scores
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -28
                        }, { // bboxDeltas
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {1, 8, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // anchors
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({0, 8, 32, 24, 8, 0, 24, 32}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // imageInfo
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({256, 256}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // scoresOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({67, 62, 57, 52}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -28
                        }, { // roiOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({35, 22, 55, 61, 11, 2, 37, 37, 26, 10, 54, 25, 16, 25, 29, 64}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // scores_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({52, 57, 47, 62, 62, 57, 52, 67}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -28
                        }, { // dummy2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-28}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -28
                        }, { // param14
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // bboxDeltas_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({10, -5, 8, -4, 2, 2, -2, -4, 2, -2, -4, 4, 2, -2, 4, 4, 10, -5, 8, -4, 2, 2, -2, -4, 10, 4, -4, 4, 2, 2, 4, 4}),
                            .dimensions = {1, 8, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // dummy3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // param15
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {14, 15, 16},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {17, 18, 19},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
                            .outputs = {11, 12, 13},
                            .type = TestOperationType::GENERATE_PROPOSALS
                        }},
                .outputIndexes = {11, 12, 13}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_nchw_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("generate_proposals_quant8_signed_nchw_quant8_signed_all_inputs_as_internal", get_test_model_nchw_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::generate_proposals_quant8_signed

namespace generated_tests::generate_proposals_quant8_signed {

const TestModel& get_test_model_nhwc_quant8_signed_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2, 3},
                .operands = {{ // scores1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({49, -86, 28, -14, 31, 4, 55, -5, -74, 10, 1, 61, -35, -59, 43, -17, -32, -8, 19, -2, -29, -122, -104, -83, -80, -71, -26, -65, -41, -77, -11, -116, 52, 22, -92, -38, -56, -110, -47, -98, -128, -89, -113, 34, 46, 58, -50, -95, 37, -23, 25, -107, -20, 13, 7, -68, -44, -119, -62, -125, 40, -101, 16, -53, -29, -65, -89, -80, -86, -20, 28, 16, -119, 58, -74, 19, -101, -110, 34, 13, -50, 49, -44, 61, 52, -83, 22, -68, -53, -2, 37, 7, -125, -32, 1, -5, -62, -35, 4, -8, -113, 40, -71, -14, -11, -95, -116, -56, 31, 43, -107, -38, -128, 46, -23, -77, 10, -17, -98, -59, -122, 55, -47, -41, 25, -104, -26, -92}),
                            .dimensions = {2, 4, 4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.005f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // bboxDeltas1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-19, 4, 14, 5, -15, -2, 3, 12, 0, -6, 4, -13, 8, 9, -2, 8, -2, 0, 4, 1, -2, -16, -6, -1, -10, 6, 5, -2, -17, -14, 5, -1, -15, 13, -7, -9, 9, 2, -2, 0, -7, 3, -4, -3, -5, -3, 10, -7, 12, -3, 0, 3, -7, 10, -2, -6, -13, 0, 3, 1, 4, 2, 24, 0, 1, 0, 7, -9, 1, -4, 3, -3, -7, 1, 7, 0, -3, 16, 0, 11, 4, -7, -9, 0, 0, 4, -6, 4, -19, -12, 0, -3, 2, 0, 1, 8, 0, 9, -17, 3, 7, -7, 7, 12, -4, -1, -6, 6, -4, -2, 3, -5, 0, 10, -1, -3, -8, 1, -12, -24, 1, 14, 4, 1, -11, 4, -4, -2, 1, 0, 7, 1, -13, 1, -4, -2, 2, 1, -8, 0, -14, 20, -6, -5, 0, 10, -14, -11, 6, -7, 4, 11, -11, 16, -3, 0, -7, 3, -13, 0, 0, 0, -3, 0, -11, -15, 9, -14, -7, 1, -14, 9, 1, 2, -1, -17, 2, -3, -9, 11, 1, 10, 10, -9, 7, 0, -3, 2, -8, -5, 6, -12, 10, 6, 0, -16, 1, -12, 7, 8, 5, -2, -8, -13, -3, 0, 0, 3, -6, -3, 13, 1, 22, 12, -11, 1, 12, 12, 13, -9, 1, -5, 1, -7, -13, 13, 1, 20, 0, 2, 6, 0, -1, -4, -5, 1, -6, -3, 2, -4, -4, -7, -18, 4, -7, 4, 14, -3, 8, 0, 4, -1, -10, 2, 5, -6, -11, 2, 16, -2, -4, -9, 0, 3, 0, 3, -3, 3, 3, 19, 3, -5, -8, -13, -8, 2, 2, -4, -3, 6, 2, -2, 12, 0, 0, -3, 3, -15, -10, -3, -7, -3, -4, -10, -6, -7, -2, 6, -3, 5, -2, 3, -5, -17, 0, -7, -1, -15, -9, 6, 3, -1, 2, 5, 6, -8, -3, 6, 9, -3, 1, -17, -15, 0, -1, -3, 7, -3, -4, 0, -4, -3, 1, 11, 18, -9, 6, 5, 2, -7, 2, 1, 12, 22, 3, 6, 4, 1, 2, 0, -11, -2, -7, 0, -12, 6, -6, -2, -4, 0, 7, -12, 8, 0, -3, 2, 6, -10, -1, -1, 0, -4, -2, 4, -14, 3, 1, 13, -2, -7, 6, 7, 6, 1, -4, 1, -2, -8, 0, -13, 12, 14, 11, 5, 3, 0, 1, -4, 5, -1, -5, 3, -7, 9, -1, -4, 2, -8, 10, 10, 1, 1, -2, 0, -4, -3, -8, 7, -9, -3, -3, -28, 10, 14, 0, -26, 11, -11, 5, 1, -4, -15, 0, 3, -3, -2, 7, -8, -1, 5, 7, 14, -12, -10, -6, 2, 11, -9, 7, -4, 0, 0, -2, -2, 1, 0, 0, -7, -7, -14, -9, -5, -6, 4, 3, 0, 9, -2, 7, 12, 5, 8, -5, 10, 2, -5, 13, -5, 3, 12, -3, -1, 13, 2, 6, -14, -1, -2, -4, -9, 12, -9, -2, -12, -10, -2, -16, 21, -6, -2, -3, 5, 9, -4, 0, -1, 1, -6, -10, -7, 2, -2}),
                            .dimensions = {2, 4, 4, 16},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // anchors1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({0, 48, 128, 80, 48, 0, 80, 128, 24, 40, 104, 88, 40, 24, 88, 104}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // imageInfo1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({512, 512, 256, 256}),
                            .dimensions = {2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // param6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({10.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({10.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param8
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({32}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param9
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({16}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.2f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // scoresOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({61, 58, 55, 52, 46, 40, 34, 31, 28, 25, 22, 19, 16, 13, 10, 7, 61, 55, 52, 49, 46, 40, 34, 28, 19, 16, -2, -8, -11, -20}),
                            .dimensions = {30},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.005f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // roiOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({135, 20, 265, 60, 264, 326, 351, 378, 0, 73, 130, 112, 0, 206, 206, 242, 304, 166, 356, 262, 242, 258, 263, 305, 207, 233, 251, 247, 23, 46, 214, 82, 4, 29, 124, 42, 86, 288, 122, 301, 12, 184, 33, 289, 0, 125, 89, 173, 308, 282, 428, 326, 209, 387, 220, 512, 240, 26, 266, 154, 93, 352, 387, 371, 0, 216, 214, 248, 229, 72, 256, 177, 143, 152, 256, 168, 29, 58, 93, 153, 40, 214, 75, 256, 121, 144, 160, 202, 247, 77, 256, 99, 27, 30, 256, 78, 200, 81, 229, 95, 112, 31, 176, 49, 194, 54, 216, 182, 29, 156, 108, 228, 96, 235, 148, 256, 139, 0, 162, 129}),
                            .dimensions = {30, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {30},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
                            .outputs = {11, 12, 13},
                            .type = TestOperationType::GENERATE_PROPOSALS
                        }},
                .outputIndexes = {11, 12, 13}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_nhwc_quant8_signed_2 = TestModelManager::get().add("generate_proposals_quant8_signed_nhwc_quant8_signed_2", get_test_model_nhwc_quant8_signed_2());

}  // namespace generated_tests::generate_proposals_quant8_signed

namespace generated_tests::generate_proposals_quant8_signed {

const TestModel& get_test_model_nhwc_quant8_signed_all_inputs_as_internal_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2, 3, 14, 17},
                .operands = {{ // scores1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 4, 4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.005f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // bboxDeltas1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 4, 4, 16},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // anchors1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({0, 48, 128, 80, 48, 0, 80, 128, 24, 40, 104, 88, 40, 24, 88, 104}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // imageInfo1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({512, 512, 256, 256}),
                            .dimensions = {2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // param6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({10.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({10.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param8
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({32}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param9
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({16}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.2f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // scoresOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({61, 58, 55, 52, 46, 40, 34, 31, 28, 25, 22, 19, 16, 13, 10, 7, 61, 55, 52, 49, 46, 40, 34, 28, 19, 16, -2, -8, -11, -20}),
                            .dimensions = {30},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.005f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // roiOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({135, 20, 265, 60, 264, 326, 351, 378, 0, 73, 130, 112, 0, 206, 206, 242, 304, 166, 356, 262, 242, 258, 263, 305, 207, 233, 251, 247, 23, 46, 214, 82, 4, 29, 124, 42, 86, 288, 122, 301, 12, 184, 33, 289, 0, 125, 89, 173, 308, 282, 428, 326, 209, 387, 220, 512, 240, 26, 266, 154, 93, 352, 387, 371, 0, 216, 214, 248, 229, 72, 256, 177, 143, 152, 256, 168, 29, 58, 93, 153, 40, 214, 75, 256, 121, 144, 160, 202, 247, 77, 256, 99, 27, 30, 256, 78, 200, 81, 229, 95, 112, 31, 176, 49, 194, 54, 216, 182, 29, 156, 108, 228, 96, 235, 148, 256, 139, 0, 162, 129}),
                            .dimensions = {30, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {30},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // scores1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({49, -86, 28, -14, 31, 4, 55, -5, -74, 10, 1, 61, -35, -59, 43, -17, -32, -8, 19, -2, -29, -122, -104, -83, -80, -71, -26, -65, -41, -77, -11, -116, 52, 22, -92, -38, -56, -110, -47, -98, -128, -89, -113, 34, 46, 58, -50, -95, 37, -23, 25, -107, -20, 13, 7, -68, -44, -119, -62, -125, 40, -101, 16, -53, -29, -65, -89, -80, -86, -20, 28, 16, -119, 58, -74, 19, -101, -110, 34, 13, -50, 49, -44, 61, 52, -83, 22, -68, -53, -2, 37, 7, -125, -32, 1, -5, -62, -35, 4, -8, -113, 40, -71, -14, -11, -95, -116, -56, 31, 43, -107, -38, -128, 46, -23, -77, 10, -17, -98, -59, -122, 55, -47, -41, 25, -104, -26, -92}),
                            .dimensions = {2, 4, 4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.005f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.005f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param16
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // bboxDeltas1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-19, 4, 14, 5, -15, -2, 3, 12, 0, -6, 4, -13, 8, 9, -2, 8, -2, 0, 4, 1, -2, -16, -6, -1, -10, 6, 5, -2, -17, -14, 5, -1, -15, 13, -7, -9, 9, 2, -2, 0, -7, 3, -4, -3, -5, -3, 10, -7, 12, -3, 0, 3, -7, 10, -2, -6, -13, 0, 3, 1, 4, 2, 24, 0, 1, 0, 7, -9, 1, -4, 3, -3, -7, 1, 7, 0, -3, 16, 0, 11, 4, -7, -9, 0, 0, 4, -6, 4, -19, -12, 0, -3, 2, 0, 1, 8, 0, 9, -17, 3, 7, -7, 7, 12, -4, -1, -6, 6, -4, -2, 3, -5, 0, 10, -1, -3, -8, 1, -12, -24, 1, 14, 4, 1, -11, 4, -4, -2, 1, 0, 7, 1, -13, 1, -4, -2, 2, 1, -8, 0, -14, 20, -6, -5, 0, 10, -14, -11, 6, -7, 4, 11, -11, 16, -3, 0, -7, 3, -13, 0, 0, 0, -3, 0, -11, -15, 9, -14, -7, 1, -14, 9, 1, 2, -1, -17, 2, -3, -9, 11, 1, 10, 10, -9, 7, 0, -3, 2, -8, -5, 6, -12, 10, 6, 0, -16, 1, -12, 7, 8, 5, -2, -8, -13, -3, 0, 0, 3, -6, -3, 13, 1, 22, 12, -11, 1, 12, 12, 13, -9, 1, -5, 1, -7, -13, 13, 1, 20, 0, 2, 6, 0, -1, -4, -5, 1, -6, -3, 2, -4, -4, -7, -18, 4, -7, 4, 14, -3, 8, 0, 4, -1, -10, 2, 5, -6, -11, 2, 16, -2, -4, -9, 0, 3, 0, 3, -3, 3, 3, 19, 3, -5, -8, -13, -8, 2, 2, -4, -3, 6, 2, -2, 12, 0, 0, -3, 3, -15, -10, -3, -7, -3, -4, -10, -6, -7, -2, 6, -3, 5, -2, 3, -5, -17, 0, -7, -1, -15, -9, 6, 3, -1, 2, 5, 6, -8, -3, 6, 9, -3, 1, -17, -15, 0, -1, -3, 7, -3, -4, 0, -4, -3, 1, 11, 18, -9, 6, 5, 2, -7, 2, 1, 12, 22, 3, 6, 4, 1, 2, 0, -11, -2, -7, 0, -12, 6, -6, -2, -4, 0, 7, -12, 8, 0, -3, 2, 6, -10, -1, -1, 0, -4, -2, 4, -14, 3, 1, 13, -2, -7, 6, 7, 6, 1, -4, 1, -2, -8, 0, -13, 12, 14, 11, 5, 3, 0, 1, -4, 5, -1, -5, 3, -7, 9, -1, -4, 2, -8, 10, 10, 1, 1, -2, 0, -4, -3, -8, 7, -9, -3, -3, -28, 10, 14, 0, -26, 11, -11, 5, 1, -4, -15, 0, 3, -3, -2, 7, -8, -1, 5, 7, 14, -12, -10, -6, 2, 11, -9, 7, -4, 0, 0, -2, -2, 1, 0, 0, -7, -7, -14, -9, -5, -6, 4, 3, 0, 9, -2, 7, 12, 5, 8, -5, 10, 2, -5, 13, -5, 3, 12, -3, -1, 13, 2, 6, -14, -1, -2, -4, -9, 12, -9, -2, -12, -10, -2, -16, 21, -6, -2, -3, 5, 9, -4, 0, -1, 1, -6, -10, -7, 2, -2}),
                            .dimensions = {2, 4, 4, 16},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // dummy5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // param17
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {14, 15, 16},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {17, 18, 19},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
                            .outputs = {11, 12, 13},
                            .type = TestOperationType::GENERATE_PROPOSALS
                        }},
                .outputIndexes = {11, 12, 13}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_nhwc_quant8_signed_all_inputs_as_internal_2 = TestModelManager::get().add("generate_proposals_quant8_signed_nhwc_quant8_signed_all_inputs_as_internal_2", get_test_model_nhwc_quant8_signed_all_inputs_as_internal_2());

}  // namespace generated_tests::generate_proposals_quant8_signed

namespace generated_tests::generate_proposals_quant8_signed {

const TestModel& get_test_model_nchw_quant8_signed_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2, 3},
                .operands = {{ // scores1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({49, 31, -74, -35, -32, -29, -80, -41, 52, -56, -128, 46, 37, -20, -44, 40, -86, 4, 10, -59, -8, -122, -71, -77, 22, -110, -89, 58, -23, 13, -119, -101, 28, 55, 1, 43, 19, -104, -26, -11, -92, -47, -113, -50, 25, 7, -62, 16, -14, -5, 61, -17, -2, -83, -65, -116, -38, -98, 34, -95, -107, -68, -125, -53, -29, -86, -119, -101, -50, 52, -53, -125, -62, -113, -11, 31, -128, 10, -122, 25, -65, -20, 58, -110, 49, -83, -2, -32, -35, 40, -95, 43, 46, -17, 55, -104, -89, 28, -74, 34, -44, 22, 37, 1, 4, -71, -116, -107, -23, -98, -47, -26, -80, 16, 19, 13, 61, -68, 7, -5, -8, -14, -56, -38, -77, -59, -41, -92}),
                            .dimensions = {2, 4, 4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.005f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // bboxDeltas1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-19, -2, -15, 12, 1, 4, 0, 0, 1, 0, 0, 2, 10, -6, -13, -4, 4, 0, 13, -3, 0, -7, 9, 10, 0, 10, 0, -3, 6, -3, 13, -7, 14, 4, -7, 0, 7, -9, -17, -1, 7, -14, -3, -9, 0, 13, 1, -18, 5, 1, -9, 3, -9, 0, 3, -3, 1, -11, 0, 11, -16, 1, 20, 4, -15, -2, 9, -7, 1, 0, 7, -8, -13, 6, -11, 1, 1, 22, 0, -7, -2, -16, 2, 10, -4, 4, -7, 1, 1, -7, -15, 10, -12, 12, 2, 4, 3, -6, -2, -2, 3, -6, 7, -12, -4, 4, 9, 10, 7, -11, 6, 14, 12, -1, 0, -6, -3, 4, 12, -24, -2, 11, -14, -9, 8, 1, 0, -3, 0, -10, -7, -13, -7, -19, -4, 1, 2, -11, -7, 7, 5, 12, -1, 8, -6, 6, 3, 0, 1, -12, -1, 14, 1, 16, 1, 0, -2, 12, -4, 0, 4, 5, -4, 3, 7, 0, -6, 4, -8, -3, -14, -3, -8, 13, -5, 4, -13, -2, -3, 1, 0, -3, 6, 1, 0, 0, 9, 2, -13, -9, 1, -1, 8, -17, -5, 4, -3, 2, -4, -11, -14, -7, 1, -8, -3, 1, -6, -10, 9, -14, -3, 2, 16, 0, -2, 4, 20, 3, 2, -5, 0, -5, -3, 2, -2, 5, 10, 24, 0, 1, 3, -4, -6, -13, -1, 6, 0, 1, 2, 5, 8, -1, -7, 0, 11, 8, -5, -2, -5, 0, -17, -12, 3, -7, -4, -6, -11, -8, -10, 0, 1, 6, -7, -1, -4, -5, -8, 0, 7, 3, -3, -16, 2, -13, -3, -7, -17, 5, 0, -1, 1, 3, 7, 3, -4, 0, -1, 21, 16, -8, -7, -1, -15, 2, -12, 0, -2, -7, -9, -3, 0, 9, 13, -6, -2, 2, -3, -15, 0, -7, 6, -4, -8, 9, -3, -2, 0, -2, 2, -2, -4, 2, -4, -9, -1, 2, -6, -2, 0, -1, -3, 7, -2, 7, 6, -3, -9, -4, -10, 6, -3, 1, -2, 4, -13, -4, -28, -8, -2, 12, -14, 5, 0, -3, -6, 3, 7, 12, -4, -14, 12, 2, 10, -1, 1, 5, -1, 9, 3, 6, -7, -1, -3, 22, 0, 3, 14, -8, 14, 5, 0, 8, -2, -4, 0, 2, -2, 2, -4, 3, 7, 1, 11, 10, 0, 7, 0, -5, -4, 0, 3, -2, 6, 5, 0, 6, -12, 13, 5, 10, -26, 14, -7, 10, -9, -1, -3, 12, -3, 6, -4, 4, 8, -2, 3, 1, 11, -12, -7, 2, 12, 1, 3, 0, 5, -8, -3, 1, 0, -7, 0, 1, -11, -10, -14, -5, -9, -6, 3, 0, -2, -3, 1, 2, -3, 6, 1, -2, 5, -6, -9, 13, -2, -10, 19, -3, 3, 6, 11, 0, 2, 7, -4, 0, 1, 2, -5, -5, -12, -7, 3, 3, -5, 9, 18, -11, 6, 6, 5, -4, -4, 11, -6, 3, -10, 2, -5, -15, -17, -3, -9, -2, -10, 1, -1, -3, -15, -9, 4, 12, -2, -2}),
                            .dimensions = {2, 16, 4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // anchors1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({0, 48, 128, 80, 48, 0, 80, 128, 24, 40, 104, 88, 40, 24, 88, 104}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // imageInfo1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({512, 512, 256, 256}),
                            .dimensions = {2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // param6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({10.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({10.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param8
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({32}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param9
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({16}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.2f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // scoresOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({61, 58, 55, 52, 46, 40, 34, 31, 28, 25, 22, 19, 16, 13, 10, 7, 61, 55, 52, 49, 46, 40, 34, 28, 19, 16, -2, -8, -11, -20}),
                            .dimensions = {30},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.005f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // roiOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({135, 20, 265, 60, 264, 326, 351, 378, 0, 73, 130, 112, 0, 206, 206, 242, 304, 166, 356, 262, 242, 258, 263, 305, 207, 233, 251, 247, 23, 46, 214, 82, 4, 29, 124, 42, 86, 288, 122, 301, 12, 184, 33, 289, 0, 125, 89, 173, 308, 282, 428, 326, 209, 387, 220, 512, 240, 26, 266, 154, 93, 352, 387, 371, 0, 216, 214, 248, 229, 72, 256, 177, 143, 152, 256, 168, 29, 58, 93, 153, 40, 214, 75, 256, 121, 144, 160, 202, 247, 77, 256, 99, 27, 30, 256, 78, 200, 81, 229, 95, 112, 31, 176, 49, 194, 54, 216, 182, 29, 156, 108, 228, 96, 235, 148, 256, 139, 0, 162, 129}),
                            .dimensions = {30, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {30},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
                            .outputs = {11, 12, 13},
                            .type = TestOperationType::GENERATE_PROPOSALS
                        }},
                .outputIndexes = {11, 12, 13}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_nchw_quant8_signed_2 = TestModelManager::get().add("generate_proposals_quant8_signed_nchw_quant8_signed_2", get_test_model_nchw_quant8_signed_2());

}  // namespace generated_tests::generate_proposals_quant8_signed

namespace generated_tests::generate_proposals_quant8_signed {

const TestModel& get_test_model_nchw_quant8_signed_all_inputs_as_internal_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2, 3, 14, 17},
                .operands = {{ // scores1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 4, 4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.005f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // bboxDeltas1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 16, 4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // anchors1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({0, 48, 128, 80, 48, 0, 80, 128, 24, 40, 104, 88, 40, 24, 88, 104}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // imageInfo1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({512, 512, 256, 256}),
                            .dimensions = {2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // param6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({10.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({10.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param8
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({32}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param9
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({16}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.2f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // scoresOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({61, 58, 55, 52, 46, 40, 34, 31, 28, 25, 22, 19, 16, 13, 10, 7, 61, 55, 52, 49, 46, 40, 34, 28, 19, 16, -2, -8, -11, -20}),
                            .dimensions = {30},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.005f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // roiOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({135, 20, 265, 60, 264, 326, 351, 378, 0, 73, 130, 112, 0, 206, 206, 242, 304, 166, 356, 262, 242, 258, 263, 305, 207, 233, 251, 247, 23, 46, 214, 82, 4, 29, 124, 42, 86, 288, 122, 301, 12, 184, 33, 289, 0, 125, 89, 173, 308, 282, 428, 326, 209, 387, 220, 512, 240, 26, 266, 154, 93, 352, 387, 371, 0, 216, 214, 248, 229, 72, 256, 177, 143, 152, 256, 168, 29, 58, 93, 153, 40, 214, 75, 256, 121, 144, 160, 202, 247, 77, 256, 99, 27, 30, 256, 78, 200, 81, 229, 95, 112, 31, 176, 49, 194, 54, 216, 182, 29, 156, 108, 228, 96, 235, 148, 256, 139, 0, 162, 129}),
                            .dimensions = {30, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {30},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // scores1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({49, 31, -74, -35, -32, -29, -80, -41, 52, -56, -128, 46, 37, -20, -44, 40, -86, 4, 10, -59, -8, -122, -71, -77, 22, -110, -89, 58, -23, 13, -119, -101, 28, 55, 1, 43, 19, -104, -26, -11, -92, -47, -113, -50, 25, 7, -62, 16, -14, -5, 61, -17, -2, -83, -65, -116, -38, -98, 34, -95, -107, -68, -125, -53, -29, -86, -119, -101, -50, 52, -53, -125, -62, -113, -11, 31, -128, 10, -122, 25, -65, -20, 58, -110, 49, -83, -2, -32, -35, 40, -95, 43, 46, -17, 55, -104, -89, 28, -74, 34, -44, 22, 37, 1, 4, -71, -116, -107, -23, -98, -47, -26, -80, 16, 19, 13, 61, -68, 7, -5, -8, -14, -56, -38, -77, -59, -41, -92}),
                            .dimensions = {2, 4, 4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.005f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.005f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param18
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // bboxDeltas1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-19, -2, -15, 12, 1, 4, 0, 0, 1, 0, 0, 2, 10, -6, -13, -4, 4, 0, 13, -3, 0, -7, 9, 10, 0, 10, 0, -3, 6, -3, 13, -7, 14, 4, -7, 0, 7, -9, -17, -1, 7, -14, -3, -9, 0, 13, 1, -18, 5, 1, -9, 3, -9, 0, 3, -3, 1, -11, 0, 11, -16, 1, 20, 4, -15, -2, 9, -7, 1, 0, 7, -8, -13, 6, -11, 1, 1, 22, 0, -7, -2, -16, 2, 10, -4, 4, -7, 1, 1, -7, -15, 10, -12, 12, 2, 4, 3, -6, -2, -2, 3, -6, 7, -12, -4, 4, 9, 10, 7, -11, 6, 14, 12, -1, 0, -6, -3, 4, 12, -24, -2, 11, -14, -9, 8, 1, 0, -3, 0, -10, -7, -13, -7, -19, -4, 1, 2, -11, -7, 7, 5, 12, -1, 8, -6, 6, 3, 0, 1, -12, -1, 14, 1, 16, 1, 0, -2, 12, -4, 0, 4, 5, -4, 3, 7, 0, -6, 4, -8, -3, -14, -3, -8, 13, -5, 4, -13, -2, -3, 1, 0, -3, 6, 1, 0, 0, 9, 2, -13, -9, 1, -1, 8, -17, -5, 4, -3, 2, -4, -11, -14, -7, 1, -8, -3, 1, -6, -10, 9, -14, -3, 2, 16, 0, -2, 4, 20, 3, 2, -5, 0, -5, -3, 2, -2, 5, 10, 24, 0, 1, 3, -4, -6, -13, -1, 6, 0, 1, 2, 5, 8, -1, -7, 0, 11, 8, -5, -2, -5, 0, -17, -12, 3, -7, -4, -6, -11, -8, -10, 0, 1, 6, -7, -1, -4, -5, -8, 0, 7, 3, -3, -16, 2, -13, -3, -7, -17, 5, 0, -1, 1, 3, 7, 3, -4, 0, -1, 21, 16, -8, -7, -1, -15, 2, -12, 0, -2, -7, -9, -3, 0, 9, 13, -6, -2, 2, -3, -15, 0, -7, 6, -4, -8, 9, -3, -2, 0, -2, 2, -2, -4, 2, -4, -9, -1, 2, -6, -2, 0, -1, -3, 7, -2, 7, 6, -3, -9, -4, -10, 6, -3, 1, -2, 4, -13, -4, -28, -8, -2, 12, -14, 5, 0, -3, -6, 3, 7, 12, -4, -14, 12, 2, 10, -1, 1, 5, -1, 9, 3, 6, -7, -1, -3, 22, 0, 3, 14, -8, 14, 5, 0, 8, -2, -4, 0, 2, -2, 2, -4, 3, 7, 1, 11, 10, 0, 7, 0, -5, -4, 0, 3, -2, 6, 5, 0, 6, -12, 13, 5, 10, -26, 14, -7, 10, -9, -1, -3, 12, -3, 6, -4, 4, 8, -2, 3, 1, 11, -12, -7, 2, 12, 1, 3, 0, 5, -8, -3, 1, 0, -7, 0, 1, -11, -10, -14, -5, -9, -6, 3, 0, -2, -3, 1, 2, -3, 6, 1, -2, 5, -6, -9, 13, -2, -10, 19, -3, 3, 6, 11, 0, 2, 7, -4, 0, 1, 2, -5, -5, -12, -7, 3, 3, -5, 9, 18, -11, 6, 6, 5, -4, -4, 11, -6, 3, -10, 2, -5, -15, -17, -3, -9, -2, -10, 1, -1, -3, -15, -9, 4, 12, -2, -2}),
                            .dimensions = {2, 16, 4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // dummy7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // param19
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {14, 15, 16},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {17, 18, 19},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
                            .outputs = {11, 12, 13},
                            .type = TestOperationType::GENERATE_PROPOSALS
                        }},
                .outputIndexes = {11, 12, 13}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_nchw_quant8_signed_all_inputs_as_internal_2 = TestModelManager::get().add("generate_proposals_quant8_signed_nchw_quant8_signed_all_inputs_as_internal_2", get_test_model_nchw_quant8_signed_all_inputs_as_internal_2());

}  // namespace generated_tests::generate_proposals_quant8_signed

