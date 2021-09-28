// Generated from transpose_quant8_1.mod.py
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;

namespace generated_tests::transpose_quant8_1 {

const TestModel& get_test_model() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119}),
                            .dimensions = {2, 3, 4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // perms
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2, 0, 1, 3}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 20, 21, 22, 23, 24, 40, 41, 42, 43, 44, 60, 61, 62, 63, 64, 80, 81, 82, 83, 84, 100, 101, 102, 103, 104, 5, 6, 7, 8, 9, 25, 26, 27, 28, 29, 45, 46, 47, 48, 49, 65, 66, 67, 68, 69, 85, 86, 87, 88, 89, 105, 106, 107, 108, 109, 10, 11, 12, 13, 14, 30, 31, 32, 33, 34, 50, 51, 52, 53, 54, 70, 71, 72, 73, 74, 90, 91, 92, 93, 94, 110, 111, 112, 113, 114, 15, 16, 17, 18, 19, 35, 36, 37, 38, 39, 55, 56, 57, 58, 59, 75, 76, 77, 78, 79, 95, 96, 97, 98, 99, 115, 116, 117, 118, 119}),
                            .dimensions = {4, 2, 3, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::TRANSPOSE
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_1,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model = TestModelManager::get().add("transpose_quant8_1", get_test_model());

}  // namespace generated_tests::transpose_quant8_1

namespace generated_tests::transpose_quant8_1 {

const TestModel& get_test_model_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // input
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {2, 3, 4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // perms
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2, 0, 1, 3}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 20, 21, 22, 23, 24, 40, 41, 42, 43, 44, 60, 61, 62, 63, 64, 80, 81, 82, 83, 84, 100, 101, 102, 103, 104, 5, 6, 7, 8, 9, 25, 26, 27, 28, 29, 45, 46, 47, 48, 49, 65, 66, 67, 68, 69, 85, 86, 87, 88, 89, 105, 106, 107, 108, 109, 10, 11, 12, 13, 14, 30, 31, 32, 33, 34, 50, 51, 52, 53, 54, 70, 71, 72, 73, 74, 90, 91, 92, 93, 94, 110, 111, 112, 113, 114, 15, 16, 17, 18, 19, 35, 36, 37, 38, 39, 55, 56, 57, 58, 59, 75, 76, 77, 78, 79, 95, 96, 97, 98, 99, 115, 116, 117, 118, 119}),
                            .dimensions = {4, 2, 3, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // input_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119}),
                            .dimensions = {2, 3, 4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // dummy
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
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
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::TRANSPOSE
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_1,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal = TestModelManager::get().add("transpose_quant8_1_all_inputs_as_internal", get_test_model_all_inputs_as_internal());

}  // namespace generated_tests::transpose_quant8_1

namespace generated_tests::transpose_quant8_1 {

const TestModel& get_test_model_all_tensors_as_inputs() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1},
                .operands = {{ // input
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119}),
                            .dimensions = {2, 3, 4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // perms
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2, 0, 1, 3}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 20, 21, 22, 23, 24, 40, 41, 42, 43, 44, 60, 61, 62, 63, 64, 80, 81, 82, 83, 84, 100, 101, 102, 103, 104, 5, 6, 7, 8, 9, 25, 26, 27, 28, 29, 45, 46, 47, 48, 49, 65, 66, 67, 68, 69, 85, 86, 87, 88, 89, 105, 106, 107, 108, 109, 10, 11, 12, 13, 14, 30, 31, 32, 33, 34, 50, 51, 52, 53, 54, 70, 71, 72, 73, 74, 90, 91, 92, 93, 94, 110, 111, 112, 113, 114, 15, 16, 17, 18, 19, 35, 36, 37, 38, 39, 55, 56, 57, 58, 59, 75, 76, 77, 78, 79, 95, 96, 97, 98, 99, 115, 116, 117, 118, 119}),
                            .dimensions = {4, 2, 3, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::TRANSPOSE
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_1,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_tensors_as_inputs = TestModelManager::get().add("transpose_quant8_1_all_tensors_as_inputs", get_test_model_all_tensors_as_inputs());

}  // namespace generated_tests::transpose_quant8_1

namespace generated_tests::transpose_quant8_1 {

const TestModel& get_test_model_all_tensors_as_inputs_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {1, 3},
                .operands = {{ // input
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {2, 3, 4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // perms
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2, 0, 1, 3}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 20, 21, 22, 23, 24, 40, 41, 42, 43, 44, 60, 61, 62, 63, 64, 80, 81, 82, 83, 84, 100, 101, 102, 103, 104, 5, 6, 7, 8, 9, 25, 26, 27, 28, 29, 45, 46, 47, 48, 49, 65, 66, 67, 68, 69, 85, 86, 87, 88, 89, 105, 106, 107, 108, 109, 10, 11, 12, 13, 14, 30, 31, 32, 33, 34, 50, 51, 52, 53, 54, 70, 71, 72, 73, 74, 90, 91, 92, 93, 94, 110, 111, 112, 113, 114, 15, 16, 17, 18, 19, 35, 36, 37, 38, 39, 55, 56, 57, 58, 59, 75, 76, 77, 78, 79, 95, 96, 97, 98, 99, 115, 116, 117, 118, 119}),
                            .dimensions = {4, 2, 3, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // input_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119}),
                            .dimensions = {2, 3, 4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // dummy1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
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
                        }},
                .operations = {{
                            .inputs = {3, 4, 5},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::TRANSPOSE
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_1,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_tensors_as_inputs_all_inputs_as_internal = TestModelManager::get().add("transpose_quant8_1_all_tensors_as_inputs_all_inputs_as_internal", get_test_model_all_tensors_as_inputs_all_inputs_as_internal());

}  // namespace generated_tests::transpose_quant8_1

