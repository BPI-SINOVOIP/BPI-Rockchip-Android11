// Generated from box_with_nms_limit_quant8_signed.mod.py
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;

namespace generated_tests::box_with_nms_limit_quant8_signed {

const TestModel& get_test_model_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2},
                .operands = {{ // scores
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-38, -33, -53, -48, -58, -43, -68, -38, -33, -38, -63, -38, -48, -43, -48, -68, -68, -108, -68, -48, -88, -38, -73, -68, -38, -53, -58, -48, -58, -43, -38, -33, -53, -48, -43, -48, -68, -38, -33, -68, -68, -108, -78, -38, -48, -38, -53, -58, -38, -63, -38, -38, -73, -68, -68, -48, -88}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // roi
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({8, 8, 80, 80, 0, 0, 80, 80, 0, 0, 80, 80, 16, 16, 88, 88, 8, 8, 88, 88, 8, 8, 88, 88, 24, 24, 96, 96, 16, 16, 96, 96, 16, 16, 96, 96, 32, 32, 104, 104, 24, 24, 104, 104, 24, 24, 104, 104, 40, 40, 112, 112, 32, 32, 112, 112, 32, 32, 112, 112, 48, 48, 120, 120, 40, 40, 120, 120, 40, 40, 120, 120, 56, 56, 128, 128, 48, 48, 128, 128, 48, 48, 128, 128, 64, 64, 136, 136, 56, 56, 136, 136, 56, 56, 136, 136, 72, 72, 144, 144, 64, 64, 144, 144, 64, 64, 144, 144, 16, 16, 88, 88, 16, 16, 96, 96, 16, 16, 96, 96, 8, 8, 80, 80, 8, 8, 88, 88, 8, 8, 88, 88, 40, 40, 112, 112, 40, 40, 120, 120, 40, 40, 120, 120, 24, 24, 96, 96, 24, 24, 104, 104, 24, 24, 104, 104, 48, 48, 120, 120, 48, 48, 128, 128, 48, 48, 128, 128, 0, 0, 8, 8, 0, 0, 16, 16, 0, 0, 16, 16, 72, 72, 144, 144, 72, 72, 152, 152, 72, 72, 152, 152, 32, 32, 104, 104, 32, 32, 112, 112, 32, 32, 112, 112, 64, 64, 136, 136, 64, 64, 144, 144, 64, 64, 144, 144, 56, 56, 128, 128, 56, 56, 136, 136, 56, 56, 136, 136}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // scoresOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-33, -49, -76, -81, -33, -59, -80, -86, -33, -38, -49, -76, -81, -33, -48, -59, -80, -86}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // roiOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({0, 0, 80, 80, 48, 48, 128, 128, 16, 16, 96, 96, 64, 64, 144, 144, 16, 16, 96, 96, 64, 64, 144, 144, 0, 0, 80, 80, 32, 32, 112, 112, 8, 8, 88, 88, 0, 0, 16, 16, 56, 56, 136, 136, 24, 24, 104, 104, 72, 72, 152, 152, 24, 24, 104, 104, 0, 0, 16, 16, 72, 72, 152, 152, 8, 8, 88, 88, 40, 40, 120, 120}),
                            .dimensions = {18, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // classesOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_signed = TestModelManager::get().add("box_with_nms_limit_quant8_signed_quant8_signed", get_test_model_quant8_signed());

}  // namespace generated_tests::box_with_nms_limit_quant8_signed

namespace generated_tests::box_with_nms_limit_quant8_signed {

const TestModel& get_test_model_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {1, 2, 13},
                .operands = {{ // scores
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // roi
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({8, 8, 80, 80, 0, 0, 80, 80, 0, 0, 80, 80, 16, 16, 88, 88, 8, 8, 88, 88, 8, 8, 88, 88, 24, 24, 96, 96, 16, 16, 96, 96, 16, 16, 96, 96, 32, 32, 104, 104, 24, 24, 104, 104, 24, 24, 104, 104, 40, 40, 112, 112, 32, 32, 112, 112, 32, 32, 112, 112, 48, 48, 120, 120, 40, 40, 120, 120, 40, 40, 120, 120, 56, 56, 128, 128, 48, 48, 128, 128, 48, 48, 128, 128, 64, 64, 136, 136, 56, 56, 136, 136, 56, 56, 136, 136, 72, 72, 144, 144, 64, 64, 144, 144, 64, 64, 144, 144, 16, 16, 88, 88, 16, 16, 96, 96, 16, 16, 96, 96, 8, 8, 80, 80, 8, 8, 88, 88, 8, 8, 88, 88, 40, 40, 112, 112, 40, 40, 120, 120, 40, 40, 120, 120, 24, 24, 96, 96, 24, 24, 104, 104, 24, 24, 104, 104, 48, 48, 120, 120, 48, 48, 128, 128, 48, 48, 128, 128, 0, 0, 8, 8, 0, 0, 16, 16, 0, 0, 16, 16, 72, 72, 144, 144, 72, 72, 152, 152, 72, 72, 152, 152, 32, 32, 104, 104, 32, 32, 112, 112, 32, 32, 112, 112, 64, 64, 136, 136, 64, 64, 144, 144, 64, 64, 144, 144, 56, 56, 128, 128, 56, 56, 136, 136, 56, 56, 136, 136}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // scoresOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-33, -49, -76, -81, -33, -59, -80, -86, -33, -38, -49, -76, -81, -33, -48, -59, -80, -86}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // roiOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({0, 0, 80, 80, 48, 48, 128, 128, 16, 16, 96, 96, 64, 64, 144, 144, 16, 16, 96, 96, 64, 64, 144, 144, 0, 0, 80, 80, 32, 32, 112, 112, 8, 8, 88, 88, 0, 0, 16, 16, 56, 56, 136, 136, 24, 24, 104, 104, 72, 72, 152, 152, 24, 24, 104, 104, 0, 0, 16, 16, 72, 72, 152, 152, 8, 8, 88, 88, 40, 40, 120, 120}),
                            .dimensions = {18, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // classesOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // scores_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-38, -33, -53, -48, -58, -43, -68, -38, -33, -38, -63, -38, -48, -43, -48, -68, -68, -108, -68, -48, -88, -38, -73, -68, -38, -53, -58, -48, -58, -43, -38, -33, -53, -48, -43, -48, -68, -38, -33, -68, -68, -108, -78, -38, -48, -38, -53, -58, -38, -63, -38, -38, -73, -68, -68, -48, -88}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param36
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
                            .inputs = {13, 14, 15},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("box_with_nms_limit_quant8_signed_quant8_signed_all_inputs_as_internal", get_test_model_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::box_with_nms_limit_quant8_signed

namespace generated_tests::box_with_nms_limit_quant8_signed {

const TestModel& get_test_model_quant8_signed_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2},
                .operands = {{ // scores1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({90, 95, 75, 80, 70, 85, 60, 90, 95, 90, 65, 90, 80, 85, 80, 60, 60, 20, 60, 80, 40, 90, 55, 60, 90, 75, 70, 80, 70, 85, 90, 95, 75, 80, 85, 80, 60, 90, 95, 60, 60, 20, 50, 90, 80, 90, 75, 70, 90, 65, 90, 90, 55, 60, 60, 80, 40}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // roi1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({8, 8, 80, 80, 0, 0, 80, 80, 0, 0, 80, 80, 16, 16, 88, 88, 8, 8, 88, 88, 8, 8, 88, 88, 24, 24, 96, 96, 16, 16, 96, 96, 16, 16, 96, 96, 32, 32, 104, 104, 24, 24, 104, 104, 24, 24, 104, 104, 40, 40, 112, 112, 32, 32, 112, 112, 32, 32, 112, 112, 48, 48, 120, 120, 40, 40, 120, 120, 40, 40, 120, 120, 56, 56, 128, 128, 48, 48, 128, 128, 48, 48, 128, 128, 64, 64, 136, 136, 56, 56, 136, 136, 56, 56, 136, 136, 72, 72, 144, 144, 64, 64, 144, 144, 64, 64, 144, 144, 16, 16, 88, 88, 16, 16, 96, 96, 16, 16, 96, 96, 8, 8, 80, 80, 8, 8, 88, 88, 8, 8, 88, 88, 40, 40, 112, 112, 40, 40, 120, 120, 40, 40, 120, 120, 24, 24, 96, 96, 24, 24, 104, 104, 24, 24, 104, 104, 48, 48, 120, 120, 48, 48, 128, 128, 48, 48, 128, 128, 0, 0, 8, 8, 0, 0, 16, 16, 0, 0, 16, 16, 72, 72, 144, 144, 72, 72, 152, 152, 72, 72, 152, 152, 32, 32, 104, 104, 32, 32, 112, 112, 32, 32, 112, 112, 64, 64, 136, 136, 64, 64, 144, 144, 64, 64, 144, 144, 56, 56, 128, 128, 56, 56, 136, 136, 56, 56, 136, 136}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({5}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param8
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param9
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // scoresOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({95, 79, 52, 95, 69, 95, 90, 79, 95, 80}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // roiOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({0, 0, 80, 80, 48, 48, 128, 128, 16, 16, 96, 96, 16, 16, 96, 96, 64, 64, 144, 144, 8, 8, 88, 88, 0, 0, 16, 16, 56, 56, 136, 136, 24, 24, 104, 104, 0, 0, 16, 16}),
                            .dimensions = {10, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // classesOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 2, 2, 1, 1, 1, 2, 2}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 3, 3, 3, 3, 3}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_signed_2 = TestModelManager::get().add("box_with_nms_limit_quant8_signed_quant8_signed_2", get_test_model_quant8_signed_2());

}  // namespace generated_tests::box_with_nms_limit_quant8_signed

namespace generated_tests::box_with_nms_limit_quant8_signed {

const TestModel& get_test_model_quant8_signed_all_inputs_as_internal_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {1, 2, 13},
                .operands = {{ // scores1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // roi1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({8, 8, 80, 80, 0, 0, 80, 80, 0, 0, 80, 80, 16, 16, 88, 88, 8, 8, 88, 88, 8, 8, 88, 88, 24, 24, 96, 96, 16, 16, 96, 96, 16, 16, 96, 96, 32, 32, 104, 104, 24, 24, 104, 104, 24, 24, 104, 104, 40, 40, 112, 112, 32, 32, 112, 112, 32, 32, 112, 112, 48, 48, 120, 120, 40, 40, 120, 120, 40, 40, 120, 120, 56, 56, 128, 128, 48, 48, 128, 128, 48, 48, 128, 128, 64, 64, 136, 136, 56, 56, 136, 136, 56, 56, 136, 136, 72, 72, 144, 144, 64, 64, 144, 144, 64, 64, 144, 144, 16, 16, 88, 88, 16, 16, 96, 96, 16, 16, 96, 96, 8, 8, 80, 80, 8, 8, 88, 88, 8, 8, 88, 88, 40, 40, 112, 112, 40, 40, 120, 120, 40, 40, 120, 120, 24, 24, 96, 96, 24, 24, 104, 104, 24, 24, 104, 104, 48, 48, 120, 120, 48, 48, 128, 128, 48, 48, 128, 128, 0, 0, 8, 8, 0, 0, 16, 16, 0, 0, 16, 16, 72, 72, 144, 144, 72, 72, 152, 152, 72, 72, 152, 152, 32, 32, 104, 104, 32, 32, 112, 112, 32, 32, 112, 112, 64, 64, 136, 136, 64, 64, 144, 144, 64, 64, 144, 144, 56, 56, 128, 128, 56, 56, 136, 136, 56, 56, 136, 136}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({5}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param8
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param9
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // scoresOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({95, 79, 52, 95, 69, 95, 90, 79, 95, 80}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // roiOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({0, 0, 80, 80, 48, 48, 128, 128, 16, 16, 96, 96, 16, 16, 96, 96, 64, 64, 144, 144, 8, 8, 88, 88, 0, 0, 16, 16, 56, 56, 136, 136, 24, 24, 104, 104, 0, 0, 16, 16}),
                            .dimensions = {10, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // classesOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 2, 2, 1, 1, 1, 2, 2}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 3, 3, 3, 3, 3}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // scores1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({90, 95, 75, 80, 70, 85, 60, 90, 95, 90, 65, 90, 80, 85, 80, 60, 60, 20, 60, 80, 40, 90, 55, 60, 90, 75, 70, 80, 70, 85, 90, 95, 75, 80, 85, 80, 60, 90, 95, 60, 60, 20, 50, 90, 80, 90, 75, 70, 90, 65, 90, 90, 55, 60, 60, 80, 40}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // dummy1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // param37
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
                            .inputs = {13, 14, 15},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_signed_all_inputs_as_internal_2 = TestModelManager::get().add("box_with_nms_limit_quant8_signed_quant8_signed_all_inputs_as_internal_2", get_test_model_quant8_signed_all_inputs_as_internal_2());

}  // namespace generated_tests::box_with_nms_limit_quant8_signed

namespace generated_tests::box_with_nms_limit_quant8_signed {

const TestModel& get_test_model_quant8_signed_3() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2},
                .operands = {{ // scores2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-38, -33, -53, -48, -58, -43, -68, -38, -33, -38, -63, -38, -48, -43, -48, -68, -68, -108, -68, -48, -88, -38, -73, -68, -38, -53, -58, -48, -58, -43, -38, -33, -53, -48, -43, -48, -68, -38, -33, -68, -68, -108, -78, -38, -48, -38, -53, -58, -38, -63, -38, -38, -73, -68, -68, -48, -88}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // roi2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({8, 8, 80, 80, 0, 0, 80, 80, 0, 0, 80, 80, 16, 16, 88, 88, 8, 8, 88, 88, 8, 8, 88, 88, 24, 24, 96, 96, 16, 16, 96, 96, 16, 16, 96, 96, 32, 32, 104, 104, 24, 24, 104, 104, 24, 24, 104, 104, 40, 40, 112, 112, 32, 32, 112, 112, 32, 32, 112, 112, 48, 48, 120, 120, 40, 40, 120, 120, 40, 40, 120, 120, 56, 56, 128, 128, 48, 48, 128, 128, 48, 48, 128, 128, 64, 64, 136, 136, 56, 56, 136, 136, 56, 56, 136, 136, 72, 72, 144, 144, 64, 64, 144, 144, 64, 64, 144, 144, 16, 16, 88, 88, 16, 16, 96, 96, 16, 16, 96, 96, 8, 8, 80, 80, 8, 8, 88, 88, 8, 8, 88, 88, 40, 40, 112, 112, 40, 40, 120, 120, 40, 40, 120, 120, 24, 24, 96, 96, 24, 24, 104, 104, 24, 24, 104, 104, 48, 48, 120, 120, 48, 48, 128, 128, 48, 48, 128, 128, 0, 0, 8, 8, 0, 0, 16, 16, 0, 0, 16, 16, 72, 72, 144, 144, 72, 72, 152, 152, 72, 72, 152, 152, 32, 32, 104, 104, 32, 32, 112, 112, 32, 32, 112, 112, 64, 64, 136, 136, 64, 64, 144, 144, 64, 64, 144, 144, 56, 56, 128, 128, 56, 56, 136, 136, 56, 56, 136, 136}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param12
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param13
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
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
                        }, { // param15
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param16
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param17
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // scoresOut2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-33, -43, -53, -33, -58, -33, -38, -43, -53, -33, -48, -58}),
                            .dimensions = {12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // roiOut2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({0, 0, 80, 80, 32, 32, 112, 112, 64, 64, 144, 144, 16, 16, 96, 96, 64, 64, 144, 144, 8, 8, 88, 88, 0, 0, 16, 16, 40, 40, 120, 120, 72, 72, 152, 152, 24, 24, 104, 104, 0, 0, 16, 16, 72, 72, 152, 152}),
                            .dimensions = {12, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // classesOut2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 2, 2, 1, 1, 1, 1, 2, 2, 2}),
                            .dimensions = {12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_signed_3 = TestModelManager::get().add("box_with_nms_limit_quant8_signed_quant8_signed_3", get_test_model_quant8_signed_3());

}  // namespace generated_tests::box_with_nms_limit_quant8_signed

namespace generated_tests::box_with_nms_limit_quant8_signed {

const TestModel& get_test_model_quant8_signed_all_inputs_as_internal_3() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {1, 2, 13},
                .operands = {{ // scores2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // roi2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({8, 8, 80, 80, 0, 0, 80, 80, 0, 0, 80, 80, 16, 16, 88, 88, 8, 8, 88, 88, 8, 8, 88, 88, 24, 24, 96, 96, 16, 16, 96, 96, 16, 16, 96, 96, 32, 32, 104, 104, 24, 24, 104, 104, 24, 24, 104, 104, 40, 40, 112, 112, 32, 32, 112, 112, 32, 32, 112, 112, 48, 48, 120, 120, 40, 40, 120, 120, 40, 40, 120, 120, 56, 56, 128, 128, 48, 48, 128, 128, 48, 48, 128, 128, 64, 64, 136, 136, 56, 56, 136, 136, 56, 56, 136, 136, 72, 72, 144, 144, 64, 64, 144, 144, 64, 64, 144, 144, 16, 16, 88, 88, 16, 16, 96, 96, 16, 16, 96, 96, 8, 8, 80, 80, 8, 8, 88, 88, 8, 8, 88, 88, 40, 40, 112, 112, 40, 40, 120, 120, 40, 40, 120, 120, 24, 24, 96, 96, 24, 24, 104, 104, 24, 24, 104, 104, 48, 48, 120, 120, 48, 48, 128, 128, 48, 48, 128, 128, 0, 0, 8, 8, 0, 0, 16, 16, 0, 0, 16, 16, 72, 72, 144, 144, 72, 72, 152, 152, 72, 72, 152, 152, 32, 32, 104, 104, 32, 32, 112, 112, 32, 32, 112, 112, 64, 64, 136, 136, 64, 64, 144, 144, 64, 64, 144, 144, 56, 56, 128, 128, 56, 56, 136, 136, 56, 56, 136, 136}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param12
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param13
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
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
                        }, { // param15
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param16
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param17
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // scoresOut2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-33, -43, -53, -33, -58, -33, -38, -43, -53, -33, -48, -58}),
                            .dimensions = {12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // roiOut2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({0, 0, 80, 80, 32, 32, 112, 112, 64, 64, 144, 144, 16, 16, 96, 96, 64, 64, 144, 144, 8, 8, 88, 88, 0, 0, 16, 16, 40, 40, 120, 120, 72, 72, 152, 152, 24, 24, 104, 104, 0, 0, 16, 16, 72, 72, 152, 152}),
                            .dimensions = {12, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // classesOut2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 2, 2, 1, 1, 1, 1, 2, 2, 2}),
                            .dimensions = {12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // scores2_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-38, -33, -53, -48, -58, -43, -68, -38, -33, -38, -63, -38, -48, -43, -48, -68, -68, -108, -68, -48, -88, -38, -73, -68, -38, -53, -58, -48, -58, -43, -38, -33, -53, -48, -43, -48, -68, -38, -33, -68, -68, -108, -78, -38, -48, -38, -53, -58, -38, -63, -38, -38, -73, -68, -68, -48, -88}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param38
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
                            .inputs = {13, 14, 15},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_signed_all_inputs_as_internal_3 = TestModelManager::get().add("box_with_nms_limit_quant8_signed_quant8_signed_all_inputs_as_internal_3", get_test_model_quant8_signed_all_inputs_as_internal_3());

}  // namespace generated_tests::box_with_nms_limit_quant8_signed

namespace generated_tests::box_with_nms_limit_quant8_signed {

const TestModel& get_test_model_quant8_signed_4() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2},
                .operands = {{ // scores3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({90, 95, 75, 80, 70, 85, 60, 90, 95, 90, 65, 90, 80, 85, 80, 60, 60, 20, 60, 80, 40, 90, 55, 60, 90, 75, 70, 80, 70, 85, 90, 95, 75, 80, 85, 80, 60, 90, 95, 60, 60, 20, 50, 90, 80, 90, 75, 70, 90, 65, 90, 90, 55, 60, 60, 80, 40}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // roi3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({8, 8, 80, 80, 0, 0, 80, 80, 0, 0, 80, 80, 16, 16, 88, 88, 8, 8, 88, 88, 8, 8, 88, 88, 24, 24, 96, 96, 16, 16, 96, 96, 16, 16, 96, 96, 32, 32, 104, 104, 24, 24, 104, 104, 24, 24, 104, 104, 40, 40, 112, 112, 32, 32, 112, 112, 32, 32, 112, 112, 48, 48, 120, 120, 40, 40, 120, 120, 40, 40, 120, 120, 56, 56, 128, 128, 48, 48, 128, 128, 48, 48, 128, 128, 64, 64, 136, 136, 56, 56, 136, 136, 56, 56, 136, 136, 72, 72, 144, 144, 64, 64, 144, 144, 64, 64, 144, 144, 16, 16, 88, 88, 16, 16, 96, 96, 16, 16, 96, 96, 8, 8, 80, 80, 8, 8, 88, 88, 8, 8, 88, 88, 40, 40, 112, 112, 40, 40, 120, 120, 40, 40, 120, 120, 24, 24, 96, 96, 24, 24, 104, 104, 24, 24, 104, 104, 48, 48, 120, 120, 48, 48, 128, 128, 48, 48, 128, 128, 0, 0, 8, 8, 0, 0, 16, 16, 0, 0, 16, 16, 72, 72, 144, 144, 72, 72, 152, 152, 72, 72, 152, 152, 32, 32, 104, 104, 32, 32, 112, 112, 32, 32, 112, 112, 64, 64, 136, 136, 64, 64, 144, 144, 64, 64, 144, 144, 56, 56, 128, 128, 56, 56, 136, 136, 56, 56, 136, 136}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param18
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param19
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({5}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param20
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param21
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param22
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param23
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // scoresOut3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({95, 85, 75, 95, 70, 95, 90, 85, 95, 80}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // roiOut3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({0, 0, 80, 80, 32, 32, 112, 112, 64, 64, 144, 144, 16, 16, 96, 96, 64, 64, 144, 144, 8, 8, 88, 88, 0, 0, 16, 16, 40, 40, 120, 120, 24, 24, 104, 104, 0, 0, 16, 16}),
                            .dimensions = {10, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // classesOut3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 2, 2, 1, 1, 1, 2, 2}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 3, 3, 3, 3, 3}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_signed_4 = TestModelManager::get().add("box_with_nms_limit_quant8_signed_quant8_signed_4", get_test_model_quant8_signed_4());

}  // namespace generated_tests::box_with_nms_limit_quant8_signed

namespace generated_tests::box_with_nms_limit_quant8_signed {

const TestModel& get_test_model_quant8_signed_all_inputs_as_internal_4() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {1, 2, 13},
                .operands = {{ // scores3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // roi3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({8, 8, 80, 80, 0, 0, 80, 80, 0, 0, 80, 80, 16, 16, 88, 88, 8, 8, 88, 88, 8, 8, 88, 88, 24, 24, 96, 96, 16, 16, 96, 96, 16, 16, 96, 96, 32, 32, 104, 104, 24, 24, 104, 104, 24, 24, 104, 104, 40, 40, 112, 112, 32, 32, 112, 112, 32, 32, 112, 112, 48, 48, 120, 120, 40, 40, 120, 120, 40, 40, 120, 120, 56, 56, 128, 128, 48, 48, 128, 128, 48, 48, 128, 128, 64, 64, 136, 136, 56, 56, 136, 136, 56, 56, 136, 136, 72, 72, 144, 144, 64, 64, 144, 144, 64, 64, 144, 144, 16, 16, 88, 88, 16, 16, 96, 96, 16, 16, 96, 96, 8, 8, 80, 80, 8, 8, 88, 88, 8, 8, 88, 88, 40, 40, 112, 112, 40, 40, 120, 120, 40, 40, 120, 120, 24, 24, 96, 96, 24, 24, 104, 104, 24, 24, 104, 104, 48, 48, 120, 120, 48, 48, 128, 128, 48, 48, 128, 128, 0, 0, 8, 8, 0, 0, 16, 16, 0, 0, 16, 16, 72, 72, 144, 144, 72, 72, 152, 152, 72, 72, 152, 152, 32, 32, 104, 104, 32, 32, 112, 112, 32, 32, 112, 112, 64, 64, 136, 136, 64, 64, 144, 144, 64, 64, 144, 144, 56, 56, 128, 128, 56, 56, 136, 136, 56, 56, 136, 136}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param18
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param19
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({5}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param20
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param21
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param22
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param23
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // scoresOut3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({95, 85, 75, 95, 70, 95, 90, 85, 95, 80}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // roiOut3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({0, 0, 80, 80, 32, 32, 112, 112, 64, 64, 144, 144, 16, 16, 96, 96, 64, 64, 144, 144, 8, 8, 88, 88, 0, 0, 16, 16, 40, 40, 120, 120, 24, 24, 104, 104, 0, 0, 16, 16}),
                            .dimensions = {10, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // classesOut3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 2, 2, 1, 1, 1, 2, 2}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 3, 3, 3, 3, 3}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // scores3_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({90, 95, 75, 80, 70, 85, 60, 90, 95, 90, 65, 90, 80, 85, 80, 60, 60, 20, 60, 80, 40, 90, 55, 60, 90, 75, 70, 80, 70, 85, 90, 95, 75, 80, 85, 80, 60, 90, 95, 60, 60, 20, 50, 90, 80, 90, 75, 70, 90, 65, 90, 90, 55, 60, 60, 80, 40}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // dummy3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // param39
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
                            .inputs = {13, 14, 15},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_signed_all_inputs_as_internal_4 = TestModelManager::get().add("box_with_nms_limit_quant8_signed_quant8_signed_all_inputs_as_internal_4", get_test_model_quant8_signed_all_inputs_as_internal_4());

}  // namespace generated_tests::box_with_nms_limit_quant8_signed

namespace generated_tests::box_with_nms_limit_quant8_signed {

const TestModel& get_test_model_quant8_signed_5() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2},
                .operands = {{ // scores4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-38, -33, -53, -48, -58, -43, -68, -38, -33, -38, -63, -38, -48, -43, -48, -68, -68, -108, -68, -48, -88, -38, -73, -68, -38, -53, -58, -48, -58, -43, -38, -33, -53, -48, -43, -48, -68, -38, -33, -68, -68, -108, -78, -38, -48, -38, -53, -58, -38, -63, -38, -38, -73, -68, -68, -48, -88}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // roi4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({8, 8, 80, 80, 0, 0, 80, 80, 0, 0, 80, 80, 16, 16, 88, 88, 8, 8, 88, 88, 8, 8, 88, 88, 24, 24, 96, 96, 16, 16, 96, 96, 16, 16, 96, 96, 32, 32, 104, 104, 24, 24, 104, 104, 24, 24, 104, 104, 40, 40, 112, 112, 32, 32, 112, 112, 32, 32, 112, 112, 48, 48, 120, 120, 40, 40, 120, 120, 40, 40, 120, 120, 56, 56, 128, 128, 48, 48, 128, 128, 48, 48, 128, 128, 64, 64, 136, 136, 56, 56, 136, 136, 56, 56, 136, 136, 72, 72, 144, 144, 64, 64, 144, 144, 64, 64, 144, 144, 16, 16, 88, 88, 16, 16, 96, 96, 16, 16, 96, 96, 8, 8, 80, 80, 8, 8, 88, 88, 8, 8, 88, 88, 40, 40, 112, 112, 40, 40, 120, 120, 40, 40, 120, 120, 24, 24, 96, 96, 24, 24, 104, 104, 24, 24, 104, 104, 48, 48, 120, 120, 48, 48, 128, 128, 48, 48, 128, 128, 0, 0, 8, 8, 0, 0, 16, 16, 0, 0, 16, 16, 72, 72, 144, 144, 72, 72, 152, 152, 72, 72, 152, 152, 32, 32, 104, 104, 32, 32, 112, 112, 32, 32, 112, 112, 64, 64, 136, 136, 64, 64, 144, 144, 64, 64, 144, 144, 56, 56, 128, 128, 56, 56, 136, 136, 56, 56, 136, 136}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param24
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param25
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param26
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param27
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param28
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param29
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // scoresOut4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-33, -43, -53, -33, -58, -86, -88, -33, -38, -43, -53, -33, -48, -58, -86, -88}),
                            .dimensions = {16},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // roiOut4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({0, 0, 80, 80, 32, 32, 112, 112, 64, 64, 144, 144, 16, 16, 96, 96, 64, 64, 144, 144, 32, 32, 112, 112, 0, 0, 80, 80, 8, 8, 88, 88, 0, 0, 16, 16, 40, 40, 120, 120, 72, 72, 152, 152, 24, 24, 104, 104, 0, 0, 16, 16, 72, 72, 152, 152, 40, 40, 120, 120, 8, 8, 88, 88}),
                            .dimensions = {16, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // classesOut4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 2, 2, 2, 2, 2}),
                            .dimensions = {16},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {16},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_signed_5 = TestModelManager::get().add("box_with_nms_limit_quant8_signed_quant8_signed_5", get_test_model_quant8_signed_5());

}  // namespace generated_tests::box_with_nms_limit_quant8_signed

namespace generated_tests::box_with_nms_limit_quant8_signed {

const TestModel& get_test_model_quant8_signed_all_inputs_as_internal_5() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {1, 2, 13},
                .operands = {{ // scores4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // roi4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({8, 8, 80, 80, 0, 0, 80, 80, 0, 0, 80, 80, 16, 16, 88, 88, 8, 8, 88, 88, 8, 8, 88, 88, 24, 24, 96, 96, 16, 16, 96, 96, 16, 16, 96, 96, 32, 32, 104, 104, 24, 24, 104, 104, 24, 24, 104, 104, 40, 40, 112, 112, 32, 32, 112, 112, 32, 32, 112, 112, 48, 48, 120, 120, 40, 40, 120, 120, 40, 40, 120, 120, 56, 56, 128, 128, 48, 48, 128, 128, 48, 48, 128, 128, 64, 64, 136, 136, 56, 56, 136, 136, 56, 56, 136, 136, 72, 72, 144, 144, 64, 64, 144, 144, 64, 64, 144, 144, 16, 16, 88, 88, 16, 16, 96, 96, 16, 16, 96, 96, 8, 8, 80, 80, 8, 8, 88, 88, 8, 8, 88, 88, 40, 40, 112, 112, 40, 40, 120, 120, 40, 40, 120, 120, 24, 24, 96, 96, 24, 24, 104, 104, 24, 24, 104, 104, 48, 48, 120, 120, 48, 48, 128, 128, 48, 48, 128, 128, 0, 0, 8, 8, 0, 0, 16, 16, 0, 0, 16, 16, 72, 72, 144, 144, 72, 72, 152, 152, 72, 72, 152, 152, 32, 32, 104, 104, 32, 32, 112, 112, 32, 32, 112, 112, 64, 64, 136, 136, 64, 64, 144, 144, 64, 64, 144, 144, 56, 56, 128, 128, 56, 56, 136, 136, 56, 56, 136, 136}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param24
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param25
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param26
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param27
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param28
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param29
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // scoresOut4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-33, -43, -53, -33, -58, -86, -88, -33, -38, -43, -53, -33, -48, -58, -86, -88}),
                            .dimensions = {16},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // roiOut4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({0, 0, 80, 80, 32, 32, 112, 112, 64, 64, 144, 144, 16, 16, 96, 96, 64, 64, 144, 144, 32, 32, 112, 112, 0, 0, 80, 80, 8, 8, 88, 88, 0, 0, 16, 16, 40, 40, 120, 120, 72, 72, 152, 152, 24, 24, 104, 104, 0, 0, 16, 16, 72, 72, 152, 152, 40, 40, 120, 120, 8, 8, 88, 88}),
                            .dimensions = {16, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // classesOut4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 2, 2, 2, 2, 2}),
                            .dimensions = {16},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {16},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // scores4_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-38, -33, -53, -48, -58, -43, -68, -38, -33, -38, -63, -38, -48, -43, -48, -68, -68, -108, -68, -48, -88, -38, -73, -68, -38, -53, -58, -48, -58, -43, -38, -33, -53, -48, -43, -48, -68, -38, -33, -68, -68, -108, -78, -38, -48, -38, -53, -58, -38, -63, -38, -38, -73, -68, -68, -48, -88}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param40
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
                            .inputs = {13, 14, 15},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_signed_all_inputs_as_internal_5 = TestModelManager::get().add("box_with_nms_limit_quant8_signed_quant8_signed_all_inputs_as_internal_5", get_test_model_quant8_signed_all_inputs_as_internal_5());

}  // namespace generated_tests::box_with_nms_limit_quant8_signed

namespace generated_tests::box_with_nms_limit_quant8_signed {

const TestModel& get_test_model_quant8_signed_6() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2},
                .operands = {{ // scores5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({90, 95, 75, 80, 70, 85, 60, 90, 95, 90, 65, 90, 80, 85, 80, 60, 60, 20, 60, 80, 40, 90, 55, 60, 90, 75, 70, 80, 70, 85, 90, 95, 75, 80, 85, 80, 60, 90, 95, 60, 60, 20, 50, 90, 80, 90, 75, 70, 90, 65, 90, 90, 55, 60, 60, 80, 40}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // roi5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({8, 8, 80, 80, 0, 0, 80, 80, 0, 0, 80, 80, 16, 16, 88, 88, 8, 8, 88, 88, 8, 8, 88, 88, 24, 24, 96, 96, 16, 16, 96, 96, 16, 16, 96, 96, 32, 32, 104, 104, 24, 24, 104, 104, 24, 24, 104, 104, 40, 40, 112, 112, 32, 32, 112, 112, 32, 32, 112, 112, 48, 48, 120, 120, 40, 40, 120, 120, 40, 40, 120, 120, 56, 56, 128, 128, 48, 48, 128, 128, 48, 48, 128, 128, 64, 64, 136, 136, 56, 56, 136, 136, 56, 56, 136, 136, 72, 72, 144, 144, 64, 64, 144, 144, 64, 64, 144, 144, 16, 16, 88, 88, 16, 16, 96, 96, 16, 16, 96, 96, 8, 8, 80, 80, 8, 8, 88, 88, 8, 8, 88, 88, 40, 40, 112, 112, 40, 40, 120, 120, 40, 40, 120, 120, 24, 24, 96, 96, 24, 24, 104, 104, 24, 24, 104, 104, 48, 48, 120, 120, 48, 48, 128, 128, 48, 48, 128, 128, 0, 0, 8, 8, 0, 0, 16, 16, 0, 0, 16, 16, 72, 72, 144, 144, 72, 72, 152, 152, 72, 72, 152, 152, 32, 32, 104, 104, 32, 32, 112, 112, 32, 32, 112, 112, 64, 64, 136, 136, 64, 64, 144, 144, 64, 64, 144, 144, 56, 56, 128, 128, 56, 56, 136, 136, 56, 56, 136, 136}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param30
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param31
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({8}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param32
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param33
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param34
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param35
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // scoresOut5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({95, 85, 75, 95, 70, 42, 40, 95, 90, 85, 75, 95, 80, 70, 42}),
                            .dimensions = {15},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // roiOut5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({0, 0, 80, 80, 32, 32, 112, 112, 64, 64, 144, 144, 16, 16, 96, 96, 64, 64, 144, 144, 32, 32, 112, 112, 0, 0, 80, 80, 8, 8, 88, 88, 0, 0, 16, 16, 40, 40, 120, 120, 72, 72, 152, 152, 24, 24, 104, 104, 0, 0, 16, 16, 72, 72, 152, 152, 40, 40, 120, 120}),
                            .dimensions = {15, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // classesOut5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 2, 2, 2, 2}),
                            .dimensions = {15},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3}),
                            .dimensions = {15},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_signed_6 = TestModelManager::get().add("box_with_nms_limit_quant8_signed_quant8_signed_6", get_test_model_quant8_signed_6());

}  // namespace generated_tests::box_with_nms_limit_quant8_signed

namespace generated_tests::box_with_nms_limit_quant8_signed {

const TestModel& get_test_model_quant8_signed_all_inputs_as_internal_6() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {1, 2, 13},
                .operands = {{ // scores5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // roi5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({8, 8, 80, 80, 0, 0, 80, 80, 0, 0, 80, 80, 16, 16, 88, 88, 8, 8, 88, 88, 8, 8, 88, 88, 24, 24, 96, 96, 16, 16, 96, 96, 16, 16, 96, 96, 32, 32, 104, 104, 24, 24, 104, 104, 24, 24, 104, 104, 40, 40, 112, 112, 32, 32, 112, 112, 32, 32, 112, 112, 48, 48, 120, 120, 40, 40, 120, 120, 40, 40, 120, 120, 56, 56, 128, 128, 48, 48, 128, 128, 48, 48, 128, 128, 64, 64, 136, 136, 56, 56, 136, 136, 56, 56, 136, 136, 72, 72, 144, 144, 64, 64, 144, 144, 64, 64, 144, 144, 16, 16, 88, 88, 16, 16, 96, 96, 16, 16, 96, 96, 8, 8, 80, 80, 8, 8, 88, 88, 8, 8, 88, 88, 40, 40, 112, 112, 40, 40, 120, 120, 40, 40, 120, 120, 24, 24, 96, 96, 24, 24, 104, 104, 24, 24, 104, 104, 48, 48, 120, 120, 48, 48, 128, 128, 48, 48, 128, 128, 0, 0, 8, 8, 0, 0, 16, 16, 0, 0, 16, 16, 72, 72, 144, 144, 72, 72, 152, 152, 72, 72, 152, 152, 32, 32, 104, 104, 32, 32, 112, 112, 32, 32, 112, 112, 64, 64, 136, 136, 64, 64, 144, 144, 64, 64, 144, 144, 56, 56, 128, 128, 56, 56, 136, 136, 56, 56, 136, 136}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param30
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param31
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({8}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param32
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param33
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param34
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param35
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // scoresOut5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({95, 85, 75, 95, 70, 42, 40, 95, 90, 85, 75, 95, 80, 70, 42}),
                            .dimensions = {15},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // roiOut5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({0, 0, 80, 80, 32, 32, 112, 112, 64, 64, 144, 144, 16, 16, 96, 96, 64, 64, 144, 144, 32, 32, 112, 112, 0, 0, 80, 80, 8, 8, 88, 88, 0, 0, 16, 16, 40, 40, 120, 120, 72, 72, 152, 152, 24, 24, 104, 104, 0, 0, 16, 16, 72, 72, 152, 152, 40, 40, 120, 120}),
                            .dimensions = {15, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // classesOut5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 2, 2, 2, 2}),
                            .dimensions = {15},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3}),
                            .dimensions = {15},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // scores5_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({90, 95, 75, 80, 70, 85, 60, 90, 95, 90, 65, 90, 80, 85, 80, 60, 60, 20, 60, 80, 40, 90, 55, 60, 90, 75, 70, 80, 70, 85, 90, 95, 75, 80, 85, 80, 60, 90, 95, 60, 60, 20, 50, 90, 80, 90, 75, 70, 90, 65, 90, 90, 55, 60, 60, 80, 40}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // dummy5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // param41
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
                            .inputs = {13, 14, 15},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_signed_all_inputs_as_internal_6 = TestModelManager::get().add("box_with_nms_limit_quant8_signed_quant8_signed_all_inputs_as_internal_6", get_test_model_quant8_signed_all_inputs_as_internal_6());

}  // namespace generated_tests::box_with_nms_limit_quant8_signed

