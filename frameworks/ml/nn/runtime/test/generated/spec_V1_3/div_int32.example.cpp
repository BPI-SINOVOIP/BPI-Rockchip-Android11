// Generated from div_int32.mod.py
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;

namespace generated_tests::div_int32 {

const TestModel& get_test_model() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-6, -6, -6, -6, -6, -6, -5, -5, -5, -5, -5, -5, -4, -4, -4, -4, -4, -4, -3, -3, -3, -3, -3, -3, -2, -2, -2, -2, -2, -2, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9}),
                            .dimensions = {2, 2, 4, 6},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // input1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-3, -2, -1, 1, 2, 3, -3, -2, -1, 1, 2, 3, -3, -2, -1, 1, 2, 3, -3, -2, -1, 1, 2, 3, -3, -2, -1, 1, 2, 3, -3, -2, -1, 1, 2, 3, -3, -2, -1, 1, 2, 3, -3, -2, -1, 1, 2, 3, -3, -2, -1, 1, 2, 3, -3, -2, -1, 1, 2, 3, -3, -2, -1, 1, 2, 3, -3, -2, -1, 1, 2, 3, -3, -2, -1, 1, 2, 3, -3, -2, -1, 1, 2, 3, -3, -2, -1, 1, 2, 3, -3, -2, -1, 1, 2, 3}),
                            .dimensions = {2, 2, 4, 6},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
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
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2, 3, 6, -6, -3, -2, 1, 2, 5, -5, -3, -2, 1, 2, 4, -4, -2, -2, 1, 1, 3, -3, -2, -1, 0, 1, 2, -2, -1, -1, 0, 0, 1, -1, -1, -1, 0, 0, 0, 0, 0, 0, -1, -1, -1, 1, 0, 0, -1, -1, -2, 2, 1, 0, -1, -2, -3, 3, 1, 1, -2, -2, -4, 4, 2, 1, -2, -3, -5, 5, 2, 1, -2, -3, -6, 6, 3, 2, -3, -4, -7, 7, 3, 2, -3, -4, -8, 8, 4, 2, -3, -5, -9, 9, 4, 3}),
                            .dimensions = {2, 2, 4, 6},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::DIV
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model = TestModelManager::get().add("div_int32", get_test_model());

}  // namespace generated_tests::div_int32

namespace generated_tests::div_int32 {

const TestModel& get_test_model_by_zero() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // by_zero
                .inputIndexes = {0, 1},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // input11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
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
                        }, { // output1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {1},
                            .isIgnored = true,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2},
                            .outputs = {3},
                            .type = TestOperationType::DIV
                        }},
                .outputIndexes = {3}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_by_zero = TestModelManager::get().add("div_int32_by_zero", get_test_model_by_zero());

}  // namespace generated_tests::div_int32

