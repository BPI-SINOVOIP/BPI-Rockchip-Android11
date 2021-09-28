// Generated from embedding_lookup_v1_2.mod.py
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;

namespace generated_tests::embedding_lookup_v1_2 {

const TestModel& get_test_model_quant8() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // index
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 0, 2}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // value
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({127, 127, 127, 127, 127, 127, 127, 127, 129, 129, 129, 129, 129, 129, 129, 129, 131, 131, 131, 131, 131, 131, 131, 131}),
                            .dimensions = {3, 2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({129, 129, 129, 129, 129, 129, 129, 129, 127, 127, 127, 127, 127, 127, 127, 127, 131, 131, 131, 131, 131, 131, 131, 131}),
                            .dimensions = {3, 2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::EMBEDDING_LOOKUP
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8 = TestModelManager::get().add("embedding_lookup_v1_2_quant8", get_test_model_quant8());

}  // namespace generated_tests::embedding_lookup_v1_2

namespace generated_tests::embedding_lookup_v1_2 {

const TestModel& get_test_model_quant8_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 3},
                .operands = {{ // index
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 0, 2}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // value
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {3, 2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({129, 129, 129, 129, 129, 129, 129, 129, 127, 127, 127, 127, 127, 127, 127, 127, 131, 131, 131, 131, 131, 131, 131, 131}),
                            .dimensions = {3, 2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // value_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({127, 127, 127, 127, 127, 127, 127, 127, 129, 129, 129, 129, 129, 129, 129, 129, 131, 131, 131, 131, 131, 131, 131, 131}),
                            .dimensions = {3, 2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // dummy
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
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
                            .inputs = {3, 4, 5},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::EMBEDDING_LOOKUP
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_all_inputs_as_internal = TestModelManager::get().add("embedding_lookup_v1_2_quant8_all_inputs_as_internal", get_test_model_quant8_all_inputs_as_internal());

}  // namespace generated_tests::embedding_lookup_v1_2

namespace generated_tests::embedding_lookup_v1_2 {

const TestModel& get_test_model_int32() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // index
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 0, 2}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // value
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2}),
                            .dimensions = {3, 2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2}),
                            .dimensions = {3, 2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::EMBEDDING_LOOKUP
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_int32 = TestModelManager::get().add("embedding_lookup_v1_2_int32", get_test_model_int32());

}  // namespace generated_tests::embedding_lookup_v1_2

