// Generated from neg.mod.py
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;

namespace generated_tests::neg {

const TestModel& get_test_model() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-6.0f, -5.9f, -5.8f, -5.7f, -5.6f, -5.5f, -5.4f, -5.3f, -5.2f, -5.1f, -5.0f, -4.9f, -4.8f, -4.7f, -4.6f, -4.5f, -4.4f, -4.3f, -4.2f, -4.1f, -4.0f, -3.9f, -3.8f, -3.7f, -3.6f, -3.5f, -3.4f, -3.3f, -3.2f, -3.1f, -3.0f, -2.9f, -2.8f, -2.7f, -2.6f, -2.5f, -2.4f, -2.3f, -2.2f, -2.1f, -2.0f, -1.9f, -1.8f, -1.7f, -1.6f, -1.5f, -1.4f, -1.3f, -1.2f, -1.1f, -1.0f, -0.9f, -0.8f, -0.7f, -0.6f, -0.5f, -0.4f, -0.3f, -0.2f, -0.1f, 0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f, 1.7f, 1.8f, 1.9f, 2.0f, 2.1f, 2.2f, 2.3f, 2.4f, 2.5f, 2.6f, 2.7f, 2.8f, 2.9f, 3.0f, 3.1f, 3.2f, 3.3f, 3.4f, 3.5f, 3.6f, 3.7f, 3.8f, 3.9f, 4.0f, 4.1f, 4.2f, 4.3f, 4.4f, 4.5f, 4.6f, 4.7f, 4.8f, 4.9f, 5.0f, 5.1f, 5.2f, 5.3f, 5.4f, 5.5f, 5.6f, 5.7f, 5.8f, 5.9f}),
                            .dimensions = {1, 2, 3, 4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({6.0f, 5.9f, 5.8f, 5.7f, 5.6f, 5.5f, 5.4f, 5.3f, 5.2f, 5.1f, 5.0f, 4.9f, 4.8f, 4.7f, 4.6f, 4.5f, 4.4f, 4.3f, 4.2f, 4.1f, 4.0f, 3.9f, 3.8f, 3.7f, 3.6f, 3.5f, 3.4f, 3.3f, 3.2f, 3.1f, 3.0f, 2.9f, 2.8f, 2.7f, 2.6f, 2.5f, 2.4f, 2.3f, 2.2f, 2.1f, 2.0f, 1.9f, 1.8f, 1.7f, 1.6f, 1.5f, 1.4f, 1.3f, 1.2f, 1.1f, 1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f, -0.0f, -0.1f, -0.2f, -0.3f, -0.4f, -0.5f, -0.6f, -0.7f, -0.8f, -0.9f, -1.0f, -1.1f, -1.2f, -1.3f, -1.4f, -1.5f, -1.6f, -1.7f, -1.8f, -1.9f, -2.0f, -2.1f, -2.2f, -2.3f, -2.4f, -2.5f, -2.6f, -2.7f, -2.8f, -2.9f, -3.0f, -3.1f, -3.2f, -3.3f, -3.4f, -3.5f, -3.6f, -3.7f, -3.8f, -3.9f, -4.0f, -4.1f, -4.2f, -4.3f, -4.4f, -4.5f, -4.6f, -4.7f, -4.8f, -4.9f, -5.0f, -5.1f, -5.2f, -5.3f, -5.4f, -5.5f, -5.6f, -5.7f, -5.8f, -5.9f}),
                            .dimensions = {1, 2, 3, 4, 5},
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
                            .type = TestOperationType::NEG
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model = TestModelManager::get().add("neg", get_test_model());

}  // namespace generated_tests::neg

namespace generated_tests::neg {

const TestModel& get_test_model_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-6.0f, -5.9f, -5.8f, -5.7f, -5.6f, -5.5f, -5.4f, -5.3f, -5.2f, -5.1f, -5.0f, -4.9f, -4.8f, -4.7f, -4.6f, -4.5f, -4.4f, -4.3f, -4.2f, -4.1f, -4.0f, -3.9f, -3.8f, -3.7f, -3.6f, -3.5f, -3.4f, -3.3f, -3.2f, -3.1f, -3.0f, -2.9f, -2.8f, -2.7f, -2.6f, -2.5f, -2.4f, -2.3f, -2.2f, -2.1f, -2.0f, -1.9f, -1.8f, -1.7f, -1.6f, -1.5f, -1.4f, -1.3f, -1.2f, -1.1f, -1.0f, -0.9f, -0.8f, -0.7f, -0.6f, -0.5f, -0.4f, -0.3f, -0.2f, -0.1f, 0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f, 1.7f, 1.8f, 1.9f, 2.0f, 2.1f, 2.2f, 2.3f, 2.4f, 2.5f, 2.6f, 2.7f, 2.8f, 2.9f, 3.0f, 3.1f, 3.2f, 3.3f, 3.4f, 3.5f, 3.6f, 3.7f, 3.8f, 3.9f, 4.0f, 4.1f, 4.2f, 4.3f, 4.4f, 4.5f, 4.6f, 4.7f, 4.8f, 4.9f, 5.0f, 5.1f, 5.2f, 5.3f, 5.4f, 5.5f, 5.6f, 5.7f, 5.8f, 5.9f}),
                            .dimensions = {1, 2, 3, 4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({6.0f, 5.9f, 5.8f, 5.7f, 5.6f, 5.5f, 5.4f, 5.3f, 5.2f, 5.1f, 5.0f, 4.9f, 4.8f, 4.7f, 4.6f, 4.5f, 4.4f, 4.3f, 4.2f, 4.1f, 4.0f, 3.9f, 3.8f, 3.7f, 3.6f, 3.5f, 3.4f, 3.3f, 3.2f, 3.1f, 3.0f, 2.9f, 2.8f, 2.7f, 2.6f, 2.5f, 2.4f, 2.3f, 2.2f, 2.1f, 2.0f, 1.9f, 1.8f, 1.7f, 1.6f, 1.5f, 1.4f, 1.3f, 1.2f, 1.1f, 1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f, -0.0f, -0.1f, -0.2f, -0.3f, -0.4f, -0.5f, -0.6f, -0.7f, -0.8f, -0.9f, -1.0f, -1.1f, -1.2f, -1.3f, -1.4f, -1.5f, -1.6f, -1.7f, -1.8f, -1.9f, -2.0f, -2.1f, -2.2f, -2.3f, -2.4f, -2.5f, -2.6f, -2.7f, -2.8f, -2.9f, -3.0f, -3.1f, -3.2f, -3.3f, -3.4f, -3.5f, -3.6f, -3.7f, -3.8f, -3.9f, -4.0f, -4.1f, -4.2f, -4.3f, -4.4f, -4.5f, -4.6f, -4.7f, -4.8f, -4.9f, -5.0f, -5.1f, -5.2f, -5.3f, -5.4f, -5.5f, -5.6f, -5.7f, -5.8f, -5.9f}),
                            .dimensions = {1, 2, 3, 4, 5},
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
                            .type = TestOperationType::NEG
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_relaxed = TestModelManager::get().add("neg_relaxed", get_test_model_relaxed());

}  // namespace generated_tests::neg

namespace generated_tests::neg {

const TestModel& get_test_model_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({-6.0f, -5.900000095367432f, -5.800000190734863f, -5.699999809265137f, -5.599999904632568f, -5.5f, -5.400000095367432f, -5.300000190734863f, -5.199999809265137f, -5.099999904632568f, -5.0f, -4.900000095367432f, -4.800000190734863f, -4.699999809265137f, -4.599999904632568f, -4.5f, -4.400000095367432f, -4.300000190734863f, -4.199999809265137f, -4.099999904632568f, -4.0f, -3.9000000953674316f, -3.799999952316284f, -3.700000047683716f, -3.5999999046325684f, -3.5f, -3.4000000953674316f, -3.299999952316284f, -3.200000047683716f, -3.0999999046325684f, -3.0f, -2.9000000953674316f, -2.799999952316284f, -2.700000047683716f, -2.5999999046325684f, -2.5f, -2.4000000953674316f, -2.299999952316284f, -2.200000047683716f, -2.0999999046325684f, -2.0f, -1.899999976158142f, -1.7999999523162842f, -1.7000000476837158f, -1.600000023841858f, -1.5f, -1.399999976158142f, -1.2999999523162842f, -1.2000000476837158f, -1.100000023841858f, -1.0f, -0.8999999761581421f, -0.800000011920929f, -0.699999988079071f, -0.6000000238418579f, -0.5f, -0.4000000059604645f, -0.30000001192092896f, -0.20000000298023224f, -0.10000000149011612f, 0.0f, 0.10000000149011612f, 0.20000000298023224f, 0.30000001192092896f, 0.4000000059604645f, 0.5f, 0.6000000238418579f, 0.699999988079071f, 0.800000011920929f, 0.8999999761581421f, 1.0f, 1.100000023841858f, 1.2000000476837158f, 1.2999999523162842f, 1.399999976158142f, 1.5f, 1.600000023841858f, 1.7000000476837158f, 1.7999999523162842f, 1.899999976158142f, 2.0f, 2.0999999046325684f, 2.200000047683716f, 2.299999952316284f, 2.4000000953674316f, 2.5f, 2.5999999046325684f, 2.700000047683716f, 2.799999952316284f, 2.9000000953674316f, 3.0f, 3.0999999046325684f, 3.200000047683716f, 3.299999952316284f, 3.4000000953674316f, 3.5f, 3.5999999046325684f, 3.700000047683716f, 3.799999952316284f, 3.9000000953674316f, 4.0f, 4.099999904632568f, 4.199999809265137f, 4.300000190734863f, 4.400000095367432f, 4.5f, 4.599999904632568f, 4.699999809265137f, 4.800000190734863f, 4.900000095367432f, 5.0f, 5.099999904632568f, 5.199999809265137f, 5.300000190734863f, 5.400000095367432f, 5.5f, 5.599999904632568f, 5.699999809265137f, 5.800000190734863f, 5.900000095367432f}),
                            .dimensions = {1, 2, 3, 4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({6.0f, 5.900000095367432f, 5.800000190734863f, 5.699999809265137f, 5.599999904632568f, 5.5f, 5.400000095367432f, 5.300000190734863f, 5.199999809265137f, 5.099999904632568f, 5.0f, 4.900000095367432f, 4.800000190734863f, 4.699999809265137f, 4.599999904632568f, 4.5f, 4.400000095367432f, 4.300000190734863f, 4.199999809265137f, 4.099999904632568f, 4.0f, 3.9000000953674316f, 3.799999952316284f, 3.700000047683716f, 3.5999999046325684f, 3.5f, 3.4000000953674316f, 3.299999952316284f, 3.200000047683716f, 3.0999999046325684f, 3.0f, 2.9000000953674316f, 2.799999952316284f, 2.700000047683716f, 2.5999999046325684f, 2.5f, 2.4000000953674316f, 2.299999952316284f, 2.200000047683716f, 2.0999999046325684f, 2.0f, 1.899999976158142f, 1.7999999523162842f, 1.7000000476837158f, 1.600000023841858f, 1.5f, 1.399999976158142f, 1.2999999523162842f, 1.2000000476837158f, 1.100000023841858f, 1.0f, 0.8999999761581421f, 0.800000011920929f, 0.699999988079071f, 0.6000000238418579f, 0.5f, 0.4000000059604645f, 0.30000001192092896f, 0.20000000298023224f, 0.10000000149011612f, 0.0f, -0.10000000149011612f, -0.20000000298023224f, -0.30000001192092896f, -0.4000000059604645f, -0.5f, -0.6000000238418579f, -0.699999988079071f, -0.800000011920929f, -0.8999999761581421f, -1.0f, -1.100000023841858f, -1.2000000476837158f, -1.2999999523162842f, -1.399999976158142f, -1.5f, -1.600000023841858f, -1.7000000476837158f, -1.7999999523162842f, -1.899999976158142f, -2.0f, -2.0999999046325684f, -2.200000047683716f, -2.299999952316284f, -2.4000000953674316f, -2.5f, -2.5999999046325684f, -2.700000047683716f, -2.799999952316284f, -2.9000000953674316f, -3.0f, -3.0999999046325684f, -3.200000047683716f, -3.299999952316284f, -3.4000000953674316f, -3.5f, -3.5999999046325684f, -3.700000047683716f, -3.799999952316284f, -3.9000000953674316f, -4.0f, -4.099999904632568f, -4.199999809265137f, -4.300000190734863f, -4.400000095367432f, -4.5f, -4.599999904632568f, -4.699999809265137f, -4.800000190734863f, -4.900000095367432f, -5.0f, -5.099999904632568f, -5.199999809265137f, -5.300000190734863f, -5.400000095367432f, -5.5f, -5.599999904632568f, -5.699999809265137f, -5.800000190734863f, -5.900000095367432f}),
                            .dimensions = {1, 2, 3, 4, 5},
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
                            .type = TestOperationType::NEG
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_float16 = TestModelManager::get().add("neg_float16", get_test_model_float16());

}  // namespace generated_tests::neg

namespace generated_tests::neg {

const TestModel& get_test_model_int32() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-6, -6, -6, -6, -6, -6, -5, -5, -5, -5, -5, -5, -5, -5, -5, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -3, -3, -3, -3, -3, -3, -3, -3, -3, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6}),
                            .dimensions = {1, 2, 3, 4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -3, -3, -3, -3, -3, -3, -3, -3, -3, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -5, -5, -5, -5, -5, -5, -5, -5, -5, -6, -6, -6, -6, -6}),
                            .dimensions = {1, 2, 3, 4, 5},
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
                            .type = TestOperationType::NEG
                        }},
                .outputIndexes = {1}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_int32 = TestModelManager::get().add("neg_int32", get_test_model_int32());

}  // namespace generated_tests::neg

