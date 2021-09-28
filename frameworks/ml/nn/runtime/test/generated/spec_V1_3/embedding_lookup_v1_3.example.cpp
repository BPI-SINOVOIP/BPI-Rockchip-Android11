// Generated from embedding_lookup_v1_3.mod.py
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;

namespace generated_tests::embedding_lookup_v1_3 {

const TestModel& get_test_model_float16() {
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
                            .data = TestBuffer::createFromVector<_Float16>({0.0f, 0.009999999776482582f, 0.019999999552965164f, 0.029999999329447746f, 0.10000000149011612f, 0.10999999940395355f, 0.11999999731779099f, 0.12999999523162842f, 1.0f, 1.0099999904632568f, 1.0199999809265137f, 1.0299999713897705f, 1.100000023841858f, 1.1100000143051147f, 1.1200000047683716f, 1.1299999952316284f, 2.0f, 2.009999990463257f, 2.0199999809265137f, 2.0299999713897705f, 2.0999999046325684f, 2.109999895095825f, 2.119999885559082f, 2.130000114440918f}),
                            .dimensions = {3, 2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 1.0099999904632568f, 1.0199999809265137f, 1.0299999713897705f, 1.100000023841858f, 1.1100000143051147f, 1.1200000047683716f, 1.1299999952316284f, 0.0f, 0.009999999776482582f, 0.019999999552965164f, 0.029999999329447746f, 0.10000000149011612f, 0.10999999940395355f, 0.11999999731779099f, 0.12999999523162842f, 2.0f, 2.009999990463257f, 2.0199999809265137f, 2.0299999713897705f, 2.0999999046325684f, 2.109999895095825f, 2.119999885559082f, 2.130000114440918f}),
                            .dimensions = {3, 2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1},
                            .outputs = {2},
                            .type = TestOperationType::EMBEDDING_LOOKUP
                        }},
                .outputIndexes = {2}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_float16 = TestModelManager::get().add("embedding_lookup_v1_3_float16", get_test_model_float16());

}  // namespace generated_tests::embedding_lookup_v1_3

namespace generated_tests::embedding_lookup_v1_3 {

const TestModel& get_test_model_float16_all_inputs_as_internal() {
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
                            .data = TestBuffer::createFromVector<_Float16>({}),
                            .dimensions = {3, 2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 1.0099999904632568f, 1.0199999809265137f, 1.0299999713897705f, 1.100000023841858f, 1.1100000143051147f, 1.1200000047683716f, 1.1299999952316284f, 0.0f, 0.009999999776482582f, 0.019999999552965164f, 0.029999999329447746f, 0.10000000149011612f, 0.10999999940395355f, 0.11999999731779099f, 0.12999999523162842f, 2.0f, 2.009999990463257f, 2.0199999809265137f, 2.0299999713897705f, 2.0999999046325684f, 2.109999895095825f, 2.119999885559082f, 2.130000114440918f}),
                            .dimensions = {3, 2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // value_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f, 0.009999999776482582f, 0.019999999552965164f, 0.029999999329447746f, 0.10000000149011612f, 0.10999999940395355f, 0.11999999731779099f, 0.12999999523162842f, 1.0f, 1.0099999904632568f, 1.0199999809265137f, 1.0299999713897705f, 1.100000023841858f, 1.1100000143051147f, 1.1200000047683716f, 1.1299999952316284f, 2.0f, 2.009999990463257f, 2.0199999809265137f, 2.0299999713897705f, 2.0999999046325684f, 2.109999895095825f, 2.119999885559082f, 2.130000114440918f}),
                            .dimensions = {3, 2, 4},
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
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_float16_all_inputs_as_internal = TestModelManager::get().add("embedding_lookup_v1_3_float16_all_inputs_as_internal", get_test_model_float16_all_inputs_as_internal());

}  // namespace generated_tests::embedding_lookup_v1_3

