// Generated from split_quant8_3.mod.py
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;

namespace generated_tests::split_quant8_3 {

const TestModel& get_test_model() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({1, 2, 3, 4, 5, 6}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 2.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 3
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // num_splits
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({1, 4}),
                            .dimensions = {2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 2.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 3
                        }, { // output1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({2, 5}),
                            .dimensions = {2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 2.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 3
                        }, { // output2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({3, 6}),
                            .dimensions = {2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 2.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 3
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3, 4, 5},
                            .type = TestOperationType::SPLIT
                        }},
                .outputIndexes = {3, 4, 5}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model = TestModelManager::get().add("split_quant8_3", get_test_model());

}  // namespace generated_tests::split_quant8_3

namespace generated_tests::split_quant8_3 {

const TestModel& get_test_model_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {6},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 2.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 3
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // num_splits
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({1, 4}),
                            .dimensions = {2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 2.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 3
                        }, { // output1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({2, 5}),
                            .dimensions = {2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 2.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 3
                        }, { // output2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({3, 6}),
                            .dimensions = {2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 2.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 3
                        }, { // input0_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({1, 2, 3, 4, 5, 6}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 2.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 3
                        }, { // dummy
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({3}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 2.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 3
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
                        }},
                .operations = {{
                            .inputs = {6, 7, 8},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2},
                            .outputs = {3, 4, 5},
                            .type = TestOperationType::SPLIT
                        }},
                .outputIndexes = {3, 4, 5}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal = TestModelManager::get().add("split_quant8_3_all_inputs_as_internal", get_test_model_all_inputs_as_internal());

}  // namespace generated_tests::split_quant8_3

