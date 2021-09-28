// Generated from l2_normalization_quant8_signed.mod.py
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim4_axis0_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -16, 24, -6, 24, 104, 64, -66, -96, -36, -96, 24, 54, -6, 54, -56, -56, -96, -6, 104, 64, 24, 24}),
                            .dimensions = {3, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 102, 46, 61, 102, 82, 77, 0, 77, 0, 61, 77, 46, 77, 102, 102, 0, 77, 102, 82, 61, 61}),
                            .dimensions = {3, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim4_axis0_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim4_axis0_quant8_signed", get_test_model_dim4_axis0_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim4_axis0_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {3, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 102, 46, 61, 102, 82, 77, 0, 77, 0, 61, 77, 46, 77, 102, 102, 0, 77, 102, 82, 61, 61}),
                            .dimensions = {3, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -16, 24, -6, 24, 104, 64, -66, -96, -36, -96, 24, 54, -6, 54, -56, -56, -96, -6, 104, 64, 24, 24}),
                            .dimensions = {3, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim4_axis0_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim4_axis0_quant8_signed_all_inputs_as_internal", get_test_model_dim4_axis0_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim4_axis0_neg_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -16, 24, -6, 24, 104, 64, -66, -96, -36, -96, 24, 54, -6, 54, -56, -56, -96, -6, 104, 64, 24, 24}),
                            .dimensions = {3, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-4}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 102, 46, 61, 102, 82, 77, 0, 77, 0, 61, 77, 46, 77, 102, 102, 0, 77, 102, 82, 61, 61}),
                            .dimensions = {3, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim4_axis0_neg_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim4_axis0_neg_quant8_signed", get_test_model_dim4_axis0_neg_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim4_axis0_neg_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {3, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-4}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 102, 46, 61, 102, 82, 77, 0, 77, 0, 61, 77, 46, 77, 102, 102, 0, 77, 102, 82, 61, 61}),
                            .dimensions = {3, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -16, 24, -6, 24, 104, 64, -66, -96, -36, -96, 24, 54, -6, 54, -56, -56, -96, -6, 104, 64, 24, 24}),
                            .dimensions = {3, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim4_axis0_neg_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim4_axis0_neg_quant8_signed_all_inputs_as_internal", get_test_model_dim4_axis0_neg_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim4_axis1_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -16, 24, -66, -96, -36, -96, -56, -56, -96, -6, -6, 24, 104, 64, 24, 54, -6, 54, 104, 64, 24, 24}),
                            .dimensions = {2, 3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 102, 77, 0, 77, 0, 102, 102, 0, 77, 46, 61, 102, 82, 61, 77, 46, 77, 102, 82, 61, 61}),
                            .dimensions = {2, 3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim4_axis1_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim4_axis1_quant8_signed", get_test_model_dim4_axis1_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim4_axis1_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 102, 77, 0, 77, 0, 102, 102, 0, 77, 46, 61, 102, 82, 61, 77, 46, 77, 102, 82, 61, 61}),
                            .dimensions = {2, 3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -16, 24, -66, -96, -36, -96, -56, -56, -96, -6, -6, 24, 104, 64, 24, 54, -6, 54, 104, 64, 24, 24}),
                            .dimensions = {2, 3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                            .inputs = {3, 4, 5},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim4_axis1_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim4_axis1_quant8_signed_all_inputs_as_internal", get_test_model_dim4_axis1_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim4_axis1_neg_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -16, 24, -66, -96, -36, -96, -56, -56, -96, -6, -6, 24, 104, 64, 24, 54, -6, 54, 104, 64, 24, 24}),
                            .dimensions = {2, 3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 102, 77, 0, 77, 0, 102, 102, 0, 77, 46, 61, 102, 82, 61, 77, 46, 77, 102, 82, 61, 61}),
                            .dimensions = {2, 3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim4_axis1_neg_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim4_axis1_neg_quant8_signed", get_test_model_dim4_axis1_neg_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim4_axis1_neg_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 102, 77, 0, 77, 0, 102, 102, 0, 77, 46, 61, 102, 82, 61, 77, 46, 77, 102, 82, 61, 61}),
                            .dimensions = {2, 3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -16, 24, -66, -96, -36, -96, -56, -56, -96, -6, -6, 24, 104, 64, 24, 54, -6, 54, 104, 64, 24, 24}),
                            .dimensions = {2, 3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                            .inputs = {3, 4, 5},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim4_axis1_neg_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim4_axis1_neg_quant8_signed_all_inputs_as_internal", get_test_model_dim4_axis1_neg_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim4_axis2_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -66, -96, -56, -56, -16, 24, -36, -96, -96, -6, -6, 24, 24, 54, 104, 64, 104, 64, -6, 54, 24, 24}),
                            .dimensions = {2, 2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 77, 0, 102, 102, 102, 102, 77, 0, 0, 77, 46, 61, 61, 77, 102, 82, 102, 82, 46, 77, 61, 61}),
                            .dimensions = {2, 2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim4_axis2_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim4_axis2_quant8_signed", get_test_model_dim4_axis2_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim4_axis2_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 77, 0, 102, 102, 102, 102, 77, 0, 0, 77, 46, 61, 61, 77, 102, 82, 102, 82, 46, 77, 61, 61}),
                            .dimensions = {2, 2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -66, -96, -56, -56, -16, 24, -36, -96, -96, -6, -6, 24, 24, 54, 104, 64, 104, 64, -6, 54, 24, 24}),
                            .dimensions = {2, 2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                            .inputs = {3, 4, 5},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim4_axis2_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim4_axis2_quant8_signed_all_inputs_as_internal", get_test_model_dim4_axis2_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim4_axis2_neg_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -66, -96, -56, -56, -16, 24, -36, -96, -96, -6, -6, 24, 24, 54, 104, 64, 104, 64, -6, 54, 24, 24}),
                            .dimensions = {2, 2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 77, 0, 102, 102, 102, 102, 77, 0, 0, 77, 46, 61, 61, 77, 102, 82, 102, 82, 46, 77, 61, 61}),
                            .dimensions = {2, 2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim4_axis2_neg_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim4_axis2_neg_quant8_signed", get_test_model_dim4_axis2_neg_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim4_axis2_neg_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 77, 0, 102, 102, 102, 102, 77, 0, 0, 77, 46, 61, 61, 77, 102, 82, 102, 82, 46, 77, 61, 61}),
                            .dimensions = {2, 2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -66, -96, -56, -56, -16, 24, -36, -96, -96, -6, -6, 24, 24, 54, 104, 64, 104, 64, -6, 54, 24, 24}),
                            .dimensions = {2, 2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                            .inputs = {3, 4, 5},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim4_axis2_neg_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim4_axis2_neg_quant8_signed_all_inputs_as_internal", get_test_model_dim4_axis2_neg_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim4_axis3_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56, -66, -96, -56, -16, -36, -96, 24, -96, -6, -6, 24, 104, 24, 54, 64, 104, -6, 24, 64, 54, 24}),
                            .dimensions = {2, 2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 77, 0, 102, 102, 77, 0, 102, 0, 77, 46, 61, 102, 61, 77, 82, 102, 46, 61, 82, 77, 61}),
                            .dimensions = {2, 2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim4_axis3_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim4_axis3_quant8_signed", get_test_model_dim4_axis3_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim4_axis3_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 77, 0, 102, 102, 77, 0, 102, 0, 77, 46, 61, 102, 61, 77, 82, 102, 46, 61, 82, 77, 61}),
                            .dimensions = {2, 2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56, -66, -96, -56, -16, -36, -96, 24, -96, -6, -6, 24, 104, 24, 54, 64, 104, -6, 24, 64, 54, 24}),
                            .dimensions = {2, 2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                            .inputs = {3, 4, 5},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim4_axis3_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim4_axis3_quant8_signed_all_inputs_as_internal", get_test_model_dim4_axis3_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim4_axis3_neg_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56, -66, -96, -56, -16, -36, -96, 24, -96, -6, -6, 24, 104, 24, 54, 64, 104, -6, 24, 64, 54, 24}),
                            .dimensions = {2, 2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 77, 0, 102, 102, 77, 0, 102, 0, 77, 46, 61, 102, 61, 77, 82, 102, 46, 61, 82, 77, 61}),
                            .dimensions = {2, 2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim4_axis3_neg_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim4_axis3_neg_quant8_signed", get_test_model_dim4_axis3_neg_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim4_axis3_neg_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 77, 0, 102, 102, 77, 0, 102, 0, 77, 46, 61, 102, 61, 77, 82, 102, 46, 61, 82, 77, 61}),
                            .dimensions = {2, 2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56, -66, -96, -56, -16, -36, -96, 24, -96, -6, -6, 24, 104, 24, 54, 64, 104, -6, 24, 64, 54, 24}),
                            .dimensions = {2, 2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                        }},
                .operations = {{
                            .inputs = {3, 4, 5},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim4_axis3_neg_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim4_axis3_neg_quant8_signed_all_inputs_as_internal", get_test_model_dim4_axis3_neg_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim3_axis0_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -16, 24, -66, -96, -36, -96, -56, -56, -96, -6}),
                            .dimensions = {3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 102, 77, 0, 77, 0, 102, 102, 0, 77}),
                            .dimensions = {3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim3_axis0_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim3_axis0_quant8_signed", get_test_model_dim3_axis0_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim3_axis0_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 102, 77, 0, 77, 0, 102, 102, 0, 77}),
                            .dimensions = {3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -16, 24, -66, -96, -36, -96, -56, -56, -96, -6}),
                            .dimensions = {3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy8
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                        }},
                .operations = {{
                            .inputs = {3, 4, 5},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim3_axis0_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim3_axis0_quant8_signed_all_inputs_as_internal", get_test_model_dim3_axis0_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim3_axis0_neg_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -16, 24, -66, -96, -36, -96, -56, -56, -96, -6}),
                            .dimensions = {3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 102, 77, 0, 77, 0, 102, 102, 0, 77}),
                            .dimensions = {3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim3_axis0_neg_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim3_axis0_neg_quant8_signed", get_test_model_dim3_axis0_neg_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim3_axis0_neg_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 102, 77, 0, 77, 0, 102, 102, 0, 77}),
                            .dimensions = {3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -16, 24, -66, -96, -36, -96, -56, -56, -96, -6}),
                            .dimensions = {3, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy9
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                        }},
                .operations = {{
                            .inputs = {3, 4, 5},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim3_axis0_neg_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim3_axis0_neg_quant8_signed_all_inputs_as_internal", get_test_model_dim3_axis0_neg_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim3_axis1_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -66, -96, -56, -56, -16, 24, -36, -96, -96, -6}),
                            .dimensions = {2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 77, 0, 102, 102, 102, 102, 77, 0, 0, 77}),
                            .dimensions = {2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim3_axis1_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim3_axis1_quant8_signed", get_test_model_dim3_axis1_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim3_axis1_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 77, 0, 102, 102, 102, 102, 77, 0, 0, 77}),
                            .dimensions = {2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -66, -96, -56, -56, -16, 24, -36, -96, -96, -6}),
                            .dimensions = {2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                        }},
                .operations = {{
                            .inputs = {3, 4, 5},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim3_axis1_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim3_axis1_quant8_signed_all_inputs_as_internal", get_test_model_dim3_axis1_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim3_axis1_neg_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -66, -96, -56, -56, -16, 24, -36, -96, -96, -6}),
                            .dimensions = {2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 77, 0, 102, 102, 102, 102, 77, 0, 0, 77}),
                            .dimensions = {2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim3_axis1_neg_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim3_axis1_neg_quant8_signed", get_test_model_dim3_axis1_neg_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim3_axis1_neg_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 77, 0, 102, 102, 102, 102, 77, 0, 0, 77}),
                            .dimensions = {2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -66, -96, -56, -56, -16, 24, -36, -96, -96, -6}),
                            .dimensions = {2, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                        }},
                .operations = {{
                            .inputs = {3, 4, 5},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim3_axis1_neg_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim3_axis1_neg_quant8_signed_all_inputs_as_internal", get_test_model_dim3_axis1_neg_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim3_axis2_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56, -66, -96, -56, -16, -36, -96, 24, -96, -6}),
                            .dimensions = {2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 77, 0, 102, 102, 77, 0, 102, 0, 77}),
                            .dimensions = {2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim3_axis2_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim3_axis2_quant8_signed", get_test_model_dim3_axis2_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim3_axis2_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 77, 0, 102, 102, 77, 0, 102, 0, 77}),
                            .dimensions = {2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56, -66, -96, -56, -16, -36, -96, 24, -96, -6}),
                            .dimensions = {2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy12
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                        }},
                .operations = {{
                            .inputs = {3, 4, 5},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim3_axis2_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim3_axis2_quant8_signed_all_inputs_as_internal", get_test_model_dim3_axis2_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim3_axis2_neg_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56, -66, -96, -56, -16, -36, -96, 24, -96, -6}),
                            .dimensions = {2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 77, 0, 102, 102, 77, 0, 102, 0, 77}),
                            .dimensions = {2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim3_axis2_neg_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim3_axis2_neg_quant8_signed", get_test_model_dim3_axis2_neg_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim3_axis2_neg_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 77, 0, 102, 102, 77, 0, 102, 0, 77}),
                            .dimensions = {2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56, -66, -96, -56, -16, -36, -96, 24, -96, -6}),
                            .dimensions = {2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy13
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                            .inputs = {3, 4, 5},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim3_axis2_neg_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim3_axis2_neg_quant8_signed_all_inputs_as_internal", get_test_model_dim3_axis2_neg_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim2_axis0_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -66, -96, -56, -56}),
                            .dimensions = {3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 77, 0, 102, 102}),
                            .dimensions = {3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim2_axis0_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim2_axis0_quant8_signed", get_test_model_dim2_axis0_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim2_axis0_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 77, 0, 102, 102}),
                            .dimensions = {3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -66, -96, -56, -56}),
                            .dimensions = {3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy14
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                            .inputs = {3, 4, 5},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim2_axis0_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim2_axis0_quant8_signed_all_inputs_as_internal", get_test_model_dim2_axis0_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim2_axis0_neg_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -66, -96, -56, -56}),
                            .dimensions = {3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 77, 0, 102, 102}),
                            .dimensions = {3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim2_axis0_neg_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim2_axis0_neg_quant8_signed", get_test_model_dim2_axis0_neg_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim2_axis0_neg_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 77, 0, 102, 102}),
                            .dimensions = {3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -66, -96, -56, -56}),
                            .dimensions = {3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy15
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                            .inputs = {3, 4, 5},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim2_axis0_neg_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim2_axis0_neg_quant8_signed_all_inputs_as_internal", get_test_model_dim2_axis0_neg_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim2_axis1_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56, -66, -96, -56}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 77, 0, 102}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim2_axis1_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim2_axis1_quant8_signed", get_test_model_dim2_axis1_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim2_axis1_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 77, 0, 102}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56, -66, -96, -56}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy16
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                            .inputs = {3, 4, 5},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim2_axis1_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim2_axis1_quant8_signed_all_inputs_as_internal", get_test_model_dim2_axis1_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim2_axis1_neg_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56, -66, -96, -56}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 77, 0, 102}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim2_axis1_neg_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim2_axis1_neg_quant8_signed", get_test_model_dim2_axis1_neg_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim2_axis1_neg_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 77, 0, 102}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56, -66, -96, -56}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy17
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                            .inputs = {3, 4, 5},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim2_axis1_neg_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim2_axis1_neg_quant8_signed_all_inputs_as_internal", get_test_model_dim2_axis1_neg_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim1_axis0_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim1_axis0_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim1_axis0_quant8_signed", get_test_model_dim1_axis0_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim1_axis0_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy18
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                            .inputs = {3, 4, 5},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim1_axis0_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim1_axis0_quant8_signed_all_inputs_as_internal", get_test_model_dim1_axis0_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim1_axis0_neg_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim1_axis0_neg_quant8_signed = TestModelManager::get().add("l2_normalization_quant8_signed_dim1_axis0_neg_quant8_signed", get_test_model_dim1_axis0_neg_quant8_signed());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim1_axis0_neg_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {3},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // axis
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy19
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                            .inputs = {3, 4, 5},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim1_axis0_neg_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("l2_normalization_quant8_signed_dim1_axis0_neg_quant8_signed_all_inputs_as_internal", get_test_model_dim1_axis0_neg_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim4_axis3_quant8_signed_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56, -66, -96, -56, -16, -36, -96, 24, -96, -6, -6, 24, 104, 24, 54, 64, 104, -6, 24, 64, 54, 24}),
                            .dimensions = {2, 2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // op21
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 77, 0, 102, 102, 77, 0, 102, 0, 77, 46, 61, 102, 61, 77, 82, 102, 46, 61, 82, 77, 61}),
                            .dimensions = {2, 2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim4_axis3_quant8_signed_2 = TestModelManager::get().add("l2_normalization_quant8_signed_dim4_axis3_quant8_signed_2", get_test_model_dim4_axis3_quant8_signed_2());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim4_axis3_quant8_signed_all_inputs_as_internal_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // op11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // op21
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 77, 0, 102, 102, 77, 0, 102, 0, 77, 46, 61, 102, 61, 77, 82, 102, 46, 61, 82, 77, 61}),
                            .dimensions = {2, 2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op11_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56, -66, -96, -56, -16, -36, -96, 24, -96, -6, -6, 24, 104, 24, 54, 64, 104, -6, 24, 64, 54, 24}),
                            .dimensions = {2, 2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy20
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                        }},
                .operations = {{
                            .inputs = {2, 3, 4},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim4_axis3_quant8_signed_all_inputs_as_internal_2 = TestModelManager::get().add("l2_normalization_quant8_signed_dim4_axis3_quant8_signed_all_inputs_as_internal_2", get_test_model_dim4_axis3_quant8_signed_all_inputs_as_internal_2());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim3_axis2_quant8_signed_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56, -66, -96, -56, -16, -36, -96, 24, -96, -6}),
                            .dimensions = {2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // op21
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 77, 0, 102, 102, 77, 0, 102, 0, 77}),
                            .dimensions = {2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim3_axis2_quant8_signed_2 = TestModelManager::get().add("l2_normalization_quant8_signed_dim3_axis2_quant8_signed_2", get_test_model_dim3_axis2_quant8_signed_2());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim3_axis2_quant8_signed_all_inputs_as_internal_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // op11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // op21
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 77, 0, 102, 102, 77, 0, 102, 0, 77}),
                            .dimensions = {2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op11_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56, -66, -96, -56, -16, -36, -96, 24, -96, -6}),
                            .dimensions = {2, 2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy21
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                        }},
                .operations = {{
                            .inputs = {2, 3, 4},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim3_axis2_quant8_signed_all_inputs_as_internal_2 = TestModelManager::get().add("l2_normalization_quant8_signed_dim3_axis2_quant8_signed_all_inputs_as_internal_2", get_test_model_dim3_axis2_quant8_signed_all_inputs_as_internal_2());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim2_axis1_quant8_signed_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56, -66, -96, -56}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // op21
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 77, 0, 102}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim2_axis1_quant8_signed_2 = TestModelManager::get().add("l2_normalization_quant8_signed_dim2_axis1_quant8_signed_2", get_test_model_dim2_axis1_quant8_signed_2());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim2_axis1_quant8_signed_all_inputs_as_internal_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // op11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // op21
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102, 77, 0, 102}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op11_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56, -66, -96, -56}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy22
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                        }},
                .operations = {{
                            .inputs = {2, 3, 4},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim2_axis1_quant8_signed_all_inputs_as_internal_2 = TestModelManager::get().add("l2_normalization_quant8_signed_dim2_axis1_quant8_signed_all_inputs_as_internal_2", get_test_model_dim2_axis1_quant8_signed_all_inputs_as_internal_2());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim1_axis0_quant8_signed_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // op21
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim1_axis0_quant8_signed_2 = TestModelManager::get().add("l2_normalization_quant8_signed_dim1_axis0_quant8_signed_2", get_test_model_dim1_axis0_quant8_signed_2());

}  // namespace generated_tests::l2_normalization_quant8_signed

namespace generated_tests::l2_normalization_quant8_signed {

const TestModel& get_test_model_dim1_axis0_quant8_signed_all_inputs_as_internal_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2},
                .operands = {{ // op11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // op21
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 77, 102}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // op11_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96, -66, -56}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
                        }, { // dummy23
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-96}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.1f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = -96
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
                        }},
                .operations = {{
                            .inputs = {2, 3, 4},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0},
                            .outputs = {1},
                            .type = TestOperationType::L2_NORMALIZATION
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_dim1_axis0_quant8_signed_all_inputs_as_internal_2 = TestModelManager::get().add("l2_normalization_quant8_signed_dim1_axis0_quant8_signed_all_inputs_as_internal_2", get_test_model_dim1_axis0_quant8_signed_all_inputs_as_internal_2());

}  // namespace generated_tests::l2_normalization_quant8_signed

