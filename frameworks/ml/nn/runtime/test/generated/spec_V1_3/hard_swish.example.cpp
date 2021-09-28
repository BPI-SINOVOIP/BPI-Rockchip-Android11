// Generated from hard_swish.mod.py
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;

namespace generated_tests::hard_swish {

const TestModel& get_test_model_simple() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.53125f, 3.90625f, 3.046875f, -8.59375f, -1.328125f, 1.328125f, 0.0f, -8.515625f, -8.984375f, -0.234375f, 0.859375f, 9.84375f, -0.15625f, -8.515625f, 8.671875f, 4.609375f, 9.21875f, -1.796875f, 1.171875f, 9.375f, -8.75f, 2.421875f, -8.125f, -1.09375f, -9.609375f, -1.015625f, -9.84375f, 2.578125f, 4.921875f, -5.078125f, 5.0f, -0.859375f, 1.953125f, -6.640625f, -7.8125f, 4.453125f, -4.453125f, -6.875f, 0.78125f, 0.859375f}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.53125f, 3.90625f, 3.046875f, 0.0f, -0.3700765f, 0.9580485f, 0.0f, 0.0f, 0.0f, -0.1080322f, 0.5527751f, 9.84375f, -0.074056f, 0.0f, 8.671875f, 4.609375f, 9.21875f, -0.3603109f, 0.8148193f, 9.375f, 0.0f, 2.1885173f, 0.0f, -0.3474935f, 0.0f, -0.3358968f, 0.0f, 2.3968506f, 4.921875f, 0.0f, 5.0f, -0.3065999f, 1.6123454f, 0.0f, 0.0f, 4.453125f, 0.0f, 0.0f, 0.4923503f, 0.5527751f}),
                            .dimensions = {40},
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
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_simple = TestModelManager::get().add("hard_swish_simple", get_test_model_simple());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_simple_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.53125f, 3.90625f, 3.046875f, 0.0f, -0.3700765f, 0.9580485f, 0.0f, 0.0f, 0.0f, -0.1080322f, 0.5527751f, 9.84375f, -0.074056f, 0.0f, 8.671875f, 4.609375f, 9.21875f, -0.3603109f, 0.8148193f, 9.375f, 0.0f, 2.1885173f, 0.0f, -0.3474935f, 0.0f, -0.3358968f, 0.0f, 2.3968506f, 4.921875f, 0.0f, 5.0f, -0.3065999f, 1.6123454f, 0.0f, 0.0f, 4.453125f, 0.0f, 0.0f, 0.4923503f, 0.5527751f}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input0_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.53125f, 3.90625f, 3.046875f, -8.59375f, -1.328125f, 1.328125f, 0.0f, -8.515625f, -8.984375f, -0.234375f, 0.859375f, 9.84375f, -0.15625f, -8.515625f, 8.671875f, 4.609375f, 9.21875f, -1.796875f, 1.171875f, 9.375f, -8.75f, 2.421875f, -8.125f, -1.09375f, -9.609375f, -1.015625f, -9.84375f, 2.578125f, 4.921875f, -5.078125f, 5.0f, -0.859375f, 1.953125f, -6.640625f, -7.8125f, 4.453125f, -4.453125f, -6.875f, 0.78125f, 0.859375f}),
                            .dimensions = {40},
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
                            .inputs = {2, 3, 4},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_simple_all_inputs_as_internal = TestModelManager::get().add("hard_swish_simple_all_inputs_as_internal", get_test_model_simple_all_inputs_as_internal());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_simple_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({4.53125f, 3.90625f, 3.046875f, -8.59375f, -1.328125f, 1.328125f, 0.0f, -8.515625f, -8.984375f, -0.234375f, 0.859375f, 9.84375f, -0.15625f, -8.515625f, 8.671875f, 4.609375f, 9.21875f, -1.796875f, 1.171875f, 9.375f, -8.75f, 2.421875f, -8.125f, -1.09375f, -9.609375f, -1.015625f, -9.84375f, 2.578125f, 4.921875f, -5.078125f, 5.0f, -0.859375f, 1.953125f, -6.640625f, -7.8125f, 4.453125f, -4.453125f, -6.875f, 0.78125f, 0.859375f}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({4.53125f, 3.90625f, 3.046875f, 0.0f, -0.3700765073299408f, 0.9580485224723816f, 0.0f, 0.0f, 0.0f, -0.10803219676017761f, 0.5527750849723816f, 9.84375f, -0.0740559995174408f, 0.0f, 8.671875f, 4.609375f, 9.21875f, -0.3603109121322632f, 0.8148192763328552f, 9.375f, 0.0f, 2.1885173320770264f, 0.0f, -0.3474934995174408f, 0.0f, -0.3358967900276184f, 0.0f, 2.3968505859375f, 4.921875f, 0.0f, 5.0f, -0.306599885225296f, 1.6123454570770264f, 0.0f, 0.0f, 4.453125f, 0.0f, 0.0f, 0.492350310087204f, 0.5527750849723816f}),
                            .dimensions = {40},
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
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_simple_float16 = TestModelManager::get().add("hard_swish_simple_float16", get_test_model_simple_float16());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_simple_float16_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({4.53125f, 3.90625f, 3.046875f, 0.0f, -0.3700765073299408f, 0.9580485224723816f, 0.0f, 0.0f, 0.0f, -0.10803219676017761f, 0.5527750849723816f, 9.84375f, -0.0740559995174408f, 0.0f, 8.671875f, 4.609375f, 9.21875f, -0.3603109121322632f, 0.8148192763328552f, 9.375f, 0.0f, 2.1885173320770264f, 0.0f, -0.3474934995174408f, 0.0f, -0.3358967900276184f, 0.0f, 2.3968505859375f, 4.921875f, 0.0f, 5.0f, -0.306599885225296f, 1.6123454570770264f, 0.0f, 0.0f, 4.453125f, 0.0f, 0.0f, 0.492350310087204f, 0.5527750849723816f}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // input0_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({4.53125f, 3.90625f, 3.046875f, -8.59375f, -1.328125f, 1.328125f, 0.0f, -8.515625f, -8.984375f, -0.234375f, 0.859375f, 9.84375f, -0.15625f, -8.515625f, 8.671875f, 4.609375f, 9.21875f, -1.796875f, 1.171875f, 9.375f, -8.75f, 2.421875f, -8.125f, -1.09375f, -9.609375f, -1.015625f, -9.84375f, 2.578125f, 4.921875f, -5.078125f, 5.0f, -0.859375f, 1.953125f, -6.640625f, -7.8125f, 4.453125f, -4.453125f, -6.875f, 0.78125f, 0.859375f}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // dummy1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
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
                            .inputs = {2, 3, 4},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_simple_float16_all_inputs_as_internal = TestModelManager::get().add("hard_swish_simple_float16_all_inputs_as_internal", get_test_model_simple_float16_all_inputs_as_internal());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_simple_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.53125f, 3.90625f, 3.046875f, -8.59375f, -1.328125f, 1.328125f, 0.0f, -8.515625f, -8.984375f, -0.234375f, 0.859375f, 9.84375f, -0.15625f, -8.515625f, 8.671875f, 4.609375f, 9.21875f, -1.796875f, 1.171875f, 9.375f, -8.75f, 2.421875f, -8.125f, -1.09375f, -9.609375f, -1.015625f, -9.84375f, 2.578125f, 4.921875f, -5.078125f, 5.0f, -0.859375f, 1.953125f, -6.640625f, -7.8125f, 4.453125f, -4.453125f, -6.875f, 0.78125f, 0.859375f}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.53125f, 3.90625f, 3.046875f, 0.0f, -0.3700765f, 0.9580485f, 0.0f, 0.0f, 0.0f, -0.1080322f, 0.5527751f, 9.84375f, -0.074056f, 0.0f, 8.671875f, 4.609375f, 9.21875f, -0.3603109f, 0.8148193f, 9.375f, 0.0f, 2.1885173f, 0.0f, -0.3474935f, 0.0f, -0.3358968f, 0.0f, 2.3968506f, 4.921875f, 0.0f, 5.0f, -0.3065999f, 1.6123454f, 0.0f, 0.0f, 4.453125f, 0.0f, 0.0f, 0.4923503f, 0.5527751f}),
                            .dimensions = {40},
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
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_simple_relaxed = TestModelManager::get().add("hard_swish_simple_relaxed", get_test_model_simple_relaxed());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_simple_relaxed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.53125f, 3.90625f, 3.046875f, 0.0f, -0.3700765f, 0.9580485f, 0.0f, 0.0f, 0.0f, -0.1080322f, 0.5527751f, 9.84375f, -0.074056f, 0.0f, 8.671875f, 4.609375f, 9.21875f, -0.3603109f, 0.8148193f, 9.375f, 0.0f, 2.1885173f, 0.0f, -0.3474935f, 0.0f, -0.3358968f, 0.0f, 2.3968506f, 4.921875f, 0.0f, 5.0f, -0.3065999f, 1.6123454f, 0.0f, 0.0f, 4.453125f, 0.0f, 0.0f, 0.4923503f, 0.5527751f}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input0_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.53125f, 3.90625f, 3.046875f, -8.59375f, -1.328125f, 1.328125f, 0.0f, -8.515625f, -8.984375f, -0.234375f, 0.859375f, 9.84375f, -0.15625f, -8.515625f, 8.671875f, 4.609375f, 9.21875f, -1.796875f, 1.171875f, 9.375f, -8.75f, 2.421875f, -8.125f, -1.09375f, -9.609375f, -1.015625f, -9.84375f, 2.578125f, 4.921875f, -5.078125f, 5.0f, -0.859375f, 1.953125f, -6.640625f, -7.8125f, 4.453125f, -4.453125f, -6.875f, 0.78125f, 0.859375f}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
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
                        }},
                .operations = {{
                            .inputs = {2, 3, 4},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_simple_relaxed_all_inputs_as_internal = TestModelManager::get().add("hard_swish_simple_relaxed_all_inputs_as_internal", get_test_model_simple_relaxed_all_inputs_as_internal());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_simple_quant8() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({186, 178, 167, 18, 111, 145, 128, 19, 13, 125, 139, 254, 126, 19, 239, 187, 246, 105, 143, 248, 16, 159, 24, 114, 5, 115, 2, 161, 191, 63, 192, 117, 153, 43, 28, 185, 71, 40, 138, 139}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({186, 178, 167, 128, 123, 140, 128, 128, 128, 127, 135, 254, 127, 128, 239, 187, 246, 123, 138, 248, 128, 156, 128, 124, 128, 124, 128, 159, 191, 128, 192, 124, 149, 128, 128, 185, 128, 128, 134, 135}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_simple_quant8 = TestModelManager::get().add("hard_swish_simple_quant8", get_test_model_simple_quant8());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_simple_quant8_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({186, 178, 167, 128, 123, 140, 128, 128, 128, 127, 135, 254, 127, 128, 239, 187, 246, 123, 138, 248, 128, 156, 128, 124, 128, 124, 128, 159, 191, 128, 192, 124, 149, 128, 128, 185, 128, 128, 134, 135}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // input0_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({186, 178, 167, 18, 111, 145, 128, 19, 13, 125, 139, 254, 126, 19, 239, 187, 246, 105, 143, 248, 16, 159, 24, 114, 5, 115, 2, 161, 191, 63, 192, 117, 153, 43, 28, 185, 71, 40, 138, 139}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                        }},
                .operations = {{
                            .inputs = {2, 3, 4},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_simple_quant8_all_inputs_as_internal = TestModelManager::get().add("hard_swish_simple_quant8_all_inputs_as_internal", get_test_model_simple_quant8_all_inputs_as_internal());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_simple_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({58, 50, 39, -110, -17, 17, 0, -109, -115, -3, 11, 126, -2, -109, 111, 59, 118, -23, 15, 120, -112, 31, -104, -14, -123, -13, -126, 33, 63, -65, 64, -11, 25, -85, -100, 57, -57, -88, 10, 11}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({58, 50, 39, 0, -5, 12, 0, 0, 0, -1, 7, 126, -1, 0, 111, 59, 118, -5, 10, 120, 0, 28, 0, -4, 0, -4, 0, 31, 63, 0, 64, -4, 21, 0, 0, 57, 0, 0, 6, 7}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_simple_quant8_signed = TestModelManager::get().add("hard_swish_simple_quant8_signed", get_test_model_simple_quant8_signed());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_simple_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({58, 50, 39, 0, -5, 12, 0, 0, 0, -1, 7, 126, -1, 0, 111, 59, 118, -5, 10, 120, 0, 28, 0, -4, 0, -4, 0, 31, 63, 0, 64, -4, 21, 0, 0, 57, 0, 0, 6, 7}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // input0_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({58, 50, 39, -110, -17, 17, 0, -109, -115, -3, 11, 126, -2, -109, 111, 59, 118, -23, 15, 120, -112, 31, -104, -14, -123, -13, -126, 33, 63, -65, 64, -11, 25, -85, -100, 57, -57, -88, 10, 11}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // dummy4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
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
                        }},
                .operations = {{
                            .inputs = {2, 3, 4},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_simple_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("hard_swish_simple_quant8_signed_all_inputs_as_internal", get_test_model_simple_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_simple_quant8_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({186, 178, 167, 18, 111, 145, 128, 19, 13, 125, 139, 254, 126, 19, 239, 187, 246, 105, 143, 248, 16, 159, 24, 114, 5, 115, 2, 161, 191, 63, 192, 117, 153, 43, 28, 185, 71, 40, 138, 139}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({145, 125, 98, 0, 0, 31, 0, 0, 0, 0, 18, 255, 0, 0, 255, 148, 255, 0, 26, 255, 0, 70, 0, 0, 0, 0, 0, 77, 158, 0, 160, 0, 52, 0, 0, 142, 0, 0, 16, 18}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.03125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_simple_quant8_2 = TestModelManager::get().add("hard_swish_simple_quant8_2", get_test_model_simple_quant8_2());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_simple_quant8_all_inputs_as_internal_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({145, 125, 98, 0, 0, 31, 0, 0, 0, 0, 18, 255, 0, 0, 255, 148, 255, 0, 26, 255, 0, 70, 0, 0, 0, 0, 0, 77, 158, 0, 160, 0, 52, 0, 0, 142, 0, 0, 16, 18}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.03125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // input0_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({186, 178, 167, 18, 111, 145, 128, 19, 13, 125, 139, 254, 126, 19, 239, 187, 246, 105, 143, 248, 16, 159, 24, 114, 5, 115, 2, 161, 191, 63, 192, 117, 153, 43, 28, 185, 71, 40, 138, 139}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                        }},
                .operations = {{
                            .inputs = {2, 3, 4},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_simple_quant8_all_inputs_as_internal_2 = TestModelManager::get().add("hard_swish_simple_quant8_all_inputs_as_internal_2", get_test_model_simple_quant8_all_inputs_as_internal_2());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_simple_quant8_signed_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({58, 50, 39, -110, -17, 17, 0, -109, -115, -3, 11, 126, -2, -109, 111, 59, 118, -23, 15, 120, -112, 31, -104, -14, -123, -13, -126, 33, 63, -65, 64, -11, 25, -85, -100, 57, -57, -88, 10, 11}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({17, -3, -30, -128, -128, -97, -128, -128, -128, -128, -110, 127, -128, -128, 127, 20, 127, -128, -102, 127, -128, -58, -128, -128, -128, -128, -128, -51, 30, -128, 32, -128, -76, -128, -128, 14, -128, -128, -112, -110}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.03125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_simple_quant8_signed_2 = TestModelManager::get().add("hard_swish_simple_quant8_signed_2", get_test_model_simple_quant8_signed_2());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_simple_quant8_signed_all_inputs_as_internal_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({17, -3, -30, -128, -128, -97, -128, -128, -128, -128, -110, 127, -128, -128, 127, 20, 127, -128, -102, 127, -128, -58, -128, -128, -128, -128, -128, -51, 30, -128, 32, -128, -76, -128, -128, 14, -128, -128, -112, -110}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.03125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }, { // input0_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({58, 50, 39, -110, -17, 17, 0, -109, -115, -3, 11, 126, -2, -109, 111, 59, 118, -23, 15, 120, -112, 31, -104, -14, -123, -13, -126, 33, 63, -65, 64, -11, 25, -85, -100, 57, -57, -88, 10, 11}),
                            .dimensions = {40},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // dummy6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
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
                        }},
                .operations = {{
                            .inputs = {2, 3, 4},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_simple_quant8_signed_all_inputs_as_internal_2 = TestModelManager::get().add("hard_swish_simple_quant8_signed_all_inputs_as_internal_2", get_test_model_simple_quant8_signed_all_inputs_as_internal_2());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_5d_input() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.53125f, 3.90625f, 3.046875f, -8.59375f, -1.328125f, 1.328125f, 0.0f, -8.515625f, -8.984375f, -0.234375f, 0.859375f, 9.84375f, -0.15625f, -8.515625f, 8.671875f, 4.609375f, 9.21875f, -1.796875f, 1.171875f, 9.375f, -8.75f, 2.421875f, -8.125f, -1.09375f, -9.609375f, -1.015625f, -9.84375f, 2.578125f, 4.921875f, -5.078125f, 5.0f, -0.859375f, 1.953125f, -6.640625f, -7.8125f, 4.453125f, -4.453125f, -6.875f, 0.78125f, 0.859375f}),
                            .dimensions = {1, 2, 2, 2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.53125f, 3.90625f, 3.046875f, 0.0f, -0.3700765f, 0.9580485f, 0.0f, 0.0f, 0.0f, -0.1080322f, 0.5527751f, 9.84375f, -0.074056f, 0.0f, 8.671875f, 4.609375f, 9.21875f, -0.3603109f, 0.8148193f, 9.375f, 0.0f, 2.1885173f, 0.0f, -0.3474935f, 0.0f, -0.3358968f, 0.0f, 2.3968506f, 4.921875f, 0.0f, 5.0f, -0.3065999f, 1.6123454f, 0.0f, 0.0f, 4.453125f, 0.0f, 0.0f, 0.4923503f, 0.5527751f}),
                            .dimensions = {1, 2, 2, 2, 5},
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
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_5d_input = TestModelManager::get().add("hard_swish_5d_input", get_test_model_5d_input());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_5d_input_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({4.53125f, 3.90625f, 3.046875f, -8.59375f, -1.328125f, 1.328125f, 0.0f, -8.515625f, -8.984375f, -0.234375f, 0.859375f, 9.84375f, -0.15625f, -8.515625f, 8.671875f, 4.609375f, 9.21875f, -1.796875f, 1.171875f, 9.375f, -8.75f, 2.421875f, -8.125f, -1.09375f, -9.609375f, -1.015625f, -9.84375f, 2.578125f, 4.921875f, -5.078125f, 5.0f, -0.859375f, 1.953125f, -6.640625f, -7.8125f, 4.453125f, -4.453125f, -6.875f, 0.78125f, 0.859375f}),
                            .dimensions = {1, 2, 2, 2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({4.53125f, 3.90625f, 3.046875f, 0.0f, -0.3700765073299408f, 0.9580485224723816f, 0.0f, 0.0f, 0.0f, -0.10803219676017761f, 0.5527750849723816f, 9.84375f, -0.0740559995174408f, 0.0f, 8.671875f, 4.609375f, 9.21875f, -0.3603109121322632f, 0.8148192763328552f, 9.375f, 0.0f, 2.1885173320770264f, 0.0f, -0.3474934995174408f, 0.0f, -0.3358967900276184f, 0.0f, 2.3968505859375f, 4.921875f, 0.0f, 5.0f, -0.306599885225296f, 1.6123454570770264f, 0.0f, 0.0f, 4.453125f, 0.0f, 0.0f, 0.492350310087204f, 0.5527750849723816f}),
                            .dimensions = {1, 2, 2, 2, 5},
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
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_5d_input_float16 = TestModelManager::get().add("hard_swish_5d_input_float16", get_test_model_5d_input_float16());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_5d_input_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.53125f, 3.90625f, 3.046875f, -8.59375f, -1.328125f, 1.328125f, 0.0f, -8.515625f, -8.984375f, -0.234375f, 0.859375f, 9.84375f, -0.15625f, -8.515625f, 8.671875f, 4.609375f, 9.21875f, -1.796875f, 1.171875f, 9.375f, -8.75f, 2.421875f, -8.125f, -1.09375f, -9.609375f, -1.015625f, -9.84375f, 2.578125f, 4.921875f, -5.078125f, 5.0f, -0.859375f, 1.953125f, -6.640625f, -7.8125f, 4.453125f, -4.453125f, -6.875f, 0.78125f, 0.859375f}),
                            .dimensions = {1, 2, 2, 2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({4.53125f, 3.90625f, 3.046875f, 0.0f, -0.3700765f, 0.9580485f, 0.0f, 0.0f, 0.0f, -0.1080322f, 0.5527751f, 9.84375f, -0.074056f, 0.0f, 8.671875f, 4.609375f, 9.21875f, -0.3603109f, 0.8148193f, 9.375f, 0.0f, 2.1885173f, 0.0f, -0.3474935f, 0.0f, -0.3358968f, 0.0f, 2.3968506f, 4.921875f, 0.0f, 5.0f, -0.3065999f, 1.6123454f, 0.0f, 0.0f, 4.453125f, 0.0f, 0.0f, 0.4923503f, 0.5527751f}),
                            .dimensions = {1, 2, 2, 2, 5},
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
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_5d_input_relaxed = TestModelManager::get().add("hard_swish_5d_input_relaxed", get_test_model_5d_input_relaxed());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_5d_input_quant8() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({186, 178, 167, 18, 111, 145, 128, 19, 13, 125, 139, 254, 126, 19, 239, 187, 246, 105, 143, 248, 16, 159, 24, 114, 5, 115, 2, 161, 191, 63, 192, 117, 153, 43, 28, 185, 71, 40, 138, 139}),
                            .dimensions = {1, 2, 2, 2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({186, 178, 167, 128, 123, 140, 128, 128, 128, 127, 135, 254, 127, 128, 239, 187, 246, 123, 138, 248, 128, 156, 128, 124, 128, 124, 128, 159, 191, 128, 192, 124, 149, 128, 128, 185, 128, 128, 134, 135}),
                            .dimensions = {1, 2, 2, 2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_5d_input_quant8 = TestModelManager::get().add("hard_swish_5d_input_quant8", get_test_model_5d_input_quant8());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_5d_input_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({58, 50, 39, -110, -17, 17, 0, -109, -115, -3, 11, 126, -2, -109, 111, 59, 118, -23, 15, 120, -112, 31, -104, -14, -123, -13, -126, 33, 63, -65, 64, -11, 25, -85, -100, 57, -57, -88, 10, 11}),
                            .dimensions = {1, 2, 2, 2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({58, 50, 39, 0, -5, 12, 0, 0, 0, -1, 7, 126, -1, 0, 111, 59, 118, -5, 10, 120, 0, 28, 0, -4, 0, -4, 0, 31, 63, 0, 64, -4, 21, 0, 0, 57, 0, 0, 6, 7}),
                            .dimensions = {1, 2, 2, 2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_5d_input_quant8_signed = TestModelManager::get().add("hard_swish_5d_input_quant8_signed", get_test_model_5d_input_quant8_signed());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_5d_input_quant8_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({186, 178, 167, 18, 111, 145, 128, 19, 13, 125, 139, 254, 126, 19, 239, 187, 246, 105, 143, 248, 16, 159, 24, 114, 5, 115, 2, 161, 191, 63, 192, 117, 153, 43, 28, 185, 71, 40, 138, 139}),
                            .dimensions = {1, 2, 2, 2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({145, 125, 98, 0, 0, 31, 0, 0, 0, 0, 18, 255, 0, 0, 255, 148, 255, 0, 26, 255, 0, 70, 0, 0, 0, 0, 0, 77, 158, 0, 160, 0, 52, 0, 0, 142, 0, 0, 16, 18}),
                            .dimensions = {1, 2, 2, 2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.03125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_5d_input_quant8_2 = TestModelManager::get().add("hard_swish_5d_input_quant8_2", get_test_model_5d_input_quant8_2());

}  // namespace generated_tests::hard_swish

namespace generated_tests::hard_swish {

const TestModel& get_test_model_5d_input_quant8_signed_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({58, 50, 39, -110, -17, 17, 0, -109, -115, -3, 11, 126, -2, -109, 111, 59, 118, -23, 15, 120, -112, 31, -104, -14, -123, -13, -126, 33, 63, -65, 64, -11, 25, -85, -100, 57, -57, -88, 10, 11}),
                            .dimensions = {1, 2, 2, 2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({17, -3, -30, -128, -128, -97, -128, -128, -128, -128, -110, 127, -128, -128, 127, 20, 127, -128, -102, 127, -128, -58, -128, -128, -128, -128, -128, -51, 30, -128, 32, -128, -76, -128, -128, 14, -128, -128, -112, -110}),
                            .dimensions = {1, 2, 2, 2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.03125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -128
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::HARD_SWISH
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_5d_input_quant8_signed_2 = TestModelManager::get().add("hard_swish_5d_input_quant8_signed_2", get_test_model_5d_input_quant8_signed_2());

}  // namespace generated_tests::hard_swish

