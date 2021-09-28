// Generated from qlstm_projection.mod.py
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;

namespace generated_tests::qlstm_projection {

const TestModel& get_test_model() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23},
                .operands = {{ // input
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({90, 102, 13, 26, 38, 102, 13, 26, 51, 64}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // input_to_input_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({64, 77, 89, -102, -115, 13, 25, 38, -51, 64, -102, 89, -77, 64, -51, -64, -51, -38, -25, -13}),
                            .dimensions = {4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // input_to_forget_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-77, -13, 38, 25, 115, -64, -25, -51, 38, -102, -51, 38, -64, -51, -77, 38, -51, -77, -64, -64}),
                            .dimensions = {4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // input_to_cell_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-51, -38, -25, -13, -64, 64, -25, -38, -25, -77, 77, -13, -51, -38, -89, 89, -115, -64, 102, 77}),
                            .dimensions = {4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // input_to_output_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-102, -51, -25, -115, -13, -89, 38, -38, -102, -25, 77, -25, 51, -89, -38, -64, 13, 64, -77, -51}),
                            .dimensions = {4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // recurrent_to_input_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-25, -38, 51, 13, -64, 115, -25, -38, -89, 6, -25, -77}),
                            .dimensions = {4, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // recurrent_to_forget_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-64, -38, -64, -25, 77, 51, 115, 38, -13, 25, 64, 25}),
                            .dimensions = {4, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // recurrent_to_cell_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-38, 25, 13, -38, 102, -10, -25, 38, 102, -77, -13, 25}),
                            .dimensions = {4, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // recurrent_to_output_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({38, -13, 13, -25, -64, -89, -25, -77, -13, -51, -89, -25}),
                            .dimensions = {4, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // cell_to_input_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // cell_to_forget_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // cell_to_output_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // input_gate_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({644245, 3221226, 4724464, 8160438}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // forget_gate_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2147484, -6442451, -4294968, 2147484}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // cell_gate_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1073742, 15461883, 5368709, 1717987}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // output_gate_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1073742, -214748, 4294968, 2147484}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // projection_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-25, 51, 3, -51, 25, 127, 77, 20, 18, 51, -102, 51}),
                            .dimensions = {3, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00392157f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // projection_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // output_state_in
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 0, 0, 0, 0, 0}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // cell_state_in
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({0, 0, 0, 0, 0, 0, 0, 0}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // input_layer_norm_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({3277, 6553, 9830, 16384}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05182e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // forget_layer_norm_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({6553, 6553, 13107, 9830}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05182e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // cell_layer_norm_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({22937, 6553, 9830, 26214}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05182e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output_layer_norm_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({19660, 6553, 6553, 16384}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05182e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // cell_clip
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // projection_clip
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // input_intermediate_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007059f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // forget_intermediate_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007812f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // cell_intermediate_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007059f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // output_intermediate_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007812f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // hidden_state_zero_point
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // hidden_state_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // output_state_out
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, -108, -67, 127, 127}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // cell_state_out
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({-14650, 8939, 5771, 6715, -11843, 7847, 1508, 12939}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, -108, -67, 127, 127}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31},
                            .outputs = {32, 33, 34},
                            .type = TestOperationType::QUANTIZED_LSTM
                        }},
                .outputIndexes = {32, 33, 34}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model = TestModelManager::get().add("qlstm_projection", get_test_model());

}  // namespace generated_tests::qlstm_projection

namespace generated_tests::qlstm_projection {

const TestModel& get_test_model_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 20, 21, 22, 23, 35, 38},
                .operands = {{ // input
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // input_to_input_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({64, 77, 89, -102, -115, 13, 25, 38, -51, 64, -102, 89, -77, 64, -51, -64, -51, -38, -25, -13}),
                            .dimensions = {4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // input_to_forget_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-77, -13, 38, 25, 115, -64, -25, -51, 38, -102, -51, 38, -64, -51, -77, 38, -51, -77, -64, -64}),
                            .dimensions = {4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // input_to_cell_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-51, -38, -25, -13, -64, 64, -25, -38, -25, -77, 77, -13, -51, -38, -89, 89, -115, -64, 102, 77}),
                            .dimensions = {4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // input_to_output_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-102, -51, -25, -115, -13, -89, 38, -38, -102, -25, 77, -25, 51, -89, -38, -64, 13, 64, -77, -51}),
                            .dimensions = {4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // recurrent_to_input_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-25, -38, 51, 13, -64, 115, -25, -38, -89, 6, -25, -77}),
                            .dimensions = {4, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // recurrent_to_forget_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-64, -38, -64, -25, 77, 51, 115, 38, -13, 25, 64, 25}),
                            .dimensions = {4, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // recurrent_to_cell_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-38, 25, 13, -38, 102, -10, -25, 38, 102, -77, -13, 25}),
                            .dimensions = {4, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // recurrent_to_output_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({38, -13, 13, -25, -64, -89, -25, -77, -13, -51, -89, -25}),
                            .dimensions = {4, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // cell_to_input_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // cell_to_forget_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // cell_to_output_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // input_gate_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({644245, 3221226, 4724464, 8160438}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // forget_gate_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2147484, -6442451, -4294968, 2147484}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // cell_gate_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1073742, 15461883, 5368709, 1717987}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // output_gate_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1073742, -214748, 4294968, 2147484}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // projection_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-25, 51, 3, -51, 25, 127, 77, 20, 18, 51, -102, 51}),
                            .dimensions = {3, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00392157f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // projection_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // output_state_in
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // cell_state_in
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({0, 0, 0, 0, 0, 0, 0, 0}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // input_layer_norm_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({3277, 6553, 9830, 16384}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05182e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // forget_layer_norm_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({6553, 6553, 13107, 9830}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05182e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // cell_layer_norm_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({22937, 6553, 9830, 26214}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05182e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output_layer_norm_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({19660, 6553, 6553, 16384}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05182e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // cell_clip
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // projection_clip
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // input_intermediate_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007059f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // forget_intermediate_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007812f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // cell_intermediate_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007059f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // output_intermediate_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007812f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // hidden_state_zero_point
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // hidden_state_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // output_state_out
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, -108, -67, 127, 127}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // cell_state_out
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({-14650, 8939, 5771, 6715, -11843, 7847, 1508, 12939}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, -108, -67, 127, 127}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // input_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({90, 102, 13, 26, 38, 102, 13, 26, 51, 64}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // dummy
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
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
                        }, { // output_state_in_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 0, 0, 0, 0, 0}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // dummy1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
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
                            .inputs = {35, 36, 37},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {38, 39, 40},
                            .outputs = {18},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31},
                            .outputs = {32, 33, 34},
                            .type = TestOperationType::QUANTIZED_LSTM
                        }},
                .outputIndexes = {32, 33, 34}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal = TestModelManager::get().add("qlstm_projection_all_inputs_as_internal", get_test_model_all_inputs_as_internal());

}  // namespace generated_tests::qlstm_projection

namespace generated_tests::qlstm_projection {

const TestModel& get_test_model_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23},
                .operands = {{ // input
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({90, 102, 13, 26, 38, 102, 13, 26, 51, 64}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // input_to_input_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // input_to_forget_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-77, -13, 38, 25, 115, -64, -25, -51, 38, -102, -51, 38, -64, -51, -77, 38, -51, -77, -64, -64}),
                            .dimensions = {4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // input_to_cell_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-51, -38, -25, -13, -64, 64, -25, -38, -25, -77, 77, -13, -51, -38, -89, 89, -115, -64, 102, 77}),
                            .dimensions = {4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // input_to_output_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-102, -51, -25, -115, -13, -89, 38, -38, -102, -25, 77, -25, 51, -89, -38, -64, 13, 64, -77, -51}),
                            .dimensions = {4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // recurrent_to_input_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {4, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // recurrent_to_forget_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-64, -38, -64, -25, 77, 51, 115, 38, -13, 25, 64, 25}),
                            .dimensions = {4, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // recurrent_to_cell_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-38, 25, 13, -38, 102, -10, -25, 38, 102, -77, -13, 25}),
                            .dimensions = {4, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // recurrent_to_output_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({38, -13, 13, -25, -64, -89, -25, -77, -13, -51, -89, -25}),
                            .dimensions = {4, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // cell_to_input_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // cell_to_forget_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // cell_to_output_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // input_gate_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // forget_gate_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2147484, -6442451, -4294968, 2147484}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // cell_gate_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1073742, 15461883, 5368709, 1717987}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // output_gate_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1073742, -214748, 4294968, 2147484}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // projection_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-25, 51, 3, -51, 25, 127, 77, 20, 18, 51, -102, 51}),
                            .dimensions = {3, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00392157f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // projection_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // output_state_in
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 0, 0, 0, 0, 0}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // cell_state_in
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({0, 0, 0, 0, 0, 0, 0, 0}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // input_layer_norm_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05182e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // forget_layer_norm_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({6553, 6553, 13107, 9830}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05182e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // cell_layer_norm_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({22937, 6553, 9830, 26214}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05182e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output_layer_norm_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({19660, 6553, 6553, 16384}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05182e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // cell_clip
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // projection_clip
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // input_intermediate_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007059f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // forget_intermediate_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007812f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // cell_intermediate_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007059f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // output_intermediate_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007812f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // hidden_state_zero_point
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // hidden_state_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // output_state_out
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, -128, 127, 127}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // cell_state_out
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({-11692, 9960, 5491, 8861, -9422, 7726, 2056, 13149}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, -128, 127, 127}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31},
                            .outputs = {32, 33, 34},
                            .type = TestOperationType::QUANTIZED_LSTM
                        }},
                .outputIndexes = {32, 33, 34}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_2 = TestModelManager::get().add("qlstm_projection_2", get_test_model_2());

}  // namespace generated_tests::qlstm_projection

namespace generated_tests::qlstm_projection {

const TestModel& get_test_model_all_inputs_as_internal_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 20, 21, 22, 23, 35, 38},
                .operands = {{ // input
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // input_to_input_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // input_to_forget_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-77, -13, 38, 25, 115, -64, -25, -51, 38, -102, -51, 38, -64, -51, -77, 38, -51, -77, -64, -64}),
                            .dimensions = {4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // input_to_cell_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-51, -38, -25, -13, -64, 64, -25, -38, -25, -77, 77, -13, -51, -38, -89, 89, -115, -64, 102, 77}),
                            .dimensions = {4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // input_to_output_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-102, -51, -25, -115, -13, -89, 38, -38, -102, -25, 77, -25, 51, -89, -38, -64, 13, 64, -77, -51}),
                            .dimensions = {4, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // recurrent_to_input_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {4, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // recurrent_to_forget_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-64, -38, -64, -25, 77, 51, 115, 38, -13, 25, 64, 25}),
                            .dimensions = {4, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // recurrent_to_cell_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-38, 25, 13, -38, 102, -10, -25, 38, 102, -77, -13, 25}),
                            .dimensions = {4, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // recurrent_to_output_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({38, -13, 13, -25, -64, -89, -25, -77, -13, -51, -89, -25}),
                            .dimensions = {4, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00784314f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // cell_to_input_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // cell_to_forget_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // cell_to_output_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 1.0f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // input_gate_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // forget_gate_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2147484, -6442451, -4294968, 2147484}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // cell_gate_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1073742, 15461883, 5368709, 1717987}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // output_gate_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1073742, -214748, 4294968, 2147484}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // projection_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({-25, 51, 3, -51, 25, 127, 77, 20, 18, 51, -102, 51}),
                            .dimensions = {3, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.00392157f,
                            .type = TestOperandType::TENSOR_QUANT8_SYMM,
                            .zeroPoint = 0
                        }, { // projection_bias
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0}),
                            .dimensions = {3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // output_state_in
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // cell_state_in
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({0, 0, 0, 0, 0, 0, 0, 0}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // input_layer_norm_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05182e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // forget_layer_norm_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({6553, 6553, 13107, 9830}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05182e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // cell_layer_norm_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({22937, 6553, 9830, 26214}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05182e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output_layer_norm_weights
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({19660, 6553, 6553, 16384}),
                            .dimensions = {4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05182e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // cell_clip
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // projection_clip
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // input_intermediate_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007059f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // forget_intermediate_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007812f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // cell_intermediate_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007059f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // output_intermediate_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007812f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // hidden_state_zero_point
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // hidden_state_scale
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.007f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // output_state_out
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, -128, 127, 127}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // cell_state_out
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int16_t>({-11692, 9960, 5491, 8861, -9422, 7726, 2056, 13149}),
                            .dimensions = {2, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT16_SYMM,
                            .zeroPoint = 0
                        }, { // output
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({127, 127, 127, -128, 127, 127}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // input_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({90, 102, 13, 26, 38, 102, 13, 26, 51, 64}),
                            .dimensions = {2, 5},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // dummy2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0078125f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
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
                        }, { // output_state_in_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0, 0, 0, 0, 0, 0}),
                            .dimensions = {2, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // dummy3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 3.05176e-05f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
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
                            .inputs = {35, 36, 37},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {38, 39, 40},
                            .outputs = {18},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31},
                            .outputs = {32, 33, 34},
                            .type = TestOperationType::QUANTIZED_LSTM
                        }},
                .outputIndexes = {32, 33, 34}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_2 = TestModelManager::get().add("qlstm_projection_all_inputs_as_internal_2", get_test_model_all_inputs_as_internal_2());

}  // namespace generated_tests::qlstm_projection

