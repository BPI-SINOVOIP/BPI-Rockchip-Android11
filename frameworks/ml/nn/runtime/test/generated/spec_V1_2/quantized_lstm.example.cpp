// Generated from quantized_lstm.mod.py
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;

namespace generated_tests::quantized_lstm {

const TestModel& get_test_model() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
                .operands = {{ // input
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({166, 179, 50, 150}),
                            .dimensions = {2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // inputToInputWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({146, 250, 235, 171, 10, 218, 171, 108}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToForgetWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({24, 50, 132, 179, 158, 110, 3, 169}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToCellWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({133, 34, 29, 49, 206, 109, 54, 183}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToOutputWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({195, 187, 11, 99, 109, 10, 218, 48}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToInputWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({254, 206, 77, 168, 71, 20, 215, 6, 223, 7, 118, 225, 59, 130, 174, 26}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToForgetWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({137, 240, 103, 52, 68, 51, 237, 112, 0, 220, 89, 23, 69, 4, 207, 253}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToCellWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({172, 60, 205, 65, 14, 0, 140, 168, 240, 223, 133, 56, 142, 64, 246, 216}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToOutputWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({106, 214, 67, 23, 59, 158, 45, 3, 119, 132, 49, 205, 129, 218, 11, 98}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputGateBias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-7876, 13488, -726, 32839}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // forgetGateBias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({9206, -46884, -11693, -38724}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // cellGateBias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({39481, 48624, 48976, -21419}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // outputGateBias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-58999, -17050, -41852, -40538}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // prevCellState
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({876, 1034, 955, -909, 761, 1029, 796, -1036}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // prevOutput
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({136, 150, 140, 115, 135, 152, 138, 112}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // cellStateOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({1485, 1177, 1373, -1023, 1019, 1355, 1097, -1235}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({140, 151, 146, 112, 136, 156, 142, 112}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
                            .outputs = {15, 16},
                            .type = TestOperationType::QUANTIZED_16BIT_LSTM
                        }},
                .outputIndexes = {15, 16}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model = TestModelManager::get().add("quantized_lstm", get_test_model());

}  // namespace generated_tests::quantized_lstm

namespace generated_tests::quantized_lstm {

const TestModel& get_test_model_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {9, 10, 11, 12, 13, 17, 20, 23, 26, 29, 32, 35, 38, 41, 44},
                .operands = {{ // input
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // inputToInputWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToForgetWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToCellWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToOutputWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToInputWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToForgetWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToCellWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToOutputWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputGateBias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-7876, 13488, -726, 32839}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // forgetGateBias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({9206, -46884, -11693, -38724}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // cellGateBias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({39481, 48624, 48976, -21419}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // outputGateBias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-58999, -17050, -41852, -40538}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // prevCellState
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({876, 1034, 955, -909, 761, 1029, 796, -1036}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // prevOutput
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // cellStateOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({1485, 1177, 1373, -1023, 1019, 1355, 1097, -1235}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({140, 151, 146, 112, 136, 156, 142, 112}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // input_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({166, 179, 50, 150}),
                            .dimensions = {2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                        }, { // inputToInputWeights_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({146, 250, 235, 171, 10, 218, 171, 108}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
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
                        }, { // inputToForgetWeights_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({24, 50, 132, 179, 158, 110, 3, 169}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
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
                        }, { // inputToCellWeights_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({133, 34, 29, 49, 206, 109, 54, 183}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
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
                        }, { // inputToOutputWeights_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({195, 187, 11, 99, 109, 10, 218, 48}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
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
                        }, { // recurrentToInputWeights_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({254, 206, 77, 168, 71, 20, 215, 6, 223, 7, 118, 225, 59, 130, 174, 26}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
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
                        }, { // recurrentToForgetWeights_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({137, 240, 103, 52, 68, 51, 237, 112, 0, 220, 89, 23, 69, 4, 207, 253}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
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
                        }, { // recurrentToCellWeights_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({172, 60, 205, 65, 14, 0, 140, 168, 240, 223, 133, 56, 142, 64, 246, 216}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
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
                        }, { // recurrentToOutputWeights_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({106, 214, 67, 23, 59, 158, 45, 3, 119, 132, 49, 205, 129, 218, 11, 98}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy8
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
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
                        }, { // prevOutput_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({136, 150, 140, 115, 135, 152, 138, 112}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy9
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                            .inputs = {17, 18, 19},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {20, 21, 22},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {23, 24, 25},
                            .outputs = {2},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {26, 27, 28},
                            .outputs = {3},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {29, 30, 31},
                            .outputs = {4},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {32, 33, 34},
                            .outputs = {5},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {35, 36, 37},
                            .outputs = {6},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {38, 39, 40},
                            .outputs = {7},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {41, 42, 43},
                            .outputs = {8},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {44, 45, 46},
                            .outputs = {14},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
                            .outputs = {15, 16},
                            .type = TestOperationType::QUANTIZED_16BIT_LSTM
                        }},
                .outputIndexes = {15, 16}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal = TestModelManager::get().add("quantized_lstm_all_inputs_as_internal", get_test_model_all_inputs_as_internal());

}  // namespace generated_tests::quantized_lstm

namespace generated_tests::quantized_lstm {

const TestModel& get_test_model_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
                .operands = {{ // input
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({166, 179, 50, 150}),
                            .dimensions = {2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // inputToInputWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({146, 250, 235, 171, 10, 218, 171, 108}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToForgetWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({24, 50, 132, 179, 158, 110, 3, 169}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToCellWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({133, 34, 29, 49, 206, 109, 54, 183}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToOutputWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({195, 187, 11, 99, 109, 10, 218, 48}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToInputWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({254, 206, 77, 168, 71, 20, 215, 6, 223, 7, 118, 225, 59, 130, 174, 26}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToForgetWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({137, 240, 103, 52, 68, 51, 237, 112, 0, 220, 89, 23, 69, 4, 207, 253}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToCellWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({172, 60, 205, 65, 14, 0, 140, 168, 240, 223, 133, 56, 142, 64, 246, 216}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToOutputWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({106, 214, 67, 23, 59, 158, 45, 3, 119, 132, 49, 205, 129, 218, 11, 98}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputGateBias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-7876, 13488, -726, 32839}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // forgetGateBias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({9206, -46884, -11693, -38724}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // cellGateBias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({39481, 48624, 48976, -21419}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // outputGateBias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-58999, -17050, -41852, -40538}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // prevCellState
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({876, 1034, 955, -909, 761, 1029, 796, -1036}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // prevOutput
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({136, 150, 140, 115, 135, 152, 138, 112}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // cellStateOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({1485, 1177, 1373, -1023, 1019, 1355, 1097, -1235}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({140, 151, 146, 112, 136, 156, 142, 112}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
                            .outputs = {15, 16},
                            .type = TestOperationType::QUANTIZED_16BIT_LSTM
                        }},
                .outputIndexes = {15, 16}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_relaxed = TestModelManager::get().add("quantized_lstm_relaxed", get_test_model_relaxed());

}  // namespace generated_tests::quantized_lstm

namespace generated_tests::quantized_lstm {

const TestModel& get_test_model_relaxed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {9, 10, 11, 12, 13, 17, 20, 23, 26, 29, 32, 35, 38, 41, 44},
                .operands = {{ // input
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // inputToInputWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToForgetWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToCellWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToOutputWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToInputWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToForgetWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToCellWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToOutputWeights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputGateBias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-7876, 13488, -726, 32839}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // forgetGateBias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({9206, -46884, -11693, -38724}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // cellGateBias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({39481, 48624, 48976, -21419}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // outputGateBias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-58999, -17050, -41852, -40538}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // prevCellState
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({876, 1034, 955, -909, 761, 1029, 796, -1036}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // prevOutput
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // cellStateOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({1485, 1177, 1373, -1023, 1019, 1355, 1097, -1235}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({140, 151, 146, 112, 136, 156, 142, 112}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // input_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({166, 179, 50, 150}),
                            .dimensions = {2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                        }, { // inputToInputWeights_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({146, 250, 235, 171, 10, 218, 171, 108}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
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
                        }, { // inputToForgetWeights_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({24, 50, 132, 179, 158, 110, 3, 169}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy12
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
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
                        }, { // inputToCellWeights_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({133, 34, 29, 49, 206, 109, 54, 183}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy13
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
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
                        }, { // inputToOutputWeights_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({195, 187, 11, 99, 109, 10, 218, 48}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy14
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
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
                        }, { // recurrentToInputWeights_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({254, 206, 77, 168, 71, 20, 215, 6, 223, 7, 118, 225, 59, 130, 174, 26}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy15
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
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
                        }, { // recurrentToForgetWeights_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({137, 240, 103, 52, 68, 51, 237, 112, 0, 220, 89, 23, 69, 4, 207, 253}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy16
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
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
                        }, { // recurrentToCellWeights_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({172, 60, 205, 65, 14, 0, 140, 168, 240, 223, 133, 56, 142, 64, 246, 216}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy17
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
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
                        }, { // recurrentToOutputWeights_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({106, 214, 67, 23, 59, 158, 45, 3, 119, 132, 49, 205, 129, 218, 11, 98}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy18
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
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
                        }, { // prevOutput_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({136, 150, 140, 115, 135, 152, 138, 112}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy19
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                            .inputs = {17, 18, 19},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {20, 21, 22},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {23, 24, 25},
                            .outputs = {2},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {26, 27, 28},
                            .outputs = {3},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {29, 30, 31},
                            .outputs = {4},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {32, 33, 34},
                            .outputs = {5},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {35, 36, 37},
                            .outputs = {6},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {38, 39, 40},
                            .outputs = {7},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {41, 42, 43},
                            .outputs = {8},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {44, 45, 46},
                            .outputs = {14},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
                            .outputs = {15, 16},
                            .type = TestOperationType::QUANTIZED_16BIT_LSTM
                        }},
                .outputIndexes = {15, 16}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_relaxed_all_inputs_as_internal = TestModelManager::get().add("quantized_lstm_relaxed_all_inputs_as_internal", get_test_model_relaxed_all_inputs_as_internal());

}  // namespace generated_tests::quantized_lstm

namespace generated_tests::quantized_lstm {

const TestModel& get_test_model_constant_weights() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 13, 14},
                .operands = {{ // input1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({166, 179}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // inputToInputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({146, 250, 235, 171, 10, 218, 171, 108}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToForgetWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({24, 50, 132, 179, 158, 110, 3, 169}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToCellWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({133, 34, 29, 49, 206, 109, 54, 183}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToOutputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({195, 187, 11, 99, 109, 10, 218, 48}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToInputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({254, 206, 77, 168, 71, 20, 215, 6, 223, 7, 118, 225, 59, 130, 174, 26}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToForgetWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({137, 240, 103, 52, 68, 51, 237, 112, 0, 220, 89, 23, 69, 4, 207, 253}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToCellWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({172, 60, 205, 65, 14, 0, 140, 168, 240, 223, 133, 56, 142, 64, 246, 216}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToOutputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({106, 214, 67, 23, 59, 158, 45, 3, 119, 132, 49, 205, 129, 218, 11, 98}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-7876, 13488, -726, 32839}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // forgetGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({9206, -46884, -11693, -38724}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // cellGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({39481, 48624, 48976, -21419}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // outputGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-58999, -17050, -41852, -40538}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // prevCellState1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({876, 1034, 955, -909}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // prevOutput1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({136, 150, 140, 115}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // cellStateOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({1485, 1177, 1373, -1023}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({140, 151, 146, 112}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
                            .outputs = {15, 16},
                            .type = TestOperationType::QUANTIZED_16BIT_LSTM
                        }},
                .outputIndexes = {15, 16}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_constant_weights = TestModelManager::get().add("quantized_lstm_constant_weights", get_test_model_constant_weights());

}  // namespace generated_tests::quantized_lstm

namespace generated_tests::quantized_lstm {

const TestModel& get_test_model_constant_weights_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {13, 17, 20},
                .operands = {{ // input1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // inputToInputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({146, 250, 235, 171, 10, 218, 171, 108}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToForgetWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({24, 50, 132, 179, 158, 110, 3, 169}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToCellWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({133, 34, 29, 49, 206, 109, 54, 183}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToOutputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({195, 187, 11, 99, 109, 10, 218, 48}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToInputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({254, 206, 77, 168, 71, 20, 215, 6, 223, 7, 118, 225, 59, 130, 174, 26}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToForgetWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({137, 240, 103, 52, 68, 51, 237, 112, 0, 220, 89, 23, 69, 4, 207, 253}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToCellWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({172, 60, 205, 65, 14, 0, 140, 168, 240, 223, 133, 56, 142, 64, 246, 216}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToOutputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({106, 214, 67, 23, 59, 158, 45, 3, 119, 132, 49, 205, 129, 218, 11, 98}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-7876, 13488, -726, 32839}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // forgetGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({9206, -46884, -11693, -38724}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // cellGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({39481, 48624, 48976, -21419}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // outputGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-58999, -17050, -41852, -40538}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // prevCellState1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({876, 1034, 955, -909}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // prevOutput1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // cellStateOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({1485, 1177, 1373, -1023}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({140, 151, 146, 112}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // input1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({166, 179}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy20
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                        }, { // prevOutput1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({136, 150, 140, 115}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy21
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                            .inputs = {17, 18, 19},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {20, 21, 22},
                            .outputs = {14},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
                            .outputs = {15, 16},
                            .type = TestOperationType::QUANTIZED_16BIT_LSTM
                        }},
                .outputIndexes = {15, 16}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_constant_weights_all_inputs_as_internal = TestModelManager::get().add("quantized_lstm_constant_weights_all_inputs_as_internal", get_test_model_constant_weights_all_inputs_as_internal());

}  // namespace generated_tests::quantized_lstm

namespace generated_tests::quantized_lstm {

const TestModel& get_test_model_constant_weights_all_tensors_as_inputs() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
                .operands = {{ // input1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({166, 179}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // inputToInputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({146, 250, 235, 171, 10, 218, 171, 108}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToForgetWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({24, 50, 132, 179, 158, 110, 3, 169}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToCellWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({133, 34, 29, 49, 206, 109, 54, 183}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToOutputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({195, 187, 11, 99, 109, 10, 218, 48}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToInputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({254, 206, 77, 168, 71, 20, 215, 6, 223, 7, 118, 225, 59, 130, 174, 26}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToForgetWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({137, 240, 103, 52, 68, 51, 237, 112, 0, 220, 89, 23, 69, 4, 207, 253}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToCellWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({172, 60, 205, 65, 14, 0, 140, 168, 240, 223, 133, 56, 142, 64, 246, 216}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToOutputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({106, 214, 67, 23, 59, 158, 45, 3, 119, 132, 49, 205, 129, 218, 11, 98}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-7876, 13488, -726, 32839}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // forgetGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({9206, -46884, -11693, -38724}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // cellGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({39481, 48624, 48976, -21419}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // outputGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-58999, -17050, -41852, -40538}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // prevCellState1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({876, 1034, 955, -909}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // prevOutput1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({136, 150, 140, 115}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // cellStateOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({1485, 1177, 1373, -1023}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({140, 151, 146, 112}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
                            .outputs = {15, 16},
                            .type = TestOperationType::QUANTIZED_16BIT_LSTM
                        }},
                .outputIndexes = {15, 16}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_constant_weights_all_tensors_as_inputs = TestModelManager::get().add("quantized_lstm_constant_weights_all_tensors_as_inputs", get_test_model_constant_weights_all_tensors_as_inputs());

}  // namespace generated_tests::quantized_lstm

namespace generated_tests::quantized_lstm {

const TestModel& get_test_model_constant_weights_all_tensors_as_inputs_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {9, 10, 11, 12, 13, 17, 20, 23, 26, 29, 32, 35, 38, 41, 44},
                .operands = {{ // input1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // inputToInputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToForgetWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToCellWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToOutputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToInputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToForgetWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToCellWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToOutputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-7876, 13488, -726, 32839}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // forgetGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({9206, -46884, -11693, -38724}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // cellGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({39481, 48624, 48976, -21419}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // outputGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-58999, -17050, -41852, -40538}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // prevCellState1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({876, 1034, 955, -909}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // prevOutput1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // cellStateOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({1485, 1177, 1373, -1023}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({140, 151, 146, 112}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // input1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({166, 179}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy22
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                        }, { // inputToInputWeights1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({146, 250, 235, 171, 10, 218, 171, 108}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy23
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
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
                        }, { // inputToForgetWeights1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({24, 50, 132, 179, 158, 110, 3, 169}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy24
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // param24
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // inputToCellWeights1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({133, 34, 29, 49, 206, 109, 54, 183}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy25
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // param25
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // inputToOutputWeights1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({195, 187, 11, 99, 109, 10, 218, 48}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy26
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // param26
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // recurrentToInputWeights1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({254, 206, 77, 168, 71, 20, 215, 6, 223, 7, 118, 225, 59, 130, 174, 26}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy27
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // param27
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // recurrentToForgetWeights1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({137, 240, 103, 52, 68, 51, 237, 112, 0, 220, 89, 23, 69, 4, 207, 253}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy28
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // param28
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // recurrentToCellWeights1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({172, 60, 205, 65, 14, 0, 140, 168, 240, 223, 133, 56, 142, 64, 246, 216}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy29
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // param29
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // recurrentToOutputWeights1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({106, 214, 67, 23, 59, 158, 45, 3, 119, 132, 49, 205, 129, 218, 11, 98}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy30
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // param30
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // prevOutput1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({136, 150, 140, 115}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy31
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // param31
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
                            .inputs = {17, 18, 19},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {20, 21, 22},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {23, 24, 25},
                            .outputs = {2},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {26, 27, 28},
                            .outputs = {3},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {29, 30, 31},
                            .outputs = {4},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {32, 33, 34},
                            .outputs = {5},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {35, 36, 37},
                            .outputs = {6},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {38, 39, 40},
                            .outputs = {7},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {41, 42, 43},
                            .outputs = {8},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {44, 45, 46},
                            .outputs = {14},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
                            .outputs = {15, 16},
                            .type = TestOperationType::QUANTIZED_16BIT_LSTM
                        }},
                .outputIndexes = {15, 16}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_constant_weights_all_tensors_as_inputs_all_inputs_as_internal = TestModelManager::get().add("quantized_lstm_constant_weights_all_tensors_as_inputs_all_inputs_as_internal", get_test_model_constant_weights_all_tensors_as_inputs_all_inputs_as_internal());

}  // namespace generated_tests::quantized_lstm

namespace generated_tests::quantized_lstm {

const TestModel& get_test_model_constant_weights_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0, 13, 14},
                .operands = {{ // input1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({166, 179}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // inputToInputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({146, 250, 235, 171, 10, 218, 171, 108}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToForgetWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({24, 50, 132, 179, 158, 110, 3, 169}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToCellWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({133, 34, 29, 49, 206, 109, 54, 183}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToOutputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({195, 187, 11, 99, 109, 10, 218, 48}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToInputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({254, 206, 77, 168, 71, 20, 215, 6, 223, 7, 118, 225, 59, 130, 174, 26}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToForgetWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({137, 240, 103, 52, 68, 51, 237, 112, 0, 220, 89, 23, 69, 4, 207, 253}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToCellWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({172, 60, 205, 65, 14, 0, 140, 168, 240, 223, 133, 56, 142, 64, 246, 216}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToOutputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({106, 214, 67, 23, 59, 158, 45, 3, 119, 132, 49, 205, 129, 218, 11, 98}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-7876, 13488, -726, 32839}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // forgetGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({9206, -46884, -11693, -38724}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // cellGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({39481, 48624, 48976, -21419}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // outputGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-58999, -17050, -41852, -40538}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // prevCellState1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({876, 1034, 955, -909}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // prevOutput1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({136, 150, 140, 115}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // cellStateOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({1485, 1177, 1373, -1023}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({140, 151, 146, 112}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
                            .outputs = {15, 16},
                            .type = TestOperationType::QUANTIZED_16BIT_LSTM
                        }},
                .outputIndexes = {15, 16}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_constant_weights_relaxed = TestModelManager::get().add("quantized_lstm_constant_weights_relaxed", get_test_model_constant_weights_relaxed());

}  // namespace generated_tests::quantized_lstm

namespace generated_tests::quantized_lstm {

const TestModel& get_test_model_constant_weights_relaxed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {13, 17, 20},
                .operands = {{ // input1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // inputToInputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({146, 250, 235, 171, 10, 218, 171, 108}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToForgetWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({24, 50, 132, 179, 158, 110, 3, 169}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToCellWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({133, 34, 29, 49, 206, 109, 54, 183}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToOutputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({195, 187, 11, 99, 109, 10, 218, 48}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToInputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({254, 206, 77, 168, 71, 20, 215, 6, 223, 7, 118, 225, 59, 130, 174, 26}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToForgetWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({137, 240, 103, 52, 68, 51, 237, 112, 0, 220, 89, 23, 69, 4, 207, 253}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToCellWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({172, 60, 205, 65, 14, 0, 140, 168, 240, 223, 133, 56, 142, 64, 246, 216}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToOutputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({106, 214, 67, 23, 59, 158, 45, 3, 119, 132, 49, 205, 129, 218, 11, 98}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-7876, 13488, -726, 32839}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // forgetGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({9206, -46884, -11693, -38724}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // cellGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({39481, 48624, 48976, -21419}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // outputGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-58999, -17050, -41852, -40538}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // prevCellState1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({876, 1034, 955, -909}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // prevOutput1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // cellStateOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({1485, 1177, 1373, -1023}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({140, 151, 146, 112}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // input1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({166, 179}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy32
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // param32
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // prevOutput1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({136, 150, 140, 115}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy33
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // param33
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
                            .inputs = {17, 18, 19},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {20, 21, 22},
                            .outputs = {14},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
                            .outputs = {15, 16},
                            .type = TestOperationType::QUANTIZED_16BIT_LSTM
                        }},
                .outputIndexes = {15, 16}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_constant_weights_relaxed_all_inputs_as_internal = TestModelManager::get().add("quantized_lstm_constant_weights_relaxed_all_inputs_as_internal", get_test_model_constant_weights_relaxed_all_inputs_as_internal());

}  // namespace generated_tests::quantized_lstm

namespace generated_tests::quantized_lstm {

const TestModel& get_test_model_constant_weights_relaxed_all_tensors_as_inputs() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
                .operands = {{ // input1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({166, 179}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // inputToInputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({146, 250, 235, 171, 10, 218, 171, 108}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToForgetWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({24, 50, 132, 179, 158, 110, 3, 169}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToCellWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({133, 34, 29, 49, 206, 109, 54, 183}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToOutputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({195, 187, 11, 99, 109, 10, 218, 48}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToInputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({254, 206, 77, 168, 71, 20, 215, 6, 223, 7, 118, 225, 59, 130, 174, 26}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToForgetWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({137, 240, 103, 52, 68, 51, 237, 112, 0, 220, 89, 23, 69, 4, 207, 253}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToCellWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({172, 60, 205, 65, 14, 0, 140, 168, 240, 223, 133, 56, 142, 64, 246, 216}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToOutputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({106, 214, 67, 23, 59, 158, 45, 3, 119, 132, 49, 205, 129, 218, 11, 98}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-7876, 13488, -726, 32839}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // forgetGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({9206, -46884, -11693, -38724}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // cellGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({39481, 48624, 48976, -21419}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // outputGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-58999, -17050, -41852, -40538}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // prevCellState1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({876, 1034, 955, -909}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // prevOutput1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({136, 150, 140, 115}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // cellStateOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({1485, 1177, 1373, -1023}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({140, 151, 146, 112}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
                            .outputs = {15, 16},
                            .type = TestOperationType::QUANTIZED_16BIT_LSTM
                        }},
                .outputIndexes = {15, 16}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_constant_weights_relaxed_all_tensors_as_inputs = TestModelManager::get().add("quantized_lstm_constant_weights_relaxed_all_tensors_as_inputs", get_test_model_constant_weights_relaxed_all_tensors_as_inputs());

}  // namespace generated_tests::quantized_lstm

namespace generated_tests::quantized_lstm {

const TestModel& get_test_model_constant_weights_relaxed_all_tensors_as_inputs_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {9, 10, 11, 12, 13, 17, 20, 23, 26, 29, 32, 35, 38, 41, 44},
                .operands = {{ // input1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // inputToInputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToForgetWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToCellWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputToOutputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToInputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToForgetWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToCellWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // recurrentToOutputWeights1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // inputGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-7876, 13488, -726, 32839}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // forgetGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({9206, -46884, -11693, -38724}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // cellGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({39481, 48624, 48976, -21419}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // outputGateBias1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-58999, -17050, -41852, -40538}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.1876640625e-05f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // prevCellState1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({876, 1034, 955, -909}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // prevOutput1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // cellStateOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({1485, 1177, 1373, -1023}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.00048828125f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({140, 151, 146, 112}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // input1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({166, 179}),
                            .dimensions = {1, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy34
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // param34
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // inputToInputWeights1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({146, 250, 235, 171, 10, 218, 171, 108}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy35
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // param35
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // inputToForgetWeights1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({24, 50, 132, 179, 158, 110, 3, 169}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy36
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // param36
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // inputToCellWeights1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({133, 34, 29, 49, 206, 109, 54, 183}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy37
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // param37
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // inputToOutputWeights1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({195, 187, 11, 99, 109, 10, 218, 48}),
                            .dimensions = {4, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy38
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // param38
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // recurrentToInputWeights1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({254, 206, 77, 168, 71, 20, 215, 6, 223, 7, 118, 225, 59, 130, 174, 26}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy39
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // param39
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // recurrentToForgetWeights1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({137, 240, 103, 52, 68, 51, 237, 112, 0, 220, 89, 23, 69, 4, 207, 253}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy40
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // param40
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // recurrentToCellWeights1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({172, 60, 205, 65, 14, 0, 140, 168, 240, 223, 133, 56, 142, 64, 246, 216}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy41
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // param41
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // recurrentToOutputWeights1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({106, 214, 67, 23, 59, 158, 45, 3, 119, 132, 49, 205, 129, 218, 11, 98}),
                            .dimensions = {4, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // dummy42
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({100}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.00408021f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 100
                        }, { // param42
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // prevOutput1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({136, 150, 140, 115}),
                            .dimensions = {1, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy43
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // param43
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
                            .inputs = {17, 18, 19},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {20, 21, 22},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {23, 24, 25},
                            .outputs = {2},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {26, 27, 28},
                            .outputs = {3},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {29, 30, 31},
                            .outputs = {4},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {32, 33, 34},
                            .outputs = {5},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {35, 36, 37},
                            .outputs = {6},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {38, 39, 40},
                            .outputs = {7},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {41, 42, 43},
                            .outputs = {8},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {44, 45, 46},
                            .outputs = {14},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14},
                            .outputs = {15, 16},
                            .type = TestOperationType::QUANTIZED_16BIT_LSTM
                        }},
                .outputIndexes = {15, 16}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_constant_weights_relaxed_all_tensors_as_inputs_all_inputs_as_internal = TestModelManager::get().add("quantized_lstm_constant_weights_relaxed_all_tensors_as_inputs_all_inputs_as_internal", get_test_model_constant_weights_relaxed_all_tensors_as_inputs_all_inputs_as_internal());

}  // namespace generated_tests::quantized_lstm

