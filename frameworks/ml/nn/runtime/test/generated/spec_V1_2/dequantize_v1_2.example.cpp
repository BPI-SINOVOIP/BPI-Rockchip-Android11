// Generated from dequantize_v1_2.mod.py
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_1d_quant8_asymm() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 251, 252, 253, 254, 255}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-63.5f, -63.0f, -62.5f, -62.0f, -61.5f, 62.0f, 62.5f, 63.0f, 63.5f, 64.0f}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_0,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_1d_quant8_asymm = TestModelManager::get().add("dequantize_v1_2_1d_quant8_asymm", get_test_model_1d_quant8_asymm());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_1d_quant8_asymm_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-63.5f, -63.0f, -62.5f, -62.0f, -61.5f, 62.0f, 62.5f, 63.0f, 63.5f, 64.0f}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input0_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 251, 252, 253, 254, 255}),
                            .dimensions = {10},
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
                            .inputs = {2, 3, 4},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_0,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_1d_quant8_asymm_all_inputs_as_internal = TestModelManager::get().add("dequantize_v1_2_1d_quant8_asymm_all_inputs_as_internal", get_test_model_1d_quant8_asymm_all_inputs_as_internal());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_1d_quant8_asymm_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 251, 252, 253, 254, 255}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-63.5f, -63.0f, -62.5f, -62.0f, -61.5f, 62.0f, 62.5f, 63.0f, 63.5f, 64.0f}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_1d_quant8_asymm_relaxed = TestModelManager::get().add("dequantize_v1_2_1d_quant8_asymm_relaxed", get_test_model_1d_quant8_asymm_relaxed());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_1d_quant8_asymm_relaxed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-63.5f, -63.0f, -62.5f, -62.0f, -61.5f, 62.0f, 62.5f, 63.0f, 63.5f, 64.0f}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input0_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 251, 252, 253, 254, 255}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // dummy1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
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
                        }},
                .operations = {{
                            .inputs = {2, 3, 4},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_1d_quant8_asymm_relaxed_all_inputs_as_internal = TestModelManager::get().add("dequantize_v1_2_1d_quant8_asymm_relaxed_all_inputs_as_internal", get_test_model_1d_quant8_asymm_relaxed_all_inputs_as_internal());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_1d_quant8_asymm_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 251, 252, 253, 254, 255}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({-63.5f, -63.0f, -62.5f, -62.0f, -61.5f, 62.0f, 62.5f, 63.0f, 63.5f, 64.0f}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_1d_quant8_asymm_float16 = TestModelManager::get().add("dequantize_v1_2_1d_quant8_asymm_float16", get_test_model_1d_quant8_asymm_float16());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_1d_quant8_asymm_float16_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({-63.5f, -63.0f, -62.5f, -62.0f, -61.5f, 62.0f, 62.5f, 63.0f, 63.5f, 64.0f}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // input0_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 251, 252, 253, 254, 255}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // dummy2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
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
                            .inputs = {2, 3, 4},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_1d_quant8_asymm_float16_all_inputs_as_internal = TestModelManager::get().add("dequantize_v1_2_1d_quant8_asymm_float16_all_inputs_as_internal", get_test_model_1d_quant8_asymm_float16_all_inputs_as_internal());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_2d_quant8_asymm() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 251, 252, 253, 254, 255}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-63.5f, -63.0f, -62.5f, -62.0f, -61.5f, 62.0f, 62.5f, 63.0f, 63.5f, 64.0f}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_0,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_2d_quant8_asymm = TestModelManager::get().add("dequantize_v1_2_2d_quant8_asymm", get_test_model_2d_quant8_asymm());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_2d_quant8_asymm_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-63.5f, -63.0f, -62.5f, -62.0f, -61.5f, 62.0f, 62.5f, 63.0f, 63.5f, 64.0f}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input01_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 251, 252, 253, 254, 255}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // dummy3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
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
                        }},
                .operations = {{
                            .inputs = {2, 3, 4},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_0,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_2d_quant8_asymm_all_inputs_as_internal = TestModelManager::get().add("dequantize_v1_2_2d_quant8_asymm_all_inputs_as_internal", get_test_model_2d_quant8_asymm_all_inputs_as_internal());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_2d_quant8_asymm_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 251, 252, 253, 254, 255}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-63.5f, -63.0f, -62.5f, -62.0f, -61.5f, 62.0f, 62.5f, 63.0f, 63.5f, 64.0f}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_2d_quant8_asymm_relaxed = TestModelManager::get().add("dequantize_v1_2_2d_quant8_asymm_relaxed", get_test_model_2d_quant8_asymm_relaxed());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_2d_quant8_asymm_relaxed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-63.5f, -63.0f, -62.5f, -62.0f, -61.5f, 62.0f, 62.5f, 63.0f, 63.5f, 64.0f}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input01_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 251, 252, 253, 254, 255}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // dummy4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
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
                            .inputs = {2, 3, 4},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_2d_quant8_asymm_relaxed_all_inputs_as_internal = TestModelManager::get().add("dequantize_v1_2_2d_quant8_asymm_relaxed_all_inputs_as_internal", get_test_model_2d_quant8_asymm_relaxed_all_inputs_as_internal());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_2d_quant8_asymm_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 251, 252, 253, 254, 255}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({-63.5f, -63.0f, -62.5f, -62.0f, -61.5f, 62.0f, 62.5f, 63.0f, 63.5f, 64.0f}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_2d_quant8_asymm_float16 = TestModelManager::get().add("dequantize_v1_2_2d_quant8_asymm_float16", get_test_model_2d_quant8_asymm_float16());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_2d_quant8_asymm_float16_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({-63.5f, -63.0f, -62.5f, -62.0f, -61.5f, 62.0f, 62.5f, 63.0f, 63.5f, 64.0f}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // input01_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 1, 2, 3, 4, 251, 252, 253, 254, 255}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
                        }, { // dummy5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({127}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 127
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
                        }},
                .operations = {{
                            .inputs = {2, 3, 4},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_2d_quant8_asymm_float16_all_inputs_as_internal = TestModelManager::get().add("dequantize_v1_2_2d_quant8_asymm_float16_all_inputs_as_internal", get_test_model_2d_quant8_asymm_float16_all_inputs_as_internal());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_3d_quant8_symm() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, 124, 125, 126, 127}),
                            .dimensions = {2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // output02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-64.0f, -63.5f, -63.0f, -62.5f, 62.0f, 62.5f, 63.0f, 63.5f}),
                            .dimensions = {2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_3d_quant8_symm = TestModelManager::get().add("dequantize_v1_2_3d_quant8_symm", get_test_model_3d_quant8_symm());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_3d_quant8_symm_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, 124, 125, 126, 127}),
                            .dimensions = {2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // output02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-64.0f, -63.5f, -63.0f, -62.5f, 62.0f, 62.5f, 63.0f, 63.5f}),
                            .dimensions = {2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_3d_quant8_symm_relaxed = TestModelManager::get().add("dequantize_v1_2_3d_quant8_symm_relaxed", get_test_model_3d_quant8_symm_relaxed());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_3d_quant8_symm_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, 124, 125, 126, 127}),
                            .dimensions = {2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // output02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({-64.0f, -63.5f, -63.0f, -62.5f, 62.0f, 62.5f, 63.0f, 63.5f}),
                            .dimensions = {2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_3d_quant8_symm_float16 = TestModelManager::get().add("dequantize_v1_2_3d_quant8_symm_float16", get_test_model_3d_quant8_symm_float16());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_4d_quant8_symm() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, 124, 125, 126, 127}),
                            .dimensions = {2, 1, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // output03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-64.0f, -63.5f, -63.0f, -62.5f, 62.0f, 62.5f, 63.0f, 63.5f}),
                            .dimensions = {2, 1, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_4d_quant8_symm = TestModelManager::get().add("dequantize_v1_2_4d_quant8_symm", get_test_model_4d_quant8_symm());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_4d_quant8_symm_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, 124, 125, 126, 127}),
                            .dimensions = {2, 1, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // output03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-64.0f, -63.5f, -63.0f, -62.5f, 62.0f, 62.5f, 63.0f, 63.5f}),
                            .dimensions = {2, 1, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_4d_quant8_symm_relaxed = TestModelManager::get().add("dequantize_v1_2_4d_quant8_symm_relaxed", get_test_model_4d_quant8_symm_relaxed());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_4d_quant8_symm_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, 124, 125, 126, 127}),
                            .dimensions = {2, 1, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // output03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({-64.0f, -63.5f, -63.0f, -62.5f, 62.0f, 62.5f, 63.0f, 63.5f}),
                            .dimensions = {2, 1, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_4d_quant8_symm_float16 = TestModelManager::get().add("dequantize_v1_2_4d_quant8_symm_float16", get_test_model_4d_quant8_symm_float16());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_3d_per_channel_first_dim() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input04
                            .channelQuant = {
                                            .channelDim = 0,
                                            .scales = {2.0f, 0.5f}
                                        },
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, -122, -121, -120, -119, -118, -117, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {2, 3, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL,
                            .zeroPoint = 0
                        }, { // output04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-256.0f, -254.0f, -252.0f, -250.0f, -248.0f, -246.0f, -244.0f, -242.0f, -240.0f, -238.0f, -236.0f, -234.0f, 58.0f, 58.5f, 59.0f, 59.5f, 60.0f, 60.5f, 61.0f, 61.5f, 62.0f, 62.5f, 63.0f, 63.5f}),
                            .dimensions = {2, 3, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_3d_per_channel_first_dim = TestModelManager::get().add("dequantize_v1_2_3d_per_channel_first_dim", get_test_model_3d_per_channel_first_dim());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_3d_per_channel_first_dim_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input04
                            .channelQuant = {
                                            .channelDim = 0,
                                            .scales = {2.0f, 0.5f}
                                        },
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, -122, -121, -120, -119, -118, -117, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {2, 3, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL,
                            .zeroPoint = 0
                        }, { // output04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-256.0f, -254.0f, -252.0f, -250.0f, -248.0f, -246.0f, -244.0f, -242.0f, -240.0f, -238.0f, -236.0f, -234.0f, 58.0f, 58.5f, 59.0f, 59.5f, 60.0f, 60.5f, 61.0f, 61.5f, 62.0f, 62.5f, 63.0f, 63.5f}),
                            .dimensions = {2, 3, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_3d_per_channel_first_dim_relaxed = TestModelManager::get().add("dequantize_v1_2_3d_per_channel_first_dim_relaxed", get_test_model_3d_per_channel_first_dim_relaxed());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_3d_per_channel_first_dim_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input04
                            .channelQuant = {
                                            .channelDim = 0,
                                            .scales = {2.0f, 0.5f}
                                        },
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, -122, -121, -120, -119, -118, -117, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {2, 3, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL,
                            .zeroPoint = 0
                        }, { // output04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({-256.0f, -254.0f, -252.0f, -250.0f, -248.0f, -246.0f, -244.0f, -242.0f, -240.0f, -238.0f, -236.0f, -234.0f, 58.0f, 58.5f, 59.0f, 59.5f, 60.0f, 60.5f, 61.0f, 61.5f, 62.0f, 62.5f, 63.0f, 63.5f}),
                            .dimensions = {2, 3, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_3d_per_channel_first_dim_float16 = TestModelManager::get().add("dequantize_v1_2_3d_per_channel_first_dim_float16", get_test_model_3d_per_channel_first_dim_float16());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_3d_per_channel_second_dim() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input05
                            .channelQuant = {
                                            .channelDim = 1,
                                            .scales = {2.0f, 1.0f, 0.5f}
                                        },
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, -122, -121, -120, -119, -118, -117, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {2, 3, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL,
                            .zeroPoint = 0
                        }, { // output05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-256.0f, -254.0f, -252.0f, -250.0f, -124.0f, -123.0f, -122.0f, -121.0f, -60.0f, -59.5f, -59.0f, -58.5f, 232.0f, 234.0f, 236.0f, 238.0f, 120.0f, 121.0f, 122.0f, 123.0f, 62.0f, 62.5f, 63.0f, 63.5f}),
                            .dimensions = {2, 3, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_3d_per_channel_second_dim = TestModelManager::get().add("dequantize_v1_2_3d_per_channel_second_dim", get_test_model_3d_per_channel_second_dim());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_3d_per_channel_second_dim_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input05
                            .channelQuant = {
                                            .channelDim = 1,
                                            .scales = {2.0f, 1.0f, 0.5f}
                                        },
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, -122, -121, -120, -119, -118, -117, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {2, 3, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL,
                            .zeroPoint = 0
                        }, { // output05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-256.0f, -254.0f, -252.0f, -250.0f, -124.0f, -123.0f, -122.0f, -121.0f, -60.0f, -59.5f, -59.0f, -58.5f, 232.0f, 234.0f, 236.0f, 238.0f, 120.0f, 121.0f, 122.0f, 123.0f, 62.0f, 62.5f, 63.0f, 63.5f}),
                            .dimensions = {2, 3, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_3d_per_channel_second_dim_relaxed = TestModelManager::get().add("dequantize_v1_2_3d_per_channel_second_dim_relaxed", get_test_model_3d_per_channel_second_dim_relaxed());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_3d_per_channel_second_dim_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input05
                            .channelQuant = {
                                            .channelDim = 1,
                                            .scales = {2.0f, 1.0f, 0.5f}
                                        },
                            .data = TestBuffer::createFromVector<int8_t>({-128, -127, -126, -125, -124, -123, -122, -121, -120, -119, -118, -117, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127}),
                            .dimensions = {2, 3, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL,
                            .zeroPoint = 0
                        }, { // output05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({-256.0f, -254.0f, -252.0f, -250.0f, -124.0f, -123.0f, -122.0f, -121.0f, -60.0f, -59.5f, -59.0f, -58.5f, 232.0f, 234.0f, 236.0f, 238.0f, 120.0f, 121.0f, 122.0f, 123.0f, 62.0f, 62.5f, 63.0f, 63.5f}),
                            .dimensions = {2, 3, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_3d_per_channel_second_dim_float16 = TestModelManager::get().add("dequantize_v1_2_3d_per_channel_second_dim_float16", get_test_model_3d_per_channel_second_dim_float16());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 32, 128, 255}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f, 32.0f, 128.0f, 255.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model = TestModelManager::get().add("dequantize_v1_2", get_test_model());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f, 32.0f, 128.0f, 255.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0, 32, 128, 255}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // dummy6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
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
                            .inputs = {2, 3, 4},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal = TestModelManager::get().add("dequantize_v1_2_all_inputs_as_internal", get_test_model_all_inputs_as_internal());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_zero_sized() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // zero_sized
                .inputIndexes = {13},
                .operands = {{ // scores
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({137, 129}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
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
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
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
                        }, { // scoresOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {0},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                            .data = TestBuffer::createFromVector<uint8_t>({1}),
                            .dimensions = {1, 1, 1, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // param7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
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
                            .data = TestBuffer::createFromVector<float>({2.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({2.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({4}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param12
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
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {0, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // out
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {0, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
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
                            .inputs = {21},
                            .outputs = {22},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {9, 11, 22}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_zero_sized = TestModelManager::get().add("dequantize_v1_2_zero_sized", get_test_model_zero_sized());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_zero_sized_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = { // zero_sized
                .inputIndexes = {13},
                .operands = {{ // scores
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({137, 129}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
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
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
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
                        }, { // scoresOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {0},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                            .data = TestBuffer::createFromVector<uint8_t>({1}),
                            .dimensions = {1, 1, 1, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // param7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
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
                            .data = TestBuffer::createFromVector<float>({2.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({2.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({4}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param12
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
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {0, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // out
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {0, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
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
                            .inputs = {21},
                            .outputs = {22},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {9, 11, 22}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_zero_sized_relaxed = TestModelManager::get().add("dequantize_v1_2_zero_sized_relaxed", get_test_model_zero_sized_relaxed());

}  // namespace generated_tests::dequantize_v1_2

namespace generated_tests::dequantize_v1_2 {

const TestModel& get_test_model_zero_sized_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // zero_sized
                .inputIndexes = {13},
                .operands = {{ // scores
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({137, 129}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
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
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
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
                        }, { // scoresOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {0},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                            .data = TestBuffer::createFromVector<uint8_t>({1}),
                            .dimensions = {1, 1, 1, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // param7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
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
                            .data = TestBuffer::createFromVector<float>({2.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({2.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({4}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param12
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
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {0, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // out
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({}),
                            .dimensions = {0, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
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
                            .inputs = {21},
                            .outputs = {22},
                            .type = TestOperationType::DEQUANTIZE
                        }},
                .outputIndexes = {9, 11, 22}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_zero_sized_float16 = TestModelManager::get().add("dequantize_v1_2_zero_sized_float16", get_test_model_zero_sized_float16());

}  // namespace generated_tests::dequantize_v1_2

