// Generated from cast_identity.mod.py
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;

namespace generated_tests::cast_identity {

const TestModel& get_test_model_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}),
                            .dimensions = {2, 3},
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
                            .type = TestOperationType::CAST
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_float16 = TestModelManager::get().add("cast_identity_float16", get_test_model_float16());

}  // namespace generated_tests::cast_identity

namespace generated_tests::cast_identity {

const TestModel& get_test_model_float16_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // input0_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // dummy
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
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
                            .type = TestOperationType::CAST
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_float16_all_inputs_as_internal = TestModelManager::get().add("cast_identity_float16_all_inputs_as_internal", get_test_model_float16_all_inputs_as_internal());

}  // namespace generated_tests::cast_identity

namespace generated_tests::cast_identity {

const TestModel& get_test_model_float32() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}),
                            .dimensions = {2, 3},
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
                            .type = TestOperationType::CAST
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_float32 = TestModelManager::get().add("cast_identity_float32", get_test_model_float32());

}  // namespace generated_tests::cast_identity

namespace generated_tests::cast_identity {

const TestModel& get_test_model_float32_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input01_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
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
                            .type = TestOperationType::CAST
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_float32_all_inputs_as_internal = TestModelManager::get().add("cast_identity_float32_all_inputs_as_internal", get_test_model_float32_all_inputs_as_internal());

}  // namespace generated_tests::cast_identity

namespace generated_tests::cast_identity {

const TestModel& get_test_model_float32_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}),
                            .dimensions = {2, 3},
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
                            .type = TestOperationType::CAST
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_float32_relaxed = TestModelManager::get().add("cast_identity_float32_relaxed", get_test_model_float32_relaxed());

}  // namespace generated_tests::cast_identity

namespace generated_tests::cast_identity {

const TestModel& get_test_model_float32_relaxed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input01_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}),
                            .dimensions = {2, 3},
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
                            .type = TestOperationType::CAST
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_float32_relaxed_all_inputs_as_internal = TestModelManager::get().add("cast_identity_float32_relaxed_all_inputs_as_internal", get_test_model_float32_relaxed_all_inputs_as_internal());

}  // namespace generated_tests::cast_identity

namespace generated_tests::cast_identity {

const TestModel& get_test_model_int32() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 2, 3, 4, 5, 6}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // output02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 2, 3, 4, 5, 6}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::CAST
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_int32 = TestModelManager::get().add("cast_identity_int32", get_test_model_int32());

}  // namespace generated_tests::cast_identity

namespace generated_tests::cast_identity {

const TestModel& get_test_model_bool8() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true, false, true, false, false, true}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_BOOL8,
                            .zeroPoint = 0
                        }, { // output03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true, false, true, false, false, true}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_BOOL8,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::CAST
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_bool8 = TestModelManager::get().add("cast_identity_bool8", get_test_model_bool8());

}  // namespace generated_tests::cast_identity

namespace generated_tests::cast_identity {

const TestModel& get_test_model_quant8_asymm() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({1, 2, 3, 4, 5, 6}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 4.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // output04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({1, 2, 3, 4, 5, 6}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 4.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::CAST
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_asymm = TestModelManager::get().add("cast_identity_quant8_asymm", get_test_model_quant8_asymm());

}  // namespace generated_tests::cast_identity

namespace generated_tests::cast_identity {

const TestModel& get_test_model_quant8_asymm_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // input04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 4.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // output04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({1, 2, 3, 4, 5, 6}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 4.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // input04_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({1, 2, 3, 4, 5, 6}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 4.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 4.0f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
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
                            .type = TestOperationType::CAST
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_asymm_all_inputs_as_internal = TestModelManager::get().add("cast_identity_quant8_asymm_all_inputs_as_internal", get_test_model_quant8_asymm_all_inputs_as_internal());

}  // namespace generated_tests::cast_identity

namespace generated_tests::cast_identity {

const TestModel& get_test_model_quant8_symm() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({1, 2, 3, 4, 5, 6}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 4.0f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // output05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({1, 2, 3, 4, 5, 6}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 4.0f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::CAST
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_symm = TestModelManager::get().add("cast_identity_quant8_symm", get_test_model_quant8_symm());

}  // namespace generated_tests::cast_identity

namespace generated_tests::cast_identity {

const TestModel& get_test_model_quant16_asymm() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({1, 2, 3, 4, 5, 6}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 4.0f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 100
                        }, { // output06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({1, 2, 3, 4, 5, 6}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 4.0f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 100
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::CAST
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant16_asymm = TestModelManager::get().add("cast_identity_quant16_asymm", get_test_model_quant16_asymm());

}  // namespace generated_tests::cast_identity

namespace generated_tests::cast_identity {

const TestModel& get_test_model_quant16_symm() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input07
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({1, 2, 3, 4, 5, 6}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 4.0f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output07
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({1, 2, 3, 4, 5, 6}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 4.0f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::CAST
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant16_symm = TestModelManager::get().add("cast_identity_quant16_symm", get_test_model_quant16_symm());

}  // namespace generated_tests::cast_identity

