// Generated from embedding_lookup.mod.py
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;

namespace generated_tests::embedding_lookup {

const TestModel& get_test_model() {
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
                            .data = TestBuffer::createFromVector<float>({0.0f, 0.01f, 0.02f, 0.03f, 0.1f, 0.11f, 0.12000000000000001f, 0.13f, 1.0f, 1.01f, 1.02f, 1.03f, 1.1f, 1.11f, 1.12f, 1.1300000000000001f, 2.0f, 2.01f, 2.02f, 2.03f, 2.1f, 2.11f, 2.12f, 2.13f}),
                            .dimensions = {3, 2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 1.01f, 1.02f, 1.03f, 1.1f, 1.11f, 1.12f, 1.13f, 0.0f, 0.01f, 0.02f, 0.03f, 0.1f, 0.11f, 0.12f, 0.13f, 2.0f, 2.01f, 2.02f, 2.03f, 2.1f, 2.11f, 2.12f, 2.13f}),
                            .dimensions = {3, 2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::EMBEDDING_LOOKUP
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_0,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model = TestModelManager::get().add("embedding_lookup", get_test_model());

}  // namespace generated_tests::embedding_lookup

namespace generated_tests::embedding_lookup {

const TestModel& get_test_model_all_inputs_as_internal() {
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
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {3, 2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 1.01f, 1.02f, 1.03f, 1.1f, 1.11f, 1.12f, 1.13f, 0.0f, 0.01f, 0.02f, 0.03f, 0.1f, 0.11f, 0.12f, 0.13f, 2.0f, 2.01f, 2.02f, 2.03f, 2.1f, 2.11f, 2.12f, 2.13f}),
                            .dimensions = {3, 2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // value_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f, 0.01f, 0.02f, 0.03f, 0.1f, 0.11f, 0.12000000000000001f, 0.13f, 1.0f, 1.01f, 1.02f, 1.03f, 1.1f, 1.11f, 1.12f, 1.1300000000000001f, 2.0f, 2.01f, 2.02f, 2.03f, 2.1f, 2.11f, 2.12f, 2.13f}),
                            .dimensions = {3, 2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
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
        .minSupportedVersion = TestHalVersion::V1_0,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal = TestModelManager::get().add("embedding_lookup_all_inputs_as_internal", get_test_model_all_inputs_as_internal());

}  // namespace generated_tests::embedding_lookup

