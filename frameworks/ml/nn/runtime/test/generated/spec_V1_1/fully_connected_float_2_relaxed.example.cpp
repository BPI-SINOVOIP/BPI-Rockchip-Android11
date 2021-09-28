// Generated from fully_connected_float_2_relaxed.mod.py
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;

namespace generated_tests::fully_connected_float_2_relaxed {

const TestModel& get_test_model() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.503691f, 0.196961f, 0.521017f, 0.554248f, 0.288678f, 0.792476f, 0.561653f, 0.46223f, 0.650736f, 0.163132f, 0.029658f, 0.411544f, 0.470539f, 0.57239f, 0.538755f, 0.21203f}),
                            .dimensions = {2, 8},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.091327f, 0.103366f, -0.316505f, -0.08312f, 0.149366f, -0.196636f, -0.123672f, 0.0628f, 0.063031f, 0.19167f, -0.062001f, -0.061504f, -0.275581f, 0.059388f, -0.118497f, -0.079224f, 0.109758f, 0.008307f, -0.062657f, -0.060962f, -0.049782f, -0.106719f, -0.319482f, -0.10365f, 0.266455f, 0.051517f, -0.123448f, 0.322464f, 0.043282f, -0.173782f, -0.190381f, 0.002013f, 0.096086f, 0.131157f, 0.031164f, 0.100638f, -0.312191f, -0.080923f, -0.101318f, -0.116614f, 0.142238f, 0.08654f, -0.139154f, 0.174268f, -0.073161f, 0.080072f, 0.006874f, 0.229382f, -0.104321f, -0.176035f, -0.208587f, -0.001019f, -0.162032f, 0.080824f, -0.025021f, 0.07446f, -0.252595f, -0.16175f, -0.136403f, 0.008308f, 0.00571f, 0.0966f, 0.289839f, 0.218816f, -0.304651f, -0.070958f, 0.054598f, 0.147113f, -0.139112f, -0.072798f, -0.163335f, -0.167863f, -0.128762f, -0.03578f, 0.117262f, 0.017177f, 0.263335f, -0.176612f, 0.262961f, -0.093654f, -0.339283f, 0.333071f, 0.180827f, 0.287583f, 0.06635f, -0.197947f, -0.114449f, -0.236035f, 0.103532f, -0.034284f, 0.093299f, -0.145361f, 0.054001f, 0.25057f, 0.15701f, -0.14348f, -0.139061f, -0.048873f, 0.067557f, 0.139038f, 0.324106f, 0.227041f, 0.037793f, -0.225747f, -0.241619f, 0.357835f, 0.135762f, -0.306764f, -0.125982f, 0.091916f, 0.266587f, 0.030135f, 0.265148f, 0.141627f, 0.02012f, 0.083815f, -0.124556f, -0.100124f, -0.048159f, 0.181172f, 0.302309f, -0.041084f, 0.146334f, -0.061511f, -0.232605f, 0.281324f, 0.145408f, -0.221897f}),
                            .dimensions = {16, 8},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // b0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-0.160594f, 0.20577f, -0.078307f, -0.077984f, 0.001937f, 0.01586f, 0.03681f, 0.012346f, 0.001028f, 0.038551f, 0.075415f, 0.020804f, 0.048478f, -0.03227f, 0.175688f, -0.085662f}),
                            .dimensions = {16},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // act_relu
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f, 0.0732134f, 0.0f, 0.0f, 0.0f, 0.280859f, 0.0f, 0.128927f, 0.0f, 0.0777251f, 0.0f, 0.270268f, 0.271435f, 0.0173503f, 0.335465f, 0.235562f, 0.0f, 0.0745866f, 0.0f, 0.051611f, 0.0f, 0.253876f, 0.0f, 0.0814873f, 0.0f, 0.104104f, 0.0f, 0.248529f, 0.264194f, 0.0f, 0.302973f, 0.166252f}),
                            .dimensions = {2, 16},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3},
                            .outputs = {4},
                            .type = TestOperationType::FULLY_CONNECTED
                        }},
                .outputIndexes = {4}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model = TestModelManager::get().add("fully_connected_float_2_relaxed", get_test_model());

}  // namespace generated_tests::fully_connected_float_2_relaxed

namespace generated_tests::fully_connected_float_2_relaxed {

const TestModel& get_test_model_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {5},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {2, 8},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.091327f, 0.103366f, -0.316505f, -0.08312f, 0.149366f, -0.196636f, -0.123672f, 0.0628f, 0.063031f, 0.19167f, -0.062001f, -0.061504f, -0.275581f, 0.059388f, -0.118497f, -0.079224f, 0.109758f, 0.008307f, -0.062657f, -0.060962f, -0.049782f, -0.106719f, -0.319482f, -0.10365f, 0.266455f, 0.051517f, -0.123448f, 0.322464f, 0.043282f, -0.173782f, -0.190381f, 0.002013f, 0.096086f, 0.131157f, 0.031164f, 0.100638f, -0.312191f, -0.080923f, -0.101318f, -0.116614f, 0.142238f, 0.08654f, -0.139154f, 0.174268f, -0.073161f, 0.080072f, 0.006874f, 0.229382f, -0.104321f, -0.176035f, -0.208587f, -0.001019f, -0.162032f, 0.080824f, -0.025021f, 0.07446f, -0.252595f, -0.16175f, -0.136403f, 0.008308f, 0.00571f, 0.0966f, 0.289839f, 0.218816f, -0.304651f, -0.070958f, 0.054598f, 0.147113f, -0.139112f, -0.072798f, -0.163335f, -0.167863f, -0.128762f, -0.03578f, 0.117262f, 0.017177f, 0.263335f, -0.176612f, 0.262961f, -0.093654f, -0.339283f, 0.333071f, 0.180827f, 0.287583f, 0.06635f, -0.197947f, -0.114449f, -0.236035f, 0.103532f, -0.034284f, 0.093299f, -0.145361f, 0.054001f, 0.25057f, 0.15701f, -0.14348f, -0.139061f, -0.048873f, 0.067557f, 0.139038f, 0.324106f, 0.227041f, 0.037793f, -0.225747f, -0.241619f, 0.357835f, 0.135762f, -0.306764f, -0.125982f, 0.091916f, 0.266587f, 0.030135f, 0.265148f, 0.141627f, 0.02012f, 0.083815f, -0.124556f, -0.100124f, -0.048159f, 0.181172f, 0.302309f, -0.041084f, 0.146334f, -0.061511f, -0.232605f, 0.281324f, 0.145408f, -0.221897f}),
                            .dimensions = {16, 8},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // b0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-0.160594f, 0.20577f, -0.078307f, -0.077984f, 0.001937f, 0.01586f, 0.03681f, 0.012346f, 0.001028f, 0.038551f, 0.075415f, 0.020804f, 0.048478f, -0.03227f, 0.175688f, -0.085662f}),
                            .dimensions = {16},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // act_relu
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f, 0.0732134f, 0.0f, 0.0f, 0.0f, 0.280859f, 0.0f, 0.128927f, 0.0f, 0.0777251f, 0.0f, 0.270268f, 0.271435f, 0.0173503f, 0.335465f, 0.235562f, 0.0f, 0.0745866f, 0.0f, 0.051611f, 0.0f, 0.253876f, 0.0f, 0.0814873f, 0.0f, 0.104104f, 0.0f, 0.248529f, 0.264194f, 0.0f, 0.302973f, 0.166252f}),
                            .dimensions = {2, 16},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.503691f, 0.196961f, 0.521017f, 0.554248f, 0.288678f, 0.792476f, 0.561653f, 0.46223f, 0.650736f, 0.163132f, 0.029658f, 0.411544f, 0.470539f, 0.57239f, 0.538755f, 0.21203f}),
                            .dimensions = {2, 8},
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
                            .inputs = {5, 6, 7},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3},
                            .outputs = {4},
                            .type = TestOperationType::FULLY_CONNECTED
                        }},
                .outputIndexes = {4}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal = TestModelManager::get().add("fully_connected_float_2_relaxed_all_inputs_as_internal", get_test_model_all_inputs_as_internal());

}  // namespace generated_tests::fully_connected_float_2_relaxed

namespace generated_tests::fully_connected_float_2_relaxed {

const TestModel& get_test_model_all_tensors_as_inputs() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0, 1, 2},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.503691f, 0.196961f, 0.521017f, 0.554248f, 0.288678f, 0.792476f, 0.561653f, 0.46223f, 0.650736f, 0.163132f, 0.029658f, 0.411544f, 0.470539f, 0.57239f, 0.538755f, 0.21203f}),
                            .dimensions = {2, 8},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.091327f, 0.103366f, -0.316505f, -0.08312f, 0.149366f, -0.196636f, -0.123672f, 0.0628f, 0.063031f, 0.19167f, -0.062001f, -0.061504f, -0.275581f, 0.059388f, -0.118497f, -0.079224f, 0.109758f, 0.008307f, -0.062657f, -0.060962f, -0.049782f, -0.106719f, -0.319482f, -0.10365f, 0.266455f, 0.051517f, -0.123448f, 0.322464f, 0.043282f, -0.173782f, -0.190381f, 0.002013f, 0.096086f, 0.131157f, 0.031164f, 0.100638f, -0.312191f, -0.080923f, -0.101318f, -0.116614f, 0.142238f, 0.08654f, -0.139154f, 0.174268f, -0.073161f, 0.080072f, 0.006874f, 0.229382f, -0.104321f, -0.176035f, -0.208587f, -0.001019f, -0.162032f, 0.080824f, -0.025021f, 0.07446f, -0.252595f, -0.16175f, -0.136403f, 0.008308f, 0.00571f, 0.0966f, 0.289839f, 0.218816f, -0.304651f, -0.070958f, 0.054598f, 0.147113f, -0.139112f, -0.072798f, -0.163335f, -0.167863f, -0.128762f, -0.03578f, 0.117262f, 0.017177f, 0.263335f, -0.176612f, 0.262961f, -0.093654f, -0.339283f, 0.333071f, 0.180827f, 0.287583f, 0.06635f, -0.197947f, -0.114449f, -0.236035f, 0.103532f, -0.034284f, 0.093299f, -0.145361f, 0.054001f, 0.25057f, 0.15701f, -0.14348f, -0.139061f, -0.048873f, 0.067557f, 0.139038f, 0.324106f, 0.227041f, 0.037793f, -0.225747f, -0.241619f, 0.357835f, 0.135762f, -0.306764f, -0.125982f, 0.091916f, 0.266587f, 0.030135f, 0.265148f, 0.141627f, 0.02012f, 0.083815f, -0.124556f, -0.100124f, -0.048159f, 0.181172f, 0.302309f, -0.041084f, 0.146334f, -0.061511f, -0.232605f, 0.281324f, 0.145408f, -0.221897f}),
                            .dimensions = {16, 8},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // b0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-0.160594f, 0.20577f, -0.078307f, -0.077984f, 0.001937f, 0.01586f, 0.03681f, 0.012346f, 0.001028f, 0.038551f, 0.075415f, 0.020804f, 0.048478f, -0.03227f, 0.175688f, -0.085662f}),
                            .dimensions = {16},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // act_relu
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f, 0.0732134f, 0.0f, 0.0f, 0.0f, 0.280859f, 0.0f, 0.128927f, 0.0f, 0.0777251f, 0.0f, 0.270268f, 0.271435f, 0.0173503f, 0.335465f, 0.235562f, 0.0f, 0.0745866f, 0.0f, 0.051611f, 0.0f, 0.253876f, 0.0f, 0.0814873f, 0.0f, 0.104104f, 0.0f, 0.248529f, 0.264194f, 0.0f, 0.302973f, 0.166252f}),
                            .dimensions = {2, 16},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3},
                            .outputs = {4},
                            .type = TestOperationType::FULLY_CONNECTED
                        }},
                .outputIndexes = {4}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_tensors_as_inputs = TestModelManager::get().add("fully_connected_float_2_relaxed_all_tensors_as_inputs", get_test_model_all_tensors_as_inputs());

}  // namespace generated_tests::fully_connected_float_2_relaxed

namespace generated_tests::fully_connected_float_2_relaxed {

const TestModel& get_test_model_all_tensors_as_inputs_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {5, 8, 11},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {2, 8},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // op2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {16, 8},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // b0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {16},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // act_relu
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // op3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f, 0.0732134f, 0.0f, 0.0f, 0.0f, 0.280859f, 0.0f, 0.128927f, 0.0f, 0.0777251f, 0.0f, 0.270268f, 0.271435f, 0.0173503f, 0.335465f, 0.235562f, 0.0f, 0.0745866f, 0.0f, 0.051611f, 0.0f, 0.253876f, 0.0f, 0.0814873f, 0.0f, 0.104104f, 0.0f, 0.248529f, 0.264194f, 0.0f, 0.302973f, 0.166252f}),
                            .dimensions = {2, 16},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.503691f, 0.196961f, 0.521017f, 0.554248f, 0.288678f, 0.792476f, 0.561653f, 0.46223f, 0.650736f, 0.163132f, 0.029658f, 0.411544f, 0.470539f, 0.57239f, 0.538755f, 0.21203f}),
                            .dimensions = {2, 8},
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
                        }, { // op2_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.091327f, 0.103366f, -0.316505f, -0.08312f, 0.149366f, -0.196636f, -0.123672f, 0.0628f, 0.063031f, 0.19167f, -0.062001f, -0.061504f, -0.275581f, 0.059388f, -0.118497f, -0.079224f, 0.109758f, 0.008307f, -0.062657f, -0.060962f, -0.049782f, -0.106719f, -0.319482f, -0.10365f, 0.266455f, 0.051517f, -0.123448f, 0.322464f, 0.043282f, -0.173782f, -0.190381f, 0.002013f, 0.096086f, 0.131157f, 0.031164f, 0.100638f, -0.312191f, -0.080923f, -0.101318f, -0.116614f, 0.142238f, 0.08654f, -0.139154f, 0.174268f, -0.073161f, 0.080072f, 0.006874f, 0.229382f, -0.104321f, -0.176035f, -0.208587f, -0.001019f, -0.162032f, 0.080824f, -0.025021f, 0.07446f, -0.252595f, -0.16175f, -0.136403f, 0.008308f, 0.00571f, 0.0966f, 0.289839f, 0.218816f, -0.304651f, -0.070958f, 0.054598f, 0.147113f, -0.139112f, -0.072798f, -0.163335f, -0.167863f, -0.128762f, -0.03578f, 0.117262f, 0.017177f, 0.263335f, -0.176612f, 0.262961f, -0.093654f, -0.339283f, 0.333071f, 0.180827f, 0.287583f, 0.06635f, -0.197947f, -0.114449f, -0.236035f, 0.103532f, -0.034284f, 0.093299f, -0.145361f, 0.054001f, 0.25057f, 0.15701f, -0.14348f, -0.139061f, -0.048873f, 0.067557f, 0.139038f, 0.324106f, 0.227041f, 0.037793f, -0.225747f, -0.241619f, 0.357835f, 0.135762f, -0.306764f, -0.125982f, 0.091916f, 0.266587f, 0.030135f, 0.265148f, 0.141627f, 0.02012f, 0.083815f, -0.124556f, -0.100124f, -0.048159f, 0.181172f, 0.302309f, -0.041084f, 0.146334f, -0.061511f, -0.232605f, 0.281324f, 0.145408f, -0.221897f}),
                            .dimensions = {16, 8},
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
                        }, { // b0_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({-0.160594f, 0.20577f, -0.078307f, -0.077984f, 0.001937f, 0.01586f, 0.03681f, 0.012346f, 0.001028f, 0.038551f, 0.075415f, 0.020804f, 0.048478f, -0.03227f, 0.175688f, -0.085662f}),
                            .dimensions = {16},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
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
                        }},
                .operations = {{
                            .inputs = {5, 6, 7},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {8, 9, 10},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {11, 12, 13},
                            .outputs = {2},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3},
                            .outputs = {4},
                            .type = TestOperationType::FULLY_CONNECTED
                        }},
                .outputIndexes = {4}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_tensors_as_inputs_all_inputs_as_internal = TestModelManager::get().add("fully_connected_float_2_relaxed_all_tensors_as_inputs_all_inputs_as_internal", get_test_model_all_tensors_as_inputs_all_inputs_as_internal());

}  // namespace generated_tests::fully_connected_float_2_relaxed

