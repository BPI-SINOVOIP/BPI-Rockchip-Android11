// Generated from sub_quant8_signed.mod.py
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, 122, 121, 120, 119, 118, 117, -128, -128, -128, -128, -128, -128, 123, 122, 121, 120, 119, 118, -127, -128, -128, -128, -128, -128, 124, 123, 122, 121, 120, 119, -126, -127, -128, -128, -128, -128, 125, 124, 123, 122, 121, 120, -125, -126, -127, -128, -128, -128, 126, 125, 124, 123, 122, 121, -124, -125, -126, -127, -128, -128, 127, 126, 125, 124, 123, 122, -123, -124, -125, -126, -127, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model = TestModelManager::get().add("sub_quant8_signed", get_test_model());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, 122, 121, 120, 119, 118, 117, -128, -128, -128, -128, -128, -128, 123, 122, 121, 120, 119, 118, -127, -128, -128, -128, -128, -128, 124, 123, 122, 121, 120, 119, -126, -127, -128, -128, -128, -128, 125, 124, 123, 122, 121, 120, -125, -126, -127, -128, -128, -128, 126, 125, 124, 123, 122, 121, -124, -125, -126, -127, -128, -128, 127, 126, 125, 124, 123, 122, -123, -124, -125, -126, -127, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input0_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param80
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param81
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal", get_test_model_all_inputs_as_internal());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -122, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, 123, 122, 121, 120, 119, 118, -127, -128, -128, -128, -128, -128, 124, 123, 122, 121, 120, 119, -126, -127, -128, -128, -128, -128, 125, 124, 123, 122, 121, 120, -125, -126, -127, -128, -128, -128, 126, 125, 124, 123, 122, 121, -124, -125, -126, -127, -128, -128, 127, 126, 125, 124, 123, 122, -123, -124, -125, -126, -127, -128, 127, 127, 126, 125, 124, 123, -122, -123, -124, -125, -126, -127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_2 = TestModelManager::get().add("sub_quant8_signed_2", get_test_model_2());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -122, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, 123, 122, 121, 120, 119, 118, -127, -128, -128, -128, -128, -128, 124, 123, 122, 121, 120, 119, -126, -127, -128, -128, -128, -128, 125, 124, 123, 122, 121, 120, -125, -126, -127, -128, -128, -128, 126, 125, 124, 123, 122, 121, -124, -125, -126, -127, -128, -128, 127, 126, 125, 124, 123, 122, -123, -124, -125, -126, -127, -128, 127, 127, 126, 125, 124, 123, -122, -123, -124, -125, -126, -127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input01_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param82
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input11_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param83
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_2 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_2", get_test_model_all_inputs_as_internal_2());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_3() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input12
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -108, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 92, -8, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -8, -108, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_3 = TestModelManager::get().add("sub_quant8_signed_3", get_test_model_3());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_3() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input12
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -108, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 92, -8, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -8, -108, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input02_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param84
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input12_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param85
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_3 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_3", get_test_model_all_inputs_as_internal_3());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_4() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input13
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -34, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -32, -33, -33, -33, -33, -33, 17, 17, 17, 17, 17, 16, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 18, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_4 = TestModelManager::get().add("sub_quant8_signed_4", get_test_model_4());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_4() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input13
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -34, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -32, -33, -33, -33, -33, -33, 17, 17, 17, 17, 17, 16, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 18, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input03_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param86
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input13_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param87
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_4 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_4", get_test_model_all_inputs_as_internal_4());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_5() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input14
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -122, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, 123, 122, 121, 120, 119, 118, -127, -128, -128, -128, -128, -128, 124, 123, 122, 121, 120, 119, -126, -127, -128, -128, -128, -128, 125, 124, 123, 122, 121, 120, -125, -126, -127, -128, -128, -128, 126, 125, 124, 123, 122, 121, -124, -125, -126, -127, -128, -128, 127, 126, 125, 124, 123, 122, -123, -124, -125, -126, -127, -128, 127, 127, 126, 125, 124, 123, -122, -123, -124, -125, -126, -127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_5 = TestModelManager::get().add("sub_quant8_signed_5", get_test_model_5());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_5() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input14
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -122, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, 123, 122, 121, 120, 119, 118, -127, -128, -128, -128, -128, -128, 124, 123, 122, 121, 120, 119, -126, -127, -128, -128, -128, -128, 125, 124, 123, 122, 121, 120, -125, -126, -127, -128, -128, -128, 126, 125, 124, 123, 122, 121, -124, -125, -126, -127, -128, -128, 127, 126, 125, 124, 123, 122, -123, -124, -125, -126, -127, -128, 127, 127, 126, 125, 124, 123, -122, -123, -124, -125, -126, -127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input04_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy8
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param88
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input14_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy9
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param89
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_5 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_5", get_test_model_all_inputs_as_internal_5());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_6() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input15
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -122, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -121, -122, -123, -124, -125, -126, -128, -128, -128, -128, -128, -128, 124, 123, 122, 121, 120, 119, -126, -127, -128, -128, -128, -128, 125, 124, 123, 122, 121, 120, -125, -126, -127, -128, -128, -128, 126, 125, 124, 123, 122, 121, -124, -125, -126, -127, -128, -128, 127, 126, 125, 124, 123, 122, -123, -124, -125, -126, -127, -128, 127, 127, 126, 125, 124, 123, -122, -123, -124, -125, -126, -127, 127, 127, 127, 126, 125, 124, -121, -122, -123, -124, -125, -126}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_6 = TestModelManager::get().add("sub_quant8_signed_6", get_test_model_6());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_6() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input15
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -122, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -121, -122, -123, -124, -125, -126, -128, -128, -128, -128, -128, -128, 124, 123, 122, 121, 120, 119, -126, -127, -128, -128, -128, -128, 125, 124, 123, 122, 121, 120, -125, -126, -127, -128, -128, -128, 126, 125, 124, 123, 122, 121, -124, -125, -126, -127, -128, -128, 127, 126, 125, 124, 123, 122, -123, -124, -125, -126, -127, -128, 127, 127, 126, 125, 124, 123, -122, -123, -124, -125, -126, -127, 127, 127, 127, 126, 125, 124, -121, -122, -123, -124, -125, -126}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input05_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param90
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input15_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param91
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_6 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_6", get_test_model_all_inputs_as_internal_6());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_7() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input16
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({92, -8, -108, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 92, -8, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 92, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 92}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_7 = TestModelManager::get().add("sub_quant8_signed_7", get_test_model_7());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_7() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input16
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({92, -8, -108, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 92, -8, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 92, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 92}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input06_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy12
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param92
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input16_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy13
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param93
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_7 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_7", get_test_model_all_inputs_as_internal_7());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_8() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input07
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input17
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output07
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -32, -33, -33, -33, -33, -33, -7, -8, -8, -8, -8, -8, -32, -32, -33, -33, -33, -33, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 18, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 18, 18, 17, 17, 17, 17, -7, -8, -8, -8, -8, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_8 = TestModelManager::get().add("sub_quant8_signed_8", get_test_model_8());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_8() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input07
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input17
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output07
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -32, -33, -33, -33, -33, -33, -7, -8, -8, -8, -8, -8, -32, -32, -33, -33, -33, -33, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 18, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 18, 18, 17, 17, 17, 17, -7, -8, -8, -8, -8, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input07_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy14
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param94
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input17_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy15
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param95
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_8 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_8", get_test_model_all_inputs_as_internal_8());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_9() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input08
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input18
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param8
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output08
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -126, -126, -126, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -125, -125, -125, -125, -127, -127, -127, -127, -127, -127, -124, -124, -124, -124, -124, -124, -126, -126, -126, -126, -126, -126, -123, -123, -123, -123, -123, -123, -125, -125, -125, -125, -125, -125, -122, -122, -122, -122, -122, -122, -124, -124, -124, -124, -124, -124, 123, 123, 123, 123, 123, 123, 121, 121, 121, 121, 121, 121, 124, 124, 124, 124, 124, 124, 122, 122, 122, 122, 122, 122, 125, 125, 125, 125, 125, 125, 123, 123, 123, 123, 123, 123, 126, 126, 126, 126, 126, 126, 124, 124, 124, 124, 124, 124, 127, 127, 127, 127, 127, 127, 125, 125, 125, 125, 125, 125, 127, 127, 127, 127, 127, 127, 126, 126, 126, 126, 126, 126}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_9 = TestModelManager::get().add("sub_quant8_signed_9", get_test_model_9());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_9() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input08
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input18
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param8
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output08
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -126, -126, -126, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -125, -125, -125, -125, -127, -127, -127, -127, -127, -127, -124, -124, -124, -124, -124, -124, -126, -126, -126, -126, -126, -126, -123, -123, -123, -123, -123, -123, -125, -125, -125, -125, -125, -125, -122, -122, -122, -122, -122, -122, -124, -124, -124, -124, -124, -124, 123, 123, 123, 123, 123, 123, 121, 121, 121, 121, 121, 121, 124, 124, 124, 124, 124, 124, 122, 122, 122, 122, 122, 122, 125, 125, 125, 125, 125, 125, 123, 123, 123, 123, 123, 123, 126, 126, 126, 126, 126, 126, 124, 124, 124, 124, 124, 124, 127, 127, 127, 127, 127, 127, 125, 125, 125, 125, 125, 125, 127, 127, 127, 127, 127, 127, 126, 126, 126, 126, 126, 126}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input08_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy16
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param96
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input18_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy17
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param97
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_9 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_9", get_test_model_all_inputs_as_internal_9());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_10() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input09
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input19
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param9
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output09
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-126, -126, -126, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -125, -125, -125, -125, -127, -127, -127, -127, -127, -127, -124, -124, -124, -124, -124, -124, -126, -126, -126, -126, -126, -126, -123, -123, -123, -123, -123, -123, -125, -125, -125, -125, -125, -125, -122, -122, -122, -122, -122, -122, -124, -124, -124, -124, -124, -124, -121, -121, -121, -121, -121, -121, -123, -123, -123, -123, -123, -123, 124, 124, 124, 124, 124, 124, 122, 122, 122, 122, 122, 122, 125, 125, 125, 125, 125, 125, 123, 123, 123, 123, 123, 123, 126, 126, 126, 126, 126, 126, 124, 124, 124, 124, 124, 124, 127, 127, 127, 127, 127, 127, 125, 125, 125, 125, 125, 125, 127, 127, 127, 127, 127, 127, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_10 = TestModelManager::get().add("sub_quant8_signed_10", get_test_model_10());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_10() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input09
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input19
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param9
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output09
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-126, -126, -126, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -125, -125, -125, -125, -127, -127, -127, -127, -127, -127, -124, -124, -124, -124, -124, -124, -126, -126, -126, -126, -126, -126, -123, -123, -123, -123, -123, -123, -125, -125, -125, -125, -125, -125, -122, -122, -122, -122, -122, -122, -124, -124, -124, -124, -124, -124, -121, -121, -121, -121, -121, -121, -123, -123, -123, -123, -123, -123, 124, 124, 124, 124, 124, 124, 122, 122, 122, 122, 122, 122, 125, 125, 125, 125, 125, 125, 123, 123, 123, 123, 123, 123, 126, 126, 126, 126, 126, 126, 124, 124, 124, 124, 124, 124, 127, 127, 127, 127, 127, 127, 125, 125, 125, 125, 125, 125, 127, 127, 127, 127, 127, 127, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input09_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy18
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param98
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input19_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy19
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param99
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_10 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_10", get_test_model_all_inputs_as_internal_10());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_11() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input010
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input110
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output010
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -38, -39, -40, -41, -42, -43, 127, 127, 127, 127, 127, 127, 62, 61, 60, 59, 58, 57, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_11 = TestModelManager::get().add("sub_quant8_signed_11", get_test_model_11());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_11() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input010
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input110
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output010
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -38, -39, -40, -41, -42, -43, 127, 127, 127, 127, 127, 127, 62, 61, 60, 59, 58, 57, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input010_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy20
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param100
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input110_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy21
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param101
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_11 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_11", get_test_model_all_inputs_as_internal_11());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_12() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input011
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input111
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output011
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -7, -7, -7, -7, -7, -7, -8, -8, -8, -8, -8, -8, -7, -7, -7, -7, -7, -7, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 17, 17, 17, 17, 17, 17}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_12 = TestModelManager::get().add("sub_quant8_signed_12", get_test_model_12());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_12() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input011
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input111
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output011
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -7, -7, -7, -7, -7, -7, -8, -8, -8, -8, -8, -8, -7, -7, -7, -7, -7, -7, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 17, 17, 17, 17, 17, 17}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input011_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy22
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param102
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input111_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy23
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param103
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_12 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_12", get_test_model_all_inputs_as_internal_12());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_13() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input012
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input112
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
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
                        }, { // output012
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_13 = TestModelManager::get().add("sub_quant8_signed_13", get_test_model_13());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_13() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input012
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input112
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
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
                        }, { // output012
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input012_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy24
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param104
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input112_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy25
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param105
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_13 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_13", get_test_model_all_inputs_as_internal_13());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_14() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input013
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input113
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
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
                        }, { // output013
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_14 = TestModelManager::get().add("sub_quant8_signed_14", get_test_model_14());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_14() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input013
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input113
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
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
                        }, { // output013
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input013_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy26
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param106
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input113_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy27
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param107
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_14 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_14", get_test_model_all_inputs_as_internal_14());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_15() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input014
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input114
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
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
                        }, { // output014
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_15 = TestModelManager::get().add("sub_quant8_signed_15", get_test_model_15());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_15() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input014
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input114
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
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
                        }, { // output014
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input014_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy28
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param108
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input114_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy29
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param109
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_15 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_15", get_test_model_all_inputs_as_internal_15());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input015
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input115
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
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
                        }, { // output015
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 112, 110, 110, 108, 108, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -112, -114, -114, -116, -116, -118}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_16 = TestModelManager::get().add("sub_quant8_signed_16", get_test_model_16());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input015
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input115
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
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
                        }, { // output015
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 112, 110, 110, 108, 108, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -112, -114, -114, -116, -116, -118}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input015_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy30
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param110
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input115_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy31
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param111
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_16 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_16", get_test_model_all_inputs_as_internal_16());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_17() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input016
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input116
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
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
                        }, { // output016
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, 121, 120, 119, 118, 117, 116, -128, -128, -128, -128, -128, -128, 122, 121, 120, 119, 118, 117, -128, -128, -128, -128, -128, -128, 123, 122, 121, 120, 119, 118, -127, -128, -128, -128, -128, -128, 124, 123, 122, 121, 120, 119, -126, -127, -128, -128, -128, -128, 125, 124, 123, 122, 121, 120, -125, -126, -127, -128, -128, -128, 126, 125, 124, 123, 122, 121, -124, -125, -126, -127, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_17 = TestModelManager::get().add("sub_quant8_signed_17", get_test_model_17());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_17() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input016
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input116
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
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
                        }, { // output016
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, 121, 120, 119, 118, 117, 116, -128, -128, -128, -128, -128, -128, 122, 121, 120, 119, 118, 117, -128, -128, -128, -128, -128, -128, 123, 122, 121, 120, 119, 118, -127, -128, -128, -128, -128, -128, 124, 123, 122, 121, 120, 119, -126, -127, -128, -128, -128, -128, 125, 124, 123, 122, 121, 120, -125, -126, -127, -128, -128, -128, 126, 125, 124, 123, 122, 121, -124, -125, -126, -127, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input016_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy32
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param112
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input116_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy33
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param113
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_17 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_17", get_test_model_all_inputs_as_internal_17());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_18() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input017
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input117
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
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
                        }, { // output017
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, 122, 121, 120, 119, 118, 117, -128, -128, -128, -128, -128, -128, 123, 122, 121, 120, 119, 118, -127, -128, -128, -128, -128, -128, 124, 123, 122, 121, 120, 119, -126, -127, -128, -128, -128, -128, 125, 124, 123, 122, 121, 120, -125, -126, -127, -128, -128, -128, 126, 125, 124, 123, 122, 121, -124, -125, -126, -127, -128, -128, 127, 126, 125, 124, 123, 122, -123, -124, -125, -126, -127, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_18 = TestModelManager::get().add("sub_quant8_signed_18", get_test_model_18());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_18() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input017
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input117
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
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
                        }, { // output017
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, 122, 121, 120, 119, 118, 117, -128, -128, -128, -128, -128, -128, 123, 122, 121, 120, 119, 118, -127, -128, -128, -128, -128, -128, 124, 123, 122, 121, 120, 119, -126, -127, -128, -128, -128, -128, 125, 124, 123, 122, 121, 120, -125, -126, -127, -128, -128, -128, 126, 125, 124, 123, 122, 121, -124, -125, -126, -127, -128, -128, 127, 126, 125, 124, 123, 122, -123, -124, -125, -126, -127, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input017_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy34
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param114
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input117_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy35
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param115
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_18 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_18", get_test_model_all_inputs_as_internal_18());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_19() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input018
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input118
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
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
                        }, { // output018
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-108, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -8, -108, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -108, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -8, -108, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_19 = TestModelManager::get().add("sub_quant8_signed_19", get_test_model_19());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_19() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input018
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input118
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
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
                        }, { // output018
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-108, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -8, -108, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -108, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -8, -108, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input018_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy36
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param116
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input118_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy37
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param117
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_19 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_19", get_test_model_all_inputs_as_internal_19());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_20() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input019
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input119
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
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
                        }, { // output019
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -8, -8, -8, -8, -9, -33, -33, -33, -33, -34, -34, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -34, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, 17, 17, 17, 17, 16, 16, -8, -8, -8, -8, -8, -9, 17, 17, 17, 17, 17, 16, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_20 = TestModelManager::get().add("sub_quant8_signed_20", get_test_model_20());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_20() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input019
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input119
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
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
                        }, { // output019
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -8, -8, -8, -8, -9, -33, -33, -33, -33, -34, -34, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -34, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, 17, 17, 17, 17, 16, 16, -8, -8, -8, -8, -8, -9, 17, 17, 17, 17, 17, 16, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input019_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy38
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param118
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input119_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy39
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param119
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_20 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_20", get_test_model_all_inputs_as_internal_20());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_21() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input020
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input120
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
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
                        }, { // output020
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, 122, 121, 120, 119, 118, 117, -128, -128, -128, -128, -128, -128, 123, 122, 121, 120, 119, 118, -127, -128, -128, -128, -128, -128, 124, 123, 122, 121, 120, 119, -126, -127, -128, -128, -128, -128, 125, 124, 123, 122, 121, 120, -125, -126, -127, -128, -128, -128, 126, 125, 124, 123, 122, 121, -124, -125, -126, -127, -128, -128, 127, 126, 125, 124, 123, 122, -123, -124, -125, -126, -127, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_21 = TestModelManager::get().add("sub_quant8_signed_21", get_test_model_21());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_21() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input020
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input120
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
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
                        }, { // output020
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, 122, 121, 120, 119, 118, 117, -128, -128, -128, -128, -128, -128, 123, 122, 121, 120, 119, 118, -127, -128, -128, -128, -128, -128, 124, 123, 122, 121, 120, 119, -126, -127, -128, -128, -128, -128, 125, 124, 123, 122, 121, 120, -125, -126, -127, -128, -128, -128, 126, 125, 124, 123, 122, 121, -124, -125, -126, -127, -128, -128, 127, 126, 125, 124, 123, 122, -123, -124, -125, -126, -127, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input020_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy40
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param120
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input120_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy41
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param121
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_21 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_21", get_test_model_all_inputs_as_internal_21());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_22() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input021
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input121
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param21
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output021
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -122, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, 123, 122, 121, 120, 119, 118, -127, -128, -128, -128, -128, -128, 124, 123, 122, 121, 120, 119, -126, -127, -128, -128, -128, -128, 125, 124, 123, 122, 121, 120, -125, -126, -127, -128, -128, -128, 126, 125, 124, 123, 122, 121, -124, -125, -126, -127, -128, -128, 127, 126, 125, 124, 123, 122, -123, -124, -125, -126, -127, -128, 127, 127, 126, 125, 124, 123, -122, -123, -124, -125, -126, -127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_22 = TestModelManager::get().add("sub_quant8_signed_22", get_test_model_22());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_22() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input021
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input121
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param21
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output021
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -122, -123, -124, -125, -126, -127, -128, -128, -128, -128, -128, -128, 123, 122, 121, 120, 119, 118, -127, -128, -128, -128, -128, -128, 124, 123, 122, 121, 120, 119, -126, -127, -128, -128, -128, -128, 125, 124, 123, 122, 121, 120, -125, -126, -127, -128, -128, -128, 126, 125, 124, 123, 122, 121, -124, -125, -126, -127, -128, -128, 127, 126, 125, 124, 123, 122, -123, -124, -125, -126, -127, -128, 127, 127, 126, 125, 124, 123, -122, -123, -124, -125, -126, -127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input021_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy42
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param122
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input121_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy43
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param123
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_22 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_22", get_test_model_all_inputs_as_internal_22());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_23() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input022
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input122
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param22
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output022
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -108, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 92, -8, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -8, -108, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_23 = TestModelManager::get().add("sub_quant8_signed_23", get_test_model_23());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_23() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input022
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input122
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param22
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output022
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -108, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 92, -8, -108, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 92, -8, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -8, -108, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8, -108, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 92, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input022_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy44
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param124
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input122_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy45
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param125
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_23 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_23", get_test_model_all_inputs_as_internal_23());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_24() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input023
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input123
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param23
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output023
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -34, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -32, -33, -33, -33, -33, -33, 17, 17, 17, 17, 17, 16, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 18, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_24 = TestModelManager::get().add("sub_quant8_signed_24", get_test_model_24());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_24() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input023
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input123
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param23
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output023
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -34, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -32, -33, -33, -33, -33, -33, 17, 17, 17, 17, 17, 16, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8, 18, 17, 17, 17, 17, 17, -8, -8, -8, -8, -8, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input023_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy46
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param126
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input123_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy47
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param127
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_24 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_24", get_test_model_all_inputs_as_internal_24());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_25() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input024
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input124
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param24
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output024
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -126, -126, -126, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -125, -125, -125, -125, -127, -127, -127, -127, -127, -127, -124, -124, -124, -124, -124, -124, -126, -126, -126, -126, -126, -126, -123, -123, -123, -123, -123, -123, -125, -125, -125, -125, -125, -125, 122, 122, 122, 122, 122, 122, 120, 120, 120, 120, 120, 120, 123, 123, 123, 123, 123, 123, 121, 121, 121, 121, 121, 121, 124, 124, 124, 124, 124, 124, 122, 122, 122, 122, 122, 122, 125, 125, 125, 125, 125, 125, 123, 123, 123, 123, 123, 123, 126, 126, 126, 126, 126, 126, 124, 124, 124, 124, 124, 124, 127, 127, 127, 127, 127, 127, 125, 125, 125, 125, 125, 125}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_25 = TestModelManager::get().add("sub_quant8_signed_25", get_test_model_25());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_25() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input024
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input124
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param24
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output024
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -126, -126, -126, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -125, -125, -125, -125, -127, -127, -127, -127, -127, -127, -124, -124, -124, -124, -124, -124, -126, -126, -126, -126, -126, -126, -123, -123, -123, -123, -123, -123, -125, -125, -125, -125, -125, -125, 122, 122, 122, 122, 122, 122, 120, 120, 120, 120, 120, 120, 123, 123, 123, 123, 123, 123, 121, 121, 121, 121, 121, 121, 124, 124, 124, 124, 124, 124, 122, 122, 122, 122, 122, 122, 125, 125, 125, 125, 125, 125, 123, 123, 123, 123, 123, 123, 126, 126, 126, 126, 126, 126, 124, 124, 124, 124, 124, 124, 127, 127, 127, 127, 127, 127, 125, 125, 125, 125, 125, 125}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input024_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy48
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param128
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input124_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy49
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param129
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_25 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_25", get_test_model_all_inputs_as_internal_25());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_26() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input025
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input125
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param25
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output025
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -126, -126, -126, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -125, -125, -125, -125, -127, -127, -127, -127, -127, -127, -124, -124, -124, -124, -124, -124, -126, -126, -126, -126, -126, -126, -123, -123, -123, -123, -123, -123, -125, -125, -125, -125, -125, -125, -122, -122, -122, -122, -122, -122, -124, -124, -124, -124, -124, -124, 123, 123, 123, 123, 123, 123, 121, 121, 121, 121, 121, 121, 124, 124, 124, 124, 124, 124, 122, 122, 122, 122, 122, 122, 125, 125, 125, 125, 125, 125, 123, 123, 123, 123, 123, 123, 126, 126, 126, 126, 126, 126, 124, 124, 124, 124, 124, 124, 127, 127, 127, 127, 127, 127, 125, 125, 125, 125, 125, 125, 127, 127, 127, 127, 127, 127, 126, 126, 126, 126, 126, 126}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_26 = TestModelManager::get().add("sub_quant8_signed_26", get_test_model_26());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_26() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input025
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input125
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param25
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output025
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -126, -126, -126, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -125, -125, -125, -125, -127, -127, -127, -127, -127, -127, -124, -124, -124, -124, -124, -124, -126, -126, -126, -126, -126, -126, -123, -123, -123, -123, -123, -123, -125, -125, -125, -125, -125, -125, -122, -122, -122, -122, -122, -122, -124, -124, -124, -124, -124, -124, 123, 123, 123, 123, 123, 123, 121, 121, 121, 121, 121, 121, 124, 124, 124, 124, 124, 124, 122, 122, 122, 122, 122, 122, 125, 125, 125, 125, 125, 125, 123, 123, 123, 123, 123, 123, 126, 126, 126, 126, 126, 126, 124, 124, 124, 124, 124, 124, 127, 127, 127, 127, 127, 127, 125, 125, 125, 125, 125, 125, 127, 127, 127, 127, 127, 127, 126, 126, 126, 126, 126, 126}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input025_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy50
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param130
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input125_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy51
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param131
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_26 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_26", get_test_model_all_inputs_as_internal_26());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_27() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input026
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input126
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param26
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output026
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({12, 11, 10, 9, 8, 7, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -38, -39, -40, -41, -42, -43, 127, 127, 127, 127, 127, 127, 62, 61, 60, 59, 58, 57, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_27 = TestModelManager::get().add("sub_quant8_signed_27", get_test_model_27());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_27() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input026
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input126
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param26
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output026
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({12, 11, 10, 9, 8, 7, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -38, -39, -40, -41, -42, -43, 127, 127, 127, 127, 127, 127, 62, 61, 60, 59, 58, 57, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input026_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy52
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param132
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input126_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy53
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param133
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_27 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_27", get_test_model_all_inputs_as_internal_27());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_28() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input027
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input127
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param27
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output027
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -7, -7, -7, -7, -7, -7, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 17, 17, 17, 17, 17, 17}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_28 = TestModelManager::get().add("sub_quant8_signed_28", get_test_model_28());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_28() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input027
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input127
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param27
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output027
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -7, -7, -7, -7, -7, -7, -8, -8, -8, -8, -8, -8, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 17, 17, 17, 17, 17, 17}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input027_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy54
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param134
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input127_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy55
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param135
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_28 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_28", get_test_model_all_inputs_as_internal_28());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_29() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input028
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input128
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param28
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output028
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_29 = TestModelManager::get().add("sub_quant8_signed_29", get_test_model_29());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_29() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input028
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input128
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param28
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output028
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input028_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy56
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param136
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input128_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy57
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param137
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_29 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_29", get_test_model_all_inputs_as_internal_29());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_30() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input029
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input129
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param29
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output029
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_30 = TestModelManager::get().add("sub_quant8_signed_30", get_test_model_30());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_30() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input029
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input129
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param29
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output029
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input029_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy58
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param138
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input129_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy59
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param139
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_30 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_30", get_test_model_all_inputs_as_internal_30());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_31() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input030
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input130
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param30
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output030
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_31 = TestModelManager::get().add("sub_quant8_signed_31", get_test_model_31());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_31() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input030
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input130
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param30
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output030
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input030_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy60
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param140
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input130_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy61
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param141
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_31 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_31", get_test_model_all_inputs_as_internal_31());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_32() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input031
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input131
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param31
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output031
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_32 = TestModelManager::get().add("sub_quant8_signed_32", get_test_model_32());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_32() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input031
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input131
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param31
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output031
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118, 127, 127, 127, 127, 127, 127, -113, -114, -115, -116, -117, -118}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input031_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy62
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param142
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input131_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy63
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param143
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_32 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_32", get_test_model_all_inputs_as_internal_32());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_33() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input032
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input132
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param32
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output032
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_33 = TestModelManager::get().add("sub_quant8_signed_33", get_test_model_33());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_33() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input032
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input132
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param32
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output032
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input032_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy64
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param144
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input132_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy65
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param145
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_33 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_33", get_test_model_all_inputs_as_internal_33());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_34() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input033
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input133
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param33
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output033
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_34 = TestModelManager::get().add("sub_quant8_signed_34", get_test_model_34());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_34() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input033
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input133
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param33
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output033
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input033_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy66
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param146
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input133_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy67
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param147
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_34 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_34", get_test_model_all_inputs_as_internal_34());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_35() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input034
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input134
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param34
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output034
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -124, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -123, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 122, 22, -78, -128, -128, -128, -128, -128, -128, -128, -128, -128, 123, 23, -77, -128, -128, -128, -128, -128, -128, -128, -128, -128, 124, 24, -76, -128, -128, -128, -128, -128, -128, -128, -128, -128, 125, 25, -75, -128, -128, -128, -128, -128, -128, -128, -128, -128, 126, 26, -74, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 27, -73, -128, -128, -128, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_35 = TestModelManager::get().add("sub_quant8_signed_35", get_test_model_35());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_35() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input034
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input134
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param34
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output034
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -124, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -123, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 122, 22, -78, -128, -128, -128, -128, -128, -128, -128, -128, -128, 123, 23, -77, -128, -128, -128, -128, -128, -128, -128, -128, -128, 124, 24, -76, -128, -128, -128, -128, -128, -128, -128, -128, -128, 125, 25, -75, -128, -128, -128, -128, -128, -128, -128, -128, -128, 126, 26, -74, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 27, -73, -128, -128, -128, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input034_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy68
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param148
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input134_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy69
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param149
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_35 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_35", get_test_model_all_inputs_as_internal_35());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_36() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input035
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input135
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param35
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output035
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -8, -8, -8, -9, -9, -33, -33, -33, -33, -34, -34, -8, -8, -8, -8, -9, -9, -33, -33, -33, -33, -34, -34, -8, -8, -8, -8, -9, -9, -33, -33, -33, -33, -34, -34, -8, -8, -8, -8, -9, -9, -33, -33, -33, -33, -34, -34, -8, -8, -8, -8, -9, -9, -33, -33, -33, -33, -34, -34, -8, -8, -8, -8, -9, -9, -33, -33, -33, -33, -34, -34, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_36 = TestModelManager::get().add("sub_quant8_signed_36", get_test_model_36());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_36() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input035
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input135
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param35
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output035
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -8, -8, -8, -9, -9, -33, -33, -33, -33, -34, -34, -8, -8, -8, -8, -9, -9, -33, -33, -33, -33, -34, -34, -8, -8, -8, -8, -9, -9, -33, -33, -33, -33, -34, -34, -8, -8, -8, -8, -9, -9, -33, -33, -33, -33, -34, -34, -8, -8, -8, -8, -9, -9, -33, -33, -33, -33, -34, -34, -8, -8, -8, -8, -9, -9, -33, -33, -33, -33, -34, -34, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input035_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy70
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param150
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input135_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy71
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param151
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_36 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_36", get_test_model_all_inputs_as_internal_36());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_37() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input036
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input136
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
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
                        }, { // output036
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_37 = TestModelManager::get().add("sub_quant8_signed_37", get_test_model_37());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_37() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input036
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input136
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
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
                        }, { // output036
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input036_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy72
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param152
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input136_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy73
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param153
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_37 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_37", get_test_model_all_inputs_as_internal_37());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_38() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input037
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input137
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
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
                        }, { // output037
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_38 = TestModelManager::get().add("sub_quant8_signed_38", get_test_model_38());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_38() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input037
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input137
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
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
                        }, { // output037
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -125, -126, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input037_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy74
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param154
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input137_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy75
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param155
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_38 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_38", get_test_model_all_inputs_as_internal_38());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_39() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input038
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input138
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
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
                        }, { // output038
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-28, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -27, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -26, -126, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -25, -125, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -24, -124, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -23, -123, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 122, 22, -78, -128, -128, -128, -128, -128, -128, -128, -128, 127, 123, 23, -77, -128, -128, -128, -128, -128, -128, -128, -128, 127, 124, 24, -76, -128, -128, -128, -128, -128, -128, -128, -128, 127, 125, 25, -75, -128, -128, -128, -128, -128, -128, -128, -128, 127, 126, 26, -74, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 27, -73, -128, -128, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_39 = TestModelManager::get().add("sub_quant8_signed_39", get_test_model_39());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_39() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input038
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input138
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
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
                        }, { // output038
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-28, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -27, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -26, -126, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -25, -125, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -24, -124, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -23, -123, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 122, 22, -78, -128, -128, -128, -128, -128, -128, -128, -128, 127, 123, 23, -77, -128, -128, -128, -128, -128, -128, -128, -128, 127, 124, 24, -76, -128, -128, -128, -128, -128, -128, -128, -128, 127, 125, 25, -75, -128, -128, -128, -128, -128, -128, -128, -128, 127, 126, 26, -74, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 27, -73, -128, -128, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input038_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy76
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param156
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input138_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy77
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param157
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_39 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_39", get_test_model_all_inputs_as_internal_39());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_40() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input039
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input139
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
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
                        }, { // output039
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -8, -8, -8, -8, -9, -33, -33, -33, -33, -33, -34, -8, -8, -8, -8, -8, -9, -33, -33, -33, -33, -33, -34, -8, -8, -8, -8, -8, -9, -33, -33, -33, -33, -33, -34, -8, -8, -8, -8, -8, -9, -33, -33, -33, -33, -33, -34, -8, -8, -8, -8, -8, -9, -33, -33, -33, -33, -33, -34, -8, -8, -8, -8, -8, -9, -33, -33, -33, -33, -33, -34, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_40 = TestModelManager::get().add("sub_quant8_signed_40", get_test_model_40());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_40() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input039
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input139
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
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
                        }, { // output039
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -8, -8, -8, -8, -9, -33, -33, -33, -33, -33, -34, -8, -8, -8, -8, -8, -9, -33, -33, -33, -33, -33, -34, -8, -8, -8, -8, -8, -9, -33, -33, -33, -33, -33, -34, -8, -8, -8, -8, -8, -9, -33, -33, -33, -33, -33, -34, -8, -8, -8, -8, -8, -9, -33, -33, -33, -33, -33, -34, -8, -8, -8, -8, -8, -9, -33, -33, -33, -33, -33, -34, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33, -8, -8, -8, -8, -8, -8, -33, -33, -33, -33, -33, -33}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input039_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy78
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param158
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input139_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy79
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param159
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_40 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_40", get_test_model_all_inputs_as_internal_40());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_41() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input040
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input140
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
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
                        }, { // output040
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -126, -126, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -126, -126, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -126, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -125, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -125, -125, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -125, -125, -125, -126, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_41 = TestModelManager::get().add("sub_quant8_signed_41", get_test_model_41());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_41() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input040
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input140
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
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
                        }, { // output040
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -126, -126, -126, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -126, -126, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -126, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -125, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -125, -125, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -125, -125, -125, -126, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input040_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy80
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param160
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input140_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy81
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param161
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_41 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_41", get_test_model_all_inputs_as_internal_41());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_42() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input041
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input141
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
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
                        }, { // output041
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -125, -125, -125, -125, -125, -125, -127, -127, -127, -127, -127, -127, -124, -125, -125, -125, -125, -125, -127, -127, -127, -127, -127, -127, -124, -124, -125, -125, -125, -125, -127, -127, -127, -127, -127, -127, -124, -124, -124, -125, -125, -125, -127, -127, -127, -127, -127, -127, -124, -124, -124, -124, -125, -125, -127, -127, -127, -127, -127, -127, -124, -124, -124, -124, -124, -125, -127, -127, -127, -127, -127, -127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_42 = TestModelManager::get().add("sub_quant8_signed_42", get_test_model_42());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_42() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input041
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input141
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
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
                        }, { // output041
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -125, -125, -125, -125, -125, -125, -127, -127, -127, -127, -127, -127, -124, -125, -125, -125, -125, -125, -127, -127, -127, -127, -127, -127, -124, -124, -125, -125, -125, -125, -127, -127, -127, -127, -127, -127, -124, -124, -124, -125, -125, -125, -127, -127, -127, -127, -127, -127, -124, -124, -124, -124, -125, -125, -127, -127, -127, -127, -127, -127, -124, -124, -124, -124, -124, -125, -127, -127, -127, -127, -127, -127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input041_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy82
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param162
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input141_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy83
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param163
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_42 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_42", get_test_model_all_inputs_as_internal_42());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_43() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input042
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input142
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param42
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output042
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -9, -10, -11, -12, -13, -128, -128, -128, -128, -128, -128, -7, -8, -9, -10, -11, -12, -128, -128, -128, -128, -128, -128, -6, -7, -8, -9, -10, -11, -128, -128, -128, -128, -128, -128, -5, -6, -7, -8, -9, -10, -128, -128, -128, -128, -128, -128, -4, -5, -6, -7, -8, -9, -128, -128, -128, -128, -128, -128, -3, -4, -5, -6, -7, -8, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -8, -9, -10, -11, -12, -13, 127, 127, 127, 127, 127, 127, -7, -8, -9, -10, -11, -12, 127, 127, 127, 127, 127, 127, -6, -7, -8, -9, -10, -11, 127, 127, 127, 127, 127, 127, -5, -6, -7, -8, -9, -10, 127, 127, 127, 127, 127, 127, -4, -5, -6, -7, -8, -9, 127, 127, 127, 127, 127, 127, -3, -4, -5, -6, -7, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_43 = TestModelManager::get().add("sub_quant8_signed_43", get_test_model_43());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_43() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input042
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input142
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param42
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output042
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -9, -10, -11, -12, -13, -128, -128, -128, -128, -128, -128, -7, -8, -9, -10, -11, -12, -128, -128, -128, -128, -128, -128, -6, -7, -8, -9, -10, -11, -128, -128, -128, -128, -128, -128, -5, -6, -7, -8, -9, -10, -128, -128, -128, -128, -128, -128, -4, -5, -6, -7, -8, -9, -128, -128, -128, -128, -128, -128, -3, -4, -5, -6, -7, -8, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -8, -9, -10, -11, -12, -13, 127, 127, 127, 127, 127, 127, -7, -8, -9, -10, -11, -12, 127, 127, 127, 127, 127, 127, -6, -7, -8, -9, -10, -11, 127, 127, 127, 127, 127, 127, -5, -6, -7, -8, -9, -10, 127, 127, 127, 127, 127, 127, -4, -5, -6, -7, -8, -9, 127, 127, 127, 127, 127, 127, -3, -4, -5, -6, -7, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input042_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy84
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param164
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input142_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy85
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param165
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_43 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_43", get_test_model_all_inputs_as_internal_43());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_44() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input043
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input143
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param43
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output043
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_44 = TestModelManager::get().add("sub_quant8_signed_44", get_test_model_44());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_44() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input043
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input143
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param43
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output043
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input043_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy86
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param166
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input143_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy87
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param167
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_44 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_44", get_test_model_all_inputs_as_internal_44());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_45() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input044
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input144
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param44
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output044
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_45 = TestModelManager::get().add("sub_quant8_signed_45", get_test_model_45());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_45() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input044
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input144
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param44
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output044
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input044_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy88
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param168
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input144_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy89
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param169
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_45 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_45", get_test_model_all_inputs_as_internal_45());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_46() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input045
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input145
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param45
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output045
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_46 = TestModelManager::get().add("sub_quant8_signed_46", get_test_model_46());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_46() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input045
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input145
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param45
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output045
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input045_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy90
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param170
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input145_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy91
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param171
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_46 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_46", get_test_model_all_inputs_as_internal_46());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_47() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input046
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input146
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param46
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output046
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_47 = TestModelManager::get().add("sub_quant8_signed_47", get_test_model_47());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_47() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input046
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input146
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param46
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output046
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input046_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy92
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param172
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input146_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy93
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param173
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_47 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_47", get_test_model_all_inputs_as_internal_47());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_48() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input047
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input147
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param47
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output047
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_48 = TestModelManager::get().add("sub_quant8_signed_48", get_test_model_48());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_48() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input047
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input147
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param47
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output047
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128, 112, 111, 110, 109, 108, 107, -128, -128, -128, -128, -128, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input047_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy94
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param174
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input147_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy95
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param175
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_48 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_48", get_test_model_all_inputs_as_internal_48());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_49() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input048
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input148
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param48
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output048
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_49 = TestModelManager::get().add("sub_quant8_signed_49", get_test_model_49());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_49() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input048
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input148
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param48
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output048
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input048_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy96
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param176
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input148_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy97
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param177
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_49 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_49", get_test_model_all_inputs_as_internal_49());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_50() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input049
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input149
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param49
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output049
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_50 = TestModelManager::get().add("sub_quant8_signed_50", get_test_model_50());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_50() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input049
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input149
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param49
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output049
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input049_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy98
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param178
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input149_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy99
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param179
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_50 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_50", get_test_model_all_inputs_as_internal_50());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_51() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input050
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input150
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param50
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output050
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_51 = TestModelManager::get().add("sub_quant8_signed_51", get_test_model_51());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_51() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input050
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input150
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param50
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output050
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input050_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy100
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param180
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input150_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy101
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param181
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_51 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_51", get_test_model_all_inputs_as_internal_51());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_52() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input051
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input151
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param51
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output051
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -128, -126, -126, -126, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -125, -125, -125, -126, -128, -128, -128, -128, -128, -128, -124, -124, -124, -124, -124, -124, -128, -128, -128, -128, -128, -128, -123, -123, -123, -123, -123, -124, -128, -128, -128, -128, -128, -128, 122, 122, 122, 122, 122, 122, 97, 97, 97, 97, 97, 96, 123, 123, 123, 123, 123, 122, 98, 98, 98, 98, 98, 98, 124, 124, 124, 124, 124, 124, 99, 99, 99, 99, 99, 98, 125, 125, 125, 125, 125, 124, 100, 100, 100, 100, 100, 100, 126, 126, 126, 126, 126, 126, 101, 101, 101, 101, 101, 100, 127, 127, 127, 127, 127, 126, 102, 102, 102, 102, 102, 102}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_52 = TestModelManager::get().add("sub_quant8_signed_52", get_test_model_52());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_52() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input051
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input151
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param51
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output051
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -128, -126, -126, -126, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -125, -125, -125, -126, -128, -128, -128, -128, -128, -128, -124, -124, -124, -124, -124, -124, -128, -128, -128, -128, -128, -128, -123, -123, -123, -123, -123, -124, -128, -128, -128, -128, -128, -128, 122, 122, 122, 122, 122, 122, 97, 97, 97, 97, 97, 96, 123, 123, 123, 123, 123, 122, 98, 98, 98, 98, 98, 98, 124, 124, 124, 124, 124, 124, 99, 99, 99, 99, 99, 98, 125, 125, 125, 125, 125, 124, 100, 100, 100, 100, 100, 100, 126, 126, 126, 126, 126, 126, 101, 101, 101, 101, 101, 100, 127, 127, 127, 127, 127, 126, 102, 102, 102, 102, 102, 102}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input051_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy102
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param182
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input151_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy103
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param183
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_52 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_52", get_test_model_all_inputs_as_internal_52());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_53() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input052
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input152
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param52
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output052
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_53 = TestModelManager::get().add("sub_quant8_signed_53", get_test_model_53());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_53() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input052
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input152
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param52
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output052
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input052_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy104
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param184
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input152_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy105
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param185
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_53 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_53", get_test_model_all_inputs_as_internal_53());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_54() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input053
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input153
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param53
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output053
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_54 = TestModelManager::get().add("sub_quant8_signed_54", get_test_model_54());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_54() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input053
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input153
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param53
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output053
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input053_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy106
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param186
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input153_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy107
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param187
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_54 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_54", get_test_model_all_inputs_as_internal_54());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_55() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input054
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input154
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param54
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output054
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_55 = TestModelManager::get().add("sub_quant8_signed_55", get_test_model_55());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_55() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input054
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input154
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param54
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output054
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input054_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy108
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param188
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input154_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy109
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param189
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_55 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_55", get_test_model_all_inputs_as_internal_55());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_56() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input055
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input155
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param55
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output055
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -126, -126, -126, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -125, -125, -125, -125, -128, -128, -128, -128, -128, -128, -124, -124, -124, -124, -124, -124, -128, -128, -128, -128, -128, -128, -123, -123, -123, -123, -123, -123, -128, -128, -128, -128, -128, -128, 122, 122, 122, 122, 122, 122, 97, 97, 97, 97, 97, 97, 123, 123, 123, 123, 123, 123, 98, 98, 98, 98, 98, 98, 124, 124, 124, 124, 124, 124, 99, 99, 99, 99, 99, 99, 125, 125, 125, 125, 125, 125, 100, 100, 100, 100, 100, 100, 126, 126, 126, 126, 126, 126, 101, 101, 101, 101, 101, 101, 127, 127, 127, 127, 127, 127, 102, 102, 102, 102, 102, 102}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_56 = TestModelManager::get().add("sub_quant8_signed_56", get_test_model_56());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_56() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input055
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input155
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param55
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output055
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -128, -128, -128, -128, -128, -128, -126, -126, -126, -126, -126, -126, -128, -128, -128, -128, -128, -128, -125, -125, -125, -125, -125, -125, -128, -128, -128, -128, -128, -128, -124, -124, -124, -124, -124, -124, -128, -128, -128, -128, -128, -128, -123, -123, -123, -123, -123, -123, -128, -128, -128, -128, -128, -128, 122, 122, 122, 122, 122, 122, 97, 97, 97, 97, 97, 97, 123, 123, 123, 123, 123, 123, 98, 98, 98, 98, 98, 98, 124, 124, 124, 124, 124, 124, 99, 99, 99, 99, 99, 99, 125, 125, 125, 125, 125, 125, 100, 100, 100, 100, 100, 100, 126, 126, 126, 126, 126, 126, 101, 101, 101, 101, 101, 101, 127, 127, 127, 127, 127, 127, 102, 102, 102, 102, 102, 102}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input055_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy110
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param190
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input155_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // dummy111
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // param191
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_56 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_56", get_test_model_all_inputs_as_internal_56());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_57() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input056
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input156
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param56
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output056
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_57 = TestModelManager::get().add("sub_quant8_signed_57", get_test_model_57());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_57() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input056
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input156
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param56
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output056
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input056_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy112
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param192
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input156_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy113
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param193
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_57 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_57", get_test_model_all_inputs_as_internal_57());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_58() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input057
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input157
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param57
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output057
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_58 = TestModelManager::get().add("sub_quant8_signed_58", get_test_model_58());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_58() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input057
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input157
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param57
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output057
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input057_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy114
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param194
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input157_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy115
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param195
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_58 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_58", get_test_model_all_inputs_as_internal_58());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_59() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input058
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input158
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param58
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output058
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_59 = TestModelManager::get().add("sub_quant8_signed_59", get_test_model_59());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_59() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input058
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input158
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param58
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output058
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input058_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy116
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param196
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input158_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy117
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param197
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_59 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_59", get_test_model_all_inputs_as_internal_59());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_60() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input059
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input159
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param59
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output059
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_60 = TestModelManager::get().add("sub_quant8_signed_60", get_test_model_60());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_60() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input059
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input159
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param59
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output059
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input059_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy118
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param198
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input159_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy119
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param199
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_60 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_60", get_test_model_all_inputs_as_internal_60());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_61() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input060
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input160
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param60
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output060
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -118, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -108, -118, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -98, -108, -118, -128, -128, -128, -128, -128, -128, -128, -128, -128, -88, -98, -108, -118, -128, -128, -128, -128, -128, -128, -128, -128, -78, -88, -98, -108, -118, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -118, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -108, -118, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -98, -108, -118, -128, -128, -128, 127, 127, 127, 127, 127, 127, -88, -98, -108, -118, -128, -128, 127, 127, 127, 127, 127, 127, -78, -88, -98, -108, -118, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_61 = TestModelManager::get().add("sub_quant8_signed_61", get_test_model_61());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_61() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input060
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input160
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param60
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output060
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -118, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -108, -118, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -98, -108, -118, -128, -128, -128, -128, -128, -128, -128, -128, -128, -88, -98, -108, -118, -128, -128, -128, -128, -128, -128, -128, -128, -78, -88, -98, -108, -118, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -118, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -108, -118, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -98, -108, -118, -128, -128, -128, 127, 127, 127, 127, 127, 127, -88, -98, -108, -118, -128, -128, 127, 127, 127, 127, 127, 127, -78, -88, -98, -108, -118, -128}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input060_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy120
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param200
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input160_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy121
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param201
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_61 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_61", get_test_model_all_inputs_as_internal_61());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_62() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input061
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input161
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param61
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output061
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -117, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -107, -117, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -97, -107, -117, -127, -128, -128, -128, -128, -128, -128, -128, -128, -87, -97, -107, -117, -127, -128, -128, -128, -128, -128, -128, -128, -77, -87, -97, -107, -117, -127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -127, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -117, -127, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -107, -117, -127, -128, -128, -128, 127, 127, 127, 127, 127, 127, -97, -107, -117, -127, -128, -128, 127, 127, 127, 127, 127, 127, -87, -97, -107, -117, -127, -128, 127, 127, 127, 127, 127, 127, -77, -87, -97, -107, -117, -127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_62 = TestModelManager::get().add("sub_quant8_signed_62", get_test_model_62());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_62() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input061
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input161
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param61
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output061
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -117, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -107, -117, -127, -128, -128, -128, -128, -128, -128, -128, -128, -128, -97, -107, -117, -127, -128, -128, -128, -128, -128, -128, -128, -128, -87, -97, -107, -117, -127, -128, -128, -128, -128, -128, -128, -128, -77, -87, -97, -107, -117, -127, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -127, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -117, -127, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -107, -117, -127, -128, -128, -128, 127, 127, 127, 127, 127, 127, -97, -107, -117, -127, -128, -128, 127, 127, 127, 127, 127, 127, -87, -97, -107, -117, -127, -128, 127, 127, 127, 127, 127, 127, -77, -87, -97, -107, -117, -127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -127
                        }, { // input061_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy122
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param202
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input161_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy123
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param203
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_62 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_62", get_test_model_all_inputs_as_internal_62());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_63() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input062
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input162
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param62
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output062
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, -8, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, -8, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, -8, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, -8, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, -8, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -8, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, -8, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, -8, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, -8, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, -8, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_63 = TestModelManager::get().add("sub_quant8_signed_63", get_test_model_63());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_63() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input062
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input162
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param62
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output062
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, -8, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, -8, -128, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, -8, -128, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, -8, -128, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, -8, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -8, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, -8, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, -8, -128, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, -8, -128, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, -8, -128, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input062_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy124
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param204
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input162_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy125
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param205
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_63 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_63", get_test_model_all_inputs_as_internal_63());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_64() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input063
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input163
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param63
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output063
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -9, -10, -11, -12, -13, -128, -128, -128, -128, -128, -128, -7, -8, -9, -10, -11, -12, -128, -128, -128, -128, -128, -128, -6, -7, -8, -9, -10, -11, -128, -128, -128, -128, -128, -128, -5, -6, -7, -8, -9, -10, -128, -128, -128, -128, -128, -128, -4, -5, -6, -7, -8, -9, -128, -128, -128, -128, -128, -128, -3, -4, -5, -6, -7, -8, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -8, -9, -10, -11, -12, -13, 127, 127, 127, 127, 127, 127, -7, -8, -9, -10, -11, -12, 127, 127, 127, 127, 127, 127, -6, -7, -8, -9, -10, -11, 127, 127, 127, 127, 127, 127, -5, -6, -7, -8, -9, -10, 127, 127, 127, 127, 127, 127, -4, -5, -6, -7, -8, -9, 127, 127, 127, 127, 127, 127, -3, -4, -5, -6, -7, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_64 = TestModelManager::get().add("sub_quant8_signed_64", get_test_model_64());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_all_inputs_as_internal_64() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {4, 7},
                .operands = {{ // input063
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input163
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param63
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output063
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8, -9, -10, -11, -12, -13, -128, -128, -128, -128, -128, -128, -7, -8, -9, -10, -11, -12, -128, -128, -128, -128, -128, -128, -6, -7, -8, -9, -10, -11, -128, -128, -128, -128, -128, -128, -5, -6, -7, -8, -9, -10, -128, -128, -128, -128, -128, -128, -4, -5, -6, -7, -8, -9, -128, -128, -128, -128, -128, -128, -3, -4, -5, -6, -7, -8, -128, -128, -128, -128, -128, -128, 127, 127, 127, 127, 127, 127, -8, -9, -10, -11, -12, -13, 127, 127, 127, 127, 127, 127, -7, -8, -9, -10, -11, -12, 127, 127, 127, 127, 127, 127, -6, -7, -8, -9, -10, -11, 127, 127, 127, 127, 127, 127, -5, -6, -7, -8, -9, -10, 127, 127, 127, 127, 127, 127, -4, -5, -6, -7, -8, -9, 127, 127, 127, 127, 127, 127, -3, -4, -5, -6, -7, -8}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // input063_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, -123, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy126
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param206
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input163_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127, -128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {144},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // dummy127
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-8}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 10.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -8
                        }, { // param207
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_64 = TestModelManager::get().add("sub_quant8_signed_all_inputs_as_internal_64", get_test_model_all_inputs_as_internal_64());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_quant8() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // quant8
                .inputIndexes = {0, 1},
                .operands = {{ // input064
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-28, 72}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input164
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -126, -125, -124}),
                            .dimensions = {2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param64
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output064
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-29, 70, -31, 68}),
                            .dimensions = {2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8 = TestModelManager::get().add("sub_quant8_signed_quant8", get_test_model_quant8());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_quant8_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // quant8
                .inputIndexes = {4, 7},
                .operands = {{ // input064
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input164
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param64
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output064
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-29, 70, -31, 68}),
                            .dimensions = {2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input064_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-28, 72}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy128
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param208
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input164_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -126, -125, -124}),
                            .dimensions = {2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy129
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param209
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_all_inputs_as_internal = TestModelManager::get().add("sub_quant8_signed_quant8_all_inputs_as_internal", get_test_model_quant8_all_inputs_as_internal());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_quant8_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // quant8
                .inputIndexes = {0, 1},
                .operands = {{ // input065
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, -122, -121, -120, -119, -118, -117, -116, -115, -114, -113, -112, -111, -110, -109, -108, -107, -106, -105, -104, -103, -102, -101, -100, -99, -98, -97, -96, -95, -94, -93, -92, -91, -90, -89, -88, -87, -86, -85, -84, -83, -82, -81, -80, -79, -78, -77, -76, -75, -74, -73, -72, -71, -70, -69, -68, -67, -66, -65, -64, -63, -62, -61, -60, -59, -58, -57, -56, -55, -54, -53, -52, -51, -50, -49, -48, -47, -46, -45, -44, -43, -42, -41, -40, -39, -38, -37, -36, -35, -34, -33, -32, -31, -30, -29, -28, -27, -26, -25, -24, -23, -22, -21, -20, -19, -18, -17, -16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {2, 4, 16, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input165
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -40, 4, 105, 34, -89, 57, 109, 110, 31, 36, -52, -69, 16, -31, -34, 86, 68, 85, 93, -63, -12, -79, 94, 96, -65, -77, -10, 29, -22, -75, -83, 63, -70, 125, -57, 20, 126, 3, -88, -85, -71, -115, 0, 50, -98, -82, 98, 55, -61, 115, -84, -122, 64, 44, -99, -96, 82, -46, 42, 14, -109, 103, -1, 33, 18, 40, 67, -23, -59, 121, 118, -102, 23, 87, 62, -36, 117, -42, -124, -16, -19, -117, -78, -29, -32, 48, -11, -33, 116, 70, 49, -41, 41, -60, 25, 101, -123, -18, -39, 90, 9, -116, -121, -24, -74, -9, -107, -27, 27, -100, 83, -5, -94, -35, -126, 38, 102, -20, -86, 81, -53, 59, -114, -50, -87, 123, 112, 61, -13, 7, 124, 108, -68, 74, -58, 6, -28, 46, -119, -90, -95, -106, -111, -7, 73, -120, 111, 54, -81, 39, 51, 19, 45, -30, 24, 88, 75, -55, 22, 37, 95, 78, 10, 60, 71, -97, -54, 77, 114, -101, -3, 120, -47, -108, 127, -14, 11, -92, -67, -72, 17, -80, -112, 97, -45, 91, -66, -43, -2, 80, -128, 32, 43, 53, -26, 56, -105, -125, 12, -113, 122, 5, -15, 113, 13, -76, 35, 28, -48, -17, -38, 92, 15, -8, -44, 47, 89, -110, 58, -103, -49, -91, 26, 79, 52, 8, -64, 76, 30, -104, 65, 106, -56, -93, 1, -73, 104, 100, 21, -37, -6, -51, 84, 72, 107, -25, -4, 2, 119, -62, -118, -21, 99, 66, 69}),
                            .dimensions = {2, 4, 16, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param65
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output065
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -99, -128, -128, -113, -128, -128, -128, -128, -128, -121, -82, -128, -128, -102, -104, -128, -128, -128, -128, -86, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -82, -128, -128, -128, -128, -128, -128, -53, -128, -128, -57, -95, -128, -128, -128, -128, -128, -128, -128, -128, -123, -128, -102, -128, -128, -36, -128, -118, -128, -128, -38, -32, -128, -77, -128, -42, -121, -128, -46, -128, -128, -49, -107, -15, -128, -128, -118, -51, -128, -82, -128, -19, -82, -44, -128, -128, -128, -114, -128, -128, -128, -55, -128, -63, -126, -91, -128, 2, -26, -20, -8, -2, -105, -128, 10, -128, -128, -26, -128, -128, -123, -128, -72, -125, -128, -128, -43, -119, -128, -128, -128, -103, -128, -128, 7, -35, -128, -128, 15, -82, -128, -36, 26, -128, -66, -90, 14, -10, -4, -92, 6, 39, -128, -26, -128, -3, -25, -65, -128, 63, -96, -106, -115, -35, -116, 46, 67, -69, 57, -128, -59, -38, -128, -64, 26, -84, -76, 1, -29, -7, -128, -58, -34, 3, -87, -128, 72, -95, 67, 14, 57, -59, -111, -83, -38, 35, -104, -57, 78, -90, -128, 33, 71, -22, 53, -123, -118, -38, 21, -9, 37, -97, -84, -118, 15, -5, -10, -126, 56, 113, 17, -102, -68, -70}),
                            .dimensions = {2, 4, 16, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_2 = TestModelManager::get().add("sub_quant8_signed_quant8_2", get_test_model_quant8_2());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_quant8_all_inputs_as_internal_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // quant8
                .inputIndexes = {4, 7},
                .operands = {{ // input065
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 4, 16, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input165
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 4, 16, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param65
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output065
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -99, -128, -128, -113, -128, -128, -128, -128, -128, -121, -82, -128, -128, -102, -104, -128, -128, -128, -128, -86, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -82, -128, -128, -128, -128, -128, -128, -53, -128, -128, -57, -95, -128, -128, -128, -128, -128, -128, -128, -128, -123, -128, -102, -128, -128, -36, -128, -118, -128, -128, -38, -32, -128, -77, -128, -42, -121, -128, -46, -128, -128, -49, -107, -15, -128, -128, -118, -51, -128, -82, -128, -19, -82, -44, -128, -128, -128, -114, -128, -128, -128, -55, -128, -63, -126, -91, -128, 2, -26, -20, -8, -2, -105, -128, 10, -128, -128, -26, -128, -128, -123, -128, -72, -125, -128, -128, -43, -119, -128, -128, -128, -103, -128, -128, 7, -35, -128, -128, 15, -82, -128, -36, 26, -128, -66, -90, 14, -10, -4, -92, 6, 39, -128, -26, -128, -3, -25, -65, -128, 63, -96, -106, -115, -35, -116, 46, 67, -69, 57, -128, -59, -38, -128, -64, 26, -84, -76, 1, -29, -7, -128, -58, -34, 3, -87, -128, 72, -95, 67, 14, 57, -59, -111, -83, -38, 35, -104, -57, 78, -90, -128, 33, 71, -22, 53, -123, -118, -38, 21, -9, 37, -97, -84, -118, 15, -5, -10, -126, 56, 113, 17, -102, -68, -70}),
                            .dimensions = {2, 4, 16, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input065_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, -122, -121, -120, -119, -118, -117, -116, -115, -114, -113, -112, -111, -110, -109, -108, -107, -106, -105, -104, -103, -102, -101, -100, -99, -98, -97, -96, -95, -94, -93, -92, -91, -90, -89, -88, -87, -86, -85, -84, -83, -82, -81, -80, -79, -78, -77, -76, -75, -74, -73, -72, -71, -70, -69, -68, -67, -66, -65, -64, -63, -62, -61, -60, -59, -58, -57, -56, -55, -54, -53, -52, -51, -50, -49, -48, -47, -46, -45, -44, -43, -42, -41, -40, -39, -38, -37, -36, -35, -34, -33, -32, -31, -30, -29, -28, -27, -26, -25, -24, -23, -22, -21, -20, -19, -18, -17, -16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {2, 4, 16, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy130
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param210
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // input165_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-127, -40, 4, 105, 34, -89, 57, 109, 110, 31, 36, -52, -69, 16, -31, -34, 86, 68, 85, 93, -63, -12, -79, 94, 96, -65, -77, -10, 29, -22, -75, -83, 63, -70, 125, -57, 20, 126, 3, -88, -85, -71, -115, 0, 50, -98, -82, 98, 55, -61, 115, -84, -122, 64, 44, -99, -96, 82, -46, 42, 14, -109, 103, -1, 33, 18, 40, 67, -23, -59, 121, 118, -102, 23, 87, 62, -36, 117, -42, -124, -16, -19, -117, -78, -29, -32, 48, -11, -33, 116, 70, 49, -41, 41, -60, 25, 101, -123, -18, -39, 90, 9, -116, -121, -24, -74, -9, -107, -27, 27, -100, 83, -5, -94, -35, -126, 38, 102, -20, -86, 81, -53, 59, -114, -50, -87, 123, 112, 61, -13, 7, 124, 108, -68, 74, -58, 6, -28, 46, -119, -90, -95, -106, -111, -7, 73, -120, 111, 54, -81, 39, 51, 19, 45, -30, 24, 88, 75, -55, 22, 37, 95, 78, 10, 60, 71, -97, -54, 77, 114, -101, -3, 120, -47, -108, 127, -14, 11, -92, -67, -72, 17, -80, -112, 97, -45, 91, -66, -43, -2, 80, -128, 32, 43, 53, -26, 56, -105, -125, 12, -113, 122, 5, -15, 113, 13, -76, 35, 28, -48, -17, -38, 92, 15, -8, -44, 47, 89, -110, 58, -103, -49, -91, 26, 79, 52, 8, -64, 76, 30, -104, 65, 106, -56, -93, 1, -73, 104, 100, 21, -37, -6, -51, 84, 72, 107, -25, -4, 2, 119, -62, -118, -21, 99, 66, 69}),
                            .dimensions = {2, 4, 16, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // dummy131
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // param211
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
                            .inputs = {4, 5, 6},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {7, 8, 9},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_all_inputs_as_internal_2 = TestModelManager::get().add("sub_quant8_signed_quant8_all_inputs_as_internal_2", get_test_model_quant8_all_inputs_as_internal_2());

}  // namespace generated_tests::sub_quant8_signed

namespace generated_tests::sub_quant8_signed {

const TestModel& get_test_model_zero_sized_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // zero_sized
                .inputIndexes = {13},
                .operands = {{ // scores
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({9, 1}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // roi
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({8, 8, 80, 80, 0, 0, 80, 80}),
                            .dimensions = {1, 8},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // param66
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param67
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param68
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param69
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param70
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param71
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param72
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
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {0},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // roiOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({}),
                            .dimensions = {0, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // classesOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({}),
                            .dimensions = {0},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({}),
                            .dimensions = {0},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // in
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({10, 20}),
                            .dimensions = {1, 1, 1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // param73
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param74
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param75
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({2.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param76
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({2.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param77
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({4}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param78
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({4}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
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
                        }, { // featureMap
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {0, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({10, 20, 30, 40}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // param79
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // out
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {0, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }, {
                            .inputs = {13, 10, 12, 14, 15, 16, 17, 18, 19, 20},
                            .outputs = {21},
                            .type = TestOperationType::ROI_ALIGN
                        }, {
                            .inputs = {21, 22, 23},
                            .outputs = {24},
                            .type = TestOperationType::SUB
                        }},
                .outputIndexes = {9, 11, 24}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_zero_sized_quant8_signed = TestModelManager::get().add("sub_quant8_signed_zero_sized_quant8_signed", get_test_model_zero_sized_quant8_signed());

}  // namespace generated_tests::sub_quant8_signed

