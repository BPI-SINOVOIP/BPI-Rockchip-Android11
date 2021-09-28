// Generated from resize_bilinear_v1_3.mod.py
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 1.0f, 2.0f, 3.0f, 4.0f}),
                            .dimensions = {2, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
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
                        }, { // align_corners
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 1.5f, 2.0f, 2.0f, 2.5f, 3.0f, 3.0f, 3.5f, 4.0f, 1.0f, 1.5f, 2.0f, 2.0f, 2.5f, 3.0f, 3.0f, 3.5f, 4.0f}),
                            .dimensions = {2, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1 = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x2x2x1_to_2x3x3x1", get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {2, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
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
                        }, { // align_corners
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 1.5f, 2.0f, 2.0f, 2.5f, 3.0f, 3.0f, 3.5f, 4.0f, 1.0f, 1.5f, 2.0f, 2.0f, 2.5f, 3.0f, 3.0f, 3.5f, 4.0f}),
                            .dimensions = {2, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input0_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 1.0f, 2.0f, 3.0f, 4.0f}),
                            .dimensions = {2, 2, 2, 1},
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x2x2x1_to_2x3x3x1_all_inputs_as_internal", get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 2.0f, 3.0f, 4.0f, 1.0f, 2.0f, 3.0f, 4.0f}),
                            .dimensions = {2, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output_width
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
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
                        }, { // align_corners
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 1.5f, 2.0f, 2.0f, 2.5f, 3.0f, 3.0f, 3.5f, 4.0f, 1.0f, 1.5f, 2.0f, 2.0f, 2.5f, 3.0f, 3.0f, 3.5f, 4.0f}),
                            .dimensions = {2, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_float16 = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x2x2x1_to_2x3x3x1_float16", get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_float16());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_float16_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({}),
                            .dimensions = {2, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output_width
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
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
                        }, { // align_corners
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 1.5f, 2.0f, 2.0f, 2.5f, 3.0f, 3.0f, 3.5f, 4.0f, 1.0f, 1.5f, 2.0f, 2.0f, 2.5f, 3.0f, 3.0f, 3.5f, 4.0f}),
                            .dimensions = {2, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // input0_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 2.0f, 3.0f, 4.0f, 1.0f, 2.0f, 3.0f, 4.0f}),
                            .dimensions = {2, 2, 2, 1},
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_float16_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x2x2x1_to_2x3x3x1_float16_all_inputs_as_internal", get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_float16_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 1.0f, 2.0f, 3.0f, 4.0f}),
                            .dimensions = {2, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
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
                        }, { // align_corners
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 1.5f, 2.0f, 2.0f, 2.5f, 3.0f, 3.0f, 3.5f, 4.0f, 1.0f, 1.5f, 2.0f, 2.0f, 2.5f, 3.0f, 3.0f, 3.5f, 4.0f}),
                            .dimensions = {2, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_relaxed = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x2x2x1_to_2x3x3x1_relaxed", get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_relaxed());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_relaxed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {2, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
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
                        }, { // align_corners
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 1.5f, 2.0f, 2.0f, 2.5f, 3.0f, 3.0f, 3.5f, 4.0f, 1.0f, 1.5f, 2.0f, 2.0f, 2.5f, 3.0f, 3.0f, 3.5f, 4.0f}),
                            .dimensions = {2, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input0_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 1.0f, 2.0f, 3.0f, 4.0f}),
                            .dimensions = {2, 2, 2, 1},
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_relaxed_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x2x2x1_to_2x3x3x1_relaxed_all_inputs_as_internal", get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_relaxed_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_quant8() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130, 132, 134, 136, 130, 132, 134, 136}),
                            .dimensions = {2, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // output_width
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
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
                        }, { // align_corners
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130, 131, 132, 132, 133, 134, 134, 135, 136, 130, 131, 132, 132, 133, 134, 134, 135, 136}),
                            .dimensions = {2, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_quant8 = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x2x2x1_to_2x3x3x1_quant8", get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_quant8());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_quant8_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {2, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // output_width
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
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
                        }, { // align_corners
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130, 131, 132, 132, 133, 134, 134, 135, 136, 130, 131, 132, 132, 133, 134, 134, 135, 136}),
                            .dimensions = {2, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // input0_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130, 132, 134, 136, 130, 132, 134, 136}),
                            .dimensions = {2, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_quant8_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x2x2x1_to_2x3x3x1_quant8_all_inputs_as_internal", get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_quant8_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2, 4, 6, 8, 2, 4, 6, 8}),
                            .dimensions = {2, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // output_width
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
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
                        }, { // align_corners
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2, 3, 4, 4, 5, 6, 6, 7, 8, 2, 3, 4, 4, 5, 6, 6, 7, 8}),
                            .dimensions = {2, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_quant8_signed = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x2x2x1_to_2x3x3x1_quant8_signed", get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_quant8_signed());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {2, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // output_width
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
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
                        }, { // align_corners
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output0
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2, 3, 4, 4, 5, 6, 6, 7, 8, 2, 3, 4, 4, 5, 6, 6, 7, 8}),
                            .dimensions = {2, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // input0_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2, 4, 6, 8, 2, 4, 6, 8}),
                            .dimensions = {2, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // dummy4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x2x2x1_to_2x3x3x1_quant8_signed_all_inputs_as_internal", get_test_model_half_pixel_centers_2x2x2x1_to_2x3x3x1_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x14x13x3_to_2x6x7x3() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.827637f, 0.98301f, 0.250087f, 0.575817f, 0.063061f, 0.534553f, 0.675679f, 0.694844f, 0.497918f, 0.664793f, 0.0200533f, 0.577735f, 0.814857f, 0.843088f, 0.328894f, 0.700158f, 0.540654f, 0.483906f, 0.0713209f, 0.231078f, 0.0430164f, 0.146823f, 0.556193f, 0.372893f, 0.42612f, 0.640223f, 0.326292f, 0.489664f, 0.267468f, 0.413154f, 0.88774f, 0.610036f, 0.942792f, 0.23379f, 0.0979913f, 0.458068f, 0.223809f, 0.0780525f, 0.770556f, 0.391897f, 0.891485f, 0.364972f, 0.847238f, 0.428266f, 0.660148f, 0.990963f, 0.670023f, 0.982757f, 0.0835382f, 0.208294f, 0.689837f, 0.673272f, 0.317975f, 0.382154f, 0.368691f, 0.408292f, 0.0825884f, 0.979362f, 0.667223f, 0.452756f, 0.531345f, 0.361022f, 0.974065f, 0.203924f, 0.0682611f, 0.930153f, 0.272573f, 0.398286f, 0.582229f, 0.552098f, 0.236405f, 0.831928f, 0.253404f, 0.102948f, 0.701941f, 0.472118f, 0.415567f, 0.830793f, 0.995918f, 0.873392f, 0.148214f, 0.385363f, 0.900278f, 0.0703746f, 0.108f, 0.528804f, 0.944798f, 0.949568f, 0.122064f, 0.840799f, 0.532888f, 0.0518012f, 0.966821f, 0.611478f, 0.0889368f, 0.591055f, 0.71221f, 0.399697f, 0.736551f, 0.675313f, 0.0995619f, 0.975917f, 0.329392f, 0.513981f, 0.563206f, 0.86733f, 0.716193f, 0.2221f, 0.215225f, 0.226138f, 0.661863f, 0.465466f, 0.164099f, 0.807117f, 0.22327f, 0.0895369f, 0.982572f, 0.429804f, 0.0558029f, 0.799344f, 0.169512f, 0.569326f, 0.135653f, 0.777692f, 0.11906f, 0.362015f, 0.40525f, 0.0627866f, 0.515949f, 0.000611305f, 0.583503f, 0.947306f, 0.869187f, 0.869985f, 0.945147f, 0.570262f, 0.709512f, 0.0355313f, 0.446349f, 0.80268f, 0.766533f, 0.161885f, 0.0908636f, 0.450652f, 0.111231f, 0.346606f, 0.84161f, 0.524124f, 0.607721f, 0.393173f, 0.103109f, 0.943106f, 0.453668f, 0.598608f, 0.323443f, 0.79512f, 0.227289f, 0.13272f, 0.944727f, 0.653672f, 0.688597f, 0.40432f, 0.0568643f, 0.568614f, 0.962205f, 0.94967f, 0.0944915f, 0.10954f, 0.333137f, 0.815286f, 0.0110344f, 0.3073f, 0.661422f, 0.884207f, 0.744278f, 0.89397f, 0.00762653f, 0.703588f, 0.812244f, 0.194066f, 0.174366f, 0.884352f, 0.898688f, 0.114173f, 0.479097f, 0.452601f, 0.00374603f, 0.791901f, 0.0691f, 0.688598f, 0.329226f, 0.872065f, 0.056097f, 0.810847f, 0.0154784f, 0.986826f, 0.133701f, 0.84835f, 0.301012f, 0.429658f, 0.434824f, 0.63403f, 0.109551f, 0.594964f, 0.414044f, 0.512716f, 0.986874f, 0.365579f, 0.129968f, 0.553444f, 0.518211f, 0.111823f, 0.290679f, 0.335546f, 0.0241963f, 0.420873f, 0.831382f, 0.0305338f, 0.779728f, 0.351471f, 0.606134f, 0.0897753f, 0.118784f, 0.761163f, 0.641289f, 0.883826f, 0.877834f, 0.854164f, 0.725214f, 0.69578f, 0.913621f, 0.912614f, 0.81458f, 0.547955f, 0.0563488f, 0.976676f, 0.0151438f, 0.767199f, 0.173275f, 0.543553f, 0.0929028f, 0.471326f, 0.181049f, 0.0777475f, 0.278217f, 0.101287f, 0.421686f, 0.55636f, 0.344953f, 0.681681f, 0.01902f, 0.646946f, 0.310552f, 0.622743f, 0.709501f, 0.0542059f, 0.559718f, 0.0417655f, 0.847932f, 0.35674f, 0.280232f, 0.605592f, 0.267366f, 0.180051f, 0.522576f, 0.039578f, 0.717534f, 0.109444f, 0.384435f, 0.365782f, 0.800604f, 0.000903726f, 0.11138f, 0.615857f, 0.335662f, 0.824687f, 0.909823f, 0.479827f, 0.509285f, 0.748855f, 0.546289f, 0.240599f, 0.135308f, 0.487517f, 0.543101f, 0.918314f, 0.736137f, 0.167971f, 0.907674f, 0.86526f, 0.92979f, 0.347061f, 0.0289562f, 0.183843f, 0.109654f, 0.25219f, 0.913093f, 0.211304f, 0.0239445f, 0.321891f, 0.0132581f, 0.491192f, 0.0598186f, 0.638829f, 0.922965f, 0.970296f, 0.688952f, 0.129948f, 0.147574f, 0.260364f, 0.556817f, 0.676236f, 0.967277f, 0.741944f, 0.455842f, 0.372167f, 0.594965f, 0.194687f, 0.110991f, 0.828473f, 0.81903f, 0.300324f, 0.799584f, 0.888032f, 0.401994f, 0.751757f, 0.341877f, 0.168591f, 0.958628f, 0.989044f, 0.368874f, 0.499499f, 0.984334f, 0.405659f, 0.259569f, 0.151238f, 0.764487f, 0.784618f, 0.441848f, 0.972594f, 0.131611f, 0.0371498f, 0.0693474f, 0.416801f, 0.324805f, 0.0487738f, 0.336595f, 0.00118566f, 0.484068f, 0.856299f, 0.780161f, 0.168135f, 0.902528f, 0.00840139f, 0.0114626f, 0.363331f, 0.368221f, 0.560426f, 0.19353f, 0.724254f, 0.954187f, 0.68854f, 0.160637f, 0.777354f, 0.692321f, 0.157433f, 0.369148f, 0.192732f, 0.949813f, 0.307579f, 0.958941f, 0.483992f, 0.602389f, 0.401443f, 0.250001f, 0.000342846f, 0.120461f, 0.875903f, 0.407285f, 0.973199f, 0.0558323f, 0.293595f, 0.362371f, 0.619545f, 0.366578f, 0.970044f, 0.883848f, 0.314051f, 0.272863f, 0.910542f, 0.335767f, 0.364503f, 0.217633f, 0.586304f, 0.62947f, 0.93676f, 0.747138f, 0.81246f, 0.643208f, 0.740491f, 0.55069f, 0.336218f, 0.980699f, 0.261752f, 0.0782728f, 0.45507f, 0.485201f, 0.443404f, 0.80983f, 0.146657f, 0.313562f, 0.137156f, 0.634548f, 0.418098f, 0.592411f, 0.0155032f, 0.336527f, 0.273182f, 0.306312f, 0.4013f, 0.519121f, 0.403324f, 0.0888798f, 0.553309f, 0.637661f, 0.643027f, 0.711509f, 0.169574f, 0.531634f, 0.184579f, 0.802464f, 0.262788f, 0.582185f, 0.838128f, 0.903281f, 0.89136f, 0.400026f, 0.962787f, 0.393862f, 0.0912223f, 0.924886f, 0.732427f, 0.922716f, 0.320009f, 0.017068f, 0.246307f, 0.0766512f, 0.28073f, 0.956669f, 0.000580311f, 0.0147009f, 0.468664f, 0.606133f, 0.589364f, 0.960154f, 0.457708f, 0.0992912f, 0.229694f, 0.722875f, 0.0905478f, 0.492376f, 0.460984f, 0.822356f, 0.631099f, 0.120013f, 0.9392f, 0.025933f, 0.493204f, 0.0777059f, 0.996434f, 0.90678f, 0.570172f, 0.137066f, 0.587406f, 0.152012f, 0.330419f, 0.597728f, 0.653196f, 0.210094f, 0.0150489f, 0.499774f, 0.512619f, 0.662554f, 0.0727493f, 0.902094f, 0.35155f, 0.660929f, 0.538246f, 0.493768f, 0.436716f, 0.347757f, 0.718451f, 0.0751067f, 0.591153f, 0.476367f, 0.871649f, 0.770177f, 0.49412f, 0.207645f, 0.300805f, 0.944687f, 0.540949f, 0.334802f, 0.658609f, 0.280322f, 0.0614085f, 0.523165f, 0.480998f, 0.452187f, 0.396826f, 0.814263f, 0.444388f, 0.0979315f, 0.613517f, 0.591663f, 0.301699f, 0.0100771f, 0.646675f, 0.775883f, 0.0320501f, 0.362856f, 0.0123467f, 0.335263f, 0.785518f, 0.0548519f, 0.236233f, 0.982908f, 0.397743f, 0.196763f, 0.273222f, 0.738824f, 0.921409f, 0.746146f, 0.184881f, 0.780506f, 0.790668f, 0.838174f, 0.140729f, 0.344877f, 0.437886f, 0.270443f, 0.113299f, 0.0567528f, 0.872003f, 0.461713f, 0.396225f, 0.334525f, 0.571957f, 0.406145f, 0.655128f, 0.111169f, 0.512328f, 0.579207f, 0.681437f, 0.313002f, 0.526335f, 0.682192f, 0.543195f, 0.883647f, 0.680931f, 0.936027f, 0.887212f, 0.254619f, 0.537763f, 0.929323f, 0.899553f, 0.583033f, 0.479059f, 0.58146f, 0.605686f, 0.0800954f, 0.554806f, 0.805312f, 0.730701f, 0.461951f, 0.861251f, 0.338622f, 0.949928f, 0.676404f, 0.779811f, 0.66965f, 0.413489f, 0.244133f, 0.253003f, 0.668361f, 0.861042f, 0.111543f, 0.374333f, 0.620414f, 0.730657f, 0.257099f, 0.992287f, 0.962735f, 0.48282f, 0.103903f, 0.120562f, 0.371291f, 0.828842f, 0.19805f, 0.3552f, 0.139294f, 0.983518f, 0.773706f, 0.304598f, 0.321077f, 0.148372f, 0.975174f, 0.902274f, 0.021295f, 0.109916f, 0.0552523f, 0.129696f, 0.834911f, 0.35488f, 0.332393f, 0.638469f, 0.505043f, 0.607849f, 0.444755f, 0.507821f, 0.0467358f, 0.966812f, 0.403f, 0.907803f, 0.491011f, 0.330456f, 0.400001f, 0.246176f, 0.0303432f, 0.744088f, 0.712685f, 0.382909f, 0.0748539f, 0.0552307f, 0.250542f, 0.432398f, 0.269405f, 0.965367f, 0.255643f, 0.00846922f, 0.477259f, 0.956944f, 0.263619f, 0.472611f, 0.818297f, 0.637181f, 0.837281f, 0.29029f, 0.52285f, 0.411453f, 0.327927f, 0.493403f, 0.292899f, 0.326031f, 0.0675526f, 0.043205f, 0.117051f, 0.782362f, 0.974107f, 0.713485f, 0.192605f, 0.757342f, 0.0791711f, 0.773982f, 0.918075f, 0.0538017f, 0.0902326f, 0.385477f, 0.61089f, 0.893094f, 0.278611f, 0.749566f, 0.297577f, 0.343f, 0.700941f, 0.021899f, 0.785716f, 0.575491f, 0.571529f, 0.895896f, 0.540937f, 0.686329f, 0.519124f, 0.214197f, 0.257485f, 0.479406f, 0.723769f, 0.133132f, 0.654102f, 0.464483f, 0.520407f, 0.739025f, 0.425871f, 0.0430206f, 0.367081f, 0.819075f, 0.502319f, 0.808982f, 0.586205f, 0.693161f, 0.578652f, 0.670991f, 0.564139f, 0.168943f, 0.578697f, 0.0289714f, 0.331961f, 0.0185615f, 0.981341f, 0.79096f, 0.683865f, 0.203928f, 0.860001f, 0.630191f, 0.382722f, 0.887468f, 0.209518f, 0.582372f, 0.196313f, 0.0263103f, 0.305729f, 0.76197f, 0.52452f, 0.178965f, 0.719016f, 0.605278f, 0.117012f, 0.838881f, 0.10849f, 0.0151021f, 0.695286f, 0.134018f, 0.923889f, 0.361624f, 0.801152f, 0.345261f, 0.992369f, 0.279426f, 0.574633f, 0.724684f, 0.264346f, 0.216093f, 0.622767f, 0.908065f, 0.882313f, 0.796358f, 0.154155f, 0.308829f, 0.7388f, 0.483272f, 0.726135f, 0.0916826f, 0.831043f, 0.147788f, 0.982867f, 0.639764f, 0.308342f, 0.480933f, 0.682025f, 0.0305799f, 0.448529f, 0.485899f, 0.684693f, 0.604456f, 0.221594f, 0.461711f, 0.350639f, 0.58591f, 0.959523f, 0.831963f, 0.914305f, 0.361302f, 0.620165f, 0.268338f, 0.296164f, 0.0403097f, 0.822701f, 0.809845f, 0.524713f, 0.645126f, 0.207234f, 0.188755f, 0.135637f, 0.756508f, 0.171036f, 0.332887f, 0.513941f, 0.154996f, 0.555009f, 0.98974f, 0.303486f, 0.473306f, 0.430805f, 0.187765f, 0.377134f, 0.612192f, 0.902819f, 0.29555f, 0.409627f, 0.869718f, 0.226232f, 0.756249f, 0.935133f, 0.546867f, 0.384815f, 0.188028f, 0.750887f, 0.310038f, 0.44034f, 0.824127f, 0.747097f, 0.128963f, 0.24836f, 0.528062f, 0.553374f, 0.689335f, 0.126468f, 0.175476f, 0.975612f, 0.586703f, 0.671013f, 0.962735f, 0.808907f, 0.0861794f, 0.533115f, 0.101796f, 0.0706959f, 0.0917822f, 0.0471025f, 0.777679f, 0.952353f, 0.385382f, 0.820037f, 0.815421f, 0.47614f, 0.424147f, 0.946672f, 0.910313f, 0.884953f, 0.476174f, 0.426554f, 0.985588f, 0.032336f, 0.383584f, 0.0543795f, 0.745913f, 0.93714f, 0.984151f, 0.643788f, 0.680333f, 0.660295f, 0.451879f, 0.81687f, 0.423028f, 0.0934314f, 0.317183f, 0.32431f, 0.798049f, 0.301885f, 0.482865f, 0.0412549f, 0.305506f, 0.498927f, 0.183137f, 0.440198f, 0.708901f, 0.142019f, 0.74945f, 0.744077f, 0.406925f, 0.502505f, 0.977357f, 0.981186f, 0.454713f, 0.694598f, 0.583974f, 0.0061208f, 0.313697f, 0.384455f, 0.141156f, 0.121607f, 0.225996f, 0.252465f, 0.0127006f, 0.692692f, 0.542434f, 0.385375f, 0.0158385f, 0.469898f, 0.88728f, 0.580118f, 0.516798f, 0.124881f, 0.17155f, 0.10754f, 0.64776f, 0.863566f, 0.585709f, 0.737403f, 0.881578f, 0.768254f, 0.926182f, 0.734802f, 0.0924621f, 0.560301f, 0.973171f, 0.197622f, 0.39735f, 0.831218f, 0.831679f, 0.626448f, 0.255363f, 0.221987f, 0.510817f, 0.119718f, 0.457004f, 0.751342f, 0.682048f, 0.925028f, 0.472245f, 0.946413f, 0.702793f, 0.849434f, 0.835024f, 0.757633f, 0.227651f, 0.15449f, 0.581614f, 0.344011f, 0.27533f, 0.861278f, 0.0480329f, 0.296223f, 0.639267f, 0.422976f, 0.0219772f, 0.547381f, 0.143436f, 0.483599f, 0.922199f, 0.657291f, 0.968148f, 0.826014f, 0.0362797f, 0.285515f, 0.754099f, 0.819942f, 0.07358f, 0.485557f, 0.0095253f, 0.381697f, 0.690303f, 0.166019f, 0.162123f, 0.746008f, 0.317472f, 0.684392f, 0.824646f, 0.189942f, 0.155815f, 0.865175f, 0.508899f, 0.498846f, 0.510297f, 0.314311f, 0.0580601f, 0.25675f, 0.849481f, 0.997329f, 0.867238f, 0.255343f, 0.575945f, 0.929926f, 0.164465f, 0.382339f, 0.0175594f, 0.531675f, 0.133809f, 0.424927f, 0.932461f, 0.47343f, 0.670699f, 0.451685f, 0.727976f, 0.157594f, 0.668399f, 0.431421f, 0.692803f, 0.309855f, 0.320758f, 0.600577f, 0.851317f, 0.110719f, 0.72173f, 0.052147f, 0.508466f, 0.331822f, 0.732259f, 0.300817f, 0.709938f, 0.341123f, 0.698617f, 0.212518f, 0.202616f, 0.018757f, 0.177552f, 0.613048f, 0.758948f, 0.547059f, 0.400971f, 0.115883f, 0.256621f, 0.727341f, 0.47802f, 0.377706f, 0.344168f, 0.903898f, 0.485407f, 0.654939f, 0.98362f, 0.858975f, 0.575978f, 0.698721f, 0.721705f, 0.841963f, 0.209065f, 0.43947f, 0.626849f, 0.616108f, 0.503407f, 0.551573f, 0.393978f, 0.0142356f, 0.1245f, 0.0788039f, 0.329059f, 0.540094f, 0.963904f, 0.579622f, 0.806494f, 0.378628f, 0.793516f, 0.649193f, 0.151952f, 0.572264f, 0.460979f, 0.198079f, 0.932621f, 0.719997f, 0.919287f, 0.0693846f, 0.840908f, 0.843045f, 0.055732f, 0.457907f, 0.992388f, 0.945144f, 0.943088f, 0.676341f, 0.553268f, 0.586761f, 0.224883f, 0.861508f, 0.566519f, 0.911452f, 0.591536f, 0.0733222f, 0.170432f, 0.351752f, 0.157091f, 0.327673f, 0.868404f, 0.808417f, 0.569937f, 0.453585f, 0.498946f, 0.80573f, 0.626621f, 0.349575f, 0.642979f, 0.53113f, 0.138728f, 0.4121f, 0.425972f, 0.807962f, 0.831206f, 0.839713f, 0.129973f, 0.553252f, 0.147851f, 0.733317f, 0.196179f}),
                            .dimensions = {2, 14, 13, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({7}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({6}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.631267f, 0.658224f, 0.451651f, 0.711989f, 0.526083f, 0.772951f, 0.685998f, 0.487213f, 0.343252f, 0.676681f, 0.521841f, 0.316176f, 0.295872f, 0.282796f, 0.735267f, 0.572824f, 0.359158f, 0.770879f, 0.328285f, 0.216591f, 0.729258f, 0.318251f, 0.275884f, 0.514905f, 0.67128f, 0.102982f, 0.405996f, 0.124694f, 0.624429f, 0.946997f, 0.570262f, 0.709512f, 0.0355313f, 0.202523f, 0.192552f, 0.495778f, 0.406154f, 0.533116f, 0.521298f, 0.386251f, 0.589013f, 0.648783f, 0.58227f, 0.38448f, 0.527951f, 0.410309f, 0.288544f, 0.524046f, 0.411198f, 0.244783f, 0.473302f, 0.327214f, 0.223712f, 0.517082f, 0.524867f, 0.477849f, 0.207615f, 0.625594f, 0.597887f, 0.716884f, 0.722997f, 0.564202f, 0.543249f, 0.722101f, 0.585117f, 0.459117f, 0.301243f, 0.723311f, 0.668172f, 0.551366f, 0.772401f, 0.38182f, 0.361603f, 0.342651f, 0.717503f, 0.541077f, 0.491924f, 0.402338f, 0.371644f, 0.490079f, 0.398466f, 0.346357f, 0.547003f, 0.447432f, 0.44314f, 0.47806f, 0.52583f, 0.745239f, 0.737669f, 0.555497f, 0.905609f, 0.293674f, 0.130434f, 0.80983f, 0.146657f, 0.313562f, 0.527374f, 0.103938f, 0.34818f, 0.448853f, 0.375606f, 0.178143f, 0.643709f, 0.370183f, 0.579374f, 0.40045f, 0.568371f, 0.602847f, 0.34384f, 0.39982f, 0.433225f, 0.317636f, 0.519918f, 0.418223f, 0.519364f, 0.686467f, 0.46176f, 0.357139f, 0.597116f, 0.428639f, 0.372748f, 0.447356f, 0.728054f, 0.466077f, 0.223182f, 0.471054f, 0.662586f, 0.376077f, 0.36661f, 0.680342f, 0.708627f, 0.426894f, 0.781609f, 0.413516f, 0.569702f, 0.51284f, 0.513902f, 0.293259f, 0.20138f, 0.303709f, 0.335072f, 0.613278f, 0.578262f, 0.595837f, 0.653285f, 0.442471f, 0.545559f, 0.480946f, 0.689819f, 0.292554f, 0.722947f, 0.297008f, 0.735673f, 0.100418f, 0.801456f, 0.570554f, 0.686329f, 0.519124f, 0.214197f, 0.150897f, 0.629145f, 0.501524f, 0.179417f, 0.47335f, 0.706731f, 0.611372f, 0.677365f, 0.634654f, 0.481956f, 0.525163f, 0.463365f, 0.502f, 0.43508f, 0.565645f, 0.266055f, 0.70408f, 0.55236f, 0.156929f, 0.588925f, 0.399344f, 0.475003f, 0.356185f, 0.59649f, 0.562223f, 0.526428f, 0.439555f, 0.492657f, 0.323885f, 0.765127f, 0.429603f, 0.466623f, 0.327579f, 0.381093f, 0.570198f, 0.257691f, 0.463234f, 0.879136f, 0.894351f, 0.307519f, 0.504116f, 0.415028f, 0.287617f, 0.0813091f, 0.632005f, 0.578317f, 0.746861f, 0.448167f, 0.365736f, 0.404652f, 0.543348f, 0.708017f, 0.363659f, 0.691263f, 0.805467f, 0.260183f, 0.224962f, 0.514807f, 0.0318816f, 0.350329f, 0.746008f, 0.317472f, 0.684392f, 0.859385f, 0.463334f, 0.449842f, 0.329192f, 0.696575f, 0.728967f, 0.90306f, 0.203413f, 0.465313f, 0.428212f, 0.540184f, 0.911023f, 0.760282f, 0.628308f, 0.512895f, 0.484829f, 0.43916f, 0.453882f, 0.298958f, 0.352472f, 0.242515f, 0.563185f, 0.635929f, 0.530235f, 0.575659f, 0.548754f, 0.36042f, 0.441017f, 0.684306f, 0.700217f}),
                            .dimensions = {2, 6, 7, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x14x13x3_to_2x6x7x3 = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x14x13x3_to_2x6x7x3", get_test_model_half_pixel_centers_2x14x13x3_to_2x6x7x3());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x14x13x3_to_2x6x7x3_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {2, 14, 13, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({7}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({6}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.631267f, 0.658224f, 0.451651f, 0.711989f, 0.526083f, 0.772951f, 0.685998f, 0.487213f, 0.343252f, 0.676681f, 0.521841f, 0.316176f, 0.295872f, 0.282796f, 0.735267f, 0.572824f, 0.359158f, 0.770879f, 0.328285f, 0.216591f, 0.729258f, 0.318251f, 0.275884f, 0.514905f, 0.67128f, 0.102982f, 0.405996f, 0.124694f, 0.624429f, 0.946997f, 0.570262f, 0.709512f, 0.0355313f, 0.202523f, 0.192552f, 0.495778f, 0.406154f, 0.533116f, 0.521298f, 0.386251f, 0.589013f, 0.648783f, 0.58227f, 0.38448f, 0.527951f, 0.410309f, 0.288544f, 0.524046f, 0.411198f, 0.244783f, 0.473302f, 0.327214f, 0.223712f, 0.517082f, 0.524867f, 0.477849f, 0.207615f, 0.625594f, 0.597887f, 0.716884f, 0.722997f, 0.564202f, 0.543249f, 0.722101f, 0.585117f, 0.459117f, 0.301243f, 0.723311f, 0.668172f, 0.551366f, 0.772401f, 0.38182f, 0.361603f, 0.342651f, 0.717503f, 0.541077f, 0.491924f, 0.402338f, 0.371644f, 0.490079f, 0.398466f, 0.346357f, 0.547003f, 0.447432f, 0.44314f, 0.47806f, 0.52583f, 0.745239f, 0.737669f, 0.555497f, 0.905609f, 0.293674f, 0.130434f, 0.80983f, 0.146657f, 0.313562f, 0.527374f, 0.103938f, 0.34818f, 0.448853f, 0.375606f, 0.178143f, 0.643709f, 0.370183f, 0.579374f, 0.40045f, 0.568371f, 0.602847f, 0.34384f, 0.39982f, 0.433225f, 0.317636f, 0.519918f, 0.418223f, 0.519364f, 0.686467f, 0.46176f, 0.357139f, 0.597116f, 0.428639f, 0.372748f, 0.447356f, 0.728054f, 0.466077f, 0.223182f, 0.471054f, 0.662586f, 0.376077f, 0.36661f, 0.680342f, 0.708627f, 0.426894f, 0.781609f, 0.413516f, 0.569702f, 0.51284f, 0.513902f, 0.293259f, 0.20138f, 0.303709f, 0.335072f, 0.613278f, 0.578262f, 0.595837f, 0.653285f, 0.442471f, 0.545559f, 0.480946f, 0.689819f, 0.292554f, 0.722947f, 0.297008f, 0.735673f, 0.100418f, 0.801456f, 0.570554f, 0.686329f, 0.519124f, 0.214197f, 0.150897f, 0.629145f, 0.501524f, 0.179417f, 0.47335f, 0.706731f, 0.611372f, 0.677365f, 0.634654f, 0.481956f, 0.525163f, 0.463365f, 0.502f, 0.43508f, 0.565645f, 0.266055f, 0.70408f, 0.55236f, 0.156929f, 0.588925f, 0.399344f, 0.475003f, 0.356185f, 0.59649f, 0.562223f, 0.526428f, 0.439555f, 0.492657f, 0.323885f, 0.765127f, 0.429603f, 0.466623f, 0.327579f, 0.381093f, 0.570198f, 0.257691f, 0.463234f, 0.879136f, 0.894351f, 0.307519f, 0.504116f, 0.415028f, 0.287617f, 0.0813091f, 0.632005f, 0.578317f, 0.746861f, 0.448167f, 0.365736f, 0.404652f, 0.543348f, 0.708017f, 0.363659f, 0.691263f, 0.805467f, 0.260183f, 0.224962f, 0.514807f, 0.0318816f, 0.350329f, 0.746008f, 0.317472f, 0.684392f, 0.859385f, 0.463334f, 0.449842f, 0.329192f, 0.696575f, 0.728967f, 0.90306f, 0.203413f, 0.465313f, 0.428212f, 0.540184f, 0.911023f, 0.760282f, 0.628308f, 0.512895f, 0.484829f, 0.43916f, 0.453882f, 0.298958f, 0.352472f, 0.242515f, 0.563185f, 0.635929f, 0.530235f, 0.575659f, 0.548754f, 0.36042f, 0.441017f, 0.684306f, 0.700217f}),
                            .dimensions = {2, 6, 7, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input01_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.827637f, 0.98301f, 0.250087f, 0.575817f, 0.063061f, 0.534553f, 0.675679f, 0.694844f, 0.497918f, 0.664793f, 0.0200533f, 0.577735f, 0.814857f, 0.843088f, 0.328894f, 0.700158f, 0.540654f, 0.483906f, 0.0713209f, 0.231078f, 0.0430164f, 0.146823f, 0.556193f, 0.372893f, 0.42612f, 0.640223f, 0.326292f, 0.489664f, 0.267468f, 0.413154f, 0.88774f, 0.610036f, 0.942792f, 0.23379f, 0.0979913f, 0.458068f, 0.223809f, 0.0780525f, 0.770556f, 0.391897f, 0.891485f, 0.364972f, 0.847238f, 0.428266f, 0.660148f, 0.990963f, 0.670023f, 0.982757f, 0.0835382f, 0.208294f, 0.689837f, 0.673272f, 0.317975f, 0.382154f, 0.368691f, 0.408292f, 0.0825884f, 0.979362f, 0.667223f, 0.452756f, 0.531345f, 0.361022f, 0.974065f, 0.203924f, 0.0682611f, 0.930153f, 0.272573f, 0.398286f, 0.582229f, 0.552098f, 0.236405f, 0.831928f, 0.253404f, 0.102948f, 0.701941f, 0.472118f, 0.415567f, 0.830793f, 0.995918f, 0.873392f, 0.148214f, 0.385363f, 0.900278f, 0.0703746f, 0.108f, 0.528804f, 0.944798f, 0.949568f, 0.122064f, 0.840799f, 0.532888f, 0.0518012f, 0.966821f, 0.611478f, 0.0889368f, 0.591055f, 0.71221f, 0.399697f, 0.736551f, 0.675313f, 0.0995619f, 0.975917f, 0.329392f, 0.513981f, 0.563206f, 0.86733f, 0.716193f, 0.2221f, 0.215225f, 0.226138f, 0.661863f, 0.465466f, 0.164099f, 0.807117f, 0.22327f, 0.0895369f, 0.982572f, 0.429804f, 0.0558029f, 0.799344f, 0.169512f, 0.569326f, 0.135653f, 0.777692f, 0.11906f, 0.362015f, 0.40525f, 0.0627866f, 0.515949f, 0.000611305f, 0.583503f, 0.947306f, 0.869187f, 0.869985f, 0.945147f, 0.570262f, 0.709512f, 0.0355313f, 0.446349f, 0.80268f, 0.766533f, 0.161885f, 0.0908636f, 0.450652f, 0.111231f, 0.346606f, 0.84161f, 0.524124f, 0.607721f, 0.393173f, 0.103109f, 0.943106f, 0.453668f, 0.598608f, 0.323443f, 0.79512f, 0.227289f, 0.13272f, 0.944727f, 0.653672f, 0.688597f, 0.40432f, 0.0568643f, 0.568614f, 0.962205f, 0.94967f, 0.0944915f, 0.10954f, 0.333137f, 0.815286f, 0.0110344f, 0.3073f, 0.661422f, 0.884207f, 0.744278f, 0.89397f, 0.00762653f, 0.703588f, 0.812244f, 0.194066f, 0.174366f, 0.884352f, 0.898688f, 0.114173f, 0.479097f, 0.452601f, 0.00374603f, 0.791901f, 0.0691f, 0.688598f, 0.329226f, 0.872065f, 0.056097f, 0.810847f, 0.0154784f, 0.986826f, 0.133701f, 0.84835f, 0.301012f, 0.429658f, 0.434824f, 0.63403f, 0.109551f, 0.594964f, 0.414044f, 0.512716f, 0.986874f, 0.365579f, 0.129968f, 0.553444f, 0.518211f, 0.111823f, 0.290679f, 0.335546f, 0.0241963f, 0.420873f, 0.831382f, 0.0305338f, 0.779728f, 0.351471f, 0.606134f, 0.0897753f, 0.118784f, 0.761163f, 0.641289f, 0.883826f, 0.877834f, 0.854164f, 0.725214f, 0.69578f, 0.913621f, 0.912614f, 0.81458f, 0.547955f, 0.0563488f, 0.976676f, 0.0151438f, 0.767199f, 0.173275f, 0.543553f, 0.0929028f, 0.471326f, 0.181049f, 0.0777475f, 0.278217f, 0.101287f, 0.421686f, 0.55636f, 0.344953f, 0.681681f, 0.01902f, 0.646946f, 0.310552f, 0.622743f, 0.709501f, 0.0542059f, 0.559718f, 0.0417655f, 0.847932f, 0.35674f, 0.280232f, 0.605592f, 0.267366f, 0.180051f, 0.522576f, 0.039578f, 0.717534f, 0.109444f, 0.384435f, 0.365782f, 0.800604f, 0.000903726f, 0.11138f, 0.615857f, 0.335662f, 0.824687f, 0.909823f, 0.479827f, 0.509285f, 0.748855f, 0.546289f, 0.240599f, 0.135308f, 0.487517f, 0.543101f, 0.918314f, 0.736137f, 0.167971f, 0.907674f, 0.86526f, 0.92979f, 0.347061f, 0.0289562f, 0.183843f, 0.109654f, 0.25219f, 0.913093f, 0.211304f, 0.0239445f, 0.321891f, 0.0132581f, 0.491192f, 0.0598186f, 0.638829f, 0.922965f, 0.970296f, 0.688952f, 0.129948f, 0.147574f, 0.260364f, 0.556817f, 0.676236f, 0.967277f, 0.741944f, 0.455842f, 0.372167f, 0.594965f, 0.194687f, 0.110991f, 0.828473f, 0.81903f, 0.300324f, 0.799584f, 0.888032f, 0.401994f, 0.751757f, 0.341877f, 0.168591f, 0.958628f, 0.989044f, 0.368874f, 0.499499f, 0.984334f, 0.405659f, 0.259569f, 0.151238f, 0.764487f, 0.784618f, 0.441848f, 0.972594f, 0.131611f, 0.0371498f, 0.0693474f, 0.416801f, 0.324805f, 0.0487738f, 0.336595f, 0.00118566f, 0.484068f, 0.856299f, 0.780161f, 0.168135f, 0.902528f, 0.00840139f, 0.0114626f, 0.363331f, 0.368221f, 0.560426f, 0.19353f, 0.724254f, 0.954187f, 0.68854f, 0.160637f, 0.777354f, 0.692321f, 0.157433f, 0.369148f, 0.192732f, 0.949813f, 0.307579f, 0.958941f, 0.483992f, 0.602389f, 0.401443f, 0.250001f, 0.000342846f, 0.120461f, 0.875903f, 0.407285f, 0.973199f, 0.0558323f, 0.293595f, 0.362371f, 0.619545f, 0.366578f, 0.970044f, 0.883848f, 0.314051f, 0.272863f, 0.910542f, 0.335767f, 0.364503f, 0.217633f, 0.586304f, 0.62947f, 0.93676f, 0.747138f, 0.81246f, 0.643208f, 0.740491f, 0.55069f, 0.336218f, 0.980699f, 0.261752f, 0.0782728f, 0.45507f, 0.485201f, 0.443404f, 0.80983f, 0.146657f, 0.313562f, 0.137156f, 0.634548f, 0.418098f, 0.592411f, 0.0155032f, 0.336527f, 0.273182f, 0.306312f, 0.4013f, 0.519121f, 0.403324f, 0.0888798f, 0.553309f, 0.637661f, 0.643027f, 0.711509f, 0.169574f, 0.531634f, 0.184579f, 0.802464f, 0.262788f, 0.582185f, 0.838128f, 0.903281f, 0.89136f, 0.400026f, 0.962787f, 0.393862f, 0.0912223f, 0.924886f, 0.732427f, 0.922716f, 0.320009f, 0.017068f, 0.246307f, 0.0766512f, 0.28073f, 0.956669f, 0.000580311f, 0.0147009f, 0.468664f, 0.606133f, 0.589364f, 0.960154f, 0.457708f, 0.0992912f, 0.229694f, 0.722875f, 0.0905478f, 0.492376f, 0.460984f, 0.822356f, 0.631099f, 0.120013f, 0.9392f, 0.025933f, 0.493204f, 0.0777059f, 0.996434f, 0.90678f, 0.570172f, 0.137066f, 0.587406f, 0.152012f, 0.330419f, 0.597728f, 0.653196f, 0.210094f, 0.0150489f, 0.499774f, 0.512619f, 0.662554f, 0.0727493f, 0.902094f, 0.35155f, 0.660929f, 0.538246f, 0.493768f, 0.436716f, 0.347757f, 0.718451f, 0.0751067f, 0.591153f, 0.476367f, 0.871649f, 0.770177f, 0.49412f, 0.207645f, 0.300805f, 0.944687f, 0.540949f, 0.334802f, 0.658609f, 0.280322f, 0.0614085f, 0.523165f, 0.480998f, 0.452187f, 0.396826f, 0.814263f, 0.444388f, 0.0979315f, 0.613517f, 0.591663f, 0.301699f, 0.0100771f, 0.646675f, 0.775883f, 0.0320501f, 0.362856f, 0.0123467f, 0.335263f, 0.785518f, 0.0548519f, 0.236233f, 0.982908f, 0.397743f, 0.196763f, 0.273222f, 0.738824f, 0.921409f, 0.746146f, 0.184881f, 0.780506f, 0.790668f, 0.838174f, 0.140729f, 0.344877f, 0.437886f, 0.270443f, 0.113299f, 0.0567528f, 0.872003f, 0.461713f, 0.396225f, 0.334525f, 0.571957f, 0.406145f, 0.655128f, 0.111169f, 0.512328f, 0.579207f, 0.681437f, 0.313002f, 0.526335f, 0.682192f, 0.543195f, 0.883647f, 0.680931f, 0.936027f, 0.887212f, 0.254619f, 0.537763f, 0.929323f, 0.899553f, 0.583033f, 0.479059f, 0.58146f, 0.605686f, 0.0800954f, 0.554806f, 0.805312f, 0.730701f, 0.461951f, 0.861251f, 0.338622f, 0.949928f, 0.676404f, 0.779811f, 0.66965f, 0.413489f, 0.244133f, 0.253003f, 0.668361f, 0.861042f, 0.111543f, 0.374333f, 0.620414f, 0.730657f, 0.257099f, 0.992287f, 0.962735f, 0.48282f, 0.103903f, 0.120562f, 0.371291f, 0.828842f, 0.19805f, 0.3552f, 0.139294f, 0.983518f, 0.773706f, 0.304598f, 0.321077f, 0.148372f, 0.975174f, 0.902274f, 0.021295f, 0.109916f, 0.0552523f, 0.129696f, 0.834911f, 0.35488f, 0.332393f, 0.638469f, 0.505043f, 0.607849f, 0.444755f, 0.507821f, 0.0467358f, 0.966812f, 0.403f, 0.907803f, 0.491011f, 0.330456f, 0.400001f, 0.246176f, 0.0303432f, 0.744088f, 0.712685f, 0.382909f, 0.0748539f, 0.0552307f, 0.250542f, 0.432398f, 0.269405f, 0.965367f, 0.255643f, 0.00846922f, 0.477259f, 0.956944f, 0.263619f, 0.472611f, 0.818297f, 0.637181f, 0.837281f, 0.29029f, 0.52285f, 0.411453f, 0.327927f, 0.493403f, 0.292899f, 0.326031f, 0.0675526f, 0.043205f, 0.117051f, 0.782362f, 0.974107f, 0.713485f, 0.192605f, 0.757342f, 0.0791711f, 0.773982f, 0.918075f, 0.0538017f, 0.0902326f, 0.385477f, 0.61089f, 0.893094f, 0.278611f, 0.749566f, 0.297577f, 0.343f, 0.700941f, 0.021899f, 0.785716f, 0.575491f, 0.571529f, 0.895896f, 0.540937f, 0.686329f, 0.519124f, 0.214197f, 0.257485f, 0.479406f, 0.723769f, 0.133132f, 0.654102f, 0.464483f, 0.520407f, 0.739025f, 0.425871f, 0.0430206f, 0.367081f, 0.819075f, 0.502319f, 0.808982f, 0.586205f, 0.693161f, 0.578652f, 0.670991f, 0.564139f, 0.168943f, 0.578697f, 0.0289714f, 0.331961f, 0.0185615f, 0.981341f, 0.79096f, 0.683865f, 0.203928f, 0.860001f, 0.630191f, 0.382722f, 0.887468f, 0.209518f, 0.582372f, 0.196313f, 0.0263103f, 0.305729f, 0.76197f, 0.52452f, 0.178965f, 0.719016f, 0.605278f, 0.117012f, 0.838881f, 0.10849f, 0.0151021f, 0.695286f, 0.134018f, 0.923889f, 0.361624f, 0.801152f, 0.345261f, 0.992369f, 0.279426f, 0.574633f, 0.724684f, 0.264346f, 0.216093f, 0.622767f, 0.908065f, 0.882313f, 0.796358f, 0.154155f, 0.308829f, 0.7388f, 0.483272f, 0.726135f, 0.0916826f, 0.831043f, 0.147788f, 0.982867f, 0.639764f, 0.308342f, 0.480933f, 0.682025f, 0.0305799f, 0.448529f, 0.485899f, 0.684693f, 0.604456f, 0.221594f, 0.461711f, 0.350639f, 0.58591f, 0.959523f, 0.831963f, 0.914305f, 0.361302f, 0.620165f, 0.268338f, 0.296164f, 0.0403097f, 0.822701f, 0.809845f, 0.524713f, 0.645126f, 0.207234f, 0.188755f, 0.135637f, 0.756508f, 0.171036f, 0.332887f, 0.513941f, 0.154996f, 0.555009f, 0.98974f, 0.303486f, 0.473306f, 0.430805f, 0.187765f, 0.377134f, 0.612192f, 0.902819f, 0.29555f, 0.409627f, 0.869718f, 0.226232f, 0.756249f, 0.935133f, 0.546867f, 0.384815f, 0.188028f, 0.750887f, 0.310038f, 0.44034f, 0.824127f, 0.747097f, 0.128963f, 0.24836f, 0.528062f, 0.553374f, 0.689335f, 0.126468f, 0.175476f, 0.975612f, 0.586703f, 0.671013f, 0.962735f, 0.808907f, 0.0861794f, 0.533115f, 0.101796f, 0.0706959f, 0.0917822f, 0.0471025f, 0.777679f, 0.952353f, 0.385382f, 0.820037f, 0.815421f, 0.47614f, 0.424147f, 0.946672f, 0.910313f, 0.884953f, 0.476174f, 0.426554f, 0.985588f, 0.032336f, 0.383584f, 0.0543795f, 0.745913f, 0.93714f, 0.984151f, 0.643788f, 0.680333f, 0.660295f, 0.451879f, 0.81687f, 0.423028f, 0.0934314f, 0.317183f, 0.32431f, 0.798049f, 0.301885f, 0.482865f, 0.0412549f, 0.305506f, 0.498927f, 0.183137f, 0.440198f, 0.708901f, 0.142019f, 0.74945f, 0.744077f, 0.406925f, 0.502505f, 0.977357f, 0.981186f, 0.454713f, 0.694598f, 0.583974f, 0.0061208f, 0.313697f, 0.384455f, 0.141156f, 0.121607f, 0.225996f, 0.252465f, 0.0127006f, 0.692692f, 0.542434f, 0.385375f, 0.0158385f, 0.469898f, 0.88728f, 0.580118f, 0.516798f, 0.124881f, 0.17155f, 0.10754f, 0.64776f, 0.863566f, 0.585709f, 0.737403f, 0.881578f, 0.768254f, 0.926182f, 0.734802f, 0.0924621f, 0.560301f, 0.973171f, 0.197622f, 0.39735f, 0.831218f, 0.831679f, 0.626448f, 0.255363f, 0.221987f, 0.510817f, 0.119718f, 0.457004f, 0.751342f, 0.682048f, 0.925028f, 0.472245f, 0.946413f, 0.702793f, 0.849434f, 0.835024f, 0.757633f, 0.227651f, 0.15449f, 0.581614f, 0.344011f, 0.27533f, 0.861278f, 0.0480329f, 0.296223f, 0.639267f, 0.422976f, 0.0219772f, 0.547381f, 0.143436f, 0.483599f, 0.922199f, 0.657291f, 0.968148f, 0.826014f, 0.0362797f, 0.285515f, 0.754099f, 0.819942f, 0.07358f, 0.485557f, 0.0095253f, 0.381697f, 0.690303f, 0.166019f, 0.162123f, 0.746008f, 0.317472f, 0.684392f, 0.824646f, 0.189942f, 0.155815f, 0.865175f, 0.508899f, 0.498846f, 0.510297f, 0.314311f, 0.0580601f, 0.25675f, 0.849481f, 0.997329f, 0.867238f, 0.255343f, 0.575945f, 0.929926f, 0.164465f, 0.382339f, 0.0175594f, 0.531675f, 0.133809f, 0.424927f, 0.932461f, 0.47343f, 0.670699f, 0.451685f, 0.727976f, 0.157594f, 0.668399f, 0.431421f, 0.692803f, 0.309855f, 0.320758f, 0.600577f, 0.851317f, 0.110719f, 0.72173f, 0.052147f, 0.508466f, 0.331822f, 0.732259f, 0.300817f, 0.709938f, 0.341123f, 0.698617f, 0.212518f, 0.202616f, 0.018757f, 0.177552f, 0.613048f, 0.758948f, 0.547059f, 0.400971f, 0.115883f, 0.256621f, 0.727341f, 0.47802f, 0.377706f, 0.344168f, 0.903898f, 0.485407f, 0.654939f, 0.98362f, 0.858975f, 0.575978f, 0.698721f, 0.721705f, 0.841963f, 0.209065f, 0.43947f, 0.626849f, 0.616108f, 0.503407f, 0.551573f, 0.393978f, 0.0142356f, 0.1245f, 0.0788039f, 0.329059f, 0.540094f, 0.963904f, 0.579622f, 0.806494f, 0.378628f, 0.793516f, 0.649193f, 0.151952f, 0.572264f, 0.460979f, 0.198079f, 0.932621f, 0.719997f, 0.919287f, 0.0693846f, 0.840908f, 0.843045f, 0.055732f, 0.457907f, 0.992388f, 0.945144f, 0.943088f, 0.676341f, 0.553268f, 0.586761f, 0.224883f, 0.861508f, 0.566519f, 0.911452f, 0.591536f, 0.0733222f, 0.170432f, 0.351752f, 0.157091f, 0.327673f, 0.868404f, 0.808417f, 0.569937f, 0.453585f, 0.498946f, 0.80573f, 0.626621f, 0.349575f, 0.642979f, 0.53113f, 0.138728f, 0.4121f, 0.425972f, 0.807962f, 0.831206f, 0.839713f, 0.129973f, 0.553252f, 0.147851f, 0.733317f, 0.196179f}),
                            .dimensions = {2, 14, 13, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x14x13x3_to_2x6x7x3_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x14x13x3_to_2x6x7x3_all_inputs_as_internal", get_test_model_half_pixel_centers_2x14x13x3_to_2x6x7x3_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x14x13x3_to_2x6x7x3_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.8276370167732239f, 0.9830099940299988f, 0.2500869929790497f, 0.5758169889450073f, 0.06306099891662598f, 0.5345529913902283f, 0.6756790280342102f, 0.6948440074920654f, 0.4979180097579956f, 0.6647930145263672f, 0.020053299143910408f, 0.5777350068092346f, 0.814857006072998f, 0.8430879712104797f, 0.3288939893245697f, 0.7001579999923706f, 0.5406540036201477f, 0.48390600085258484f, 0.07132089883089066f, 0.23107799887657166f, 0.043016400188207626f, 0.1468230038881302f, 0.5561929941177368f, 0.37289300560951233f, 0.42612001299858093f, 0.6402230262756348f, 0.3262920081615448f, 0.4896639883518219f, 0.26746800541877747f, 0.4131540060043335f, 0.8877400159835815f, 0.6100360155105591f, 0.9427919983863831f, 0.23378999531269073f, 0.09799130260944366f, 0.45806801319122314f, 0.22380900382995605f, 0.07805249840021133f, 0.7705559730529785f, 0.3918969929218292f, 0.8914849758148193f, 0.36497199535369873f, 0.8472380042076111f, 0.4282659888267517f, 0.660148024559021f, 0.9909629821777344f, 0.6700230240821838f, 0.9827569723129272f, 0.08353819698095322f, 0.20829400420188904f, 0.6898369789123535f, 0.6732720136642456f, 0.3179750144481659f, 0.3821539878845215f, 0.36869099736213684f, 0.4082919955253601f, 0.08258839696645737f, 0.9793620109558105f, 0.6672229766845703f, 0.45275598764419556f, 0.531345009803772f, 0.3610219955444336f, 0.9740650057792664f, 0.2039240002632141f, 0.06826110184192657f, 0.9301530122756958f, 0.27257299423217773f, 0.39828601479530334f, 0.5822290182113647f, 0.5520979762077332f, 0.23640500009059906f, 0.831928014755249f, 0.25340399146080017f, 0.10294800251722336f, 0.7019410133361816f, 0.47211799025535583f, 0.415567010641098f, 0.830793023109436f, 0.9959179759025574f, 0.8733919858932495f, 0.14821399748325348f, 0.38536301255226135f, 0.9002779722213745f, 0.07037460058927536f, 0.1080000028014183f, 0.5288040041923523f, 0.9447979927062988f, 0.949567973613739f, 0.1220640018582344f, 0.8407989740371704f, 0.5328879952430725f, 0.051801200956106186f, 0.966821014881134f, 0.6114779710769653f, 0.08893679827451706f, 0.5910549759864807f, 0.7122099995613098f, 0.3996970057487488f, 0.7365509867668152f, 0.6753129959106445f, 0.0995618999004364f, 0.9759169816970825f, 0.3293919861316681f, 0.5139809846878052f, 0.5632060170173645f, 0.867330014705658f, 0.7161930203437805f, 0.22210000455379486f, 0.2152249962091446f, 0.22613799571990967f, 0.6618630290031433f, 0.4654659926891327f, 0.16409899294376373f, 0.8071169853210449f, 0.2232699990272522f, 0.08953689783811569f, 0.9825720191001892f, 0.4298039972782135f, 0.05580290034413338f, 0.799344003200531f, 0.16951200366020203f, 0.5693259835243225f, 0.13565300405025482f, 0.7776920199394226f, 0.11906000226736069f, 0.3620150089263916f, 0.40525001287460327f, 0.06278660148382187f, 0.515949010848999f, 0.0006113050039857626f, 0.583503007888794f, 0.9473059773445129f, 0.8691869974136353f, 0.8699849843978882f, 0.9451469779014587f, 0.5702620148658752f, 0.7095119953155518f, 0.03553130105137825f, 0.44634899497032166f, 0.8026800155639648f, 0.7665330171585083f, 0.16188499331474304f, 0.09086360037326813f, 0.45065200328826904f, 0.1112309992313385f, 0.34660598635673523f, 0.8416100144386292f, 0.524124026298523f, 0.6077209711074829f, 0.3931730091571808f, 0.10310900211334229f, 0.9431059956550598f, 0.4536679983139038f, 0.5986080169677734f, 0.3234429955482483f, 0.7951200008392334f, 0.22728900611400604f, 0.1327199935913086f, 0.9447270035743713f, 0.6536719799041748f, 0.6885970234870911f, 0.40432000160217285f, 0.05686429888010025f, 0.5686140060424805f, 0.9622049927711487f, 0.9496700167655945f, 0.09449149668216705f, 0.10954000055789948f, 0.33313700556755066f, 0.8152859807014465f, 0.011034400202333927f, 0.30730000138282776f, 0.6614220142364502f, 0.884207010269165f, 0.7442780137062073f, 0.8939700126647949f, 0.007626529783010483f, 0.7035880088806152f, 0.8122439980506897f, 0.19406600296497345f, 0.17436599731445312f, 0.8843520283699036f, 0.898688018321991f, 0.11417300254106522f, 0.47909700870513916f, 0.45260098576545715f, 0.003746029920876026f, 0.7919009923934937f, 0.06909999996423721f, 0.6885979771614075f, 0.32922598719596863f, 0.872065007686615f, 0.05609700083732605f, 0.8108469843864441f, 0.015478399582207203f, 0.9868260025978088f, 0.13370099663734436f, 0.8483499884605408f, 0.3010120093822479f, 0.4296579957008362f, 0.43482398986816406f, 0.6340299844741821f, 0.10955099761486053f, 0.5949640274047852f, 0.41404399275779724f, 0.5127159953117371f, 0.986873984336853f, 0.3655790090560913f, 0.12996800243854523f, 0.5534440279006958f, 0.5182110071182251f, 0.11182300001382828f, 0.290679007768631f, 0.33554598689079285f, 0.02419630065560341f, 0.4208729863166809f, 0.8313819766044617f, 0.030533799901604652f, 0.7797279953956604f, 0.3514710068702698f, 0.6061339974403381f, 0.08977530151605606f, 0.11878400295972824f, 0.7611629962921143f, 0.6412889957427979f, 0.8838260173797607f, 0.8778340220451355f, 0.8541640043258667f, 0.7252140045166016f, 0.6957799792289734f, 0.9136210083961487f, 0.9126139879226685f, 0.8145800232887268f, 0.5479549765586853f, 0.05634880065917969f, 0.9766759872436523f, 0.015143799595534801f, 0.7671989798545837f, 0.17327499389648438f, 0.5435529947280884f, 0.09290280193090439f, 0.47132599353790283f, 0.18104900419712067f, 0.07774750143289566f, 0.27821698784828186f, 0.10128699988126755f, 0.42168599367141724f, 0.5563600063323975f, 0.3449530005455017f, 0.6816809773445129f, 0.019020000472664833f, 0.6469460129737854f, 0.3105520009994507f, 0.6227430105209351f, 0.7095010280609131f, 0.05420589819550514f, 0.5597180128097534f, 0.041765499860048294f, 0.847931981086731f, 0.35673999786376953f, 0.2802320122718811f, 0.6055920124053955f, 0.2673659920692444f, 0.18005099892616272f, 0.5225759744644165f, 0.03957799822092056f, 0.7175340056419373f, 0.10944399982690811f, 0.3844349980354309f, 0.3657819926738739f, 0.800603985786438f, 0.0009037259733304381f, 0.11138000339269638f, 0.6158570051193237f, 0.33566200733184814f, 0.8246870040893555f, 0.9098230004310608f, 0.47982698678970337f, 0.5092849731445312f, 0.7488549947738647f, 0.5462890267372131f, 0.24059900641441345f, 0.13530799746513367f, 0.48751699924468994f, 0.5431010127067566f, 0.9183139801025391f, 0.7361369729042053f, 0.16797100007534027f, 0.9076740145683289f, 0.8652600049972534f, 0.9297900199890137f, 0.34706100821495056f, 0.028956200927495956f, 0.1838430017232895f, 0.109654001891613f, 0.2521899938583374f, 0.9130929708480835f, 0.21130399405956268f, 0.02394450083374977f, 0.32189100980758667f, 0.013258099555969238f, 0.4911920130252838f, 0.05981859937310219f, 0.6388289928436279f, 0.9229649901390076f, 0.9702960252761841f, 0.6889520287513733f, 0.12994800508022308f, 0.14757399260997772f, 0.26036399602890015f, 0.5568169951438904f, 0.6762359738349915f, 0.9672769904136658f, 0.7419440150260925f, 0.455841988325119f, 0.3721669912338257f, 0.5949649810791016f, 0.19468699395656586f, 0.11099100112915039f, 0.8284729719161987f, 0.8190299868583679f, 0.3003239929676056f, 0.7995839715003967f, 0.8880320191383362f, 0.401993989944458f, 0.751757025718689f, 0.3418770134449005f, 0.1685909926891327f, 0.9586279988288879f, 0.9890440106391907f, 0.3688740134239197f, 0.49949899315834045f, 0.9843339920043945f, 0.4056589901447296f, 0.259568989276886f, 0.15123799443244934f, 0.7644870281219482f, 0.7846180200576782f, 0.4418480098247528f, 0.9725940227508545f, 0.13161100447177887f, 0.03714980185031891f, 0.06934739649295807f, 0.41680100560188293f, 0.32480499148368835f, 0.04877379909157753f, 0.33659499883651733f, 0.0011856600176542997f, 0.48406800627708435f, 0.8562989830970764f, 0.7801610231399536f, 0.16813500225543976f, 0.9025279879570007f, 0.008401390165090561f, 0.011462599970400333f, 0.36333099007606506f, 0.3682210147380829f, 0.5604259967803955f, 0.19352999329566956f, 0.7242540121078491f, 0.9541869759559631f, 0.688539981842041f, 0.1606370061635971f, 0.7773540019989014f, 0.6923210024833679f, 0.15743300318717957f, 0.36914798617362976f, 0.19273200631141663f, 0.9498130083084106f, 0.30757901072502136f, 0.9589409828186035f, 0.4839920103549957f, 0.6023889780044556f, 0.4014430046081543f, 0.2500010132789612f, 0.00034284600405953825f, 0.12046100199222565f, 0.8759030103683472f, 0.40728500485420227f, 0.9731990098953247f, 0.05583230033516884f, 0.29359498620033264f, 0.3623709976673126f, 0.6195449829101562f, 0.36657801270484924f, 0.9700440168380737f, 0.8838480114936829f, 0.3140510022640228f, 0.2728630006313324f, 0.9105420112609863f, 0.33576700091362f, 0.36450299620628357f, 0.21763299405574799f, 0.5863040089607239f, 0.6294699907302856f, 0.9367600083351135f, 0.7471380233764648f, 0.8124600052833557f, 0.6432080268859863f, 0.7404909729957581f, 0.5506899952888489f, 0.336217999458313f, 0.9806990027427673f, 0.26175200939178467f, 0.07827279716730118f, 0.45506998896598816f, 0.48520100116729736f, 0.4434039890766144f, 0.8098300099372864f, 0.14665700495243073f, 0.3135620057582855f, 0.13715599477291107f, 0.634548008441925f, 0.41809800267219543f, 0.5924109816551208f, 0.015503199771046638f, 0.3365269899368286f, 0.2731820046901703f, 0.3063119947910309f, 0.40130001306533813f, 0.519120991230011f, 0.4033240079879761f, 0.08887980133295059f, 0.5533090233802795f, 0.6376609802246094f, 0.6430270075798035f, 0.7115089893341064f, 0.16957400739192963f, 0.5316339731216431f, 0.184578999876976f, 0.8024640083312988f, 0.26278799772262573f, 0.5821849703788757f, 0.8381279706954956f, 0.9032809734344482f, 0.8913599848747253f, 0.40002599358558655f, 0.9627869725227356f, 0.3938620090484619f, 0.09122230112552643f, 0.9248859882354736f, 0.7324270009994507f, 0.9227160215377808f, 0.3200089931488037f, 0.01706800051033497f, 0.24630700051784515f, 0.0766512006521225f, 0.2807300090789795f, 0.9566689729690552f, 0.0005803109961561859f, 0.014700899831950665f, 0.4686639904975891f, 0.606132984161377f, 0.5893639922142029f, 0.9601539969444275f, 0.4577080011367798f, 0.09929119795560837f, 0.22969399392604828f, 0.7228749990463257f, 0.09054780006408691f, 0.4923759996891022f, 0.4609839916229248f, 0.8223559856414795f, 0.6310989856719971f, 0.12001299858093262f, 0.9391999840736389f, 0.025932999327778816f, 0.4932039976119995f, 0.07770589739084244f, 0.9964339733123779f, 0.9067800045013428f, 0.5701720118522644f, 0.13706600666046143f, 0.5874059796333313f, 0.15201200544834137f, 0.33041900396347046f, 0.5977280139923096f, 0.6531959772109985f, 0.21009400486946106f, 0.015048899687826633f, 0.4997740089893341f, 0.5126190185546875f, 0.6625540256500244f, 0.0727493017911911f, 0.9020940065383911f, 0.35155001282691956f, 0.6609290242195129f, 0.5382459759712219f, 0.49376800656318665f, 0.4367159903049469f, 0.3477570116519928f, 0.7184510231018066f, 0.07510670274496078f, 0.5911530256271362f, 0.4763669967651367f, 0.8716490268707275f, 0.7701770067214966f, 0.4941200017929077f, 0.2076449990272522f, 0.300805002450943f, 0.944687008857727f, 0.5409489870071411f, 0.3348020017147064f, 0.6586089730262756f, 0.28032198548316956f, 0.061408501118421555f, 0.5231649875640869f, 0.4809980094432831f, 0.4521870017051697f, 0.39682599902153015f, 0.8142629861831665f, 0.4443880021572113f, 0.09793149679899216f, 0.6135169863700867f, 0.5916630029678345f, 0.3016990125179291f, 0.010077100247144699f, 0.6466749906539917f, 0.7758830189704895f, 0.03205009922385216f, 0.36285600066185f, 0.012346699833869934f, 0.3352630138397217f, 0.7855179905891418f, 0.05485190078616142f, 0.23623299598693848f, 0.9829080104827881f, 0.39774298667907715f, 0.19676299393177032f, 0.2732219994068146f, 0.7388240098953247f, 0.921409010887146f, 0.746146023273468f, 0.18488100171089172f, 0.7805060148239136f, 0.7906680107116699f, 0.8381739854812622f, 0.14072899520397186f, 0.3448770046234131f, 0.43788599967956543f, 0.27044299244880676f, 0.11329899728298187f, 0.056752800941467285f, 0.8720030188560486f, 0.4617129862308502f, 0.3962250053882599f, 0.3345249891281128f, 0.571956992149353f, 0.40614500641822815f, 0.655128002166748f, 0.1111690029501915f, 0.512328028678894f, 0.5792070031166077f, 0.6814370155334473f, 0.31300199031829834f, 0.5263350009918213f, 0.6821920275688171f, 0.5431950092315674f, 0.8836470246315002f, 0.6809309720993042f, 0.9360269904136658f, 0.8872119784355164f, 0.2546190023422241f, 0.5377629995346069f, 0.9293230175971985f, 0.8995530009269714f, 0.58303302526474f, 0.47905901074409485f, 0.5814599990844727f, 0.6056860089302063f, 0.08009540289640427f, 0.5548059940338135f, 0.8053119778633118f, 0.7307010293006897f, 0.46195098757743835f, 0.8612509965896606f, 0.33862200379371643f, 0.9499279856681824f, 0.6764039993286133f, 0.7798110246658325f, 0.6696500182151794f, 0.4134890139102936f, 0.24413299560546875f, 0.25300300121307373f, 0.6683610081672668f, 0.8610420227050781f, 0.11154299974441528f, 0.374332994222641f, 0.6204140186309814f, 0.7306569814682007f, 0.2570990025997162f, 0.9922869801521301f, 0.9627349972724915f, 0.4828200042247772f, 0.10390300303697586f, 0.12056200206279755f, 0.37129101157188416f, 0.8288419842720032f, 0.19805000722408295f, 0.35519999265670776f, 0.13929399847984314f, 0.9835180044174194f, 0.7737060189247131f, 0.30459800362586975f, 0.32107698917388916f, 0.14837199449539185f, 0.9751740097999573f, 0.9022740125656128f, 0.02129499986767769f, 0.10991600155830383f, 0.05525229871273041f, 0.12969599664211273f, 0.8349109888076782f, 0.3548800051212311f, 0.33239299058914185f, 0.6384689807891846f, 0.5050430297851562f, 0.6078490018844604f, 0.4447549879550934f, 0.5078210234642029f, 0.04673580080270767f, 0.966812014579773f, 0.40299999713897705f, 0.9078029990196228f, 0.49101099371910095f, 0.3304559886455536f, 0.40000098943710327f, 0.24617600440979004f, 0.030343199148774147f, 0.7440879940986633f, 0.7126849889755249f, 0.38290899991989136f, 0.07485389709472656f, 0.055230699479579926f, 0.250542014837265f, 0.4323979914188385f, 0.2694050073623657f, 0.9653670191764832f, 0.25564301013946533f, 0.008469220250844955f, 0.4772590100765228f, 0.9569439888000488f, 0.26361900568008423f, 0.4726110100746155f, 0.8182970285415649f, 0.6371809840202332f, 0.8372809886932373f, 0.2902899980545044f, 0.522849977016449f, 0.4114530086517334f, 0.32792699337005615f, 0.49340298771858215f, 0.2928990125656128f, 0.32603099942207336f, 0.0675525963306427f, 0.04320500046014786f, 0.11705099791288376f, 0.7823619842529297f, 0.974107027053833f, 0.7134850025177002f, 0.19260500371456146f, 0.7573419809341431f, 0.07917109876871109f, 0.7739819884300232f, 0.9180750250816345f, 0.053801700472831726f, 0.09023260325193405f, 0.3854770064353943f, 0.6108899712562561f, 0.893094003200531f, 0.2786110043525696f, 0.7495660185813904f, 0.29757699370384216f, 0.34299999475479126f, 0.7009410262107849f, 0.02189899981021881f, 0.7857159972190857f, 0.5754910111427307f, 0.571528971195221f, 0.8958960175514221f, 0.5409370064735413f, 0.6863290071487427f, 0.5191239714622498f, 0.21419699490070343f, 0.2574850022792816f, 0.4794059991836548f, 0.7237690091133118f, 0.1331319957971573f, 0.6541020274162292f, 0.46448299288749695f, 0.5204070210456848f, 0.7390249967575073f, 0.42587101459503174f, 0.04302059859037399f, 0.3670809864997864f, 0.8190749883651733f, 0.5023189783096313f, 0.8089820146560669f, 0.586205005645752f, 0.6931610107421875f, 0.578652024269104f, 0.6709910035133362f, 0.5641390085220337f, 0.16894300282001495f, 0.5786970257759094f, 0.02897140011191368f, 0.3319610059261322f, 0.018561499193310738f, 0.9813410043716431f, 0.7909600138664246f, 0.6838650107383728f, 0.20392799377441406f, 0.8600010275840759f, 0.6301910281181335f, 0.38272199034690857f, 0.8874679803848267f, 0.20951800048351288f, 0.5823720097541809f, 0.19631299376487732f, 0.026310300454497337f, 0.3057290017604828f, 0.7619699835777283f, 0.5245199799537659f, 0.1789650022983551f, 0.7190160155296326f, 0.6052780151367188f, 0.11701200157403946f, 0.8388810157775879f, 0.1084899976849556f, 0.015102099627256393f, 0.695285975933075f, 0.13401800394058228f, 0.9238889813423157f, 0.36162400245666504f, 0.8011519908905029f, 0.34526100754737854f, 0.9923689961433411f, 0.2794260084629059f, 0.574633002281189f, 0.7246840000152588f, 0.26434600353240967f, 0.2160930037498474f, 0.6227669715881348f, 0.9080650210380554f, 0.8823130130767822f, 0.7963579893112183f, 0.15415500104427338f, 0.30882900953292847f, 0.7387999892234802f, 0.483271986246109f, 0.7261350154876709f, 0.09168259799480438f, 0.831043004989624f, 0.14778800308704376f, 0.9828670024871826f, 0.6397640109062195f, 0.30834200978279114f, 0.4809330105781555f, 0.6820250153541565f, 0.03057990036904812f, 0.4485290050506592f, 0.4858990013599396f, 0.6846929788589478f, 0.6044560074806213f, 0.22159400582313538f, 0.46171098947525024f, 0.3506389856338501f, 0.5859100222587585f, 0.9595230221748352f, 0.8319630026817322f, 0.9143049716949463f, 0.361301988363266f, 0.6201649904251099f, 0.26833799481391907f, 0.29616400599479675f, 0.04030970111489296f, 0.8227009773254395f, 0.809844970703125f, 0.5247129797935486f, 0.6451259851455688f, 0.2072339951992035f, 0.18875500559806824f, 0.13563700020313263f, 0.7565079927444458f, 0.1710360050201416f, 0.3328869938850403f, 0.5139409899711609f, 0.15499599277973175f, 0.5550090074539185f, 0.9897400140762329f, 0.3034859895706177f, 0.47330600023269653f, 0.4308049976825714f, 0.1877650022506714f, 0.3771339952945709f, 0.6121919751167297f, 0.9028189778327942f, 0.29554998874664307f, 0.4096269905567169f, 0.8697180151939392f, 0.22623200714588165f, 0.7562490105628967f, 0.9351329803466797f, 0.5468670129776001f, 0.3848150074481964f, 0.1880279928445816f, 0.7508869767189026f, 0.3100380003452301f, 0.4403400123119354f, 0.8241270184516907f, 0.7470970153808594f, 0.12896299362182617f, 0.24835999310016632f, 0.5280619859695435f, 0.5533739924430847f, 0.6893349885940552f, 0.1264680027961731f, 0.17547599971294403f, 0.9756119847297668f, 0.5867030024528503f, 0.6710129976272583f, 0.9627349972724915f, 0.8089069724082947f, 0.08617939800024033f, 0.533115029335022f, 0.10179600119590759f, 0.0706958994269371f, 0.09178219735622406f, 0.04710249975323677f, 0.7776790261268616f, 0.9523530006408691f, 0.3853819966316223f, 0.8200370073318481f, 0.8154209852218628f, 0.47613999247550964f, 0.42414700984954834f, 0.9466720223426819f, 0.9103130102157593f, 0.8849530220031738f, 0.476173996925354f, 0.42655399441719055f, 0.985588014125824f, 0.032336000353097916f, 0.38358399271965027f, 0.054379500448703766f, 0.745913028717041f, 0.9371399879455566f, 0.9841510057449341f, 0.6437879800796509f, 0.6803330183029175f, 0.6602950096130371f, 0.45187899470329285f, 0.8168699741363525f, 0.42302799224853516f, 0.09343139827251434f, 0.3171829879283905f, 0.3243100047111511f, 0.7980489730834961f, 0.3018850088119507f, 0.48286500573158264f, 0.04125490039587021f, 0.30550599098205566f, 0.4989269971847534f, 0.18313699960708618f, 0.44019800424575806f, 0.7089009881019592f, 0.14201900362968445f, 0.7494500279426575f, 0.7440770268440247f, 0.4069249927997589f, 0.5025050044059753f, 0.9773569703102112f, 0.9811859726905823f, 0.45471298694610596f, 0.6945980191230774f, 0.5839740037918091f, 0.006120800040662289f, 0.3136970102787018f, 0.38445499539375305f, 0.14115600287914276f, 0.12160699814558029f, 0.2259960025548935f, 0.25246500968933105f, 0.012700599618256092f, 0.69269198179245f, 0.5424339771270752f, 0.3853749930858612f, 0.01583850011229515f, 0.469897985458374f, 0.8872799873352051f, 0.5801180005073547f, 0.5167980194091797f, 0.12488099932670593f, 0.17155000567436218f, 0.10753999650478363f, 0.6477599740028381f, 0.863565981388092f, 0.5857089757919312f, 0.7374029755592346f, 0.8815780282020569f, 0.7682539820671082f, 0.926181972026825f, 0.7348020076751709f, 0.09246210008859634f, 0.5603010058403015f, 0.9731709957122803f, 0.19762200117111206f, 0.397350013256073f, 0.8312180042266846f, 0.8316789865493774f, 0.6264479756355286f, 0.25536298751831055f, 0.22198699414730072f, 0.5108169913291931f, 0.11971800029277802f, 0.45700401067733765f, 0.7513419985771179f, 0.6820480227470398f, 0.9250280261039734f, 0.4722450077533722f, 0.946412980556488f, 0.7027930021286011f, 0.8494340181350708f, 0.8350239992141724f, 0.7576329708099365f, 0.22765100002288818f, 0.15448999404907227f, 0.5816140174865723f, 0.34401100873947144f, 0.2753300070762634f, 0.8612779974937439f, 0.04803289845585823f, 0.2962230145931244f, 0.6392670273780823f, 0.42297598719596863f, 0.021977199241518974f, 0.5473809838294983f, 0.143435999751091f, 0.4835990071296692f, 0.922199010848999f, 0.6572909951210022f, 0.9681479930877686f, 0.82601398229599f, 0.03627970069646835f, 0.2855150103569031f, 0.7540990114212036f, 0.8199419975280762f, 0.07357999682426453f, 0.4855569899082184f, 0.0095253000035882f, 0.38169699907302856f, 0.6903030276298523f, 0.1660189926624298f, 0.16212299466133118f, 0.7460079789161682f, 0.31747201085090637f, 0.684391975402832f, 0.82464599609375f, 0.18994200229644775f, 0.1558150053024292f, 0.8651750087738037f, 0.5088989734649658f, 0.49884599447250366f, 0.5102970004081726f, 0.3143109977245331f, 0.05806009843945503f, 0.2567499876022339f, 0.8494809865951538f, 0.9973289966583252f, 0.8672379851341248f, 0.2553429901599884f, 0.5759450197219849f, 0.9299259781837463f, 0.16446499526500702f, 0.3823390007019043f, 0.01755939982831478f, 0.5316749811172485f, 0.13380900025367737f, 0.4249269962310791f, 0.9324610233306885f, 0.47343000769615173f, 0.6706990003585815f, 0.45168501138687134f, 0.7279760241508484f, 0.1575939953327179f, 0.6683989763259888f, 0.4314210116863251f, 0.6928030252456665f, 0.30985501408576965f, 0.32075798511505127f, 0.6005769968032837f, 0.8513169884681702f, 0.11071900278329849f, 0.7217299938201904f, 0.05214700102806091f, 0.5084660053253174f, 0.331822007894516f, 0.7322589755058289f, 0.30081701278686523f, 0.7099379897117615f, 0.3411230146884918f, 0.6986169815063477f, 0.21251800656318665f, 0.20261600613594055f, 0.01875700056552887f, 0.1775519996881485f, 0.6130480170249939f, 0.758948028087616f, 0.5470589995384216f, 0.400970995426178f, 0.1158830001950264f, 0.25662100315093994f, 0.7273409962654114f, 0.4780200123786926f, 0.37770599126815796f, 0.3441680073738098f, 0.9038980007171631f, 0.4854069948196411f, 0.6549389958381653f, 0.9836199879646301f, 0.8589749932289124f, 0.5759779810905457f, 0.6987209916114807f, 0.7217050194740295f, 0.841962993144989f, 0.2090650051832199f, 0.43946999311447144f, 0.6268489956855774f, 0.6161080002784729f, 0.5034070014953613f, 0.5515729784965515f, 0.3939779996871948f, 0.014235599897801876f, 0.12449999898672104f, 0.0788038969039917f, 0.32905900478363037f, 0.5400940179824829f, 0.9639040231704712f, 0.5796219706535339f, 0.8064939975738525f, 0.3786279857158661f, 0.7935159802436829f, 0.6491929888725281f, 0.15195199847221375f, 0.5722640156745911f, 0.46097901463508606f, 0.19807900488376617f, 0.9326210021972656f, 0.719996988773346f, 0.9192870259284973f, 0.06938459724187851f, 0.8409079909324646f, 0.8430449962615967f, 0.05573200061917305f, 0.4579069912433624f, 0.9923880100250244f, 0.94514399766922f, 0.9430879950523376f, 0.6763409972190857f, 0.5532680153846741f, 0.5867609977722168f, 0.22488300502300262f, 0.8615080118179321f, 0.566519021987915f, 0.9114519953727722f, 0.5915359854698181f, 0.07332219928503036f, 0.17043200135231018f, 0.35175201296806335f, 0.15709100663661957f, 0.3276729881763458f, 0.868403971195221f, 0.808417022228241f, 0.569936990737915f, 0.4535849988460541f, 0.49894601106643677f, 0.8057299852371216f, 0.6266210079193115f, 0.349575012922287f, 0.6429790258407593f, 0.5311300158500671f, 0.13872799277305603f, 0.4120999872684479f, 0.42597201466560364f, 0.8079620003700256f, 0.8312060236930847f, 0.8397129774093628f, 0.12997299432754517f, 0.5532519817352295f, 0.14785100519657135f, 0.7333170175552368f, 0.19617900252342224f}),
                            .dimensions = {2, 14, 13, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output_width1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({7}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({6}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.6312670111656189f, 0.6582239866256714f, 0.451651006937027f, 0.7119889855384827f, 0.5260829925537109f, 0.7729510068893433f, 0.6859980225563049f, 0.48721298575401306f, 0.3432520031929016f, 0.6766809821128845f, 0.5218409895896912f, 0.31617599725723267f, 0.2958720028400421f, 0.2827959954738617f, 0.7352669835090637f, 0.5728240013122559f, 0.3591580092906952f, 0.7708789706230164f, 0.32828500866889954f, 0.2165910005569458f, 0.7292580008506775f, 0.31825101375579834f, 0.2758840024471283f, 0.5149049758911133f, 0.671280026435852f, 0.10298199951648712f, 0.4059959948062897f, 0.12469399720430374f, 0.6244289875030518f, 0.9469969868659973f, 0.5702620148658752f, 0.7095119953155518f, 0.03553130105137825f, 0.20252299308776855f, 0.19255200028419495f, 0.49577799439430237f, 0.40615400671958923f, 0.5331159830093384f, 0.5212979912757874f, 0.3862510025501251f, 0.5890129804611206f, 0.6487830281257629f, 0.5822700262069702f, 0.38447999954223633f, 0.5279510021209717f, 0.41030898690223694f, 0.2885439991950989f, 0.524046003818512f, 0.4111979901790619f, 0.24478299915790558f, 0.4733020067214966f, 0.32721400260925293f, 0.2237119972705841f, 0.5170819759368896f, 0.5248669981956482f, 0.47784900665283203f, 0.20761500298976898f, 0.6255940198898315f, 0.5978869795799255f, 0.7168840169906616f, 0.7229970097541809f, 0.5642020106315613f, 0.5432490110397339f, 0.7221009731292725f, 0.585116982460022f, 0.45911699533462524f, 0.30124300718307495f, 0.7233110070228577f, 0.6681720018386841f, 0.5513659715652466f, 0.772400975227356f, 0.3818199932575226f, 0.3616029918193817f, 0.34265100955963135f, 0.717503011226654f, 0.5410770177841187f, 0.491923987865448f, 0.4023379981517792f, 0.371643990278244f, 0.49007898569107056f, 0.39846599102020264f, 0.34635698795318604f, 0.5470029711723328f, 0.4474320113658905f, 0.4431400001049042f, 0.4780600070953369f, 0.5258299708366394f, 0.7452390193939209f, 0.7376689910888672f, 0.5554969906806946f, 0.9056090116500854f, 0.2936739921569824f, 0.13043400645256042f, 0.8098300099372864f, 0.14665700495243073f, 0.3135620057582855f, 0.5273740291595459f, 0.10393799841403961f, 0.34817999601364136f, 0.4488529860973358f, 0.3756060004234314f, 0.1781429946422577f, 0.6437090039253235f, 0.37018299102783203f, 0.5793740153312683f, 0.4004499912261963f, 0.5683709979057312f, 0.6028469800949097f, 0.34384000301361084f, 0.3998199999332428f, 0.43322500586509705f, 0.31763601303100586f, 0.5199180245399475f, 0.41822299361228943f, 0.5193639993667603f, 0.6864669919013977f, 0.461760014295578f, 0.357138991355896f, 0.5971159934997559f, 0.4286389946937561f, 0.3727479875087738f, 0.4473559856414795f, 0.7280539870262146f, 0.4660769999027252f, 0.22318199276924133f, 0.47105398774147034f, 0.662585973739624f, 0.37607699632644653f, 0.36660999059677124f, 0.6803420186042786f, 0.7086269855499268f, 0.42689400911331177f, 0.7816089987754822f, 0.41351601481437683f, 0.5697020292282104f, 0.5128399729728699f, 0.5139020085334778f, 0.29325899481773376f, 0.20137999951839447f, 0.3037090003490448f, 0.33507201075553894f, 0.6132779717445374f, 0.5782619714736938f, 0.5958369970321655f, 0.653285026550293f, 0.4424709975719452f, 0.5455589890480042f, 0.48094600439071655f, 0.6898189783096313f, 0.29255399107933044f, 0.7229470014572144f, 0.2970080077648163f, 0.7356730103492737f, 0.10041800141334534f, 0.8014559745788574f, 0.5705540180206299f, 0.6863290071487427f, 0.5191239714622498f, 0.21419699490070343f, 0.15089699625968933f, 0.6291450262069702f, 0.5015239715576172f, 0.17941699922084808f, 0.47334998846054077f, 0.7067310214042664f, 0.6113719940185547f, 0.6773650050163269f, 0.6346539855003357f, 0.48195600509643555f, 0.5251629948616028f, 0.46336498856544495f, 0.5019999742507935f, 0.43507999181747437f, 0.5656449794769287f, 0.26605498790740967f, 0.7040799856185913f, 0.5523599982261658f, 0.15692900121212006f, 0.5889250040054321f, 0.39934399724006653f, 0.4750030040740967f, 0.3561849892139435f, 0.5964900255203247f, 0.5622230172157288f, 0.5264279842376709f, 0.43955498933792114f, 0.49265700578689575f, 0.3238849937915802f, 0.7651270031929016f, 0.4296030104160309f, 0.4666230082511902f, 0.32757899165153503f, 0.38109299540519714f, 0.5701979994773865f, 0.25769099593162537f, 0.463234007358551f, 0.8791360259056091f, 0.8943510055541992f, 0.30751898884773254f, 0.5041159987449646f, 0.41502800583839417f, 0.2876169979572296f, 0.08130910247564316f, 0.632004976272583f, 0.5783169865608215f, 0.7468609809875488f, 0.44816699624061584f, 0.3657360076904297f, 0.4046519994735718f, 0.5433480143547058f, 0.7080169916152954f, 0.36365899443626404f, 0.6912630200386047f, 0.8054670095443726f, 0.2601830065250397f, 0.2249619960784912f, 0.5148069858551025f, 0.03188160061836243f, 0.3503290116786957f, 0.7460079789161682f, 0.31747201085090637f, 0.684391975402832f, 0.8593850135803223f, 0.46333399415016174f, 0.44984200596809387f, 0.32919201254844666f, 0.6965749859809875f, 0.728967010974884f, 0.9030600190162659f, 0.2034129947423935f, 0.46531298756599426f, 0.4282119870185852f, 0.5401840209960938f, 0.9110230207443237f, 0.7602819800376892f, 0.6283079981803894f, 0.5128949880599976f, 0.48482900857925415f, 0.43915998935699463f, 0.45388200879096985f, 0.2989580035209656f, 0.3524720072746277f, 0.24251499772071838f, 0.5631849765777588f, 0.6359289884567261f, 0.5302349925041199f, 0.5756589770317078f, 0.5487539768218994f, 0.36041998863220215f, 0.4410170018672943f, 0.6843060255050659f, 0.7002170085906982f}),
                            .dimensions = {2, 6, 7, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x14x13x3_to_2x6x7x3_float16 = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x14x13x3_to_2x6x7x3_float16", get_test_model_half_pixel_centers_2x14x13x3_to_2x6x7x3_float16());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x14x13x3_to_2x6x7x3_float16_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({}),
                            .dimensions = {2, 14, 13, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output_width1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({7}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({6}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.6312670111656189f, 0.6582239866256714f, 0.451651006937027f, 0.7119889855384827f, 0.5260829925537109f, 0.7729510068893433f, 0.6859980225563049f, 0.48721298575401306f, 0.3432520031929016f, 0.6766809821128845f, 0.5218409895896912f, 0.31617599725723267f, 0.2958720028400421f, 0.2827959954738617f, 0.7352669835090637f, 0.5728240013122559f, 0.3591580092906952f, 0.7708789706230164f, 0.32828500866889954f, 0.2165910005569458f, 0.7292580008506775f, 0.31825101375579834f, 0.2758840024471283f, 0.5149049758911133f, 0.671280026435852f, 0.10298199951648712f, 0.4059959948062897f, 0.12469399720430374f, 0.6244289875030518f, 0.9469969868659973f, 0.5702620148658752f, 0.7095119953155518f, 0.03553130105137825f, 0.20252299308776855f, 0.19255200028419495f, 0.49577799439430237f, 0.40615400671958923f, 0.5331159830093384f, 0.5212979912757874f, 0.3862510025501251f, 0.5890129804611206f, 0.6487830281257629f, 0.5822700262069702f, 0.38447999954223633f, 0.5279510021209717f, 0.41030898690223694f, 0.2885439991950989f, 0.524046003818512f, 0.4111979901790619f, 0.24478299915790558f, 0.4733020067214966f, 0.32721400260925293f, 0.2237119972705841f, 0.5170819759368896f, 0.5248669981956482f, 0.47784900665283203f, 0.20761500298976898f, 0.6255940198898315f, 0.5978869795799255f, 0.7168840169906616f, 0.7229970097541809f, 0.5642020106315613f, 0.5432490110397339f, 0.7221009731292725f, 0.585116982460022f, 0.45911699533462524f, 0.30124300718307495f, 0.7233110070228577f, 0.6681720018386841f, 0.5513659715652466f, 0.772400975227356f, 0.3818199932575226f, 0.3616029918193817f, 0.34265100955963135f, 0.717503011226654f, 0.5410770177841187f, 0.491923987865448f, 0.4023379981517792f, 0.371643990278244f, 0.49007898569107056f, 0.39846599102020264f, 0.34635698795318604f, 0.5470029711723328f, 0.4474320113658905f, 0.4431400001049042f, 0.4780600070953369f, 0.5258299708366394f, 0.7452390193939209f, 0.7376689910888672f, 0.5554969906806946f, 0.9056090116500854f, 0.2936739921569824f, 0.13043400645256042f, 0.8098300099372864f, 0.14665700495243073f, 0.3135620057582855f, 0.5273740291595459f, 0.10393799841403961f, 0.34817999601364136f, 0.4488529860973358f, 0.3756060004234314f, 0.1781429946422577f, 0.6437090039253235f, 0.37018299102783203f, 0.5793740153312683f, 0.4004499912261963f, 0.5683709979057312f, 0.6028469800949097f, 0.34384000301361084f, 0.3998199999332428f, 0.43322500586509705f, 0.31763601303100586f, 0.5199180245399475f, 0.41822299361228943f, 0.5193639993667603f, 0.6864669919013977f, 0.461760014295578f, 0.357138991355896f, 0.5971159934997559f, 0.4286389946937561f, 0.3727479875087738f, 0.4473559856414795f, 0.7280539870262146f, 0.4660769999027252f, 0.22318199276924133f, 0.47105398774147034f, 0.662585973739624f, 0.37607699632644653f, 0.36660999059677124f, 0.6803420186042786f, 0.7086269855499268f, 0.42689400911331177f, 0.7816089987754822f, 0.41351601481437683f, 0.5697020292282104f, 0.5128399729728699f, 0.5139020085334778f, 0.29325899481773376f, 0.20137999951839447f, 0.3037090003490448f, 0.33507201075553894f, 0.6132779717445374f, 0.5782619714736938f, 0.5958369970321655f, 0.653285026550293f, 0.4424709975719452f, 0.5455589890480042f, 0.48094600439071655f, 0.6898189783096313f, 0.29255399107933044f, 0.7229470014572144f, 0.2970080077648163f, 0.7356730103492737f, 0.10041800141334534f, 0.8014559745788574f, 0.5705540180206299f, 0.6863290071487427f, 0.5191239714622498f, 0.21419699490070343f, 0.15089699625968933f, 0.6291450262069702f, 0.5015239715576172f, 0.17941699922084808f, 0.47334998846054077f, 0.7067310214042664f, 0.6113719940185547f, 0.6773650050163269f, 0.6346539855003357f, 0.48195600509643555f, 0.5251629948616028f, 0.46336498856544495f, 0.5019999742507935f, 0.43507999181747437f, 0.5656449794769287f, 0.26605498790740967f, 0.7040799856185913f, 0.5523599982261658f, 0.15692900121212006f, 0.5889250040054321f, 0.39934399724006653f, 0.4750030040740967f, 0.3561849892139435f, 0.5964900255203247f, 0.5622230172157288f, 0.5264279842376709f, 0.43955498933792114f, 0.49265700578689575f, 0.3238849937915802f, 0.7651270031929016f, 0.4296030104160309f, 0.4666230082511902f, 0.32757899165153503f, 0.38109299540519714f, 0.5701979994773865f, 0.25769099593162537f, 0.463234007358551f, 0.8791360259056091f, 0.8943510055541992f, 0.30751898884773254f, 0.5041159987449646f, 0.41502800583839417f, 0.2876169979572296f, 0.08130910247564316f, 0.632004976272583f, 0.5783169865608215f, 0.7468609809875488f, 0.44816699624061584f, 0.3657360076904297f, 0.4046519994735718f, 0.5433480143547058f, 0.7080169916152954f, 0.36365899443626404f, 0.6912630200386047f, 0.8054670095443726f, 0.2601830065250397f, 0.2249619960784912f, 0.5148069858551025f, 0.03188160061836243f, 0.3503290116786957f, 0.7460079789161682f, 0.31747201085090637f, 0.684391975402832f, 0.8593850135803223f, 0.46333399415016174f, 0.44984200596809387f, 0.32919201254844666f, 0.6965749859809875f, 0.728967010974884f, 0.9030600190162659f, 0.2034129947423935f, 0.46531298756599426f, 0.4282119870185852f, 0.5401840209960938f, 0.9110230207443237f, 0.7602819800376892f, 0.6283079981803894f, 0.5128949880599976f, 0.48482900857925415f, 0.43915998935699463f, 0.45388200879096985f, 0.2989580035209656f, 0.3524720072746277f, 0.24251499772071838f, 0.5631849765777588f, 0.6359289884567261f, 0.5302349925041199f, 0.5756589770317078f, 0.5487539768218994f, 0.36041998863220215f, 0.4410170018672943f, 0.6843060255050659f, 0.7002170085906982f}),
                            .dimensions = {2, 6, 7, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // input01_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.8276370167732239f, 0.9830099940299988f, 0.2500869929790497f, 0.5758169889450073f, 0.06306099891662598f, 0.5345529913902283f, 0.6756790280342102f, 0.6948440074920654f, 0.4979180097579956f, 0.6647930145263672f, 0.020053299143910408f, 0.5777350068092346f, 0.814857006072998f, 0.8430879712104797f, 0.3288939893245697f, 0.7001579999923706f, 0.5406540036201477f, 0.48390600085258484f, 0.07132089883089066f, 0.23107799887657166f, 0.043016400188207626f, 0.1468230038881302f, 0.5561929941177368f, 0.37289300560951233f, 0.42612001299858093f, 0.6402230262756348f, 0.3262920081615448f, 0.4896639883518219f, 0.26746800541877747f, 0.4131540060043335f, 0.8877400159835815f, 0.6100360155105591f, 0.9427919983863831f, 0.23378999531269073f, 0.09799130260944366f, 0.45806801319122314f, 0.22380900382995605f, 0.07805249840021133f, 0.7705559730529785f, 0.3918969929218292f, 0.8914849758148193f, 0.36497199535369873f, 0.8472380042076111f, 0.4282659888267517f, 0.660148024559021f, 0.9909629821777344f, 0.6700230240821838f, 0.9827569723129272f, 0.08353819698095322f, 0.20829400420188904f, 0.6898369789123535f, 0.6732720136642456f, 0.3179750144481659f, 0.3821539878845215f, 0.36869099736213684f, 0.4082919955253601f, 0.08258839696645737f, 0.9793620109558105f, 0.6672229766845703f, 0.45275598764419556f, 0.531345009803772f, 0.3610219955444336f, 0.9740650057792664f, 0.2039240002632141f, 0.06826110184192657f, 0.9301530122756958f, 0.27257299423217773f, 0.39828601479530334f, 0.5822290182113647f, 0.5520979762077332f, 0.23640500009059906f, 0.831928014755249f, 0.25340399146080017f, 0.10294800251722336f, 0.7019410133361816f, 0.47211799025535583f, 0.415567010641098f, 0.830793023109436f, 0.9959179759025574f, 0.8733919858932495f, 0.14821399748325348f, 0.38536301255226135f, 0.9002779722213745f, 0.07037460058927536f, 0.1080000028014183f, 0.5288040041923523f, 0.9447979927062988f, 0.949567973613739f, 0.1220640018582344f, 0.8407989740371704f, 0.5328879952430725f, 0.051801200956106186f, 0.966821014881134f, 0.6114779710769653f, 0.08893679827451706f, 0.5910549759864807f, 0.7122099995613098f, 0.3996970057487488f, 0.7365509867668152f, 0.6753129959106445f, 0.0995618999004364f, 0.9759169816970825f, 0.3293919861316681f, 0.5139809846878052f, 0.5632060170173645f, 0.867330014705658f, 0.7161930203437805f, 0.22210000455379486f, 0.2152249962091446f, 0.22613799571990967f, 0.6618630290031433f, 0.4654659926891327f, 0.16409899294376373f, 0.8071169853210449f, 0.2232699990272522f, 0.08953689783811569f, 0.9825720191001892f, 0.4298039972782135f, 0.05580290034413338f, 0.799344003200531f, 0.16951200366020203f, 0.5693259835243225f, 0.13565300405025482f, 0.7776920199394226f, 0.11906000226736069f, 0.3620150089263916f, 0.40525001287460327f, 0.06278660148382187f, 0.515949010848999f, 0.0006113050039857626f, 0.583503007888794f, 0.9473059773445129f, 0.8691869974136353f, 0.8699849843978882f, 0.9451469779014587f, 0.5702620148658752f, 0.7095119953155518f, 0.03553130105137825f, 0.44634899497032166f, 0.8026800155639648f, 0.7665330171585083f, 0.16188499331474304f, 0.09086360037326813f, 0.45065200328826904f, 0.1112309992313385f, 0.34660598635673523f, 0.8416100144386292f, 0.524124026298523f, 0.6077209711074829f, 0.3931730091571808f, 0.10310900211334229f, 0.9431059956550598f, 0.4536679983139038f, 0.5986080169677734f, 0.3234429955482483f, 0.7951200008392334f, 0.22728900611400604f, 0.1327199935913086f, 0.9447270035743713f, 0.6536719799041748f, 0.6885970234870911f, 0.40432000160217285f, 0.05686429888010025f, 0.5686140060424805f, 0.9622049927711487f, 0.9496700167655945f, 0.09449149668216705f, 0.10954000055789948f, 0.33313700556755066f, 0.8152859807014465f, 0.011034400202333927f, 0.30730000138282776f, 0.6614220142364502f, 0.884207010269165f, 0.7442780137062073f, 0.8939700126647949f, 0.007626529783010483f, 0.7035880088806152f, 0.8122439980506897f, 0.19406600296497345f, 0.17436599731445312f, 0.8843520283699036f, 0.898688018321991f, 0.11417300254106522f, 0.47909700870513916f, 0.45260098576545715f, 0.003746029920876026f, 0.7919009923934937f, 0.06909999996423721f, 0.6885979771614075f, 0.32922598719596863f, 0.872065007686615f, 0.05609700083732605f, 0.8108469843864441f, 0.015478399582207203f, 0.9868260025978088f, 0.13370099663734436f, 0.8483499884605408f, 0.3010120093822479f, 0.4296579957008362f, 0.43482398986816406f, 0.6340299844741821f, 0.10955099761486053f, 0.5949640274047852f, 0.41404399275779724f, 0.5127159953117371f, 0.986873984336853f, 0.3655790090560913f, 0.12996800243854523f, 0.5534440279006958f, 0.5182110071182251f, 0.11182300001382828f, 0.290679007768631f, 0.33554598689079285f, 0.02419630065560341f, 0.4208729863166809f, 0.8313819766044617f, 0.030533799901604652f, 0.7797279953956604f, 0.3514710068702698f, 0.6061339974403381f, 0.08977530151605606f, 0.11878400295972824f, 0.7611629962921143f, 0.6412889957427979f, 0.8838260173797607f, 0.8778340220451355f, 0.8541640043258667f, 0.7252140045166016f, 0.6957799792289734f, 0.9136210083961487f, 0.9126139879226685f, 0.8145800232887268f, 0.5479549765586853f, 0.05634880065917969f, 0.9766759872436523f, 0.015143799595534801f, 0.7671989798545837f, 0.17327499389648438f, 0.5435529947280884f, 0.09290280193090439f, 0.47132599353790283f, 0.18104900419712067f, 0.07774750143289566f, 0.27821698784828186f, 0.10128699988126755f, 0.42168599367141724f, 0.5563600063323975f, 0.3449530005455017f, 0.6816809773445129f, 0.019020000472664833f, 0.6469460129737854f, 0.3105520009994507f, 0.6227430105209351f, 0.7095010280609131f, 0.05420589819550514f, 0.5597180128097534f, 0.041765499860048294f, 0.847931981086731f, 0.35673999786376953f, 0.2802320122718811f, 0.6055920124053955f, 0.2673659920692444f, 0.18005099892616272f, 0.5225759744644165f, 0.03957799822092056f, 0.7175340056419373f, 0.10944399982690811f, 0.3844349980354309f, 0.3657819926738739f, 0.800603985786438f, 0.0009037259733304381f, 0.11138000339269638f, 0.6158570051193237f, 0.33566200733184814f, 0.8246870040893555f, 0.9098230004310608f, 0.47982698678970337f, 0.5092849731445312f, 0.7488549947738647f, 0.5462890267372131f, 0.24059900641441345f, 0.13530799746513367f, 0.48751699924468994f, 0.5431010127067566f, 0.9183139801025391f, 0.7361369729042053f, 0.16797100007534027f, 0.9076740145683289f, 0.8652600049972534f, 0.9297900199890137f, 0.34706100821495056f, 0.028956200927495956f, 0.1838430017232895f, 0.109654001891613f, 0.2521899938583374f, 0.9130929708480835f, 0.21130399405956268f, 0.02394450083374977f, 0.32189100980758667f, 0.013258099555969238f, 0.4911920130252838f, 0.05981859937310219f, 0.6388289928436279f, 0.9229649901390076f, 0.9702960252761841f, 0.6889520287513733f, 0.12994800508022308f, 0.14757399260997772f, 0.26036399602890015f, 0.5568169951438904f, 0.6762359738349915f, 0.9672769904136658f, 0.7419440150260925f, 0.455841988325119f, 0.3721669912338257f, 0.5949649810791016f, 0.19468699395656586f, 0.11099100112915039f, 0.8284729719161987f, 0.8190299868583679f, 0.3003239929676056f, 0.7995839715003967f, 0.8880320191383362f, 0.401993989944458f, 0.751757025718689f, 0.3418770134449005f, 0.1685909926891327f, 0.9586279988288879f, 0.9890440106391907f, 0.3688740134239197f, 0.49949899315834045f, 0.9843339920043945f, 0.4056589901447296f, 0.259568989276886f, 0.15123799443244934f, 0.7644870281219482f, 0.7846180200576782f, 0.4418480098247528f, 0.9725940227508545f, 0.13161100447177887f, 0.03714980185031891f, 0.06934739649295807f, 0.41680100560188293f, 0.32480499148368835f, 0.04877379909157753f, 0.33659499883651733f, 0.0011856600176542997f, 0.48406800627708435f, 0.8562989830970764f, 0.7801610231399536f, 0.16813500225543976f, 0.9025279879570007f, 0.008401390165090561f, 0.011462599970400333f, 0.36333099007606506f, 0.3682210147380829f, 0.5604259967803955f, 0.19352999329566956f, 0.7242540121078491f, 0.9541869759559631f, 0.688539981842041f, 0.1606370061635971f, 0.7773540019989014f, 0.6923210024833679f, 0.15743300318717957f, 0.36914798617362976f, 0.19273200631141663f, 0.9498130083084106f, 0.30757901072502136f, 0.9589409828186035f, 0.4839920103549957f, 0.6023889780044556f, 0.4014430046081543f, 0.2500010132789612f, 0.00034284600405953825f, 0.12046100199222565f, 0.8759030103683472f, 0.40728500485420227f, 0.9731990098953247f, 0.05583230033516884f, 0.29359498620033264f, 0.3623709976673126f, 0.6195449829101562f, 0.36657801270484924f, 0.9700440168380737f, 0.8838480114936829f, 0.3140510022640228f, 0.2728630006313324f, 0.9105420112609863f, 0.33576700091362f, 0.36450299620628357f, 0.21763299405574799f, 0.5863040089607239f, 0.6294699907302856f, 0.9367600083351135f, 0.7471380233764648f, 0.8124600052833557f, 0.6432080268859863f, 0.7404909729957581f, 0.5506899952888489f, 0.336217999458313f, 0.9806990027427673f, 0.26175200939178467f, 0.07827279716730118f, 0.45506998896598816f, 0.48520100116729736f, 0.4434039890766144f, 0.8098300099372864f, 0.14665700495243073f, 0.3135620057582855f, 0.13715599477291107f, 0.634548008441925f, 0.41809800267219543f, 0.5924109816551208f, 0.015503199771046638f, 0.3365269899368286f, 0.2731820046901703f, 0.3063119947910309f, 0.40130001306533813f, 0.519120991230011f, 0.4033240079879761f, 0.08887980133295059f, 0.5533090233802795f, 0.6376609802246094f, 0.6430270075798035f, 0.7115089893341064f, 0.16957400739192963f, 0.5316339731216431f, 0.184578999876976f, 0.8024640083312988f, 0.26278799772262573f, 0.5821849703788757f, 0.8381279706954956f, 0.9032809734344482f, 0.8913599848747253f, 0.40002599358558655f, 0.9627869725227356f, 0.3938620090484619f, 0.09122230112552643f, 0.9248859882354736f, 0.7324270009994507f, 0.9227160215377808f, 0.3200089931488037f, 0.01706800051033497f, 0.24630700051784515f, 0.0766512006521225f, 0.2807300090789795f, 0.9566689729690552f, 0.0005803109961561859f, 0.014700899831950665f, 0.4686639904975891f, 0.606132984161377f, 0.5893639922142029f, 0.9601539969444275f, 0.4577080011367798f, 0.09929119795560837f, 0.22969399392604828f, 0.7228749990463257f, 0.09054780006408691f, 0.4923759996891022f, 0.4609839916229248f, 0.8223559856414795f, 0.6310989856719971f, 0.12001299858093262f, 0.9391999840736389f, 0.025932999327778816f, 0.4932039976119995f, 0.07770589739084244f, 0.9964339733123779f, 0.9067800045013428f, 0.5701720118522644f, 0.13706600666046143f, 0.5874059796333313f, 0.15201200544834137f, 0.33041900396347046f, 0.5977280139923096f, 0.6531959772109985f, 0.21009400486946106f, 0.015048899687826633f, 0.4997740089893341f, 0.5126190185546875f, 0.6625540256500244f, 0.0727493017911911f, 0.9020940065383911f, 0.35155001282691956f, 0.6609290242195129f, 0.5382459759712219f, 0.49376800656318665f, 0.4367159903049469f, 0.3477570116519928f, 0.7184510231018066f, 0.07510670274496078f, 0.5911530256271362f, 0.4763669967651367f, 0.8716490268707275f, 0.7701770067214966f, 0.4941200017929077f, 0.2076449990272522f, 0.300805002450943f, 0.944687008857727f, 0.5409489870071411f, 0.3348020017147064f, 0.6586089730262756f, 0.28032198548316956f, 0.061408501118421555f, 0.5231649875640869f, 0.4809980094432831f, 0.4521870017051697f, 0.39682599902153015f, 0.8142629861831665f, 0.4443880021572113f, 0.09793149679899216f, 0.6135169863700867f, 0.5916630029678345f, 0.3016990125179291f, 0.010077100247144699f, 0.6466749906539917f, 0.7758830189704895f, 0.03205009922385216f, 0.36285600066185f, 0.012346699833869934f, 0.3352630138397217f, 0.7855179905891418f, 0.05485190078616142f, 0.23623299598693848f, 0.9829080104827881f, 0.39774298667907715f, 0.19676299393177032f, 0.2732219994068146f, 0.7388240098953247f, 0.921409010887146f, 0.746146023273468f, 0.18488100171089172f, 0.7805060148239136f, 0.7906680107116699f, 0.8381739854812622f, 0.14072899520397186f, 0.3448770046234131f, 0.43788599967956543f, 0.27044299244880676f, 0.11329899728298187f, 0.056752800941467285f, 0.8720030188560486f, 0.4617129862308502f, 0.3962250053882599f, 0.3345249891281128f, 0.571956992149353f, 0.40614500641822815f, 0.655128002166748f, 0.1111690029501915f, 0.512328028678894f, 0.5792070031166077f, 0.6814370155334473f, 0.31300199031829834f, 0.5263350009918213f, 0.6821920275688171f, 0.5431950092315674f, 0.8836470246315002f, 0.6809309720993042f, 0.9360269904136658f, 0.8872119784355164f, 0.2546190023422241f, 0.5377629995346069f, 0.9293230175971985f, 0.8995530009269714f, 0.58303302526474f, 0.47905901074409485f, 0.5814599990844727f, 0.6056860089302063f, 0.08009540289640427f, 0.5548059940338135f, 0.8053119778633118f, 0.7307010293006897f, 0.46195098757743835f, 0.8612509965896606f, 0.33862200379371643f, 0.9499279856681824f, 0.6764039993286133f, 0.7798110246658325f, 0.6696500182151794f, 0.4134890139102936f, 0.24413299560546875f, 0.25300300121307373f, 0.6683610081672668f, 0.8610420227050781f, 0.11154299974441528f, 0.374332994222641f, 0.6204140186309814f, 0.7306569814682007f, 0.2570990025997162f, 0.9922869801521301f, 0.9627349972724915f, 0.4828200042247772f, 0.10390300303697586f, 0.12056200206279755f, 0.37129101157188416f, 0.8288419842720032f, 0.19805000722408295f, 0.35519999265670776f, 0.13929399847984314f, 0.9835180044174194f, 0.7737060189247131f, 0.30459800362586975f, 0.32107698917388916f, 0.14837199449539185f, 0.9751740097999573f, 0.9022740125656128f, 0.02129499986767769f, 0.10991600155830383f, 0.05525229871273041f, 0.12969599664211273f, 0.8349109888076782f, 0.3548800051212311f, 0.33239299058914185f, 0.6384689807891846f, 0.5050430297851562f, 0.6078490018844604f, 0.4447549879550934f, 0.5078210234642029f, 0.04673580080270767f, 0.966812014579773f, 0.40299999713897705f, 0.9078029990196228f, 0.49101099371910095f, 0.3304559886455536f, 0.40000098943710327f, 0.24617600440979004f, 0.030343199148774147f, 0.7440879940986633f, 0.7126849889755249f, 0.38290899991989136f, 0.07485389709472656f, 0.055230699479579926f, 0.250542014837265f, 0.4323979914188385f, 0.2694050073623657f, 0.9653670191764832f, 0.25564301013946533f, 0.008469220250844955f, 0.4772590100765228f, 0.9569439888000488f, 0.26361900568008423f, 0.4726110100746155f, 0.8182970285415649f, 0.6371809840202332f, 0.8372809886932373f, 0.2902899980545044f, 0.522849977016449f, 0.4114530086517334f, 0.32792699337005615f, 0.49340298771858215f, 0.2928990125656128f, 0.32603099942207336f, 0.0675525963306427f, 0.04320500046014786f, 0.11705099791288376f, 0.7823619842529297f, 0.974107027053833f, 0.7134850025177002f, 0.19260500371456146f, 0.7573419809341431f, 0.07917109876871109f, 0.7739819884300232f, 0.9180750250816345f, 0.053801700472831726f, 0.09023260325193405f, 0.3854770064353943f, 0.6108899712562561f, 0.893094003200531f, 0.2786110043525696f, 0.7495660185813904f, 0.29757699370384216f, 0.34299999475479126f, 0.7009410262107849f, 0.02189899981021881f, 0.7857159972190857f, 0.5754910111427307f, 0.571528971195221f, 0.8958960175514221f, 0.5409370064735413f, 0.6863290071487427f, 0.5191239714622498f, 0.21419699490070343f, 0.2574850022792816f, 0.4794059991836548f, 0.7237690091133118f, 0.1331319957971573f, 0.6541020274162292f, 0.46448299288749695f, 0.5204070210456848f, 0.7390249967575073f, 0.42587101459503174f, 0.04302059859037399f, 0.3670809864997864f, 0.8190749883651733f, 0.5023189783096313f, 0.8089820146560669f, 0.586205005645752f, 0.6931610107421875f, 0.578652024269104f, 0.6709910035133362f, 0.5641390085220337f, 0.16894300282001495f, 0.5786970257759094f, 0.02897140011191368f, 0.3319610059261322f, 0.018561499193310738f, 0.9813410043716431f, 0.7909600138664246f, 0.6838650107383728f, 0.20392799377441406f, 0.8600010275840759f, 0.6301910281181335f, 0.38272199034690857f, 0.8874679803848267f, 0.20951800048351288f, 0.5823720097541809f, 0.19631299376487732f, 0.026310300454497337f, 0.3057290017604828f, 0.7619699835777283f, 0.5245199799537659f, 0.1789650022983551f, 0.7190160155296326f, 0.6052780151367188f, 0.11701200157403946f, 0.8388810157775879f, 0.1084899976849556f, 0.015102099627256393f, 0.695285975933075f, 0.13401800394058228f, 0.9238889813423157f, 0.36162400245666504f, 0.8011519908905029f, 0.34526100754737854f, 0.9923689961433411f, 0.2794260084629059f, 0.574633002281189f, 0.7246840000152588f, 0.26434600353240967f, 0.2160930037498474f, 0.6227669715881348f, 0.9080650210380554f, 0.8823130130767822f, 0.7963579893112183f, 0.15415500104427338f, 0.30882900953292847f, 0.7387999892234802f, 0.483271986246109f, 0.7261350154876709f, 0.09168259799480438f, 0.831043004989624f, 0.14778800308704376f, 0.9828670024871826f, 0.6397640109062195f, 0.30834200978279114f, 0.4809330105781555f, 0.6820250153541565f, 0.03057990036904812f, 0.4485290050506592f, 0.4858990013599396f, 0.6846929788589478f, 0.6044560074806213f, 0.22159400582313538f, 0.46171098947525024f, 0.3506389856338501f, 0.5859100222587585f, 0.9595230221748352f, 0.8319630026817322f, 0.9143049716949463f, 0.361301988363266f, 0.6201649904251099f, 0.26833799481391907f, 0.29616400599479675f, 0.04030970111489296f, 0.8227009773254395f, 0.809844970703125f, 0.5247129797935486f, 0.6451259851455688f, 0.2072339951992035f, 0.18875500559806824f, 0.13563700020313263f, 0.7565079927444458f, 0.1710360050201416f, 0.3328869938850403f, 0.5139409899711609f, 0.15499599277973175f, 0.5550090074539185f, 0.9897400140762329f, 0.3034859895706177f, 0.47330600023269653f, 0.4308049976825714f, 0.1877650022506714f, 0.3771339952945709f, 0.6121919751167297f, 0.9028189778327942f, 0.29554998874664307f, 0.4096269905567169f, 0.8697180151939392f, 0.22623200714588165f, 0.7562490105628967f, 0.9351329803466797f, 0.5468670129776001f, 0.3848150074481964f, 0.1880279928445816f, 0.7508869767189026f, 0.3100380003452301f, 0.4403400123119354f, 0.8241270184516907f, 0.7470970153808594f, 0.12896299362182617f, 0.24835999310016632f, 0.5280619859695435f, 0.5533739924430847f, 0.6893349885940552f, 0.1264680027961731f, 0.17547599971294403f, 0.9756119847297668f, 0.5867030024528503f, 0.6710129976272583f, 0.9627349972724915f, 0.8089069724082947f, 0.08617939800024033f, 0.533115029335022f, 0.10179600119590759f, 0.0706958994269371f, 0.09178219735622406f, 0.04710249975323677f, 0.7776790261268616f, 0.9523530006408691f, 0.3853819966316223f, 0.8200370073318481f, 0.8154209852218628f, 0.47613999247550964f, 0.42414700984954834f, 0.9466720223426819f, 0.9103130102157593f, 0.8849530220031738f, 0.476173996925354f, 0.42655399441719055f, 0.985588014125824f, 0.032336000353097916f, 0.38358399271965027f, 0.054379500448703766f, 0.745913028717041f, 0.9371399879455566f, 0.9841510057449341f, 0.6437879800796509f, 0.6803330183029175f, 0.6602950096130371f, 0.45187899470329285f, 0.8168699741363525f, 0.42302799224853516f, 0.09343139827251434f, 0.3171829879283905f, 0.3243100047111511f, 0.7980489730834961f, 0.3018850088119507f, 0.48286500573158264f, 0.04125490039587021f, 0.30550599098205566f, 0.4989269971847534f, 0.18313699960708618f, 0.44019800424575806f, 0.7089009881019592f, 0.14201900362968445f, 0.7494500279426575f, 0.7440770268440247f, 0.4069249927997589f, 0.5025050044059753f, 0.9773569703102112f, 0.9811859726905823f, 0.45471298694610596f, 0.6945980191230774f, 0.5839740037918091f, 0.006120800040662289f, 0.3136970102787018f, 0.38445499539375305f, 0.14115600287914276f, 0.12160699814558029f, 0.2259960025548935f, 0.25246500968933105f, 0.012700599618256092f, 0.69269198179245f, 0.5424339771270752f, 0.3853749930858612f, 0.01583850011229515f, 0.469897985458374f, 0.8872799873352051f, 0.5801180005073547f, 0.5167980194091797f, 0.12488099932670593f, 0.17155000567436218f, 0.10753999650478363f, 0.6477599740028381f, 0.863565981388092f, 0.5857089757919312f, 0.7374029755592346f, 0.8815780282020569f, 0.7682539820671082f, 0.926181972026825f, 0.7348020076751709f, 0.09246210008859634f, 0.5603010058403015f, 0.9731709957122803f, 0.19762200117111206f, 0.397350013256073f, 0.8312180042266846f, 0.8316789865493774f, 0.6264479756355286f, 0.25536298751831055f, 0.22198699414730072f, 0.5108169913291931f, 0.11971800029277802f, 0.45700401067733765f, 0.7513419985771179f, 0.6820480227470398f, 0.9250280261039734f, 0.4722450077533722f, 0.946412980556488f, 0.7027930021286011f, 0.8494340181350708f, 0.8350239992141724f, 0.7576329708099365f, 0.22765100002288818f, 0.15448999404907227f, 0.5816140174865723f, 0.34401100873947144f, 0.2753300070762634f, 0.8612779974937439f, 0.04803289845585823f, 0.2962230145931244f, 0.6392670273780823f, 0.42297598719596863f, 0.021977199241518974f, 0.5473809838294983f, 0.143435999751091f, 0.4835990071296692f, 0.922199010848999f, 0.6572909951210022f, 0.9681479930877686f, 0.82601398229599f, 0.03627970069646835f, 0.2855150103569031f, 0.7540990114212036f, 0.8199419975280762f, 0.07357999682426453f, 0.4855569899082184f, 0.0095253000035882f, 0.38169699907302856f, 0.6903030276298523f, 0.1660189926624298f, 0.16212299466133118f, 0.7460079789161682f, 0.31747201085090637f, 0.684391975402832f, 0.82464599609375f, 0.18994200229644775f, 0.1558150053024292f, 0.8651750087738037f, 0.5088989734649658f, 0.49884599447250366f, 0.5102970004081726f, 0.3143109977245331f, 0.05806009843945503f, 0.2567499876022339f, 0.8494809865951538f, 0.9973289966583252f, 0.8672379851341248f, 0.2553429901599884f, 0.5759450197219849f, 0.9299259781837463f, 0.16446499526500702f, 0.3823390007019043f, 0.01755939982831478f, 0.5316749811172485f, 0.13380900025367737f, 0.4249269962310791f, 0.9324610233306885f, 0.47343000769615173f, 0.6706990003585815f, 0.45168501138687134f, 0.7279760241508484f, 0.1575939953327179f, 0.6683989763259888f, 0.4314210116863251f, 0.6928030252456665f, 0.30985501408576965f, 0.32075798511505127f, 0.6005769968032837f, 0.8513169884681702f, 0.11071900278329849f, 0.7217299938201904f, 0.05214700102806091f, 0.5084660053253174f, 0.331822007894516f, 0.7322589755058289f, 0.30081701278686523f, 0.7099379897117615f, 0.3411230146884918f, 0.6986169815063477f, 0.21251800656318665f, 0.20261600613594055f, 0.01875700056552887f, 0.1775519996881485f, 0.6130480170249939f, 0.758948028087616f, 0.5470589995384216f, 0.400970995426178f, 0.1158830001950264f, 0.25662100315093994f, 0.7273409962654114f, 0.4780200123786926f, 0.37770599126815796f, 0.3441680073738098f, 0.9038980007171631f, 0.4854069948196411f, 0.6549389958381653f, 0.9836199879646301f, 0.8589749932289124f, 0.5759779810905457f, 0.6987209916114807f, 0.7217050194740295f, 0.841962993144989f, 0.2090650051832199f, 0.43946999311447144f, 0.6268489956855774f, 0.6161080002784729f, 0.5034070014953613f, 0.5515729784965515f, 0.3939779996871948f, 0.014235599897801876f, 0.12449999898672104f, 0.0788038969039917f, 0.32905900478363037f, 0.5400940179824829f, 0.9639040231704712f, 0.5796219706535339f, 0.8064939975738525f, 0.3786279857158661f, 0.7935159802436829f, 0.6491929888725281f, 0.15195199847221375f, 0.5722640156745911f, 0.46097901463508606f, 0.19807900488376617f, 0.9326210021972656f, 0.719996988773346f, 0.9192870259284973f, 0.06938459724187851f, 0.8409079909324646f, 0.8430449962615967f, 0.05573200061917305f, 0.4579069912433624f, 0.9923880100250244f, 0.94514399766922f, 0.9430879950523376f, 0.6763409972190857f, 0.5532680153846741f, 0.5867609977722168f, 0.22488300502300262f, 0.8615080118179321f, 0.566519021987915f, 0.9114519953727722f, 0.5915359854698181f, 0.07332219928503036f, 0.17043200135231018f, 0.35175201296806335f, 0.15709100663661957f, 0.3276729881763458f, 0.868403971195221f, 0.808417022228241f, 0.569936990737915f, 0.4535849988460541f, 0.49894601106643677f, 0.8057299852371216f, 0.6266210079193115f, 0.349575012922287f, 0.6429790258407593f, 0.5311300158500671f, 0.13872799277305603f, 0.4120999872684479f, 0.42597201466560364f, 0.8079620003700256f, 0.8312060236930847f, 0.8397129774093628f, 0.12997299432754517f, 0.5532519817352295f, 0.14785100519657135f, 0.7333170175552368f, 0.19617900252342224f}),
                            .dimensions = {2, 14, 13, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // dummy6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x14x13x3_to_2x6x7x3_float16_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x14x13x3_to_2x6x7x3_float16_all_inputs_as_internal", get_test_model_half_pixel_centers_2x14x13x3_to_2x6x7x3_float16_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x14x13x3_to_2x6x7x3_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.827637f, 0.98301f, 0.250087f, 0.575817f, 0.063061f, 0.534553f, 0.675679f, 0.694844f, 0.497918f, 0.664793f, 0.0200533f, 0.577735f, 0.814857f, 0.843088f, 0.328894f, 0.700158f, 0.540654f, 0.483906f, 0.0713209f, 0.231078f, 0.0430164f, 0.146823f, 0.556193f, 0.372893f, 0.42612f, 0.640223f, 0.326292f, 0.489664f, 0.267468f, 0.413154f, 0.88774f, 0.610036f, 0.942792f, 0.23379f, 0.0979913f, 0.458068f, 0.223809f, 0.0780525f, 0.770556f, 0.391897f, 0.891485f, 0.364972f, 0.847238f, 0.428266f, 0.660148f, 0.990963f, 0.670023f, 0.982757f, 0.0835382f, 0.208294f, 0.689837f, 0.673272f, 0.317975f, 0.382154f, 0.368691f, 0.408292f, 0.0825884f, 0.979362f, 0.667223f, 0.452756f, 0.531345f, 0.361022f, 0.974065f, 0.203924f, 0.0682611f, 0.930153f, 0.272573f, 0.398286f, 0.582229f, 0.552098f, 0.236405f, 0.831928f, 0.253404f, 0.102948f, 0.701941f, 0.472118f, 0.415567f, 0.830793f, 0.995918f, 0.873392f, 0.148214f, 0.385363f, 0.900278f, 0.0703746f, 0.108f, 0.528804f, 0.944798f, 0.949568f, 0.122064f, 0.840799f, 0.532888f, 0.0518012f, 0.966821f, 0.611478f, 0.0889368f, 0.591055f, 0.71221f, 0.399697f, 0.736551f, 0.675313f, 0.0995619f, 0.975917f, 0.329392f, 0.513981f, 0.563206f, 0.86733f, 0.716193f, 0.2221f, 0.215225f, 0.226138f, 0.661863f, 0.465466f, 0.164099f, 0.807117f, 0.22327f, 0.0895369f, 0.982572f, 0.429804f, 0.0558029f, 0.799344f, 0.169512f, 0.569326f, 0.135653f, 0.777692f, 0.11906f, 0.362015f, 0.40525f, 0.0627866f, 0.515949f, 0.000611305f, 0.583503f, 0.947306f, 0.869187f, 0.869985f, 0.945147f, 0.570262f, 0.709512f, 0.0355313f, 0.446349f, 0.80268f, 0.766533f, 0.161885f, 0.0908636f, 0.450652f, 0.111231f, 0.346606f, 0.84161f, 0.524124f, 0.607721f, 0.393173f, 0.103109f, 0.943106f, 0.453668f, 0.598608f, 0.323443f, 0.79512f, 0.227289f, 0.13272f, 0.944727f, 0.653672f, 0.688597f, 0.40432f, 0.0568643f, 0.568614f, 0.962205f, 0.94967f, 0.0944915f, 0.10954f, 0.333137f, 0.815286f, 0.0110344f, 0.3073f, 0.661422f, 0.884207f, 0.744278f, 0.89397f, 0.00762653f, 0.703588f, 0.812244f, 0.194066f, 0.174366f, 0.884352f, 0.898688f, 0.114173f, 0.479097f, 0.452601f, 0.00374603f, 0.791901f, 0.0691f, 0.688598f, 0.329226f, 0.872065f, 0.056097f, 0.810847f, 0.0154784f, 0.986826f, 0.133701f, 0.84835f, 0.301012f, 0.429658f, 0.434824f, 0.63403f, 0.109551f, 0.594964f, 0.414044f, 0.512716f, 0.986874f, 0.365579f, 0.129968f, 0.553444f, 0.518211f, 0.111823f, 0.290679f, 0.335546f, 0.0241963f, 0.420873f, 0.831382f, 0.0305338f, 0.779728f, 0.351471f, 0.606134f, 0.0897753f, 0.118784f, 0.761163f, 0.641289f, 0.883826f, 0.877834f, 0.854164f, 0.725214f, 0.69578f, 0.913621f, 0.912614f, 0.81458f, 0.547955f, 0.0563488f, 0.976676f, 0.0151438f, 0.767199f, 0.173275f, 0.543553f, 0.0929028f, 0.471326f, 0.181049f, 0.0777475f, 0.278217f, 0.101287f, 0.421686f, 0.55636f, 0.344953f, 0.681681f, 0.01902f, 0.646946f, 0.310552f, 0.622743f, 0.709501f, 0.0542059f, 0.559718f, 0.0417655f, 0.847932f, 0.35674f, 0.280232f, 0.605592f, 0.267366f, 0.180051f, 0.522576f, 0.039578f, 0.717534f, 0.109444f, 0.384435f, 0.365782f, 0.800604f, 0.000903726f, 0.11138f, 0.615857f, 0.335662f, 0.824687f, 0.909823f, 0.479827f, 0.509285f, 0.748855f, 0.546289f, 0.240599f, 0.135308f, 0.487517f, 0.543101f, 0.918314f, 0.736137f, 0.167971f, 0.907674f, 0.86526f, 0.92979f, 0.347061f, 0.0289562f, 0.183843f, 0.109654f, 0.25219f, 0.913093f, 0.211304f, 0.0239445f, 0.321891f, 0.0132581f, 0.491192f, 0.0598186f, 0.638829f, 0.922965f, 0.970296f, 0.688952f, 0.129948f, 0.147574f, 0.260364f, 0.556817f, 0.676236f, 0.967277f, 0.741944f, 0.455842f, 0.372167f, 0.594965f, 0.194687f, 0.110991f, 0.828473f, 0.81903f, 0.300324f, 0.799584f, 0.888032f, 0.401994f, 0.751757f, 0.341877f, 0.168591f, 0.958628f, 0.989044f, 0.368874f, 0.499499f, 0.984334f, 0.405659f, 0.259569f, 0.151238f, 0.764487f, 0.784618f, 0.441848f, 0.972594f, 0.131611f, 0.0371498f, 0.0693474f, 0.416801f, 0.324805f, 0.0487738f, 0.336595f, 0.00118566f, 0.484068f, 0.856299f, 0.780161f, 0.168135f, 0.902528f, 0.00840139f, 0.0114626f, 0.363331f, 0.368221f, 0.560426f, 0.19353f, 0.724254f, 0.954187f, 0.68854f, 0.160637f, 0.777354f, 0.692321f, 0.157433f, 0.369148f, 0.192732f, 0.949813f, 0.307579f, 0.958941f, 0.483992f, 0.602389f, 0.401443f, 0.250001f, 0.000342846f, 0.120461f, 0.875903f, 0.407285f, 0.973199f, 0.0558323f, 0.293595f, 0.362371f, 0.619545f, 0.366578f, 0.970044f, 0.883848f, 0.314051f, 0.272863f, 0.910542f, 0.335767f, 0.364503f, 0.217633f, 0.586304f, 0.62947f, 0.93676f, 0.747138f, 0.81246f, 0.643208f, 0.740491f, 0.55069f, 0.336218f, 0.980699f, 0.261752f, 0.0782728f, 0.45507f, 0.485201f, 0.443404f, 0.80983f, 0.146657f, 0.313562f, 0.137156f, 0.634548f, 0.418098f, 0.592411f, 0.0155032f, 0.336527f, 0.273182f, 0.306312f, 0.4013f, 0.519121f, 0.403324f, 0.0888798f, 0.553309f, 0.637661f, 0.643027f, 0.711509f, 0.169574f, 0.531634f, 0.184579f, 0.802464f, 0.262788f, 0.582185f, 0.838128f, 0.903281f, 0.89136f, 0.400026f, 0.962787f, 0.393862f, 0.0912223f, 0.924886f, 0.732427f, 0.922716f, 0.320009f, 0.017068f, 0.246307f, 0.0766512f, 0.28073f, 0.956669f, 0.000580311f, 0.0147009f, 0.468664f, 0.606133f, 0.589364f, 0.960154f, 0.457708f, 0.0992912f, 0.229694f, 0.722875f, 0.0905478f, 0.492376f, 0.460984f, 0.822356f, 0.631099f, 0.120013f, 0.9392f, 0.025933f, 0.493204f, 0.0777059f, 0.996434f, 0.90678f, 0.570172f, 0.137066f, 0.587406f, 0.152012f, 0.330419f, 0.597728f, 0.653196f, 0.210094f, 0.0150489f, 0.499774f, 0.512619f, 0.662554f, 0.0727493f, 0.902094f, 0.35155f, 0.660929f, 0.538246f, 0.493768f, 0.436716f, 0.347757f, 0.718451f, 0.0751067f, 0.591153f, 0.476367f, 0.871649f, 0.770177f, 0.49412f, 0.207645f, 0.300805f, 0.944687f, 0.540949f, 0.334802f, 0.658609f, 0.280322f, 0.0614085f, 0.523165f, 0.480998f, 0.452187f, 0.396826f, 0.814263f, 0.444388f, 0.0979315f, 0.613517f, 0.591663f, 0.301699f, 0.0100771f, 0.646675f, 0.775883f, 0.0320501f, 0.362856f, 0.0123467f, 0.335263f, 0.785518f, 0.0548519f, 0.236233f, 0.982908f, 0.397743f, 0.196763f, 0.273222f, 0.738824f, 0.921409f, 0.746146f, 0.184881f, 0.780506f, 0.790668f, 0.838174f, 0.140729f, 0.344877f, 0.437886f, 0.270443f, 0.113299f, 0.0567528f, 0.872003f, 0.461713f, 0.396225f, 0.334525f, 0.571957f, 0.406145f, 0.655128f, 0.111169f, 0.512328f, 0.579207f, 0.681437f, 0.313002f, 0.526335f, 0.682192f, 0.543195f, 0.883647f, 0.680931f, 0.936027f, 0.887212f, 0.254619f, 0.537763f, 0.929323f, 0.899553f, 0.583033f, 0.479059f, 0.58146f, 0.605686f, 0.0800954f, 0.554806f, 0.805312f, 0.730701f, 0.461951f, 0.861251f, 0.338622f, 0.949928f, 0.676404f, 0.779811f, 0.66965f, 0.413489f, 0.244133f, 0.253003f, 0.668361f, 0.861042f, 0.111543f, 0.374333f, 0.620414f, 0.730657f, 0.257099f, 0.992287f, 0.962735f, 0.48282f, 0.103903f, 0.120562f, 0.371291f, 0.828842f, 0.19805f, 0.3552f, 0.139294f, 0.983518f, 0.773706f, 0.304598f, 0.321077f, 0.148372f, 0.975174f, 0.902274f, 0.021295f, 0.109916f, 0.0552523f, 0.129696f, 0.834911f, 0.35488f, 0.332393f, 0.638469f, 0.505043f, 0.607849f, 0.444755f, 0.507821f, 0.0467358f, 0.966812f, 0.403f, 0.907803f, 0.491011f, 0.330456f, 0.400001f, 0.246176f, 0.0303432f, 0.744088f, 0.712685f, 0.382909f, 0.0748539f, 0.0552307f, 0.250542f, 0.432398f, 0.269405f, 0.965367f, 0.255643f, 0.00846922f, 0.477259f, 0.956944f, 0.263619f, 0.472611f, 0.818297f, 0.637181f, 0.837281f, 0.29029f, 0.52285f, 0.411453f, 0.327927f, 0.493403f, 0.292899f, 0.326031f, 0.0675526f, 0.043205f, 0.117051f, 0.782362f, 0.974107f, 0.713485f, 0.192605f, 0.757342f, 0.0791711f, 0.773982f, 0.918075f, 0.0538017f, 0.0902326f, 0.385477f, 0.61089f, 0.893094f, 0.278611f, 0.749566f, 0.297577f, 0.343f, 0.700941f, 0.021899f, 0.785716f, 0.575491f, 0.571529f, 0.895896f, 0.540937f, 0.686329f, 0.519124f, 0.214197f, 0.257485f, 0.479406f, 0.723769f, 0.133132f, 0.654102f, 0.464483f, 0.520407f, 0.739025f, 0.425871f, 0.0430206f, 0.367081f, 0.819075f, 0.502319f, 0.808982f, 0.586205f, 0.693161f, 0.578652f, 0.670991f, 0.564139f, 0.168943f, 0.578697f, 0.0289714f, 0.331961f, 0.0185615f, 0.981341f, 0.79096f, 0.683865f, 0.203928f, 0.860001f, 0.630191f, 0.382722f, 0.887468f, 0.209518f, 0.582372f, 0.196313f, 0.0263103f, 0.305729f, 0.76197f, 0.52452f, 0.178965f, 0.719016f, 0.605278f, 0.117012f, 0.838881f, 0.10849f, 0.0151021f, 0.695286f, 0.134018f, 0.923889f, 0.361624f, 0.801152f, 0.345261f, 0.992369f, 0.279426f, 0.574633f, 0.724684f, 0.264346f, 0.216093f, 0.622767f, 0.908065f, 0.882313f, 0.796358f, 0.154155f, 0.308829f, 0.7388f, 0.483272f, 0.726135f, 0.0916826f, 0.831043f, 0.147788f, 0.982867f, 0.639764f, 0.308342f, 0.480933f, 0.682025f, 0.0305799f, 0.448529f, 0.485899f, 0.684693f, 0.604456f, 0.221594f, 0.461711f, 0.350639f, 0.58591f, 0.959523f, 0.831963f, 0.914305f, 0.361302f, 0.620165f, 0.268338f, 0.296164f, 0.0403097f, 0.822701f, 0.809845f, 0.524713f, 0.645126f, 0.207234f, 0.188755f, 0.135637f, 0.756508f, 0.171036f, 0.332887f, 0.513941f, 0.154996f, 0.555009f, 0.98974f, 0.303486f, 0.473306f, 0.430805f, 0.187765f, 0.377134f, 0.612192f, 0.902819f, 0.29555f, 0.409627f, 0.869718f, 0.226232f, 0.756249f, 0.935133f, 0.546867f, 0.384815f, 0.188028f, 0.750887f, 0.310038f, 0.44034f, 0.824127f, 0.747097f, 0.128963f, 0.24836f, 0.528062f, 0.553374f, 0.689335f, 0.126468f, 0.175476f, 0.975612f, 0.586703f, 0.671013f, 0.962735f, 0.808907f, 0.0861794f, 0.533115f, 0.101796f, 0.0706959f, 0.0917822f, 0.0471025f, 0.777679f, 0.952353f, 0.385382f, 0.820037f, 0.815421f, 0.47614f, 0.424147f, 0.946672f, 0.910313f, 0.884953f, 0.476174f, 0.426554f, 0.985588f, 0.032336f, 0.383584f, 0.0543795f, 0.745913f, 0.93714f, 0.984151f, 0.643788f, 0.680333f, 0.660295f, 0.451879f, 0.81687f, 0.423028f, 0.0934314f, 0.317183f, 0.32431f, 0.798049f, 0.301885f, 0.482865f, 0.0412549f, 0.305506f, 0.498927f, 0.183137f, 0.440198f, 0.708901f, 0.142019f, 0.74945f, 0.744077f, 0.406925f, 0.502505f, 0.977357f, 0.981186f, 0.454713f, 0.694598f, 0.583974f, 0.0061208f, 0.313697f, 0.384455f, 0.141156f, 0.121607f, 0.225996f, 0.252465f, 0.0127006f, 0.692692f, 0.542434f, 0.385375f, 0.0158385f, 0.469898f, 0.88728f, 0.580118f, 0.516798f, 0.124881f, 0.17155f, 0.10754f, 0.64776f, 0.863566f, 0.585709f, 0.737403f, 0.881578f, 0.768254f, 0.926182f, 0.734802f, 0.0924621f, 0.560301f, 0.973171f, 0.197622f, 0.39735f, 0.831218f, 0.831679f, 0.626448f, 0.255363f, 0.221987f, 0.510817f, 0.119718f, 0.457004f, 0.751342f, 0.682048f, 0.925028f, 0.472245f, 0.946413f, 0.702793f, 0.849434f, 0.835024f, 0.757633f, 0.227651f, 0.15449f, 0.581614f, 0.344011f, 0.27533f, 0.861278f, 0.0480329f, 0.296223f, 0.639267f, 0.422976f, 0.0219772f, 0.547381f, 0.143436f, 0.483599f, 0.922199f, 0.657291f, 0.968148f, 0.826014f, 0.0362797f, 0.285515f, 0.754099f, 0.819942f, 0.07358f, 0.485557f, 0.0095253f, 0.381697f, 0.690303f, 0.166019f, 0.162123f, 0.746008f, 0.317472f, 0.684392f, 0.824646f, 0.189942f, 0.155815f, 0.865175f, 0.508899f, 0.498846f, 0.510297f, 0.314311f, 0.0580601f, 0.25675f, 0.849481f, 0.997329f, 0.867238f, 0.255343f, 0.575945f, 0.929926f, 0.164465f, 0.382339f, 0.0175594f, 0.531675f, 0.133809f, 0.424927f, 0.932461f, 0.47343f, 0.670699f, 0.451685f, 0.727976f, 0.157594f, 0.668399f, 0.431421f, 0.692803f, 0.309855f, 0.320758f, 0.600577f, 0.851317f, 0.110719f, 0.72173f, 0.052147f, 0.508466f, 0.331822f, 0.732259f, 0.300817f, 0.709938f, 0.341123f, 0.698617f, 0.212518f, 0.202616f, 0.018757f, 0.177552f, 0.613048f, 0.758948f, 0.547059f, 0.400971f, 0.115883f, 0.256621f, 0.727341f, 0.47802f, 0.377706f, 0.344168f, 0.903898f, 0.485407f, 0.654939f, 0.98362f, 0.858975f, 0.575978f, 0.698721f, 0.721705f, 0.841963f, 0.209065f, 0.43947f, 0.626849f, 0.616108f, 0.503407f, 0.551573f, 0.393978f, 0.0142356f, 0.1245f, 0.0788039f, 0.329059f, 0.540094f, 0.963904f, 0.579622f, 0.806494f, 0.378628f, 0.793516f, 0.649193f, 0.151952f, 0.572264f, 0.460979f, 0.198079f, 0.932621f, 0.719997f, 0.919287f, 0.0693846f, 0.840908f, 0.843045f, 0.055732f, 0.457907f, 0.992388f, 0.945144f, 0.943088f, 0.676341f, 0.553268f, 0.586761f, 0.224883f, 0.861508f, 0.566519f, 0.911452f, 0.591536f, 0.0733222f, 0.170432f, 0.351752f, 0.157091f, 0.327673f, 0.868404f, 0.808417f, 0.569937f, 0.453585f, 0.498946f, 0.80573f, 0.626621f, 0.349575f, 0.642979f, 0.53113f, 0.138728f, 0.4121f, 0.425972f, 0.807962f, 0.831206f, 0.839713f, 0.129973f, 0.553252f, 0.147851f, 0.733317f, 0.196179f}),
                            .dimensions = {2, 14, 13, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({7}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({6}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.631267f, 0.658224f, 0.451651f, 0.711989f, 0.526083f, 0.772951f, 0.685998f, 0.487213f, 0.343252f, 0.676681f, 0.521841f, 0.316176f, 0.295872f, 0.282796f, 0.735267f, 0.572824f, 0.359158f, 0.770879f, 0.328285f, 0.216591f, 0.729258f, 0.318251f, 0.275884f, 0.514905f, 0.67128f, 0.102982f, 0.405996f, 0.124694f, 0.624429f, 0.946997f, 0.570262f, 0.709512f, 0.0355313f, 0.202523f, 0.192552f, 0.495778f, 0.406154f, 0.533116f, 0.521298f, 0.386251f, 0.589013f, 0.648783f, 0.58227f, 0.38448f, 0.527951f, 0.410309f, 0.288544f, 0.524046f, 0.411198f, 0.244783f, 0.473302f, 0.327214f, 0.223712f, 0.517082f, 0.524867f, 0.477849f, 0.207615f, 0.625594f, 0.597887f, 0.716884f, 0.722997f, 0.564202f, 0.543249f, 0.722101f, 0.585117f, 0.459117f, 0.301243f, 0.723311f, 0.668172f, 0.551366f, 0.772401f, 0.38182f, 0.361603f, 0.342651f, 0.717503f, 0.541077f, 0.491924f, 0.402338f, 0.371644f, 0.490079f, 0.398466f, 0.346357f, 0.547003f, 0.447432f, 0.44314f, 0.47806f, 0.52583f, 0.745239f, 0.737669f, 0.555497f, 0.905609f, 0.293674f, 0.130434f, 0.80983f, 0.146657f, 0.313562f, 0.527374f, 0.103938f, 0.34818f, 0.448853f, 0.375606f, 0.178143f, 0.643709f, 0.370183f, 0.579374f, 0.40045f, 0.568371f, 0.602847f, 0.34384f, 0.39982f, 0.433225f, 0.317636f, 0.519918f, 0.418223f, 0.519364f, 0.686467f, 0.46176f, 0.357139f, 0.597116f, 0.428639f, 0.372748f, 0.447356f, 0.728054f, 0.466077f, 0.223182f, 0.471054f, 0.662586f, 0.376077f, 0.36661f, 0.680342f, 0.708627f, 0.426894f, 0.781609f, 0.413516f, 0.569702f, 0.51284f, 0.513902f, 0.293259f, 0.20138f, 0.303709f, 0.335072f, 0.613278f, 0.578262f, 0.595837f, 0.653285f, 0.442471f, 0.545559f, 0.480946f, 0.689819f, 0.292554f, 0.722947f, 0.297008f, 0.735673f, 0.100418f, 0.801456f, 0.570554f, 0.686329f, 0.519124f, 0.214197f, 0.150897f, 0.629145f, 0.501524f, 0.179417f, 0.47335f, 0.706731f, 0.611372f, 0.677365f, 0.634654f, 0.481956f, 0.525163f, 0.463365f, 0.502f, 0.43508f, 0.565645f, 0.266055f, 0.70408f, 0.55236f, 0.156929f, 0.588925f, 0.399344f, 0.475003f, 0.356185f, 0.59649f, 0.562223f, 0.526428f, 0.439555f, 0.492657f, 0.323885f, 0.765127f, 0.429603f, 0.466623f, 0.327579f, 0.381093f, 0.570198f, 0.257691f, 0.463234f, 0.879136f, 0.894351f, 0.307519f, 0.504116f, 0.415028f, 0.287617f, 0.0813091f, 0.632005f, 0.578317f, 0.746861f, 0.448167f, 0.365736f, 0.404652f, 0.543348f, 0.708017f, 0.363659f, 0.691263f, 0.805467f, 0.260183f, 0.224962f, 0.514807f, 0.0318816f, 0.350329f, 0.746008f, 0.317472f, 0.684392f, 0.859385f, 0.463334f, 0.449842f, 0.329192f, 0.696575f, 0.728967f, 0.90306f, 0.203413f, 0.465313f, 0.428212f, 0.540184f, 0.911023f, 0.760282f, 0.628308f, 0.512895f, 0.484829f, 0.43916f, 0.453882f, 0.298958f, 0.352472f, 0.242515f, 0.563185f, 0.635929f, 0.530235f, 0.575659f, 0.548754f, 0.36042f, 0.441017f, 0.684306f, 0.700217f}),
                            .dimensions = {2, 6, 7, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x14x13x3_to_2x6x7x3_relaxed = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x14x13x3_to_2x6x7x3_relaxed", get_test_model_half_pixel_centers_2x14x13x3_to_2x6x7x3_relaxed());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x14x13x3_to_2x6x7x3_relaxed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {2, 14, 13, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({7}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({6}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output01
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.631267f, 0.658224f, 0.451651f, 0.711989f, 0.526083f, 0.772951f, 0.685998f, 0.487213f, 0.343252f, 0.676681f, 0.521841f, 0.316176f, 0.295872f, 0.282796f, 0.735267f, 0.572824f, 0.359158f, 0.770879f, 0.328285f, 0.216591f, 0.729258f, 0.318251f, 0.275884f, 0.514905f, 0.67128f, 0.102982f, 0.405996f, 0.124694f, 0.624429f, 0.946997f, 0.570262f, 0.709512f, 0.0355313f, 0.202523f, 0.192552f, 0.495778f, 0.406154f, 0.533116f, 0.521298f, 0.386251f, 0.589013f, 0.648783f, 0.58227f, 0.38448f, 0.527951f, 0.410309f, 0.288544f, 0.524046f, 0.411198f, 0.244783f, 0.473302f, 0.327214f, 0.223712f, 0.517082f, 0.524867f, 0.477849f, 0.207615f, 0.625594f, 0.597887f, 0.716884f, 0.722997f, 0.564202f, 0.543249f, 0.722101f, 0.585117f, 0.459117f, 0.301243f, 0.723311f, 0.668172f, 0.551366f, 0.772401f, 0.38182f, 0.361603f, 0.342651f, 0.717503f, 0.541077f, 0.491924f, 0.402338f, 0.371644f, 0.490079f, 0.398466f, 0.346357f, 0.547003f, 0.447432f, 0.44314f, 0.47806f, 0.52583f, 0.745239f, 0.737669f, 0.555497f, 0.905609f, 0.293674f, 0.130434f, 0.80983f, 0.146657f, 0.313562f, 0.527374f, 0.103938f, 0.34818f, 0.448853f, 0.375606f, 0.178143f, 0.643709f, 0.370183f, 0.579374f, 0.40045f, 0.568371f, 0.602847f, 0.34384f, 0.39982f, 0.433225f, 0.317636f, 0.519918f, 0.418223f, 0.519364f, 0.686467f, 0.46176f, 0.357139f, 0.597116f, 0.428639f, 0.372748f, 0.447356f, 0.728054f, 0.466077f, 0.223182f, 0.471054f, 0.662586f, 0.376077f, 0.36661f, 0.680342f, 0.708627f, 0.426894f, 0.781609f, 0.413516f, 0.569702f, 0.51284f, 0.513902f, 0.293259f, 0.20138f, 0.303709f, 0.335072f, 0.613278f, 0.578262f, 0.595837f, 0.653285f, 0.442471f, 0.545559f, 0.480946f, 0.689819f, 0.292554f, 0.722947f, 0.297008f, 0.735673f, 0.100418f, 0.801456f, 0.570554f, 0.686329f, 0.519124f, 0.214197f, 0.150897f, 0.629145f, 0.501524f, 0.179417f, 0.47335f, 0.706731f, 0.611372f, 0.677365f, 0.634654f, 0.481956f, 0.525163f, 0.463365f, 0.502f, 0.43508f, 0.565645f, 0.266055f, 0.70408f, 0.55236f, 0.156929f, 0.588925f, 0.399344f, 0.475003f, 0.356185f, 0.59649f, 0.562223f, 0.526428f, 0.439555f, 0.492657f, 0.323885f, 0.765127f, 0.429603f, 0.466623f, 0.327579f, 0.381093f, 0.570198f, 0.257691f, 0.463234f, 0.879136f, 0.894351f, 0.307519f, 0.504116f, 0.415028f, 0.287617f, 0.0813091f, 0.632005f, 0.578317f, 0.746861f, 0.448167f, 0.365736f, 0.404652f, 0.543348f, 0.708017f, 0.363659f, 0.691263f, 0.805467f, 0.260183f, 0.224962f, 0.514807f, 0.0318816f, 0.350329f, 0.746008f, 0.317472f, 0.684392f, 0.859385f, 0.463334f, 0.449842f, 0.329192f, 0.696575f, 0.728967f, 0.90306f, 0.203413f, 0.465313f, 0.428212f, 0.540184f, 0.911023f, 0.760282f, 0.628308f, 0.512895f, 0.484829f, 0.43916f, 0.453882f, 0.298958f, 0.352472f, 0.242515f, 0.563185f, 0.635929f, 0.530235f, 0.575659f, 0.548754f, 0.36042f, 0.441017f, 0.684306f, 0.700217f}),
                            .dimensions = {2, 6, 7, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input01_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.827637f, 0.98301f, 0.250087f, 0.575817f, 0.063061f, 0.534553f, 0.675679f, 0.694844f, 0.497918f, 0.664793f, 0.0200533f, 0.577735f, 0.814857f, 0.843088f, 0.328894f, 0.700158f, 0.540654f, 0.483906f, 0.0713209f, 0.231078f, 0.0430164f, 0.146823f, 0.556193f, 0.372893f, 0.42612f, 0.640223f, 0.326292f, 0.489664f, 0.267468f, 0.413154f, 0.88774f, 0.610036f, 0.942792f, 0.23379f, 0.0979913f, 0.458068f, 0.223809f, 0.0780525f, 0.770556f, 0.391897f, 0.891485f, 0.364972f, 0.847238f, 0.428266f, 0.660148f, 0.990963f, 0.670023f, 0.982757f, 0.0835382f, 0.208294f, 0.689837f, 0.673272f, 0.317975f, 0.382154f, 0.368691f, 0.408292f, 0.0825884f, 0.979362f, 0.667223f, 0.452756f, 0.531345f, 0.361022f, 0.974065f, 0.203924f, 0.0682611f, 0.930153f, 0.272573f, 0.398286f, 0.582229f, 0.552098f, 0.236405f, 0.831928f, 0.253404f, 0.102948f, 0.701941f, 0.472118f, 0.415567f, 0.830793f, 0.995918f, 0.873392f, 0.148214f, 0.385363f, 0.900278f, 0.0703746f, 0.108f, 0.528804f, 0.944798f, 0.949568f, 0.122064f, 0.840799f, 0.532888f, 0.0518012f, 0.966821f, 0.611478f, 0.0889368f, 0.591055f, 0.71221f, 0.399697f, 0.736551f, 0.675313f, 0.0995619f, 0.975917f, 0.329392f, 0.513981f, 0.563206f, 0.86733f, 0.716193f, 0.2221f, 0.215225f, 0.226138f, 0.661863f, 0.465466f, 0.164099f, 0.807117f, 0.22327f, 0.0895369f, 0.982572f, 0.429804f, 0.0558029f, 0.799344f, 0.169512f, 0.569326f, 0.135653f, 0.777692f, 0.11906f, 0.362015f, 0.40525f, 0.0627866f, 0.515949f, 0.000611305f, 0.583503f, 0.947306f, 0.869187f, 0.869985f, 0.945147f, 0.570262f, 0.709512f, 0.0355313f, 0.446349f, 0.80268f, 0.766533f, 0.161885f, 0.0908636f, 0.450652f, 0.111231f, 0.346606f, 0.84161f, 0.524124f, 0.607721f, 0.393173f, 0.103109f, 0.943106f, 0.453668f, 0.598608f, 0.323443f, 0.79512f, 0.227289f, 0.13272f, 0.944727f, 0.653672f, 0.688597f, 0.40432f, 0.0568643f, 0.568614f, 0.962205f, 0.94967f, 0.0944915f, 0.10954f, 0.333137f, 0.815286f, 0.0110344f, 0.3073f, 0.661422f, 0.884207f, 0.744278f, 0.89397f, 0.00762653f, 0.703588f, 0.812244f, 0.194066f, 0.174366f, 0.884352f, 0.898688f, 0.114173f, 0.479097f, 0.452601f, 0.00374603f, 0.791901f, 0.0691f, 0.688598f, 0.329226f, 0.872065f, 0.056097f, 0.810847f, 0.0154784f, 0.986826f, 0.133701f, 0.84835f, 0.301012f, 0.429658f, 0.434824f, 0.63403f, 0.109551f, 0.594964f, 0.414044f, 0.512716f, 0.986874f, 0.365579f, 0.129968f, 0.553444f, 0.518211f, 0.111823f, 0.290679f, 0.335546f, 0.0241963f, 0.420873f, 0.831382f, 0.0305338f, 0.779728f, 0.351471f, 0.606134f, 0.0897753f, 0.118784f, 0.761163f, 0.641289f, 0.883826f, 0.877834f, 0.854164f, 0.725214f, 0.69578f, 0.913621f, 0.912614f, 0.81458f, 0.547955f, 0.0563488f, 0.976676f, 0.0151438f, 0.767199f, 0.173275f, 0.543553f, 0.0929028f, 0.471326f, 0.181049f, 0.0777475f, 0.278217f, 0.101287f, 0.421686f, 0.55636f, 0.344953f, 0.681681f, 0.01902f, 0.646946f, 0.310552f, 0.622743f, 0.709501f, 0.0542059f, 0.559718f, 0.0417655f, 0.847932f, 0.35674f, 0.280232f, 0.605592f, 0.267366f, 0.180051f, 0.522576f, 0.039578f, 0.717534f, 0.109444f, 0.384435f, 0.365782f, 0.800604f, 0.000903726f, 0.11138f, 0.615857f, 0.335662f, 0.824687f, 0.909823f, 0.479827f, 0.509285f, 0.748855f, 0.546289f, 0.240599f, 0.135308f, 0.487517f, 0.543101f, 0.918314f, 0.736137f, 0.167971f, 0.907674f, 0.86526f, 0.92979f, 0.347061f, 0.0289562f, 0.183843f, 0.109654f, 0.25219f, 0.913093f, 0.211304f, 0.0239445f, 0.321891f, 0.0132581f, 0.491192f, 0.0598186f, 0.638829f, 0.922965f, 0.970296f, 0.688952f, 0.129948f, 0.147574f, 0.260364f, 0.556817f, 0.676236f, 0.967277f, 0.741944f, 0.455842f, 0.372167f, 0.594965f, 0.194687f, 0.110991f, 0.828473f, 0.81903f, 0.300324f, 0.799584f, 0.888032f, 0.401994f, 0.751757f, 0.341877f, 0.168591f, 0.958628f, 0.989044f, 0.368874f, 0.499499f, 0.984334f, 0.405659f, 0.259569f, 0.151238f, 0.764487f, 0.784618f, 0.441848f, 0.972594f, 0.131611f, 0.0371498f, 0.0693474f, 0.416801f, 0.324805f, 0.0487738f, 0.336595f, 0.00118566f, 0.484068f, 0.856299f, 0.780161f, 0.168135f, 0.902528f, 0.00840139f, 0.0114626f, 0.363331f, 0.368221f, 0.560426f, 0.19353f, 0.724254f, 0.954187f, 0.68854f, 0.160637f, 0.777354f, 0.692321f, 0.157433f, 0.369148f, 0.192732f, 0.949813f, 0.307579f, 0.958941f, 0.483992f, 0.602389f, 0.401443f, 0.250001f, 0.000342846f, 0.120461f, 0.875903f, 0.407285f, 0.973199f, 0.0558323f, 0.293595f, 0.362371f, 0.619545f, 0.366578f, 0.970044f, 0.883848f, 0.314051f, 0.272863f, 0.910542f, 0.335767f, 0.364503f, 0.217633f, 0.586304f, 0.62947f, 0.93676f, 0.747138f, 0.81246f, 0.643208f, 0.740491f, 0.55069f, 0.336218f, 0.980699f, 0.261752f, 0.0782728f, 0.45507f, 0.485201f, 0.443404f, 0.80983f, 0.146657f, 0.313562f, 0.137156f, 0.634548f, 0.418098f, 0.592411f, 0.0155032f, 0.336527f, 0.273182f, 0.306312f, 0.4013f, 0.519121f, 0.403324f, 0.0888798f, 0.553309f, 0.637661f, 0.643027f, 0.711509f, 0.169574f, 0.531634f, 0.184579f, 0.802464f, 0.262788f, 0.582185f, 0.838128f, 0.903281f, 0.89136f, 0.400026f, 0.962787f, 0.393862f, 0.0912223f, 0.924886f, 0.732427f, 0.922716f, 0.320009f, 0.017068f, 0.246307f, 0.0766512f, 0.28073f, 0.956669f, 0.000580311f, 0.0147009f, 0.468664f, 0.606133f, 0.589364f, 0.960154f, 0.457708f, 0.0992912f, 0.229694f, 0.722875f, 0.0905478f, 0.492376f, 0.460984f, 0.822356f, 0.631099f, 0.120013f, 0.9392f, 0.025933f, 0.493204f, 0.0777059f, 0.996434f, 0.90678f, 0.570172f, 0.137066f, 0.587406f, 0.152012f, 0.330419f, 0.597728f, 0.653196f, 0.210094f, 0.0150489f, 0.499774f, 0.512619f, 0.662554f, 0.0727493f, 0.902094f, 0.35155f, 0.660929f, 0.538246f, 0.493768f, 0.436716f, 0.347757f, 0.718451f, 0.0751067f, 0.591153f, 0.476367f, 0.871649f, 0.770177f, 0.49412f, 0.207645f, 0.300805f, 0.944687f, 0.540949f, 0.334802f, 0.658609f, 0.280322f, 0.0614085f, 0.523165f, 0.480998f, 0.452187f, 0.396826f, 0.814263f, 0.444388f, 0.0979315f, 0.613517f, 0.591663f, 0.301699f, 0.0100771f, 0.646675f, 0.775883f, 0.0320501f, 0.362856f, 0.0123467f, 0.335263f, 0.785518f, 0.0548519f, 0.236233f, 0.982908f, 0.397743f, 0.196763f, 0.273222f, 0.738824f, 0.921409f, 0.746146f, 0.184881f, 0.780506f, 0.790668f, 0.838174f, 0.140729f, 0.344877f, 0.437886f, 0.270443f, 0.113299f, 0.0567528f, 0.872003f, 0.461713f, 0.396225f, 0.334525f, 0.571957f, 0.406145f, 0.655128f, 0.111169f, 0.512328f, 0.579207f, 0.681437f, 0.313002f, 0.526335f, 0.682192f, 0.543195f, 0.883647f, 0.680931f, 0.936027f, 0.887212f, 0.254619f, 0.537763f, 0.929323f, 0.899553f, 0.583033f, 0.479059f, 0.58146f, 0.605686f, 0.0800954f, 0.554806f, 0.805312f, 0.730701f, 0.461951f, 0.861251f, 0.338622f, 0.949928f, 0.676404f, 0.779811f, 0.66965f, 0.413489f, 0.244133f, 0.253003f, 0.668361f, 0.861042f, 0.111543f, 0.374333f, 0.620414f, 0.730657f, 0.257099f, 0.992287f, 0.962735f, 0.48282f, 0.103903f, 0.120562f, 0.371291f, 0.828842f, 0.19805f, 0.3552f, 0.139294f, 0.983518f, 0.773706f, 0.304598f, 0.321077f, 0.148372f, 0.975174f, 0.902274f, 0.021295f, 0.109916f, 0.0552523f, 0.129696f, 0.834911f, 0.35488f, 0.332393f, 0.638469f, 0.505043f, 0.607849f, 0.444755f, 0.507821f, 0.0467358f, 0.966812f, 0.403f, 0.907803f, 0.491011f, 0.330456f, 0.400001f, 0.246176f, 0.0303432f, 0.744088f, 0.712685f, 0.382909f, 0.0748539f, 0.0552307f, 0.250542f, 0.432398f, 0.269405f, 0.965367f, 0.255643f, 0.00846922f, 0.477259f, 0.956944f, 0.263619f, 0.472611f, 0.818297f, 0.637181f, 0.837281f, 0.29029f, 0.52285f, 0.411453f, 0.327927f, 0.493403f, 0.292899f, 0.326031f, 0.0675526f, 0.043205f, 0.117051f, 0.782362f, 0.974107f, 0.713485f, 0.192605f, 0.757342f, 0.0791711f, 0.773982f, 0.918075f, 0.0538017f, 0.0902326f, 0.385477f, 0.61089f, 0.893094f, 0.278611f, 0.749566f, 0.297577f, 0.343f, 0.700941f, 0.021899f, 0.785716f, 0.575491f, 0.571529f, 0.895896f, 0.540937f, 0.686329f, 0.519124f, 0.214197f, 0.257485f, 0.479406f, 0.723769f, 0.133132f, 0.654102f, 0.464483f, 0.520407f, 0.739025f, 0.425871f, 0.0430206f, 0.367081f, 0.819075f, 0.502319f, 0.808982f, 0.586205f, 0.693161f, 0.578652f, 0.670991f, 0.564139f, 0.168943f, 0.578697f, 0.0289714f, 0.331961f, 0.0185615f, 0.981341f, 0.79096f, 0.683865f, 0.203928f, 0.860001f, 0.630191f, 0.382722f, 0.887468f, 0.209518f, 0.582372f, 0.196313f, 0.0263103f, 0.305729f, 0.76197f, 0.52452f, 0.178965f, 0.719016f, 0.605278f, 0.117012f, 0.838881f, 0.10849f, 0.0151021f, 0.695286f, 0.134018f, 0.923889f, 0.361624f, 0.801152f, 0.345261f, 0.992369f, 0.279426f, 0.574633f, 0.724684f, 0.264346f, 0.216093f, 0.622767f, 0.908065f, 0.882313f, 0.796358f, 0.154155f, 0.308829f, 0.7388f, 0.483272f, 0.726135f, 0.0916826f, 0.831043f, 0.147788f, 0.982867f, 0.639764f, 0.308342f, 0.480933f, 0.682025f, 0.0305799f, 0.448529f, 0.485899f, 0.684693f, 0.604456f, 0.221594f, 0.461711f, 0.350639f, 0.58591f, 0.959523f, 0.831963f, 0.914305f, 0.361302f, 0.620165f, 0.268338f, 0.296164f, 0.0403097f, 0.822701f, 0.809845f, 0.524713f, 0.645126f, 0.207234f, 0.188755f, 0.135637f, 0.756508f, 0.171036f, 0.332887f, 0.513941f, 0.154996f, 0.555009f, 0.98974f, 0.303486f, 0.473306f, 0.430805f, 0.187765f, 0.377134f, 0.612192f, 0.902819f, 0.29555f, 0.409627f, 0.869718f, 0.226232f, 0.756249f, 0.935133f, 0.546867f, 0.384815f, 0.188028f, 0.750887f, 0.310038f, 0.44034f, 0.824127f, 0.747097f, 0.128963f, 0.24836f, 0.528062f, 0.553374f, 0.689335f, 0.126468f, 0.175476f, 0.975612f, 0.586703f, 0.671013f, 0.962735f, 0.808907f, 0.0861794f, 0.533115f, 0.101796f, 0.0706959f, 0.0917822f, 0.0471025f, 0.777679f, 0.952353f, 0.385382f, 0.820037f, 0.815421f, 0.47614f, 0.424147f, 0.946672f, 0.910313f, 0.884953f, 0.476174f, 0.426554f, 0.985588f, 0.032336f, 0.383584f, 0.0543795f, 0.745913f, 0.93714f, 0.984151f, 0.643788f, 0.680333f, 0.660295f, 0.451879f, 0.81687f, 0.423028f, 0.0934314f, 0.317183f, 0.32431f, 0.798049f, 0.301885f, 0.482865f, 0.0412549f, 0.305506f, 0.498927f, 0.183137f, 0.440198f, 0.708901f, 0.142019f, 0.74945f, 0.744077f, 0.406925f, 0.502505f, 0.977357f, 0.981186f, 0.454713f, 0.694598f, 0.583974f, 0.0061208f, 0.313697f, 0.384455f, 0.141156f, 0.121607f, 0.225996f, 0.252465f, 0.0127006f, 0.692692f, 0.542434f, 0.385375f, 0.0158385f, 0.469898f, 0.88728f, 0.580118f, 0.516798f, 0.124881f, 0.17155f, 0.10754f, 0.64776f, 0.863566f, 0.585709f, 0.737403f, 0.881578f, 0.768254f, 0.926182f, 0.734802f, 0.0924621f, 0.560301f, 0.973171f, 0.197622f, 0.39735f, 0.831218f, 0.831679f, 0.626448f, 0.255363f, 0.221987f, 0.510817f, 0.119718f, 0.457004f, 0.751342f, 0.682048f, 0.925028f, 0.472245f, 0.946413f, 0.702793f, 0.849434f, 0.835024f, 0.757633f, 0.227651f, 0.15449f, 0.581614f, 0.344011f, 0.27533f, 0.861278f, 0.0480329f, 0.296223f, 0.639267f, 0.422976f, 0.0219772f, 0.547381f, 0.143436f, 0.483599f, 0.922199f, 0.657291f, 0.968148f, 0.826014f, 0.0362797f, 0.285515f, 0.754099f, 0.819942f, 0.07358f, 0.485557f, 0.0095253f, 0.381697f, 0.690303f, 0.166019f, 0.162123f, 0.746008f, 0.317472f, 0.684392f, 0.824646f, 0.189942f, 0.155815f, 0.865175f, 0.508899f, 0.498846f, 0.510297f, 0.314311f, 0.0580601f, 0.25675f, 0.849481f, 0.997329f, 0.867238f, 0.255343f, 0.575945f, 0.929926f, 0.164465f, 0.382339f, 0.0175594f, 0.531675f, 0.133809f, 0.424927f, 0.932461f, 0.47343f, 0.670699f, 0.451685f, 0.727976f, 0.157594f, 0.668399f, 0.431421f, 0.692803f, 0.309855f, 0.320758f, 0.600577f, 0.851317f, 0.110719f, 0.72173f, 0.052147f, 0.508466f, 0.331822f, 0.732259f, 0.300817f, 0.709938f, 0.341123f, 0.698617f, 0.212518f, 0.202616f, 0.018757f, 0.177552f, 0.613048f, 0.758948f, 0.547059f, 0.400971f, 0.115883f, 0.256621f, 0.727341f, 0.47802f, 0.377706f, 0.344168f, 0.903898f, 0.485407f, 0.654939f, 0.98362f, 0.858975f, 0.575978f, 0.698721f, 0.721705f, 0.841963f, 0.209065f, 0.43947f, 0.626849f, 0.616108f, 0.503407f, 0.551573f, 0.393978f, 0.0142356f, 0.1245f, 0.0788039f, 0.329059f, 0.540094f, 0.963904f, 0.579622f, 0.806494f, 0.378628f, 0.793516f, 0.649193f, 0.151952f, 0.572264f, 0.460979f, 0.198079f, 0.932621f, 0.719997f, 0.919287f, 0.0693846f, 0.840908f, 0.843045f, 0.055732f, 0.457907f, 0.992388f, 0.945144f, 0.943088f, 0.676341f, 0.553268f, 0.586761f, 0.224883f, 0.861508f, 0.566519f, 0.911452f, 0.591536f, 0.0733222f, 0.170432f, 0.351752f, 0.157091f, 0.327673f, 0.868404f, 0.808417f, 0.569937f, 0.453585f, 0.498946f, 0.80573f, 0.626621f, 0.349575f, 0.642979f, 0.53113f, 0.138728f, 0.4121f, 0.425972f, 0.807962f, 0.831206f, 0.839713f, 0.129973f, 0.553252f, 0.147851f, 0.733317f, 0.196179f}),
                            .dimensions = {2, 14, 13, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x14x13x3_to_2x6x7x3_relaxed_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x14x13x3_to_2x6x7x3_relaxed_all_inputs_as_internal", get_test_model_half_pixel_centers_2x14x13x3_to_2x6x7x3_relaxed_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x6x7x3_to_2x14x13x3() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.597099f, 0.717064f, 0.696989f, 0.37401f, 0.587701f, 0.52613f, 0.45983f, 0.834707f, 0.825826f, 0.188192f, 0.0581832f, 0.489487f, 0.897472f, 0.912709f, 0.338215f, 0.114233f, 0.975378f, 0.189915f, 0.885871f, 0.0123631f, 0.682109f, 0.0637571f, 0.021166f, 0.990439f, 0.405466f, 0.245107f, 0.235285f, 0.670532f, 0.721946f, 0.561132f, 0.00936949f, 0.0526415f, 0.447197f, 0.755065f, 0.597531f, 0.195151f, 0.269797f, 0.551208f, 0.954087f, 0.223795f, 0.397895f, 0.924789f, 0.635592f, 0.297786f, 0.0237268f, 0.101017f, 0.491771f, 0.715934f, 0.546113f, 0.782431f, 0.822342f, 0.454632f, 0.584726f, 0.427173f, 0.130061f, 0.63263f, 0.0130227f, 0.469195f, 0.689757f, 0.02537f, 0.630514f, 0.985893f, 0.257825f, 0.0865368f, 0.465077f, 0.950239f, 0.221726f, 0.349166f, 0.799219f, 0.181399f, 0.312473f, 0.0402693f, 0.533386f, 0.581182f, 0.600382f, 0.186105f, 0.322734f, 0.460414f, 0.37009f, 0.452929f, 0.387669f, 0.883785f, 0.291214f, 0.403792f, 0.288797f, 0.114932f, 0.441901f, 0.369459f, 0.00886202f, 0.577986f, 0.457492f, 0.488561f, 0.845964f, 0.0515606f, 0.210285f, 0.14478f, 0.935541f, 0.874223f, 0.0491093f, 0.81723f, 0.35612f, 0.0562574f, 0.763055f, 0.364821f, 0.731659f, 0.745522f, 0.64809f, 0.510889f, 0.430949f, 0.161836f, 0.735475f, 0.264182f, 0.476501f, 0.465461f, 0.258138f, 0.730769f, 0.899318f, 0.174359f, 0.86886f, 0.756424f, 0.685789f, 0.229707f, 0.443774f, 0.275146f, 0.658326f, 0.0486892f, 0.0457834f, 0.787678f, 0.663175f, 0.481761f, 0.573898f, 0.439558f, 0.56098f, 0.98622f, 0.807752f, 0.34172f, 0.972262f, 0.284004f, 0.396007f, 0.440433f, 0.20663f, 0.121019f, 0.0240191f, 0.0363696f, 0.93515f, 0.283339f, 0.90816f, 0.694879f, 0.28891f, 0.7681f, 0.52112f, 0.473317f, 0.933415f, 0.149368f, 0.179469f, 0.922061f, 0.625879f, 0.916547f, 0.112067f, 0.974693f, 0.553303f, 0.44083f, 0.772933f, 0.423641f, 0.0963937f, 0.937332f, 0.301141f, 0.164467f, 0.977361f, 0.224354f, 0.84855f, 0.728315f, 0.564487f, 0.733492f, 0.911596f, 0.794423f, 0.175493f, 0.156265f, 0.511936f, 0.574377f, 0.394498f, 0.522894f, 0.0984874f, 0.921798f, 0.850203f, 0.596674f, 0.0368204f, 0.326229f, 0.281598f, 0.939939f, 0.487391f, 0.596167f, 0.549989f, 0.0975398f, 0.983239f, 0.993055f, 0.0635948f, 0.749972f, 0.177322f, 0.771397f, 0.777728f, 0.531868f, 0.31498f, 0.334786f, 0.633789f, 0.959249f, 0.573456f, 0.429871f, 0.238746f, 0.376446f, 0.0555074f, 0.506892f, 0.565241f, 0.897762f, 0.409047f, 0.890007f, 0.394771f, 0.923041f, 0.0133429f, 0.940546f, 0.601826f, 0.117889f, 0.341266f, 0.77822f, 0.948948f, 0.675557f, 0.620504f, 0.274183f, 0.697336f, 0.170604f, 0.726621f, 0.583086f, 0.0914261f, 0.621153f, 0.158024f, 0.481524f, 0.801537f, 0.682243f, 0.469472f, 0.0622474f, 0.0587367f, 0.986715f, 0.24073f, 0.454288f, 0.199331f, 0.830628f, 0.866293f, 0.504453f, 0.62714f, 0.760198f, 0.848832f, 0.467382f}),
                            .dimensions = {2, 6, 7, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({13}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({14}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.597099f, 0.717064f, 0.696989f, 0.528456f, 0.67726f, 0.644417f, 0.408331f, 0.607603f, 0.552416f, 0.407018f, 0.682704f, 0.641398f, 0.453228f, 0.815707f, 0.802773f, 0.334458f, 0.476311f, 0.670593f, 0.188192f, 0.0581834f, 0.489487f, 0.570112f, 0.518313f, 0.408033f, 0.837223f, 0.91753f, 0.326807f, 0.415478f, 0.951275f, 0.246953f, 0.232946f, 0.827222f, 0.265637f, 0.648444f, 0.308675f, 0.530665f, 0.885871f, 0.0123631f, 0.682109f, 0.520907f, 0.61765f, 0.738911f, 0.477091f, 0.593376f, 0.660655f, 0.400412f, 0.550896f, 0.523708f, 0.42136f, 0.64639f, 0.601285f, 0.481359f, 0.797072f, 0.764672f, 0.338876f, 0.467272f, 0.647443f, 0.162646f, 0.0573917f, 0.483445f, 0.547367f, 0.493703f, 0.394239f, 0.820153f, 0.871307f, 0.316339f, 0.42133f, 0.896668f, 0.306273f, 0.2372f, 0.784422f, 0.363343f, 0.589802f, 0.32816f, 0.588256f, 0.791288f, 0.067439f, 0.716777f, 0.292332f, 0.319408f, 0.864675f, 0.322995f, 0.341724f, 0.70937f, 0.376654f, 0.380775f, 0.437585f, 0.464387f, 0.537448f, 0.480948f, 0.56575f, 0.741169f, 0.650369f, 0.352128f, 0.440154f, 0.577995f, 0.0860078f, 0.0550167f, 0.465321f, 0.479133f, 0.419873f, 0.352859f, 0.768945f, 0.732637f, 0.284935f, 0.438884f, 0.732846f, 0.484231f, 0.24996f, 0.656022f, 0.656461f, 0.413876f, 0.386614f, 0.76103f, 0.507542f, 0.232667f, 0.820783f, 0.0637571f, 0.021166f, 0.990439f, 0.168898f, 0.0900708f, 0.758084f, 0.352896f, 0.210654f, 0.351462f, 0.507414f, 0.428506f, 0.360611f, 0.650142f, 0.685266f, 0.536067f, 0.36538f, 0.413036f, 0.508547f, 0.00936967f, 0.0526416f, 0.447197f, 0.410898f, 0.346044f, 0.31148f, 0.717737f, 0.593968f, 0.253531f, 0.456438f, 0.569025f, 0.662189f, 0.262719f, 0.527622f, 0.94958f, 0.237949f, 0.445068f, 0.933804f, 0.223795f, 0.397895f, 0.924789f, 0.308829f, 0.139718f, 0.576134f, 0.298417f, 0.204672f, 0.534639f, 0.280194f, 0.318343f, 0.462024f, 0.406612f, 0.503531f, 0.530432f, 0.590885f, 0.717326f, 0.655248f, 0.424742f, 0.532242f, 0.564865f, 0.200196f, 0.280678f, 0.438615f, 0.35474f, 0.459391f, 0.265489f, 0.477056f, 0.612421f, 0.150863f, 0.406004f, 0.611351f, 0.387231f, 0.361845f, 0.616633f, 0.568817f, 0.384918f, 0.637799f, 0.613445f, 0.398103f, 0.649894f, 0.638947f, 0.553902f, 0.258269f, 0.161828f, 0.427935f, 0.319274f, 0.311195f, 0.207493f, 0.426031f, 0.572587f, 0.305809f, 0.578555f, 0.700253f, 0.531627f, 0.749386f, 0.77443f, 0.484104f, 0.651447f, 0.621183f, 0.391023f, 0.508714f, 0.430033f, 0.298582f, 0.572738f, 0.219499f, 0.236375f, 0.630874f, 0.048195f, 0.35557f, 0.653676f, 0.112274f, 0.460971f, 0.705645f, 0.188053f, 0.531887f, 0.83053f, 0.293086f, 0.572411f, 0.901893f, 0.353105f, 0.478719f, 0.345583f, 0.288445f, 0.373115f, 0.378028f, 0.427302f, 0.188308f, 0.434805f, 0.670301f, 0.253353f, 0.526846f, 0.685562f, 0.418339f, 0.632993f, 0.609726f, 0.458166f, 0.618414f, 0.542478f, 0.477133f, 0.583714f, 0.476661f, 0.29887f, 0.562377f, 0.295839f, 0.168751f, 0.550089f, 0.139929f, 0.327492f, 0.59209f, 0.133486f, 0.481187f, 0.647526f, 0.155137f, 0.622262f, 0.736545f, 0.247023f, 0.702877f, 0.787413f, 0.29953f, 0.24341f, 0.417279f, 0.685521f, 0.226126f, 0.408858f, 0.713184f, 0.19588f, 0.394121f, 0.761592f, 0.225071f, 0.41177f, 0.578614f, 0.278036f, 0.442374f, 0.30308f, 0.389579f, 0.509261f, 0.396261f, 0.510885f, 0.582195f, 0.550894f, 0.327381f, 0.490162f, 0.433345f, 0.187655f, 0.419685f, 0.328863f, 0.310593f, 0.478549f, 0.302783f, 0.461947f, 0.51584f, 0.296145f, 0.68434f, 0.499202f, 0.338108f, 0.811422f, 0.489694f, 0.362087f, 0.115431f, 0.415056f, 0.877619f, 0.154631f, 0.379824f, 0.843772f, 0.223231f, 0.318167f, 0.784538f, 0.234373f, 0.314811f, 0.532135f, 0.222532f, 0.334776f, 0.202464f, 0.333324f, 0.425583f, 0.33072f, 0.464554f, 0.528197f, 0.535296f, 0.372269f, 0.459986f, 0.463336f, 0.303998f, 0.404409f, 0.396939f, 0.379814f, 0.424645f, 0.363914f, 0.500517f, 0.417965f, 0.357295f, 0.73344f, 0.343997f, 0.41669f, 0.866538f, 0.30173f, 0.45063f, 0.202114f, 0.264994f, 0.65976f, 0.234124f, 0.231059f, 0.663772f, 0.29014f, 0.171674f, 0.670794f, 0.318846f, 0.254086f, 0.606595f, 0.336627f, 0.393218f, 0.513907f, 0.301731f, 0.392854f, 0.42653f, 0.258057f, 0.369241f, 0.340038f, 0.44991f, 0.513888f, 0.278301f, 0.615219f, 0.619389f, 0.223299f, 0.621274f, 0.490018f, 0.208711f, 0.654706f, 0.387712f, 0.25873f, 0.756582f, 0.35307f, 0.470267f, 0.814797f, 0.333275f, 0.591144f, 0.288797f, 0.114932f, 0.441901f, 0.313616f, 0.0822949f, 0.483773f, 0.35705f, 0.0251804f, 0.55705f, 0.403318f, 0.193361f, 0.681055f, 0.45072f, 0.451661f, 0.82535f, 0.270139f, 0.360126f, 0.522341f, 0.0515609f, 0.210285f, 0.14478f, 0.52755f, 0.56779f, 0.093265f, 0.92644f, 0.834369f, 0.0496592f, 0.862734f, 0.55539f, 0.0535081f, 0.808896f, 0.357459f, 0.160165f, 0.779725f, 0.362144f, 0.523843f, 0.763055f, 0.364821f, 0.731659f, 0.484536f, 0.343428f, 0.471467f, 0.457236f, 0.260657f, 0.52501f, 0.409462f, 0.115808f, 0.61871f, 0.387671f, 0.231718f, 0.65987f, 0.376273f, 0.451933f, 0.680014f, 0.266391f, 0.460296f, 0.583782f, 0.140094f, 0.43335f, 0.468154f, 0.392754f, 0.669506f, 0.405741f, 0.62098f, 0.82808f, 0.342252f, 0.702599f, 0.521167f, 0.272301f, 0.72906f, 0.330968f, 0.255662f, 0.617627f, 0.432558f, 0.372305f, 0.553951f, 0.490609f, 0.438957f, 0.680275f, 0.571925f, 0.501034f, 0.600857f, 0.439019f, 0.566247f, 0.461874f, 0.206435f, 0.68037f, 0.372023f, 0.270075f, 0.638685f, 0.301826f, 0.452205f, 0.534677f, 0.262642f, 0.560466f, 0.645222f, 0.228627f, 0.656414f, 0.791527f, 0.257958f, 0.771221f, 0.718217f, 0.31552f, 0.821791f, 0.634844f, 0.542464f, 0.486943f, 0.491093f, 0.649225f, 0.304478f, 0.351159f, 0.45553f, 0.502972f, 0.220766f, 0.344847f, 0.616397f, 0.146256f, 0.745522f, 0.64809f, 0.510889f, 0.64873f, 0.498473f, 0.579992f, 0.479344f, 0.236644f, 0.700923f, 0.366808f, 0.282861f, 0.631623f, 0.27701f, 0.452296f, 0.486232f, 0.261392f, 0.593856f, 0.665703f, 0.258138f, 0.730769f, 0.899318f, 0.213026f, 0.805126f, 0.822375f, 0.2137f, 0.819694f, 0.732374f, 0.489085f, 0.475535f, 0.564024f, 0.622613f, 0.295648f, 0.382992f, 0.401498f, 0.526443f, 0.170254f, 0.275146f, 0.658326f, 0.0486892f, 0.0457834f, 0.787678f, 0.663175f, 0.17993f, 0.7219f, 0.59437f, 0.414688f, 0.606787f, 0.473961f, 0.51223f, 0.732484f, 0.581171f, 0.554887f, 0.954503f, 0.77943f, 0.459783f, 0.979778f, 0.566022f, 0.34172f, 0.972261f, 0.284004f, 0.370952f, 0.685892f, 0.242341f, 0.374854f, 0.408401f, 0.193533f, 0.226783f, 0.184178f, 0.101854f, 0.24627f, 0.0639145f, 0.170491f, 0.684648f, 0.203548f, 0.639917f, 0.93515f, 0.283339f, 0.90816f, 0.138511f, 0.716425f, 0.678164f, 0.245857f, 0.66815f, 0.626455f, 0.433711f, 0.583667f, 0.535964f, 0.493074f, 0.679314f, 0.630868f, 0.501041f, 0.847013f, 0.79993f, 0.446857f, 0.914047f, 0.563479f, 0.382315f, 0.964302f, 0.259442f, 0.434202f, 0.690901f, 0.24902f, 0.458328f, 0.427676f, 0.225076f, 0.31589f, 0.225511f, 0.119999f, 0.325121f, 0.112612f, 0.161402f, 0.71352f, 0.222875f, 0.569003f, 0.935462f, 0.285882f, 0.801918f, 0.416695f, 0.502668f, 0.723132f, 0.443636f, 0.5069f, 0.722711f, 0.490781f, 0.514307f, 0.721973f, 0.435606f, 0.519806f, 0.779958f, 0.339502f, 0.524543f, 0.861432f, 0.408076f, 0.716853f, 0.555848f, 0.504097f, 0.940425f, 0.185755f, 0.623952f, 0.705927f, 0.269057f, 0.70875f, 0.485502f, 0.319706f, 0.583212f, 0.349511f, 0.174434f, 0.561673f, 0.258703f, 0.134134f, 0.800134f, 0.280854f, 0.356262f, 0.936397f, 0.293511f, 0.483193f, 0.694879f, 0.28891f, 0.7681f, 0.641415f, 0.345651f, 0.818966f, 0.547852f, 0.444947f, 0.907982f, 0.378138f, 0.360298f, 0.929048f, 0.177964f, 0.202073f, 0.922934f, 0.369296f, 0.519659f, 0.548218f, 0.625879f, 0.916547f, 0.112067f, 0.813702f, 0.720954f, 0.289094f, 0.959173f, 0.543329f, 0.414335f, 0.850533f, 0.473511f, 0.228869f, 0.798225f, 0.404795f, 0.106867f, 0.886748f, 0.338833f, 0.143521f, 0.937332f, 0.301141f, 0.164467f, 0.815943f, 0.261243f, 0.802579f, 0.75255f, 0.338519f, 0.816473f, 0.641614f, 0.473752f, 0.840787f, 0.558425f, 0.485709f, 0.75326f, 0.486336f, 0.448357f, 0.620998f, 0.452304f, 0.581538f, 0.467379f, 0.424616f, 0.743142f, 0.3102f, 0.58692f, 0.633904f, 0.301537f, 0.734553f, 0.545361f, 0.295396f, 0.794157f, 0.580998f, 0.304381f, 0.792835f, 0.561136f, 0.29601f, 0.639193f, 0.402527f, 0.244246f, 0.551399f, 0.311893f, 0.214666f, 0.937007f, 0.233576f, 0.837057f, 0.863686f, 0.331387f, 0.813979f, 0.735375f, 0.502557f, 0.773591f, 0.738712f, 0.61112f, 0.577473f, 0.794707f, 0.694641f, 0.319062f, 0.535312f, 0.643418f, 0.38654f, 0.223352f, 0.569737f, 0.508333f, 0.360138f, 0.546853f, 0.313981f, 0.509933f, 0.547394f, 0.176456f, 0.737782f, 0.688486f, 0.379893f, 0.787444f, 0.717478f, 0.485153f, 0.391639f, 0.466221f, 0.34497f, 0.165465f, 0.322645f, 0.264865f, 0.966669f, 0.299507f, 0.776441f, 0.877652f, 0.339989f, 0.785181f, 0.721873f, 0.410833f, 0.800478f, 0.776405f, 0.490512f, 0.625918f, 0.915062f, 0.573727f, 0.375416f, 0.578291f, 0.585824f, 0.474791f, 0.162281f, 0.586067f, 0.632477f, 0.308455f, 0.520064f, 0.381298f, 0.464959f, 0.495634f, 0.19862f, 0.683447f, 0.720641f, 0.426949f, 0.733296f, 0.792109f, 0.546757f, 0.361548f, 0.479734f, 0.395264f, 0.149121f, 0.301234f, 0.308697f, 0.950631f, 0.412237f, 0.668276f, 0.843034f, 0.356458f, 0.743232f, 0.654738f, 0.258843f, 0.874405f, 0.742802f, 0.246895f, 0.78648f, 0.941409f, 0.269214f, 0.610916f, 0.601254f, 0.468493f, 0.647586f, 0.171306f, 0.697265f, 0.719628f, 0.344321f, 0.523406f, 0.476052f, 0.509808f, 0.416977f, 0.291335f, 0.630132f, 0.715128f, 0.459775f, 0.654768f, 0.825885f, 0.544591f, 0.440188f, 0.468157f, 0.420344f, 0.317571f, 0.263741f, 0.349346f, 0.813592f, 0.490177f, 0.591749f, 0.74777f, 0.383058f, 0.708109f, 0.632582f, 0.1956f, 0.91174f, 0.7181f, 0.159092f, 0.844851f, 0.8839f, 0.182963f, 0.669753f, 0.620864f, 0.445203f, 0.662613f, 0.286354f, 0.747172f, 0.683465f, 0.403892f, 0.550087f, 0.542958f, 0.515033f, 0.421903f, 0.430845f, 0.587788f, 0.707126f, 0.489095f, 0.613344f, 0.805958f, 0.514667f, 0.520904f, 0.43881f, 0.458542f, 0.468081f, 0.229012f, 0.426471f, 0.434549f, 0.498534f, 0.578495f, 0.531216f, 0.429922f, 0.686639f, 0.700383f, 0.30985f, 0.875891f, 0.711202f, 0.382914f, 0.698839f, 0.658681f, 0.533233f, 0.375266f, 0.633765f, 0.609996f, 0.362104f, 0.61345f, 0.674499f, 0.400677f, 0.510875f, 0.623447f, 0.55417f, 0.441007f, 0.593996f, 0.663944f, 0.567386f, 0.694145f, 0.511401f, 0.646126f, 0.678623f, 0.429228f, 0.605769f, 0.373922f, 0.522976f, 0.582708f, 0.199808f, 0.576546f, 0.0555074f, 0.506892f, 0.565241f, 0.314663f, 0.476786f, 0.665169f, 0.768185f, 0.4241f, 0.840043f, 0.704304f, 0.606737f, 0.552828f, 0.433463f, 0.883503f, 0.0807784f, 0.646667f, 0.774788f, 0.0615947f, 0.940546f, 0.601826f, 0.117889f, 0.617857f, 0.696808f, 0.565382f, 0.366981f, 0.766088f, 0.897043f, 0.546984f, 0.681164f, 0.533708f, 0.678907f, 0.551288f, 0.343789f, 0.690635f, 0.309035f, 0.587409f, 0.697336f, 0.170604f, 0.726621f, 0.281613f, 0.328835f, 0.589203f, 0.373649f, 0.363073f, 0.670092f, 0.534713f, 0.42299f, 0.811647f, 0.556594f, 0.551088f, 0.537557f, 0.522801f, 0.706459f, 0.0972087f, 0.538583f, 0.74625f, 0.0971787f, 0.562627f, 0.766779f, 0.170535f, 0.469515f, 0.63935f, 0.562376f, 0.417981f, 0.533251f, 0.861871f, 0.615918f, 0.555136f, 0.607292f, 0.75222f, 0.553922f, 0.454692f, 0.734438f, 0.494964f, 0.557036f, 0.724277f, 0.461273f, 0.615519f, 0.507718f, 0.150778f, 0.613165f, 0.432636f, 0.249361f, 0.675015f, 0.301242f, 0.42188f, 0.783251f, 0.408884f, 0.495439f, 0.522285f, 0.612139f, 0.529414f, 0.113639f, 0.430499f, 0.717713f, 0.132763f, 0.184709f, 0.931731f, 0.223181f, 0.321173f, 0.581892f, 0.559369f, 0.468981f, 0.300414f, 0.826699f, 0.684852f, 0.429108f, 0.680876f, 0.825533f, 0.556557f, 0.565594f, 0.778242f, 0.680893f, 0.526663f, 0.751218f, 0.751942f, 0.504416f, 0.583086f, 0.0914261f, 0.621153f, 0.452298f, 0.211456f, 0.676656f, 0.223418f, 0.421509f, 0.773785f, 0.359647f, 0.476889f, 0.517195f, 0.641919f, 0.470399f, 0.119116f, 0.394471f, 0.7082f, 0.144624f, 0.0587368f, 0.986715f, 0.24073f, 0.271726f, 0.562739f, 0.558367f, 0.485981f, 0.222802f, 0.814975f, 0.70783f, 0.387099f, 0.705405f, 0.849971f, 0.557435f, 0.602562f, 0.792843f, 0.742869f, 0.516539f, 0.760198f, 0.848832f, 0.467382f}),
                            .dimensions = {2, 14, 13, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x6x7x3_to_2x14x13x3 = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x6x7x3_to_2x14x13x3", get_test_model_half_pixel_centers_2x6x7x3_to_2x14x13x3());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x6x7x3_to_2x14x13x3_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {2, 6, 7, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({13}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({14}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.597099f, 0.717064f, 0.696989f, 0.528456f, 0.67726f, 0.644417f, 0.408331f, 0.607603f, 0.552416f, 0.407018f, 0.682704f, 0.641398f, 0.453228f, 0.815707f, 0.802773f, 0.334458f, 0.476311f, 0.670593f, 0.188192f, 0.0581834f, 0.489487f, 0.570112f, 0.518313f, 0.408033f, 0.837223f, 0.91753f, 0.326807f, 0.415478f, 0.951275f, 0.246953f, 0.232946f, 0.827222f, 0.265637f, 0.648444f, 0.308675f, 0.530665f, 0.885871f, 0.0123631f, 0.682109f, 0.520907f, 0.61765f, 0.738911f, 0.477091f, 0.593376f, 0.660655f, 0.400412f, 0.550896f, 0.523708f, 0.42136f, 0.64639f, 0.601285f, 0.481359f, 0.797072f, 0.764672f, 0.338876f, 0.467272f, 0.647443f, 0.162646f, 0.0573917f, 0.483445f, 0.547367f, 0.493703f, 0.394239f, 0.820153f, 0.871307f, 0.316339f, 0.42133f, 0.896668f, 0.306273f, 0.2372f, 0.784422f, 0.363343f, 0.589802f, 0.32816f, 0.588256f, 0.791288f, 0.067439f, 0.716777f, 0.292332f, 0.319408f, 0.864675f, 0.322995f, 0.341724f, 0.70937f, 0.376654f, 0.380775f, 0.437585f, 0.464387f, 0.537448f, 0.480948f, 0.56575f, 0.741169f, 0.650369f, 0.352128f, 0.440154f, 0.577995f, 0.0860078f, 0.0550167f, 0.465321f, 0.479133f, 0.419873f, 0.352859f, 0.768945f, 0.732637f, 0.284935f, 0.438884f, 0.732846f, 0.484231f, 0.24996f, 0.656022f, 0.656461f, 0.413876f, 0.386614f, 0.76103f, 0.507542f, 0.232667f, 0.820783f, 0.0637571f, 0.021166f, 0.990439f, 0.168898f, 0.0900708f, 0.758084f, 0.352896f, 0.210654f, 0.351462f, 0.507414f, 0.428506f, 0.360611f, 0.650142f, 0.685266f, 0.536067f, 0.36538f, 0.413036f, 0.508547f, 0.00936967f, 0.0526416f, 0.447197f, 0.410898f, 0.346044f, 0.31148f, 0.717737f, 0.593968f, 0.253531f, 0.456438f, 0.569025f, 0.662189f, 0.262719f, 0.527622f, 0.94958f, 0.237949f, 0.445068f, 0.933804f, 0.223795f, 0.397895f, 0.924789f, 0.308829f, 0.139718f, 0.576134f, 0.298417f, 0.204672f, 0.534639f, 0.280194f, 0.318343f, 0.462024f, 0.406612f, 0.503531f, 0.530432f, 0.590885f, 0.717326f, 0.655248f, 0.424742f, 0.532242f, 0.564865f, 0.200196f, 0.280678f, 0.438615f, 0.35474f, 0.459391f, 0.265489f, 0.477056f, 0.612421f, 0.150863f, 0.406004f, 0.611351f, 0.387231f, 0.361845f, 0.616633f, 0.568817f, 0.384918f, 0.637799f, 0.613445f, 0.398103f, 0.649894f, 0.638947f, 0.553902f, 0.258269f, 0.161828f, 0.427935f, 0.319274f, 0.311195f, 0.207493f, 0.426031f, 0.572587f, 0.305809f, 0.578555f, 0.700253f, 0.531627f, 0.749386f, 0.77443f, 0.484104f, 0.651447f, 0.621183f, 0.391023f, 0.508714f, 0.430033f, 0.298582f, 0.572738f, 0.219499f, 0.236375f, 0.630874f, 0.048195f, 0.35557f, 0.653676f, 0.112274f, 0.460971f, 0.705645f, 0.188053f, 0.531887f, 0.83053f, 0.293086f, 0.572411f, 0.901893f, 0.353105f, 0.478719f, 0.345583f, 0.288445f, 0.373115f, 0.378028f, 0.427302f, 0.188308f, 0.434805f, 0.670301f, 0.253353f, 0.526846f, 0.685562f, 0.418339f, 0.632993f, 0.609726f, 0.458166f, 0.618414f, 0.542478f, 0.477133f, 0.583714f, 0.476661f, 0.29887f, 0.562377f, 0.295839f, 0.168751f, 0.550089f, 0.139929f, 0.327492f, 0.59209f, 0.133486f, 0.481187f, 0.647526f, 0.155137f, 0.622262f, 0.736545f, 0.247023f, 0.702877f, 0.787413f, 0.29953f, 0.24341f, 0.417279f, 0.685521f, 0.226126f, 0.408858f, 0.713184f, 0.19588f, 0.394121f, 0.761592f, 0.225071f, 0.41177f, 0.578614f, 0.278036f, 0.442374f, 0.30308f, 0.389579f, 0.509261f, 0.396261f, 0.510885f, 0.582195f, 0.550894f, 0.327381f, 0.490162f, 0.433345f, 0.187655f, 0.419685f, 0.328863f, 0.310593f, 0.478549f, 0.302783f, 0.461947f, 0.51584f, 0.296145f, 0.68434f, 0.499202f, 0.338108f, 0.811422f, 0.489694f, 0.362087f, 0.115431f, 0.415056f, 0.877619f, 0.154631f, 0.379824f, 0.843772f, 0.223231f, 0.318167f, 0.784538f, 0.234373f, 0.314811f, 0.532135f, 0.222532f, 0.334776f, 0.202464f, 0.333324f, 0.425583f, 0.33072f, 0.464554f, 0.528197f, 0.535296f, 0.372269f, 0.459986f, 0.463336f, 0.303998f, 0.404409f, 0.396939f, 0.379814f, 0.424645f, 0.363914f, 0.500517f, 0.417965f, 0.357295f, 0.73344f, 0.343997f, 0.41669f, 0.866538f, 0.30173f, 0.45063f, 0.202114f, 0.264994f, 0.65976f, 0.234124f, 0.231059f, 0.663772f, 0.29014f, 0.171674f, 0.670794f, 0.318846f, 0.254086f, 0.606595f, 0.336627f, 0.393218f, 0.513907f, 0.301731f, 0.392854f, 0.42653f, 0.258057f, 0.369241f, 0.340038f, 0.44991f, 0.513888f, 0.278301f, 0.615219f, 0.619389f, 0.223299f, 0.621274f, 0.490018f, 0.208711f, 0.654706f, 0.387712f, 0.25873f, 0.756582f, 0.35307f, 0.470267f, 0.814797f, 0.333275f, 0.591144f, 0.288797f, 0.114932f, 0.441901f, 0.313616f, 0.0822949f, 0.483773f, 0.35705f, 0.0251804f, 0.55705f, 0.403318f, 0.193361f, 0.681055f, 0.45072f, 0.451661f, 0.82535f, 0.270139f, 0.360126f, 0.522341f, 0.0515609f, 0.210285f, 0.14478f, 0.52755f, 0.56779f, 0.093265f, 0.92644f, 0.834369f, 0.0496592f, 0.862734f, 0.55539f, 0.0535081f, 0.808896f, 0.357459f, 0.160165f, 0.779725f, 0.362144f, 0.523843f, 0.763055f, 0.364821f, 0.731659f, 0.484536f, 0.343428f, 0.471467f, 0.457236f, 0.260657f, 0.52501f, 0.409462f, 0.115808f, 0.61871f, 0.387671f, 0.231718f, 0.65987f, 0.376273f, 0.451933f, 0.680014f, 0.266391f, 0.460296f, 0.583782f, 0.140094f, 0.43335f, 0.468154f, 0.392754f, 0.669506f, 0.405741f, 0.62098f, 0.82808f, 0.342252f, 0.702599f, 0.521167f, 0.272301f, 0.72906f, 0.330968f, 0.255662f, 0.617627f, 0.432558f, 0.372305f, 0.553951f, 0.490609f, 0.438957f, 0.680275f, 0.571925f, 0.501034f, 0.600857f, 0.439019f, 0.566247f, 0.461874f, 0.206435f, 0.68037f, 0.372023f, 0.270075f, 0.638685f, 0.301826f, 0.452205f, 0.534677f, 0.262642f, 0.560466f, 0.645222f, 0.228627f, 0.656414f, 0.791527f, 0.257958f, 0.771221f, 0.718217f, 0.31552f, 0.821791f, 0.634844f, 0.542464f, 0.486943f, 0.491093f, 0.649225f, 0.304478f, 0.351159f, 0.45553f, 0.502972f, 0.220766f, 0.344847f, 0.616397f, 0.146256f, 0.745522f, 0.64809f, 0.510889f, 0.64873f, 0.498473f, 0.579992f, 0.479344f, 0.236644f, 0.700923f, 0.366808f, 0.282861f, 0.631623f, 0.27701f, 0.452296f, 0.486232f, 0.261392f, 0.593856f, 0.665703f, 0.258138f, 0.730769f, 0.899318f, 0.213026f, 0.805126f, 0.822375f, 0.2137f, 0.819694f, 0.732374f, 0.489085f, 0.475535f, 0.564024f, 0.622613f, 0.295648f, 0.382992f, 0.401498f, 0.526443f, 0.170254f, 0.275146f, 0.658326f, 0.0486892f, 0.0457834f, 0.787678f, 0.663175f, 0.17993f, 0.7219f, 0.59437f, 0.414688f, 0.606787f, 0.473961f, 0.51223f, 0.732484f, 0.581171f, 0.554887f, 0.954503f, 0.77943f, 0.459783f, 0.979778f, 0.566022f, 0.34172f, 0.972261f, 0.284004f, 0.370952f, 0.685892f, 0.242341f, 0.374854f, 0.408401f, 0.193533f, 0.226783f, 0.184178f, 0.101854f, 0.24627f, 0.0639145f, 0.170491f, 0.684648f, 0.203548f, 0.639917f, 0.93515f, 0.283339f, 0.90816f, 0.138511f, 0.716425f, 0.678164f, 0.245857f, 0.66815f, 0.626455f, 0.433711f, 0.583667f, 0.535964f, 0.493074f, 0.679314f, 0.630868f, 0.501041f, 0.847013f, 0.79993f, 0.446857f, 0.914047f, 0.563479f, 0.382315f, 0.964302f, 0.259442f, 0.434202f, 0.690901f, 0.24902f, 0.458328f, 0.427676f, 0.225076f, 0.31589f, 0.225511f, 0.119999f, 0.325121f, 0.112612f, 0.161402f, 0.71352f, 0.222875f, 0.569003f, 0.935462f, 0.285882f, 0.801918f, 0.416695f, 0.502668f, 0.723132f, 0.443636f, 0.5069f, 0.722711f, 0.490781f, 0.514307f, 0.721973f, 0.435606f, 0.519806f, 0.779958f, 0.339502f, 0.524543f, 0.861432f, 0.408076f, 0.716853f, 0.555848f, 0.504097f, 0.940425f, 0.185755f, 0.623952f, 0.705927f, 0.269057f, 0.70875f, 0.485502f, 0.319706f, 0.583212f, 0.349511f, 0.174434f, 0.561673f, 0.258703f, 0.134134f, 0.800134f, 0.280854f, 0.356262f, 0.936397f, 0.293511f, 0.483193f, 0.694879f, 0.28891f, 0.7681f, 0.641415f, 0.345651f, 0.818966f, 0.547852f, 0.444947f, 0.907982f, 0.378138f, 0.360298f, 0.929048f, 0.177964f, 0.202073f, 0.922934f, 0.369296f, 0.519659f, 0.548218f, 0.625879f, 0.916547f, 0.112067f, 0.813702f, 0.720954f, 0.289094f, 0.959173f, 0.543329f, 0.414335f, 0.850533f, 0.473511f, 0.228869f, 0.798225f, 0.404795f, 0.106867f, 0.886748f, 0.338833f, 0.143521f, 0.937332f, 0.301141f, 0.164467f, 0.815943f, 0.261243f, 0.802579f, 0.75255f, 0.338519f, 0.816473f, 0.641614f, 0.473752f, 0.840787f, 0.558425f, 0.485709f, 0.75326f, 0.486336f, 0.448357f, 0.620998f, 0.452304f, 0.581538f, 0.467379f, 0.424616f, 0.743142f, 0.3102f, 0.58692f, 0.633904f, 0.301537f, 0.734553f, 0.545361f, 0.295396f, 0.794157f, 0.580998f, 0.304381f, 0.792835f, 0.561136f, 0.29601f, 0.639193f, 0.402527f, 0.244246f, 0.551399f, 0.311893f, 0.214666f, 0.937007f, 0.233576f, 0.837057f, 0.863686f, 0.331387f, 0.813979f, 0.735375f, 0.502557f, 0.773591f, 0.738712f, 0.61112f, 0.577473f, 0.794707f, 0.694641f, 0.319062f, 0.535312f, 0.643418f, 0.38654f, 0.223352f, 0.569737f, 0.508333f, 0.360138f, 0.546853f, 0.313981f, 0.509933f, 0.547394f, 0.176456f, 0.737782f, 0.688486f, 0.379893f, 0.787444f, 0.717478f, 0.485153f, 0.391639f, 0.466221f, 0.34497f, 0.165465f, 0.322645f, 0.264865f, 0.966669f, 0.299507f, 0.776441f, 0.877652f, 0.339989f, 0.785181f, 0.721873f, 0.410833f, 0.800478f, 0.776405f, 0.490512f, 0.625918f, 0.915062f, 0.573727f, 0.375416f, 0.578291f, 0.585824f, 0.474791f, 0.162281f, 0.586067f, 0.632477f, 0.308455f, 0.520064f, 0.381298f, 0.464959f, 0.495634f, 0.19862f, 0.683447f, 0.720641f, 0.426949f, 0.733296f, 0.792109f, 0.546757f, 0.361548f, 0.479734f, 0.395264f, 0.149121f, 0.301234f, 0.308697f, 0.950631f, 0.412237f, 0.668276f, 0.843034f, 0.356458f, 0.743232f, 0.654738f, 0.258843f, 0.874405f, 0.742802f, 0.246895f, 0.78648f, 0.941409f, 0.269214f, 0.610916f, 0.601254f, 0.468493f, 0.647586f, 0.171306f, 0.697265f, 0.719628f, 0.344321f, 0.523406f, 0.476052f, 0.509808f, 0.416977f, 0.291335f, 0.630132f, 0.715128f, 0.459775f, 0.654768f, 0.825885f, 0.544591f, 0.440188f, 0.468157f, 0.420344f, 0.317571f, 0.263741f, 0.349346f, 0.813592f, 0.490177f, 0.591749f, 0.74777f, 0.383058f, 0.708109f, 0.632582f, 0.1956f, 0.91174f, 0.7181f, 0.159092f, 0.844851f, 0.8839f, 0.182963f, 0.669753f, 0.620864f, 0.445203f, 0.662613f, 0.286354f, 0.747172f, 0.683465f, 0.403892f, 0.550087f, 0.542958f, 0.515033f, 0.421903f, 0.430845f, 0.587788f, 0.707126f, 0.489095f, 0.613344f, 0.805958f, 0.514667f, 0.520904f, 0.43881f, 0.458542f, 0.468081f, 0.229012f, 0.426471f, 0.434549f, 0.498534f, 0.578495f, 0.531216f, 0.429922f, 0.686639f, 0.700383f, 0.30985f, 0.875891f, 0.711202f, 0.382914f, 0.698839f, 0.658681f, 0.533233f, 0.375266f, 0.633765f, 0.609996f, 0.362104f, 0.61345f, 0.674499f, 0.400677f, 0.510875f, 0.623447f, 0.55417f, 0.441007f, 0.593996f, 0.663944f, 0.567386f, 0.694145f, 0.511401f, 0.646126f, 0.678623f, 0.429228f, 0.605769f, 0.373922f, 0.522976f, 0.582708f, 0.199808f, 0.576546f, 0.0555074f, 0.506892f, 0.565241f, 0.314663f, 0.476786f, 0.665169f, 0.768185f, 0.4241f, 0.840043f, 0.704304f, 0.606737f, 0.552828f, 0.433463f, 0.883503f, 0.0807784f, 0.646667f, 0.774788f, 0.0615947f, 0.940546f, 0.601826f, 0.117889f, 0.617857f, 0.696808f, 0.565382f, 0.366981f, 0.766088f, 0.897043f, 0.546984f, 0.681164f, 0.533708f, 0.678907f, 0.551288f, 0.343789f, 0.690635f, 0.309035f, 0.587409f, 0.697336f, 0.170604f, 0.726621f, 0.281613f, 0.328835f, 0.589203f, 0.373649f, 0.363073f, 0.670092f, 0.534713f, 0.42299f, 0.811647f, 0.556594f, 0.551088f, 0.537557f, 0.522801f, 0.706459f, 0.0972087f, 0.538583f, 0.74625f, 0.0971787f, 0.562627f, 0.766779f, 0.170535f, 0.469515f, 0.63935f, 0.562376f, 0.417981f, 0.533251f, 0.861871f, 0.615918f, 0.555136f, 0.607292f, 0.75222f, 0.553922f, 0.454692f, 0.734438f, 0.494964f, 0.557036f, 0.724277f, 0.461273f, 0.615519f, 0.507718f, 0.150778f, 0.613165f, 0.432636f, 0.249361f, 0.675015f, 0.301242f, 0.42188f, 0.783251f, 0.408884f, 0.495439f, 0.522285f, 0.612139f, 0.529414f, 0.113639f, 0.430499f, 0.717713f, 0.132763f, 0.184709f, 0.931731f, 0.223181f, 0.321173f, 0.581892f, 0.559369f, 0.468981f, 0.300414f, 0.826699f, 0.684852f, 0.429108f, 0.680876f, 0.825533f, 0.556557f, 0.565594f, 0.778242f, 0.680893f, 0.526663f, 0.751218f, 0.751942f, 0.504416f, 0.583086f, 0.0914261f, 0.621153f, 0.452298f, 0.211456f, 0.676656f, 0.223418f, 0.421509f, 0.773785f, 0.359647f, 0.476889f, 0.517195f, 0.641919f, 0.470399f, 0.119116f, 0.394471f, 0.7082f, 0.144624f, 0.0587368f, 0.986715f, 0.24073f, 0.271726f, 0.562739f, 0.558367f, 0.485981f, 0.222802f, 0.814975f, 0.70783f, 0.387099f, 0.705405f, 0.849971f, 0.557435f, 0.602562f, 0.792843f, 0.742869f, 0.516539f, 0.760198f, 0.848832f, 0.467382f}),
                            .dimensions = {2, 14, 13, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input02_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.597099f, 0.717064f, 0.696989f, 0.37401f, 0.587701f, 0.52613f, 0.45983f, 0.834707f, 0.825826f, 0.188192f, 0.0581832f, 0.489487f, 0.897472f, 0.912709f, 0.338215f, 0.114233f, 0.975378f, 0.189915f, 0.885871f, 0.0123631f, 0.682109f, 0.0637571f, 0.021166f, 0.990439f, 0.405466f, 0.245107f, 0.235285f, 0.670532f, 0.721946f, 0.561132f, 0.00936949f, 0.0526415f, 0.447197f, 0.755065f, 0.597531f, 0.195151f, 0.269797f, 0.551208f, 0.954087f, 0.223795f, 0.397895f, 0.924789f, 0.635592f, 0.297786f, 0.0237268f, 0.101017f, 0.491771f, 0.715934f, 0.546113f, 0.782431f, 0.822342f, 0.454632f, 0.584726f, 0.427173f, 0.130061f, 0.63263f, 0.0130227f, 0.469195f, 0.689757f, 0.02537f, 0.630514f, 0.985893f, 0.257825f, 0.0865368f, 0.465077f, 0.950239f, 0.221726f, 0.349166f, 0.799219f, 0.181399f, 0.312473f, 0.0402693f, 0.533386f, 0.581182f, 0.600382f, 0.186105f, 0.322734f, 0.460414f, 0.37009f, 0.452929f, 0.387669f, 0.883785f, 0.291214f, 0.403792f, 0.288797f, 0.114932f, 0.441901f, 0.369459f, 0.00886202f, 0.577986f, 0.457492f, 0.488561f, 0.845964f, 0.0515606f, 0.210285f, 0.14478f, 0.935541f, 0.874223f, 0.0491093f, 0.81723f, 0.35612f, 0.0562574f, 0.763055f, 0.364821f, 0.731659f, 0.745522f, 0.64809f, 0.510889f, 0.430949f, 0.161836f, 0.735475f, 0.264182f, 0.476501f, 0.465461f, 0.258138f, 0.730769f, 0.899318f, 0.174359f, 0.86886f, 0.756424f, 0.685789f, 0.229707f, 0.443774f, 0.275146f, 0.658326f, 0.0486892f, 0.0457834f, 0.787678f, 0.663175f, 0.481761f, 0.573898f, 0.439558f, 0.56098f, 0.98622f, 0.807752f, 0.34172f, 0.972262f, 0.284004f, 0.396007f, 0.440433f, 0.20663f, 0.121019f, 0.0240191f, 0.0363696f, 0.93515f, 0.283339f, 0.90816f, 0.694879f, 0.28891f, 0.7681f, 0.52112f, 0.473317f, 0.933415f, 0.149368f, 0.179469f, 0.922061f, 0.625879f, 0.916547f, 0.112067f, 0.974693f, 0.553303f, 0.44083f, 0.772933f, 0.423641f, 0.0963937f, 0.937332f, 0.301141f, 0.164467f, 0.977361f, 0.224354f, 0.84855f, 0.728315f, 0.564487f, 0.733492f, 0.911596f, 0.794423f, 0.175493f, 0.156265f, 0.511936f, 0.574377f, 0.394498f, 0.522894f, 0.0984874f, 0.921798f, 0.850203f, 0.596674f, 0.0368204f, 0.326229f, 0.281598f, 0.939939f, 0.487391f, 0.596167f, 0.549989f, 0.0975398f, 0.983239f, 0.993055f, 0.0635948f, 0.749972f, 0.177322f, 0.771397f, 0.777728f, 0.531868f, 0.31498f, 0.334786f, 0.633789f, 0.959249f, 0.573456f, 0.429871f, 0.238746f, 0.376446f, 0.0555074f, 0.506892f, 0.565241f, 0.897762f, 0.409047f, 0.890007f, 0.394771f, 0.923041f, 0.0133429f, 0.940546f, 0.601826f, 0.117889f, 0.341266f, 0.77822f, 0.948948f, 0.675557f, 0.620504f, 0.274183f, 0.697336f, 0.170604f, 0.726621f, 0.583086f, 0.0914261f, 0.621153f, 0.158024f, 0.481524f, 0.801537f, 0.682243f, 0.469472f, 0.0622474f, 0.0587367f, 0.986715f, 0.24073f, 0.454288f, 0.199331f, 0.830628f, 0.866293f, 0.504453f, 0.62714f, 0.760198f, 0.848832f, 0.467382f}),
                            .dimensions = {2, 6, 7, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy8
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x6x7x3_to_2x14x13x3_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x6x7x3_to_2x14x13x3_all_inputs_as_internal", get_test_model_half_pixel_centers_2x6x7x3_to_2x14x13x3_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x6x7x3_to_2x14x13x3_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.5970990061759949f, 0.7170640230178833f, 0.6969889998435974f, 0.37400999665260315f, 0.5877010226249695f, 0.5261300206184387f, 0.45982998609542847f, 0.8347070217132568f, 0.8258259892463684f, 0.1881919950246811f, 0.058183200657367706f, 0.4894869923591614f, 0.8974720239639282f, 0.9127089977264404f, 0.3382149934768677f, 0.11423300206661224f, 0.9753779768943787f, 0.1899150013923645f, 0.8858709931373596f, 0.012363100424408913f, 0.682108998298645f, 0.06375709921121597f, 0.02116600051522255f, 0.9904389977455139f, 0.4054659903049469f, 0.2451069951057434f, 0.23528499901294708f, 0.6705319881439209f, 0.7219460010528564f, 0.56113201379776f, 0.00936948973685503f, 0.052641499787569046f, 0.44719699025154114f, 0.7550650238990784f, 0.5975310206413269f, 0.19515100121498108f, 0.2697969973087311f, 0.5512080192565918f, 0.9540870189666748f, 0.22379499673843384f, 0.3978950083255768f, 0.9247890114784241f, 0.635591983795166f, 0.29778599739074707f, 0.023726800456643105f, 0.10101699829101562f, 0.49177101254463196f, 0.7159339785575867f, 0.5461130142211914f, 0.7824310064315796f, 0.8223419785499573f, 0.4546320140361786f, 0.5847259759902954f, 0.427172988653183f, 0.13006100058555603f, 0.6326299905776978f, 0.01302270032465458f, 0.46919500827789307f, 0.6897569894790649f, 0.025369999930262566f, 0.6305140256881714f, 0.9858930110931396f, 0.25782498717308044f, 0.08653680235147476f, 0.4650770127773285f, 0.9502390027046204f, 0.22172600030899048f, 0.34916600584983826f, 0.799219012260437f, 0.18139900267124176f, 0.31247299909591675f, 0.04026930034160614f, 0.5333859920501709f, 0.5811820030212402f, 0.6003819704055786f, 0.18610499799251556f, 0.322733998298645f, 0.46041399240493774f, 0.3700900077819824f, 0.45292899012565613f, 0.38766899704933167f, 0.8837850093841553f, 0.2912139892578125f, 0.40379199385643005f, 0.288796991109848f, 0.11493200063705444f, 0.44190099835395813f, 0.36945900321006775f, 0.008862020447850227f, 0.5779860019683838f, 0.45749199390411377f, 0.4885610044002533f, 0.8459640145301819f, 0.051560599356889725f, 0.2102849930524826f, 0.1447799950838089f, 0.9355409741401672f, 0.874222993850708f, 0.04910929873585701f, 0.8172299861907959f, 0.3561199903488159f, 0.056257400661706924f, 0.7630550265312195f, 0.3648209869861603f, 0.7316589951515198f, 0.7455220222473145f, 0.6480900049209595f, 0.5108889937400818f, 0.43094900250434875f, 0.16183599829673767f, 0.7354750037193298f, 0.2641820013523102f, 0.4765009880065918f, 0.46546098589897156f, 0.2581380009651184f, 0.7307689785957336f, 0.8993179798126221f, 0.17435899376869202f, 0.8688600063323975f, 0.7564240097999573f, 0.6857889890670776f, 0.2297070026397705f, 0.44377401471138f, 0.2751460075378418f, 0.6583260297775269f, 0.04868920147418976f, 0.045783400535583496f, 0.7876780033111572f, 0.6631749868392944f, 0.48176100850105286f, 0.5738980174064636f, 0.4395579993724823f, 0.5609800219535828f, 0.9862200021743774f, 0.8077520132064819f, 0.34172001481056213f, 0.9722620248794556f, 0.28400400280952454f, 0.3960070013999939f, 0.44043299555778503f, 0.20663000643253326f, 0.12101899832487106f, 0.02401909977197647f, 0.036369599401950836f, 0.9351500272750854f, 0.2833389937877655f, 0.9081599712371826f, 0.6948789954185486f, 0.28891000151634216f, 0.7681000232696533f, 0.521120011806488f, 0.4733169972896576f, 0.9334149956703186f, 0.14936800301074982f, 0.1794690042734146f, 0.922061026096344f, 0.6258789896965027f, 0.9165470004081726f, 0.11206699907779694f, 0.9746930003166199f, 0.5533030033111572f, 0.4408299922943115f, 0.7729330062866211f, 0.42364099621772766f, 0.09639369696378708f, 0.9373319745063782f, 0.30114099383354187f, 0.1644670069217682f, 0.9773610234260559f, 0.22435399889945984f, 0.848550021648407f, 0.728314995765686f, 0.5644869804382324f, 0.7334920167922974f, 0.9115960001945496f, 0.79442298412323f, 0.1754930019378662f, 0.1562650054693222f, 0.5119360089302063f, 0.5743770003318787f, 0.39449799060821533f, 0.522894024848938f, 0.09848739951848984f, 0.9217979907989502f, 0.8502029776573181f, 0.5966740250587463f, 0.03682040050625801f, 0.3262290060520172f, 0.2815980017185211f, 0.939939022064209f, 0.48739099502563477f, 0.5961670279502869f, 0.5499889850616455f, 0.09753979742527008f, 0.9832389950752258f, 0.993054986000061f, 0.06359480321407318f, 0.7499719858169556f, 0.17732200026512146f, 0.7713969945907593f, 0.7777280211448669f, 0.5318679809570312f, 0.31498000025749207f, 0.33478599786758423f, 0.6337890028953552f, 0.9592490196228027f, 0.5734559893608093f, 0.42987099289894104f, 0.23874600231647491f, 0.376446008682251f, 0.05550739914178848f, 0.5068920254707336f, 0.5652409791946411f, 0.8977620005607605f, 0.40904700756073f, 0.8900070190429688f, 0.394771009683609f, 0.9230409860610962f, 0.013342900201678276f, 0.9405459761619568f, 0.601826012134552f, 0.11788900196552277f, 0.341266006231308f, 0.778219997882843f, 0.9489480257034302f, 0.675557017326355f, 0.6205040216445923f, 0.2741830050945282f, 0.6973360180854797f, 0.17060400545597076f, 0.7266209721565247f, 0.5830860137939453f, 0.09142609685659409f, 0.6211529970169067f, 0.15802399814128876f, 0.4815239906311035f, 0.8015369772911072f, 0.6822429895401001f, 0.4694719910621643f, 0.06224739924073219f, 0.05873670056462288f, 0.9867150187492371f, 0.24073000252246857f, 0.4542880058288574f, 0.19933100044727325f, 0.830627977848053f, 0.8662930130958557f, 0.5044530034065247f, 0.6271399855613708f, 0.7601979970932007f, 0.8488320112228394f, 0.46738201379776f}),
                            .dimensions = {2, 6, 7, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output_width2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({13}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({14}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.5970990061759949f, 0.7170640230178833f, 0.6969889998435974f, 0.5284559726715088f, 0.6772599816322327f, 0.6444169878959656f, 0.4083310067653656f, 0.6076030135154724f, 0.5524160265922546f, 0.4070180058479309f, 0.682703971862793f, 0.641398012638092f, 0.4532279968261719f, 0.8157070279121399f, 0.8027729988098145f, 0.33445799350738525f, 0.47631099820137024f, 0.6705930233001709f, 0.1881919950246811f, 0.05818340182304382f, 0.4894869923591614f, 0.5701119899749756f, 0.5183129906654358f, 0.40803301334381104f, 0.8372229933738708f, 0.9175300002098083f, 0.3268069922924042f, 0.415477991104126f, 0.9512749910354614f, 0.24695299565792084f, 0.2329459935426712f, 0.8272219896316528f, 0.26563701033592224f, 0.6484439969062805f, 0.30867499113082886f, 0.5306649804115295f, 0.8858709931373596f, 0.012363100424408913f, 0.682108998298645f, 0.5209069848060608f, 0.6176499724388123f, 0.738910973072052f, 0.4770910143852234f, 0.5933759808540344f, 0.6606550216674805f, 0.400411993265152f, 0.5508959889411926f, 0.5237079858779907f, 0.42135998606681824f, 0.6463900208473206f, 0.6012849807739258f, 0.48135900497436523f, 0.7970719933509827f, 0.7646719813346863f, 0.33887600898742676f, 0.467272013425827f, 0.6474429965019226f, 0.16264599561691284f, 0.0573916994035244f, 0.4834449887275696f, 0.5473669767379761f, 0.4937030076980591f, 0.39423900842666626f, 0.820152997970581f, 0.8713070154190063f, 0.31633898615837097f, 0.4213300049304962f, 0.896668016910553f, 0.3062730133533478f, 0.23720000684261322f, 0.784421980381012f, 0.3633430004119873f, 0.5898020267486572f, 0.32815998792648315f, 0.5882560014724731f, 0.7912880182266235f, 0.0674389973282814f, 0.7167770266532898f, 0.2923319935798645f, 0.31940799951553345f, 0.864674985408783f, 0.32299500703811646f, 0.3417240083217621f, 0.7093700170516968f, 0.3766539990901947f, 0.38077500462532043f, 0.4375849962234497f, 0.4643869996070862f, 0.537447988986969f, 0.48094800114631653f, 0.565750002861023f, 0.7411689758300781f, 0.6503689885139465f, 0.3521279990673065f, 0.44015398621559143f, 0.5779950022697449f, 0.08600780367851257f, 0.05501670017838478f, 0.46532100439071655f, 0.4791330099105835f, 0.4198729991912842f, 0.3528589904308319f, 0.7689449787139893f, 0.7326369881629944f, 0.28493499755859375f, 0.4388839900493622f, 0.7328460216522217f, 0.48423099517822266f, 0.2499600052833557f, 0.6560220122337341f, 0.6564610004425049f, 0.4138759970664978f, 0.38661399483680725f, 0.7610300183296204f, 0.5075420141220093f, 0.2326669991016388f, 0.8207830190658569f, 0.06375709921121597f, 0.02116600051522255f, 0.9904389977455139f, 0.16889800131320953f, 0.09007079899311066f, 0.7580839991569519f, 0.3528960049152374f, 0.21065400540828705f, 0.3514620065689087f, 0.5074139833450317f, 0.4285059869289398f, 0.3606109917163849f, 0.6501420140266418f, 0.6852660179138184f, 0.536067008972168f, 0.3653799891471863f, 0.4130359888076782f, 0.5085470080375671f, 0.009369670413434505f, 0.052641600370407104f, 0.44719699025154114f, 0.41089800000190735f, 0.34604400396347046f, 0.31147998571395874f, 0.7177370190620422f, 0.5939679741859436f, 0.25353100895881653f, 0.45643800497055054f, 0.5690249800682068f, 0.6621890068054199f, 0.2627190053462982f, 0.5276219844818115f, 0.9495800137519836f, 0.23794899880886078f, 0.44506800174713135f, 0.9338039755821228f, 0.22379499673843384f, 0.3978950083255768f, 0.9247890114784241f, 0.30882900953292847f, 0.13971799612045288f, 0.5761340260505676f, 0.29841700196266174f, 0.20467199385166168f, 0.5346390008926392f, 0.2801940143108368f, 0.31834301352500916f, 0.4620240032672882f, 0.40661200881004333f, 0.5035309791564941f, 0.5304319858551025f, 0.5908849835395813f, 0.7173259854316711f, 0.6552479863166809f, 0.4247420132160187f, 0.5322420001029968f, 0.564864993095398f, 0.20019599795341492f, 0.28067800402641296f, 0.43861499428749084f, 0.3547399938106537f, 0.4593909978866577f, 0.26548901200294495f, 0.47705599665641785f, 0.6124209761619568f, 0.15086300671100616f, 0.40600401163101196f, 0.6113510131835938f, 0.3872309923171997f, 0.3618449866771698f, 0.6166329979896545f, 0.5688170194625854f, 0.3849180042743683f, 0.6377990245819092f, 0.613444983959198f, 0.3981029987335205f, 0.6498939990997314f, 0.6389470100402832f, 0.5539019703865051f, 0.2582690119743347f, 0.16182799637317657f, 0.42793500423431396f, 0.31927400827407837f, 0.3111949861049652f, 0.20749300718307495f, 0.4260309934616089f, 0.5725870132446289f, 0.30580899119377136f, 0.5785549879074097f, 0.7002530097961426f, 0.5316269993782043f, 0.7493860125541687f, 0.774429976940155f, 0.4841040074825287f, 0.6514469981193542f, 0.6211829781532288f, 0.39102301001548767f, 0.5087140202522278f, 0.43003299832344055f, 0.2985819876194f, 0.572737991809845f, 0.21949900686740875f, 0.23637500405311584f, 0.63087397813797f, 0.0481950007379055f, 0.355569988489151f, 0.6536759734153748f, 0.11227399855852127f, 0.46097099781036377f, 0.7056450247764587f, 0.18805299699306488f, 0.5318869948387146f, 0.8305299878120422f, 0.2930859923362732f, 0.5724110007286072f, 0.9018930196762085f, 0.35310500860214233f, 0.47871899604797363f, 0.3455829918384552f, 0.28844499588012695f, 0.37311500310897827f, 0.378028005361557f, 0.4273020029067993f, 0.1883080005645752f, 0.4348050057888031f, 0.6703010201454163f, 0.2533529996871948f, 0.5268459916114807f, 0.685562014579773f, 0.4183390140533447f, 0.6329929828643799f, 0.6097260117530823f, 0.4581660032272339f, 0.6184139847755432f, 0.5424780249595642f, 0.47713300585746765f, 0.5837140083312988f, 0.47666099667549133f, 0.2988699972629547f, 0.5623769760131836f, 0.29583901166915894f, 0.16875100135803223f, 0.5500890016555786f, 0.13992899656295776f, 0.32749199867248535f, 0.5920900106430054f, 0.13348600268363953f, 0.48118698596954346f, 0.6475260257720947f, 0.15513700246810913f, 0.6222620010375977f, 0.7365450263023376f, 0.24702300131320953f, 0.7028769850730896f, 0.7874130010604858f, 0.2995299994945526f, 0.24341000616550446f, 0.4172790050506592f, 0.6855210065841675f, 0.22612600028514862f, 0.4088580012321472f, 0.7131839990615845f, 0.1958799958229065f, 0.394120991230011f, 0.7615919709205627f, 0.2250709980726242f, 0.4117699861526489f, 0.5786139965057373f, 0.2780359983444214f, 0.44237399101257324f, 0.3030799925327301f, 0.38957899808883667f, 0.5092610120773315f, 0.3962610065937042f, 0.5108850002288818f, 0.582194983959198f, 0.550894021987915f, 0.3273810148239136f, 0.4901619851589203f, 0.4333449900150299f, 0.1876550018787384f, 0.4196850061416626f, 0.3288629949092865f, 0.31059300899505615f, 0.4785490036010742f, 0.3027830123901367f, 0.4619469940662384f, 0.5158399939537048f, 0.2961449921131134f, 0.6843400001525879f, 0.49920201301574707f, 0.33810800313949585f, 0.8114219903945923f, 0.4896939992904663f, 0.3620870113372803f, 0.11543100327253342f, 0.4150559902191162f, 0.8776190280914307f, 0.15463100373744965f, 0.3798240125179291f, 0.8437719941139221f, 0.2232310026884079f, 0.3181670010089874f, 0.7845379710197449f, 0.23437300324440002f, 0.31481099128723145f, 0.532135009765625f, 0.2225320041179657f, 0.33477601408958435f, 0.2024639993906021f, 0.33332398533821106f, 0.42558300495147705f, 0.3307200074195862f, 0.4645540118217468f, 0.5281969904899597f, 0.5352960228919983f, 0.37226900458335876f, 0.45998600125312805f, 0.4633359909057617f, 0.3039979934692383f, 0.4044089913368225f, 0.3969390094280243f, 0.3798139989376068f, 0.42464500665664673f, 0.36391401290893555f, 0.5005170106887817f, 0.41796499490737915f, 0.3572950065135956f, 0.7334399819374084f, 0.3439970016479492f, 0.41668999195098877f, 0.8665379881858826f, 0.3017300069332123f, 0.4506300091743469f, 0.20211400091648102f, 0.2649939954280853f, 0.6597599983215332f, 0.23412400484085083f, 0.2310589998960495f, 0.6637719869613647f, 0.2901400029659271f, 0.1716739982366562f, 0.6707940101623535f, 0.3188459873199463f, 0.2540859878063202f, 0.606594979763031f, 0.3366270065307617f, 0.3932180106639862f, 0.5139070153236389f, 0.3017309904098511f, 0.3928540050983429f, 0.42653000354766846f, 0.25805699825286865f, 0.36924099922180176f, 0.340038001537323f, 0.4499100148677826f, 0.5138880014419556f, 0.2783010005950928f, 0.615218997001648f, 0.619388997554779f, 0.22329899668693542f, 0.6212739944458008f, 0.49001801013946533f, 0.20871099829673767f, 0.6547060012817383f, 0.3877120018005371f, 0.2587299942970276f, 0.7565820217132568f, 0.3530699908733368f, 0.47026699781417847f, 0.8147969841957092f, 0.3332749903202057f, 0.5911440253257751f, 0.288796991109848f, 0.11493200063705444f, 0.44190099835395813f, 0.313616007566452f, 0.08229490369558334f, 0.48377299308776855f, 0.35705000162124634f, 0.025180399417877197f, 0.5570499897003174f, 0.40331798791885376f, 0.19336099922657013f, 0.6810550093650818f, 0.45072001218795776f, 0.45166099071502686f, 0.8253499865531921f, 0.27013900876045227f, 0.36012598872184753f, 0.5223410129547119f, 0.0515609011054039f, 0.2102849930524826f, 0.1447799950838089f, 0.5275499820709229f, 0.5677899718284607f, 0.09326499700546265f, 0.9264400005340576f, 0.8343690037727356f, 0.04965920001268387f, 0.8627340197563171f, 0.5553900003433228f, 0.05350809916853905f, 0.808896005153656f, 0.35745900869369507f, 0.1601649969816208f, 0.7797250151634216f, 0.36214399337768555f, 0.523842990398407f, 0.7630550265312195f, 0.3648209869861603f, 0.7316589951515198f, 0.48453599214553833f, 0.34342798590660095f, 0.471466988325119f, 0.45723599195480347f, 0.26065701246261597f, 0.5250099897384644f, 0.40946200489997864f, 0.11580800265073776f, 0.6187099814414978f, 0.38767099380493164f, 0.2317180037498474f, 0.6598700284957886f, 0.3762730062007904f, 0.45193299651145935f, 0.6800140142440796f, 0.26639100909233093f, 0.46029600501060486f, 0.5837820172309875f, 0.14009399712085724f, 0.43334999680519104f, 0.46815401315689087f, 0.3927539885044098f, 0.6695060133934021f, 0.40574100613594055f, 0.6209800243377686f, 0.8280799984931946f, 0.3422519862651825f, 0.7025989890098572f, 0.521166980266571f, 0.27230098843574524f, 0.7290599942207336f, 0.3309679925441742f, 0.2556619942188263f, 0.6176270246505737f, 0.43255800008773804f, 0.3723050057888031f, 0.5539510250091553f, 0.49060899019241333f, 0.43895700573921204f, 0.680275022983551f, 0.5719249844551086f, 0.5010340213775635f, 0.6008570194244385f, 0.43901899456977844f, 0.5662469863891602f, 0.46187400817871094f, 0.20643499493598938f, 0.6803699731826782f, 0.37202298641204834f, 0.2700749933719635f, 0.6386849880218506f, 0.30182600021362305f, 0.45220500230789185f, 0.5346770286560059f, 0.2626419961452484f, 0.5604659914970398f, 0.645222008228302f, 0.22862699627876282f, 0.6564139723777771f, 0.7915269732475281f, 0.25795799493789673f, 0.7712209820747375f, 0.7182170152664185f, 0.3155199885368347f, 0.8217909932136536f, 0.6348440051078796f, 0.542464017868042f, 0.48694300651550293f, 0.4910930097103119f, 0.6492249965667725f, 0.3044779896736145f, 0.351159006357193f, 0.45552998781204224f, 0.5029720067977905f, 0.22076599299907684f, 0.3448469936847687f, 0.6163970232009888f, 0.1462559998035431f, 0.7455220222473145f, 0.6480900049209595f, 0.5108889937400818f, 0.6487299799919128f, 0.49847298860549927f, 0.5799919962882996f, 0.4793440103530884f, 0.23664399981498718f, 0.7009230256080627f, 0.3668079972267151f, 0.28286099433898926f, 0.6316230297088623f, 0.2770099937915802f, 0.4522959887981415f, 0.4862320125102997f, 0.2613919973373413f, 0.5938559770584106f, 0.6657029986381531f, 0.2581380009651184f, 0.7307689785957336f, 0.8993179798126221f, 0.2130260020494461f, 0.8051260113716125f, 0.8223749995231628f, 0.21369999647140503f, 0.8196939826011658f, 0.7323740124702454f, 0.48908498883247375f, 0.47553500533103943f, 0.5640239715576172f, 0.6226130127906799f, 0.2956480085849762f, 0.3829919993877411f, 0.4014979898929596f, 0.5264430046081543f, 0.17025400698184967f, 0.2751460075378418f, 0.6583260297775269f, 0.04868920147418976f, 0.045783400535583496f, 0.7876780033111572f, 0.6631749868392944f, 0.17993000149726868f, 0.7218999862670898f, 0.5943700075149536f, 0.41468799114227295f, 0.6067870259284973f, 0.4739609956741333f, 0.5122299790382385f, 0.732483983039856f, 0.5811709761619568f, 0.5548869967460632f, 0.9545029997825623f, 0.7794299721717834f, 0.45978298783302307f, 0.979777991771698f, 0.5660219788551331f, 0.34172001481056213f, 0.9722610116004944f, 0.28400400280952454f, 0.3709520101547241f, 0.6858919858932495f, 0.24234099686145782f, 0.3748539984226227f, 0.4084010124206543f, 0.1935330033302307f, 0.22678300738334656f, 0.18417799472808838f, 0.10185399651527405f, 0.24627000093460083f, 0.06391450017690659f, 0.17049099504947662f, 0.6846479773521423f, 0.20354799926280975f, 0.6399170160293579f, 0.9351500272750854f, 0.2833389937877655f, 0.9081599712371826f, 0.13851100206375122f, 0.7164250016212463f, 0.678164005279541f, 0.24585700035095215f, 0.668150007724762f, 0.6264550089836121f, 0.4337109923362732f, 0.583666980266571f, 0.5359640121459961f, 0.4930739998817444f, 0.6793140172958374f, 0.6308680176734924f, 0.5010409951210022f, 0.847012996673584f, 0.7999299764633179f, 0.4468570053577423f, 0.9140470027923584f, 0.5634790062904358f, 0.3823150098323822f, 0.9643020033836365f, 0.259442001581192f, 0.4342019855976105f, 0.690900981426239f, 0.24901999533176422f, 0.4583280086517334f, 0.4276759922504425f, 0.22507600486278534f, 0.31589001417160034f, 0.22551099956035614f, 0.119998998939991f, 0.3251209855079651f, 0.11261200159788132f, 0.16140200197696686f, 0.7135199904441833f, 0.22287499904632568f, 0.5690029859542847f, 0.9354619979858398f, 0.28588199615478516f, 0.8019180297851562f, 0.4166949987411499f, 0.502668023109436f, 0.7231320142745972f, 0.4436360001564026f, 0.5069000124931335f, 0.7227110266685486f, 0.4907810091972351f, 0.5143070220947266f, 0.7219730019569397f, 0.4356060028076172f, 0.5198060274124146f, 0.7799580097198486f, 0.3395020067691803f, 0.5245429873466492f, 0.8614320158958435f, 0.4080759882926941f, 0.7168530225753784f, 0.5558480024337769f, 0.5040969848632812f, 0.9404249787330627f, 0.18575499951839447f, 0.6239519715309143f, 0.7059270143508911f, 0.2690570056438446f, 0.7087500095367432f, 0.4855020046234131f, 0.319705992937088f, 0.5832120180130005f, 0.3495109975337982f, 0.17443400621414185f, 0.5616729855537415f, 0.25870299339294434f, 0.13413399457931519f, 0.800134003162384f, 0.2808539867401123f, 0.3562619984149933f, 0.9363970160484314f, 0.2935110032558441f, 0.4831930100917816f, 0.6948789954185486f, 0.28891000151634216f, 0.7681000232696533f, 0.641414999961853f, 0.3456510007381439f, 0.8189659714698792f, 0.5478519797325134f, 0.4449470043182373f, 0.9079819917678833f, 0.37813800573349f, 0.3602980077266693f, 0.9290480017662048f, 0.1779640018939972f, 0.20207299292087555f, 0.9229339957237244f, 0.36929601430892944f, 0.5196589827537537f, 0.5482180118560791f, 0.6258789896965027f, 0.9165470004081726f, 0.11206699907779694f, 0.8137019872665405f, 0.7209540009498596f, 0.2890940010547638f, 0.9591730237007141f, 0.5433290004730225f, 0.4143350124359131f, 0.8505330085754395f, 0.4735110104084015f, 0.2288690060377121f, 0.7982249855995178f, 0.40479499101638794f, 0.1068670004606247f, 0.8867480158805847f, 0.3388330042362213f, 0.1435209959745407f, 0.9373319745063782f, 0.30114099383354187f, 0.1644670069217682f, 0.8159430027008057f, 0.26124298572540283f, 0.8025789856910706f, 0.7525500059127808f, 0.33851900696754456f, 0.8164730072021484f, 0.6416140198707581f, 0.4737519919872284f, 0.8407869935035706f, 0.5584250092506409f, 0.485709011554718f, 0.7532600164413452f, 0.48633599281311035f, 0.4483569860458374f, 0.6209980249404907f, 0.45230400562286377f, 0.5815380215644836f, 0.46737900376319885f, 0.4246160089969635f, 0.7431420087814331f, 0.3102000057697296f, 0.5869200229644775f, 0.633903980255127f, 0.30153700709342957f, 0.7345529794692993f, 0.5453609824180603f, 0.29539600014686584f, 0.7941570281982422f, 0.5809980034828186f, 0.30438101291656494f, 0.792834997177124f, 0.56113600730896f, 0.29600998759269714f, 0.6391929984092712f, 0.40252700448036194f, 0.2442460060119629f, 0.5513989925384521f, 0.3118929862976074f, 0.2146659940481186f, 0.9370070099830627f, 0.2335759997367859f, 0.8370569944381714f, 0.8636860251426697f, 0.3313870131969452f, 0.8139790296554565f, 0.7353749871253967f, 0.5025569796562195f, 0.7735909819602966f, 0.7387120127677917f, 0.6111199855804443f, 0.577472984790802f, 0.7947070002555847f, 0.6946409940719604f, 0.3190619945526123f, 0.5353119969367981f, 0.64341801404953f, 0.3865399956703186f, 0.22335200011730194f, 0.5697370171546936f, 0.5083330273628235f, 0.3601379990577698f, 0.5468530058860779f, 0.31398099660873413f, 0.5099329948425293f, 0.5473939776420593f, 0.1764560043811798f, 0.7377820014953613f, 0.6884859800338745f, 0.3798930048942566f, 0.787443995475769f, 0.7174779772758484f, 0.4851529896259308f, 0.39163899421691895f, 0.46622100472450256f, 0.3449699878692627f, 0.16546499729156494f, 0.32264500856399536f, 0.2648650109767914f, 0.9666690230369568f, 0.2995069921016693f, 0.7764409780502319f, 0.8776519894599915f, 0.3399890065193176f, 0.7851809859275818f, 0.7218729853630066f, 0.4108330011367798f, 0.8004779815673828f, 0.7764049768447876f, 0.49051201343536377f, 0.6259179711341858f, 0.9150620102882385f, 0.573727011680603f, 0.37541601061820984f, 0.5782909989356995f, 0.5858240127563477f, 0.4747909903526306f, 0.16228100657463074f, 0.5860670208930969f, 0.6324769854545593f, 0.3084549903869629f, 0.5200639963150024f, 0.3812980055809021f, 0.4649589955806732f, 0.495633989572525f, 0.19862000644207f, 0.683447003364563f, 0.720641016960144f, 0.42694899439811707f, 0.7332959771156311f, 0.7921090126037598f, 0.5467569828033447f, 0.3615480065345764f, 0.47973400354385376f, 0.39526399970054626f, 0.1491210013628006f, 0.30123400688171387f, 0.308696985244751f, 0.9506310224533081f, 0.4122369885444641f, 0.6682760119438171f, 0.843034029006958f, 0.35645800828933716f, 0.743232011795044f, 0.6547380089759827f, 0.25884300470352173f, 0.874405026435852f, 0.7428020238876343f, 0.24689500033855438f, 0.7864800095558167f, 0.9414089918136597f, 0.269214004278183f, 0.610916018486023f, 0.6012539863586426f, 0.4684930145740509f, 0.6475859880447388f, 0.17130599915981293f, 0.6972650289535522f, 0.7196279764175415f, 0.34432101249694824f, 0.5234060287475586f, 0.4760519862174988f, 0.5098080039024353f, 0.4169769883155823f, 0.29133498668670654f, 0.6301320195198059f, 0.7151280045509338f, 0.45977500081062317f, 0.6547679901123047f, 0.825884997844696f, 0.5445910096168518f, 0.4401879906654358f, 0.46815699338912964f, 0.4203439950942993f, 0.3175710141658783f, 0.26374098658561707f, 0.34934601187705994f, 0.8135920166969299f, 0.4901770055294037f, 0.5917490124702454f, 0.7477700114250183f, 0.38305801153182983f, 0.7081090211868286f, 0.6325820088386536f, 0.1956000030040741f, 0.9117400050163269f, 0.7181000113487244f, 0.1590919941663742f, 0.844851016998291f, 0.883899986743927f, 0.18296299874782562f, 0.6697530150413513f, 0.6208639740943909f, 0.4452030062675476f, 0.6626129746437073f, 0.2863540053367615f, 0.7471719980239868f, 0.6834650039672852f, 0.40389201045036316f, 0.5500869750976562f, 0.5429580211639404f, 0.5150330066680908f, 0.42190301418304443f, 0.4308449923992157f, 0.5877879858016968f, 0.7071260213851929f, 0.489095002412796f, 0.6133440136909485f, 0.8059579730033875f, 0.5146669745445251f, 0.520904004573822f, 0.43880999088287354f, 0.45854198932647705f, 0.468080997467041f, 0.22901199758052826f, 0.4264709949493408f, 0.4345490038394928f, 0.4985339939594269f, 0.5784950256347656f, 0.531216025352478f, 0.4299220144748688f, 0.6866390109062195f, 0.7003830075263977f, 0.3098500072956085f, 0.8758909702301025f, 0.7112020254135132f, 0.3829140067100525f, 0.698839008808136f, 0.6586809754371643f, 0.5332329869270325f, 0.3752659857273102f, 0.6337649822235107f, 0.6099960207939148f, 0.36210399866104126f, 0.6134499907493591f, 0.674498975276947f, 0.40067699551582336f, 0.5108749866485596f, 0.6234470009803772f, 0.5541700124740601f, 0.44100698828697205f, 0.593995988368988f, 0.6639440059661865f, 0.5673859715461731f, 0.6941450238227844f, 0.5114009976387024f, 0.6461259722709656f, 0.6786230206489563f, 0.4292280077934265f, 0.6057689785957336f, 0.3739219903945923f, 0.5229759812355042f, 0.5827080011367798f, 0.1998080015182495f, 0.5765460133552551f, 0.05550739914178848f, 0.5068920254707336f, 0.5652409791946411f, 0.31466299295425415f, 0.4767859876155853f, 0.6651690006256104f, 0.768185019493103f, 0.42410001158714294f, 0.8400430083274841f, 0.7043039798736572f, 0.6067370176315308f, 0.5528280138969421f, 0.4334630072116852f, 0.8835030198097229f, 0.08077839761972427f, 0.6466670036315918f, 0.7747880220413208f, 0.06159469857811928f, 0.9405459761619568f, 0.601826012134552f, 0.11788900196552277f, 0.6178569793701172f, 0.6968079805374146f, 0.5653820037841797f, 0.36698099970817566f, 0.7660880088806152f, 0.897042989730835f, 0.5469840168952942f, 0.681164026260376f, 0.5337079763412476f, 0.6789069771766663f, 0.5512880086898804f, 0.3437890112400055f, 0.6906350255012512f, 0.3090350031852722f, 0.5874090194702148f, 0.6973360180854797f, 0.17060400545597076f, 0.7266209721565247f, 0.28161299228668213f, 0.32883501052856445f, 0.5892030000686646f, 0.373649001121521f, 0.3630729913711548f, 0.670091986656189f, 0.5347129702568054f, 0.42298999428749084f, 0.8116469979286194f, 0.5565940141677856f, 0.5510879755020142f, 0.5375570058822632f, 0.5228009819984436f, 0.7064589858055115f, 0.09720870107412338f, 0.538582980632782f, 0.7462499737739563f, 0.09717869758605957f, 0.5626270174980164f, 0.7667790055274963f, 0.17053499817848206f, 0.46951499581336975f, 0.6393499970436096f, 0.5623760223388672f, 0.41798099875450134f, 0.5332509875297546f, 0.8618710041046143f, 0.615917980670929f, 0.5551360249519348f, 0.6072919964790344f, 0.7522199749946594f, 0.5539219975471497f, 0.454692006111145f, 0.7344380021095276f, 0.49496400356292725f, 0.5570359826087952f, 0.7242770195007324f, 0.4612730145454407f, 0.6155189871788025f, 0.507718026638031f, 0.15077799558639526f, 0.613165020942688f, 0.43263599276542664f, 0.24936099350452423f, 0.6750149726867676f, 0.30124199390411377f, 0.42188000679016113f, 0.7832509875297546f, 0.4088839888572693f, 0.49543899297714233f, 0.522284984588623f, 0.6121389865875244f, 0.5294139981269836f, 0.1136389970779419f, 0.43049898743629456f, 0.7177129983901978f, 0.13276299834251404f, 0.18470899760723114f, 0.9317309856414795f, 0.22318099439144135f, 0.3211730122566223f, 0.5818920135498047f, 0.5593690276145935f, 0.468980997800827f, 0.30041399598121643f, 0.8266990184783936f, 0.6848520040512085f, 0.42910799384117126f, 0.6808760166168213f, 0.8255329728126526f, 0.5565569996833801f, 0.5655940175056458f, 0.7782419919967651f, 0.6808930039405823f, 0.5266630053520203f, 0.7512180209159851f, 0.751941978931427f, 0.5044159889221191f, 0.5830860137939453f, 0.09142609685659409f, 0.6211529970169067f, 0.45229798555374146f, 0.21145600080490112f, 0.6766560077667236f, 0.2234179973602295f, 0.4215089976787567f, 0.7737849950790405f, 0.3596470057964325f, 0.4768890142440796f, 0.5171949863433838f, 0.6419190168380737f, 0.47039899230003357f, 0.11911600083112717f, 0.3944709897041321f, 0.7081999778747559f, 0.1446239948272705f, 0.05873680114746094f, 0.9867150187492371f, 0.24073000252246857f, 0.27172601222991943f, 0.5627390146255493f, 0.5583670139312744f, 0.4859809875488281f, 0.22280199825763702f, 0.8149750232696533f, 0.707830011844635f, 0.3870989978313446f, 0.7054049968719482f, 0.8499709963798523f, 0.5574349761009216f, 0.6025620102882385f, 0.7928429841995239f, 0.7428690195083618f, 0.5165389776229858f, 0.7601979970932007f, 0.8488320112228394f, 0.46738201379776f}),
                            .dimensions = {2, 14, 13, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x6x7x3_to_2x14x13x3_float16 = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x6x7x3_to_2x14x13x3_float16", get_test_model_half_pixel_centers_2x6x7x3_to_2x14x13x3_float16());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x6x7x3_to_2x14x13x3_float16_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({}),
                            .dimensions = {2, 6, 7, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output_width2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({13}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({14}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.5970990061759949f, 0.7170640230178833f, 0.6969889998435974f, 0.5284559726715088f, 0.6772599816322327f, 0.6444169878959656f, 0.4083310067653656f, 0.6076030135154724f, 0.5524160265922546f, 0.4070180058479309f, 0.682703971862793f, 0.641398012638092f, 0.4532279968261719f, 0.8157070279121399f, 0.8027729988098145f, 0.33445799350738525f, 0.47631099820137024f, 0.6705930233001709f, 0.1881919950246811f, 0.05818340182304382f, 0.4894869923591614f, 0.5701119899749756f, 0.5183129906654358f, 0.40803301334381104f, 0.8372229933738708f, 0.9175300002098083f, 0.3268069922924042f, 0.415477991104126f, 0.9512749910354614f, 0.24695299565792084f, 0.2329459935426712f, 0.8272219896316528f, 0.26563701033592224f, 0.6484439969062805f, 0.30867499113082886f, 0.5306649804115295f, 0.8858709931373596f, 0.012363100424408913f, 0.682108998298645f, 0.5209069848060608f, 0.6176499724388123f, 0.738910973072052f, 0.4770910143852234f, 0.5933759808540344f, 0.6606550216674805f, 0.400411993265152f, 0.5508959889411926f, 0.5237079858779907f, 0.42135998606681824f, 0.6463900208473206f, 0.6012849807739258f, 0.48135900497436523f, 0.7970719933509827f, 0.7646719813346863f, 0.33887600898742676f, 0.467272013425827f, 0.6474429965019226f, 0.16264599561691284f, 0.0573916994035244f, 0.4834449887275696f, 0.5473669767379761f, 0.4937030076980591f, 0.39423900842666626f, 0.820152997970581f, 0.8713070154190063f, 0.31633898615837097f, 0.4213300049304962f, 0.896668016910553f, 0.3062730133533478f, 0.23720000684261322f, 0.784421980381012f, 0.3633430004119873f, 0.5898020267486572f, 0.32815998792648315f, 0.5882560014724731f, 0.7912880182266235f, 0.0674389973282814f, 0.7167770266532898f, 0.2923319935798645f, 0.31940799951553345f, 0.864674985408783f, 0.32299500703811646f, 0.3417240083217621f, 0.7093700170516968f, 0.3766539990901947f, 0.38077500462532043f, 0.4375849962234497f, 0.4643869996070862f, 0.537447988986969f, 0.48094800114631653f, 0.565750002861023f, 0.7411689758300781f, 0.6503689885139465f, 0.3521279990673065f, 0.44015398621559143f, 0.5779950022697449f, 0.08600780367851257f, 0.05501670017838478f, 0.46532100439071655f, 0.4791330099105835f, 0.4198729991912842f, 0.3528589904308319f, 0.7689449787139893f, 0.7326369881629944f, 0.28493499755859375f, 0.4388839900493622f, 0.7328460216522217f, 0.48423099517822266f, 0.2499600052833557f, 0.6560220122337341f, 0.6564610004425049f, 0.4138759970664978f, 0.38661399483680725f, 0.7610300183296204f, 0.5075420141220093f, 0.2326669991016388f, 0.8207830190658569f, 0.06375709921121597f, 0.02116600051522255f, 0.9904389977455139f, 0.16889800131320953f, 0.09007079899311066f, 0.7580839991569519f, 0.3528960049152374f, 0.21065400540828705f, 0.3514620065689087f, 0.5074139833450317f, 0.4285059869289398f, 0.3606109917163849f, 0.6501420140266418f, 0.6852660179138184f, 0.536067008972168f, 0.3653799891471863f, 0.4130359888076782f, 0.5085470080375671f, 0.009369670413434505f, 0.052641600370407104f, 0.44719699025154114f, 0.41089800000190735f, 0.34604400396347046f, 0.31147998571395874f, 0.7177370190620422f, 0.5939679741859436f, 0.25353100895881653f, 0.45643800497055054f, 0.5690249800682068f, 0.6621890068054199f, 0.2627190053462982f, 0.5276219844818115f, 0.9495800137519836f, 0.23794899880886078f, 0.44506800174713135f, 0.9338039755821228f, 0.22379499673843384f, 0.3978950083255768f, 0.9247890114784241f, 0.30882900953292847f, 0.13971799612045288f, 0.5761340260505676f, 0.29841700196266174f, 0.20467199385166168f, 0.5346390008926392f, 0.2801940143108368f, 0.31834301352500916f, 0.4620240032672882f, 0.40661200881004333f, 0.5035309791564941f, 0.5304319858551025f, 0.5908849835395813f, 0.7173259854316711f, 0.6552479863166809f, 0.4247420132160187f, 0.5322420001029968f, 0.564864993095398f, 0.20019599795341492f, 0.28067800402641296f, 0.43861499428749084f, 0.3547399938106537f, 0.4593909978866577f, 0.26548901200294495f, 0.47705599665641785f, 0.6124209761619568f, 0.15086300671100616f, 0.40600401163101196f, 0.6113510131835938f, 0.3872309923171997f, 0.3618449866771698f, 0.6166329979896545f, 0.5688170194625854f, 0.3849180042743683f, 0.6377990245819092f, 0.613444983959198f, 0.3981029987335205f, 0.6498939990997314f, 0.6389470100402832f, 0.5539019703865051f, 0.2582690119743347f, 0.16182799637317657f, 0.42793500423431396f, 0.31927400827407837f, 0.3111949861049652f, 0.20749300718307495f, 0.4260309934616089f, 0.5725870132446289f, 0.30580899119377136f, 0.5785549879074097f, 0.7002530097961426f, 0.5316269993782043f, 0.7493860125541687f, 0.774429976940155f, 0.4841040074825287f, 0.6514469981193542f, 0.6211829781532288f, 0.39102301001548767f, 0.5087140202522278f, 0.43003299832344055f, 0.2985819876194f, 0.572737991809845f, 0.21949900686740875f, 0.23637500405311584f, 0.63087397813797f, 0.0481950007379055f, 0.355569988489151f, 0.6536759734153748f, 0.11227399855852127f, 0.46097099781036377f, 0.7056450247764587f, 0.18805299699306488f, 0.5318869948387146f, 0.8305299878120422f, 0.2930859923362732f, 0.5724110007286072f, 0.9018930196762085f, 0.35310500860214233f, 0.47871899604797363f, 0.3455829918384552f, 0.28844499588012695f, 0.37311500310897827f, 0.378028005361557f, 0.4273020029067993f, 0.1883080005645752f, 0.4348050057888031f, 0.6703010201454163f, 0.2533529996871948f, 0.5268459916114807f, 0.685562014579773f, 0.4183390140533447f, 0.6329929828643799f, 0.6097260117530823f, 0.4581660032272339f, 0.6184139847755432f, 0.5424780249595642f, 0.47713300585746765f, 0.5837140083312988f, 0.47666099667549133f, 0.2988699972629547f, 0.5623769760131836f, 0.29583901166915894f, 0.16875100135803223f, 0.5500890016555786f, 0.13992899656295776f, 0.32749199867248535f, 0.5920900106430054f, 0.13348600268363953f, 0.48118698596954346f, 0.6475260257720947f, 0.15513700246810913f, 0.6222620010375977f, 0.7365450263023376f, 0.24702300131320953f, 0.7028769850730896f, 0.7874130010604858f, 0.2995299994945526f, 0.24341000616550446f, 0.4172790050506592f, 0.6855210065841675f, 0.22612600028514862f, 0.4088580012321472f, 0.7131839990615845f, 0.1958799958229065f, 0.394120991230011f, 0.7615919709205627f, 0.2250709980726242f, 0.4117699861526489f, 0.5786139965057373f, 0.2780359983444214f, 0.44237399101257324f, 0.3030799925327301f, 0.38957899808883667f, 0.5092610120773315f, 0.3962610065937042f, 0.5108850002288818f, 0.582194983959198f, 0.550894021987915f, 0.3273810148239136f, 0.4901619851589203f, 0.4333449900150299f, 0.1876550018787384f, 0.4196850061416626f, 0.3288629949092865f, 0.31059300899505615f, 0.4785490036010742f, 0.3027830123901367f, 0.4619469940662384f, 0.5158399939537048f, 0.2961449921131134f, 0.6843400001525879f, 0.49920201301574707f, 0.33810800313949585f, 0.8114219903945923f, 0.4896939992904663f, 0.3620870113372803f, 0.11543100327253342f, 0.4150559902191162f, 0.8776190280914307f, 0.15463100373744965f, 0.3798240125179291f, 0.8437719941139221f, 0.2232310026884079f, 0.3181670010089874f, 0.7845379710197449f, 0.23437300324440002f, 0.31481099128723145f, 0.532135009765625f, 0.2225320041179657f, 0.33477601408958435f, 0.2024639993906021f, 0.33332398533821106f, 0.42558300495147705f, 0.3307200074195862f, 0.4645540118217468f, 0.5281969904899597f, 0.5352960228919983f, 0.37226900458335876f, 0.45998600125312805f, 0.4633359909057617f, 0.3039979934692383f, 0.4044089913368225f, 0.3969390094280243f, 0.3798139989376068f, 0.42464500665664673f, 0.36391401290893555f, 0.5005170106887817f, 0.41796499490737915f, 0.3572950065135956f, 0.7334399819374084f, 0.3439970016479492f, 0.41668999195098877f, 0.8665379881858826f, 0.3017300069332123f, 0.4506300091743469f, 0.20211400091648102f, 0.2649939954280853f, 0.6597599983215332f, 0.23412400484085083f, 0.2310589998960495f, 0.6637719869613647f, 0.2901400029659271f, 0.1716739982366562f, 0.6707940101623535f, 0.3188459873199463f, 0.2540859878063202f, 0.606594979763031f, 0.3366270065307617f, 0.3932180106639862f, 0.5139070153236389f, 0.3017309904098511f, 0.3928540050983429f, 0.42653000354766846f, 0.25805699825286865f, 0.36924099922180176f, 0.340038001537323f, 0.4499100148677826f, 0.5138880014419556f, 0.2783010005950928f, 0.615218997001648f, 0.619388997554779f, 0.22329899668693542f, 0.6212739944458008f, 0.49001801013946533f, 0.20871099829673767f, 0.6547060012817383f, 0.3877120018005371f, 0.2587299942970276f, 0.7565820217132568f, 0.3530699908733368f, 0.47026699781417847f, 0.8147969841957092f, 0.3332749903202057f, 0.5911440253257751f, 0.288796991109848f, 0.11493200063705444f, 0.44190099835395813f, 0.313616007566452f, 0.08229490369558334f, 0.48377299308776855f, 0.35705000162124634f, 0.025180399417877197f, 0.5570499897003174f, 0.40331798791885376f, 0.19336099922657013f, 0.6810550093650818f, 0.45072001218795776f, 0.45166099071502686f, 0.8253499865531921f, 0.27013900876045227f, 0.36012598872184753f, 0.5223410129547119f, 0.0515609011054039f, 0.2102849930524826f, 0.1447799950838089f, 0.5275499820709229f, 0.5677899718284607f, 0.09326499700546265f, 0.9264400005340576f, 0.8343690037727356f, 0.04965920001268387f, 0.8627340197563171f, 0.5553900003433228f, 0.05350809916853905f, 0.808896005153656f, 0.35745900869369507f, 0.1601649969816208f, 0.7797250151634216f, 0.36214399337768555f, 0.523842990398407f, 0.7630550265312195f, 0.3648209869861603f, 0.7316589951515198f, 0.48453599214553833f, 0.34342798590660095f, 0.471466988325119f, 0.45723599195480347f, 0.26065701246261597f, 0.5250099897384644f, 0.40946200489997864f, 0.11580800265073776f, 0.6187099814414978f, 0.38767099380493164f, 0.2317180037498474f, 0.6598700284957886f, 0.3762730062007904f, 0.45193299651145935f, 0.6800140142440796f, 0.26639100909233093f, 0.46029600501060486f, 0.5837820172309875f, 0.14009399712085724f, 0.43334999680519104f, 0.46815401315689087f, 0.3927539885044098f, 0.6695060133934021f, 0.40574100613594055f, 0.6209800243377686f, 0.8280799984931946f, 0.3422519862651825f, 0.7025989890098572f, 0.521166980266571f, 0.27230098843574524f, 0.7290599942207336f, 0.3309679925441742f, 0.2556619942188263f, 0.6176270246505737f, 0.43255800008773804f, 0.3723050057888031f, 0.5539510250091553f, 0.49060899019241333f, 0.43895700573921204f, 0.680275022983551f, 0.5719249844551086f, 0.5010340213775635f, 0.6008570194244385f, 0.43901899456977844f, 0.5662469863891602f, 0.46187400817871094f, 0.20643499493598938f, 0.6803699731826782f, 0.37202298641204834f, 0.2700749933719635f, 0.6386849880218506f, 0.30182600021362305f, 0.45220500230789185f, 0.5346770286560059f, 0.2626419961452484f, 0.5604659914970398f, 0.645222008228302f, 0.22862699627876282f, 0.6564139723777771f, 0.7915269732475281f, 0.25795799493789673f, 0.7712209820747375f, 0.7182170152664185f, 0.3155199885368347f, 0.8217909932136536f, 0.6348440051078796f, 0.542464017868042f, 0.48694300651550293f, 0.4910930097103119f, 0.6492249965667725f, 0.3044779896736145f, 0.351159006357193f, 0.45552998781204224f, 0.5029720067977905f, 0.22076599299907684f, 0.3448469936847687f, 0.6163970232009888f, 0.1462559998035431f, 0.7455220222473145f, 0.6480900049209595f, 0.5108889937400818f, 0.6487299799919128f, 0.49847298860549927f, 0.5799919962882996f, 0.4793440103530884f, 0.23664399981498718f, 0.7009230256080627f, 0.3668079972267151f, 0.28286099433898926f, 0.6316230297088623f, 0.2770099937915802f, 0.4522959887981415f, 0.4862320125102997f, 0.2613919973373413f, 0.5938559770584106f, 0.6657029986381531f, 0.2581380009651184f, 0.7307689785957336f, 0.8993179798126221f, 0.2130260020494461f, 0.8051260113716125f, 0.8223749995231628f, 0.21369999647140503f, 0.8196939826011658f, 0.7323740124702454f, 0.48908498883247375f, 0.47553500533103943f, 0.5640239715576172f, 0.6226130127906799f, 0.2956480085849762f, 0.3829919993877411f, 0.4014979898929596f, 0.5264430046081543f, 0.17025400698184967f, 0.2751460075378418f, 0.6583260297775269f, 0.04868920147418976f, 0.045783400535583496f, 0.7876780033111572f, 0.6631749868392944f, 0.17993000149726868f, 0.7218999862670898f, 0.5943700075149536f, 0.41468799114227295f, 0.6067870259284973f, 0.4739609956741333f, 0.5122299790382385f, 0.732483983039856f, 0.5811709761619568f, 0.5548869967460632f, 0.9545029997825623f, 0.7794299721717834f, 0.45978298783302307f, 0.979777991771698f, 0.5660219788551331f, 0.34172001481056213f, 0.9722610116004944f, 0.28400400280952454f, 0.3709520101547241f, 0.6858919858932495f, 0.24234099686145782f, 0.3748539984226227f, 0.4084010124206543f, 0.1935330033302307f, 0.22678300738334656f, 0.18417799472808838f, 0.10185399651527405f, 0.24627000093460083f, 0.06391450017690659f, 0.17049099504947662f, 0.6846479773521423f, 0.20354799926280975f, 0.6399170160293579f, 0.9351500272750854f, 0.2833389937877655f, 0.9081599712371826f, 0.13851100206375122f, 0.7164250016212463f, 0.678164005279541f, 0.24585700035095215f, 0.668150007724762f, 0.6264550089836121f, 0.4337109923362732f, 0.583666980266571f, 0.5359640121459961f, 0.4930739998817444f, 0.6793140172958374f, 0.6308680176734924f, 0.5010409951210022f, 0.847012996673584f, 0.7999299764633179f, 0.4468570053577423f, 0.9140470027923584f, 0.5634790062904358f, 0.3823150098323822f, 0.9643020033836365f, 0.259442001581192f, 0.4342019855976105f, 0.690900981426239f, 0.24901999533176422f, 0.4583280086517334f, 0.4276759922504425f, 0.22507600486278534f, 0.31589001417160034f, 0.22551099956035614f, 0.119998998939991f, 0.3251209855079651f, 0.11261200159788132f, 0.16140200197696686f, 0.7135199904441833f, 0.22287499904632568f, 0.5690029859542847f, 0.9354619979858398f, 0.28588199615478516f, 0.8019180297851562f, 0.4166949987411499f, 0.502668023109436f, 0.7231320142745972f, 0.4436360001564026f, 0.5069000124931335f, 0.7227110266685486f, 0.4907810091972351f, 0.5143070220947266f, 0.7219730019569397f, 0.4356060028076172f, 0.5198060274124146f, 0.7799580097198486f, 0.3395020067691803f, 0.5245429873466492f, 0.8614320158958435f, 0.4080759882926941f, 0.7168530225753784f, 0.5558480024337769f, 0.5040969848632812f, 0.9404249787330627f, 0.18575499951839447f, 0.6239519715309143f, 0.7059270143508911f, 0.2690570056438446f, 0.7087500095367432f, 0.4855020046234131f, 0.319705992937088f, 0.5832120180130005f, 0.3495109975337982f, 0.17443400621414185f, 0.5616729855537415f, 0.25870299339294434f, 0.13413399457931519f, 0.800134003162384f, 0.2808539867401123f, 0.3562619984149933f, 0.9363970160484314f, 0.2935110032558441f, 0.4831930100917816f, 0.6948789954185486f, 0.28891000151634216f, 0.7681000232696533f, 0.641414999961853f, 0.3456510007381439f, 0.8189659714698792f, 0.5478519797325134f, 0.4449470043182373f, 0.9079819917678833f, 0.37813800573349f, 0.3602980077266693f, 0.9290480017662048f, 0.1779640018939972f, 0.20207299292087555f, 0.9229339957237244f, 0.36929601430892944f, 0.5196589827537537f, 0.5482180118560791f, 0.6258789896965027f, 0.9165470004081726f, 0.11206699907779694f, 0.8137019872665405f, 0.7209540009498596f, 0.2890940010547638f, 0.9591730237007141f, 0.5433290004730225f, 0.4143350124359131f, 0.8505330085754395f, 0.4735110104084015f, 0.2288690060377121f, 0.7982249855995178f, 0.40479499101638794f, 0.1068670004606247f, 0.8867480158805847f, 0.3388330042362213f, 0.1435209959745407f, 0.9373319745063782f, 0.30114099383354187f, 0.1644670069217682f, 0.8159430027008057f, 0.26124298572540283f, 0.8025789856910706f, 0.7525500059127808f, 0.33851900696754456f, 0.8164730072021484f, 0.6416140198707581f, 0.4737519919872284f, 0.8407869935035706f, 0.5584250092506409f, 0.485709011554718f, 0.7532600164413452f, 0.48633599281311035f, 0.4483569860458374f, 0.6209980249404907f, 0.45230400562286377f, 0.5815380215644836f, 0.46737900376319885f, 0.4246160089969635f, 0.7431420087814331f, 0.3102000057697296f, 0.5869200229644775f, 0.633903980255127f, 0.30153700709342957f, 0.7345529794692993f, 0.5453609824180603f, 0.29539600014686584f, 0.7941570281982422f, 0.5809980034828186f, 0.30438101291656494f, 0.792834997177124f, 0.56113600730896f, 0.29600998759269714f, 0.6391929984092712f, 0.40252700448036194f, 0.2442460060119629f, 0.5513989925384521f, 0.3118929862976074f, 0.2146659940481186f, 0.9370070099830627f, 0.2335759997367859f, 0.8370569944381714f, 0.8636860251426697f, 0.3313870131969452f, 0.8139790296554565f, 0.7353749871253967f, 0.5025569796562195f, 0.7735909819602966f, 0.7387120127677917f, 0.6111199855804443f, 0.577472984790802f, 0.7947070002555847f, 0.6946409940719604f, 0.3190619945526123f, 0.5353119969367981f, 0.64341801404953f, 0.3865399956703186f, 0.22335200011730194f, 0.5697370171546936f, 0.5083330273628235f, 0.3601379990577698f, 0.5468530058860779f, 0.31398099660873413f, 0.5099329948425293f, 0.5473939776420593f, 0.1764560043811798f, 0.7377820014953613f, 0.6884859800338745f, 0.3798930048942566f, 0.787443995475769f, 0.7174779772758484f, 0.4851529896259308f, 0.39163899421691895f, 0.46622100472450256f, 0.3449699878692627f, 0.16546499729156494f, 0.32264500856399536f, 0.2648650109767914f, 0.9666690230369568f, 0.2995069921016693f, 0.7764409780502319f, 0.8776519894599915f, 0.3399890065193176f, 0.7851809859275818f, 0.7218729853630066f, 0.4108330011367798f, 0.8004779815673828f, 0.7764049768447876f, 0.49051201343536377f, 0.6259179711341858f, 0.9150620102882385f, 0.573727011680603f, 0.37541601061820984f, 0.5782909989356995f, 0.5858240127563477f, 0.4747909903526306f, 0.16228100657463074f, 0.5860670208930969f, 0.6324769854545593f, 0.3084549903869629f, 0.5200639963150024f, 0.3812980055809021f, 0.4649589955806732f, 0.495633989572525f, 0.19862000644207f, 0.683447003364563f, 0.720641016960144f, 0.42694899439811707f, 0.7332959771156311f, 0.7921090126037598f, 0.5467569828033447f, 0.3615480065345764f, 0.47973400354385376f, 0.39526399970054626f, 0.1491210013628006f, 0.30123400688171387f, 0.308696985244751f, 0.9506310224533081f, 0.4122369885444641f, 0.6682760119438171f, 0.843034029006958f, 0.35645800828933716f, 0.743232011795044f, 0.6547380089759827f, 0.25884300470352173f, 0.874405026435852f, 0.7428020238876343f, 0.24689500033855438f, 0.7864800095558167f, 0.9414089918136597f, 0.269214004278183f, 0.610916018486023f, 0.6012539863586426f, 0.4684930145740509f, 0.6475859880447388f, 0.17130599915981293f, 0.6972650289535522f, 0.7196279764175415f, 0.34432101249694824f, 0.5234060287475586f, 0.4760519862174988f, 0.5098080039024353f, 0.4169769883155823f, 0.29133498668670654f, 0.6301320195198059f, 0.7151280045509338f, 0.45977500081062317f, 0.6547679901123047f, 0.825884997844696f, 0.5445910096168518f, 0.4401879906654358f, 0.46815699338912964f, 0.4203439950942993f, 0.3175710141658783f, 0.26374098658561707f, 0.34934601187705994f, 0.8135920166969299f, 0.4901770055294037f, 0.5917490124702454f, 0.7477700114250183f, 0.38305801153182983f, 0.7081090211868286f, 0.6325820088386536f, 0.1956000030040741f, 0.9117400050163269f, 0.7181000113487244f, 0.1590919941663742f, 0.844851016998291f, 0.883899986743927f, 0.18296299874782562f, 0.6697530150413513f, 0.6208639740943909f, 0.4452030062675476f, 0.6626129746437073f, 0.2863540053367615f, 0.7471719980239868f, 0.6834650039672852f, 0.40389201045036316f, 0.5500869750976562f, 0.5429580211639404f, 0.5150330066680908f, 0.42190301418304443f, 0.4308449923992157f, 0.5877879858016968f, 0.7071260213851929f, 0.489095002412796f, 0.6133440136909485f, 0.8059579730033875f, 0.5146669745445251f, 0.520904004573822f, 0.43880999088287354f, 0.45854198932647705f, 0.468080997467041f, 0.22901199758052826f, 0.4264709949493408f, 0.4345490038394928f, 0.4985339939594269f, 0.5784950256347656f, 0.531216025352478f, 0.4299220144748688f, 0.6866390109062195f, 0.7003830075263977f, 0.3098500072956085f, 0.8758909702301025f, 0.7112020254135132f, 0.3829140067100525f, 0.698839008808136f, 0.6586809754371643f, 0.5332329869270325f, 0.3752659857273102f, 0.6337649822235107f, 0.6099960207939148f, 0.36210399866104126f, 0.6134499907493591f, 0.674498975276947f, 0.40067699551582336f, 0.5108749866485596f, 0.6234470009803772f, 0.5541700124740601f, 0.44100698828697205f, 0.593995988368988f, 0.6639440059661865f, 0.5673859715461731f, 0.6941450238227844f, 0.5114009976387024f, 0.6461259722709656f, 0.6786230206489563f, 0.4292280077934265f, 0.6057689785957336f, 0.3739219903945923f, 0.5229759812355042f, 0.5827080011367798f, 0.1998080015182495f, 0.5765460133552551f, 0.05550739914178848f, 0.5068920254707336f, 0.5652409791946411f, 0.31466299295425415f, 0.4767859876155853f, 0.6651690006256104f, 0.768185019493103f, 0.42410001158714294f, 0.8400430083274841f, 0.7043039798736572f, 0.6067370176315308f, 0.5528280138969421f, 0.4334630072116852f, 0.8835030198097229f, 0.08077839761972427f, 0.6466670036315918f, 0.7747880220413208f, 0.06159469857811928f, 0.9405459761619568f, 0.601826012134552f, 0.11788900196552277f, 0.6178569793701172f, 0.6968079805374146f, 0.5653820037841797f, 0.36698099970817566f, 0.7660880088806152f, 0.897042989730835f, 0.5469840168952942f, 0.681164026260376f, 0.5337079763412476f, 0.6789069771766663f, 0.5512880086898804f, 0.3437890112400055f, 0.6906350255012512f, 0.3090350031852722f, 0.5874090194702148f, 0.6973360180854797f, 0.17060400545597076f, 0.7266209721565247f, 0.28161299228668213f, 0.32883501052856445f, 0.5892030000686646f, 0.373649001121521f, 0.3630729913711548f, 0.670091986656189f, 0.5347129702568054f, 0.42298999428749084f, 0.8116469979286194f, 0.5565940141677856f, 0.5510879755020142f, 0.5375570058822632f, 0.5228009819984436f, 0.7064589858055115f, 0.09720870107412338f, 0.538582980632782f, 0.7462499737739563f, 0.09717869758605957f, 0.5626270174980164f, 0.7667790055274963f, 0.17053499817848206f, 0.46951499581336975f, 0.6393499970436096f, 0.5623760223388672f, 0.41798099875450134f, 0.5332509875297546f, 0.8618710041046143f, 0.615917980670929f, 0.5551360249519348f, 0.6072919964790344f, 0.7522199749946594f, 0.5539219975471497f, 0.454692006111145f, 0.7344380021095276f, 0.49496400356292725f, 0.5570359826087952f, 0.7242770195007324f, 0.4612730145454407f, 0.6155189871788025f, 0.507718026638031f, 0.15077799558639526f, 0.613165020942688f, 0.43263599276542664f, 0.24936099350452423f, 0.6750149726867676f, 0.30124199390411377f, 0.42188000679016113f, 0.7832509875297546f, 0.4088839888572693f, 0.49543899297714233f, 0.522284984588623f, 0.6121389865875244f, 0.5294139981269836f, 0.1136389970779419f, 0.43049898743629456f, 0.7177129983901978f, 0.13276299834251404f, 0.18470899760723114f, 0.9317309856414795f, 0.22318099439144135f, 0.3211730122566223f, 0.5818920135498047f, 0.5593690276145935f, 0.468980997800827f, 0.30041399598121643f, 0.8266990184783936f, 0.6848520040512085f, 0.42910799384117126f, 0.6808760166168213f, 0.8255329728126526f, 0.5565569996833801f, 0.5655940175056458f, 0.7782419919967651f, 0.6808930039405823f, 0.5266630053520203f, 0.7512180209159851f, 0.751941978931427f, 0.5044159889221191f, 0.5830860137939453f, 0.09142609685659409f, 0.6211529970169067f, 0.45229798555374146f, 0.21145600080490112f, 0.6766560077667236f, 0.2234179973602295f, 0.4215089976787567f, 0.7737849950790405f, 0.3596470057964325f, 0.4768890142440796f, 0.5171949863433838f, 0.6419190168380737f, 0.47039899230003357f, 0.11911600083112717f, 0.3944709897041321f, 0.7081999778747559f, 0.1446239948272705f, 0.05873680114746094f, 0.9867150187492371f, 0.24073000252246857f, 0.27172601222991943f, 0.5627390146255493f, 0.5583670139312744f, 0.4859809875488281f, 0.22280199825763702f, 0.8149750232696533f, 0.707830011844635f, 0.3870989978313446f, 0.7054049968719482f, 0.8499709963798523f, 0.5574349761009216f, 0.6025620102882385f, 0.7928429841995239f, 0.7428690195083618f, 0.5165389776229858f, 0.7601979970932007f, 0.8488320112228394f, 0.46738201379776f}),
                            .dimensions = {2, 14, 13, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // input02_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.5970990061759949f, 0.7170640230178833f, 0.6969889998435974f, 0.37400999665260315f, 0.5877010226249695f, 0.5261300206184387f, 0.45982998609542847f, 0.8347070217132568f, 0.8258259892463684f, 0.1881919950246811f, 0.058183200657367706f, 0.4894869923591614f, 0.8974720239639282f, 0.9127089977264404f, 0.3382149934768677f, 0.11423300206661224f, 0.9753779768943787f, 0.1899150013923645f, 0.8858709931373596f, 0.012363100424408913f, 0.682108998298645f, 0.06375709921121597f, 0.02116600051522255f, 0.9904389977455139f, 0.4054659903049469f, 0.2451069951057434f, 0.23528499901294708f, 0.6705319881439209f, 0.7219460010528564f, 0.56113201379776f, 0.00936948973685503f, 0.052641499787569046f, 0.44719699025154114f, 0.7550650238990784f, 0.5975310206413269f, 0.19515100121498108f, 0.2697969973087311f, 0.5512080192565918f, 0.9540870189666748f, 0.22379499673843384f, 0.3978950083255768f, 0.9247890114784241f, 0.635591983795166f, 0.29778599739074707f, 0.023726800456643105f, 0.10101699829101562f, 0.49177101254463196f, 0.7159339785575867f, 0.5461130142211914f, 0.7824310064315796f, 0.8223419785499573f, 0.4546320140361786f, 0.5847259759902954f, 0.427172988653183f, 0.13006100058555603f, 0.6326299905776978f, 0.01302270032465458f, 0.46919500827789307f, 0.6897569894790649f, 0.025369999930262566f, 0.6305140256881714f, 0.9858930110931396f, 0.25782498717308044f, 0.08653680235147476f, 0.4650770127773285f, 0.9502390027046204f, 0.22172600030899048f, 0.34916600584983826f, 0.799219012260437f, 0.18139900267124176f, 0.31247299909591675f, 0.04026930034160614f, 0.5333859920501709f, 0.5811820030212402f, 0.6003819704055786f, 0.18610499799251556f, 0.322733998298645f, 0.46041399240493774f, 0.3700900077819824f, 0.45292899012565613f, 0.38766899704933167f, 0.8837850093841553f, 0.2912139892578125f, 0.40379199385643005f, 0.288796991109848f, 0.11493200063705444f, 0.44190099835395813f, 0.36945900321006775f, 0.008862020447850227f, 0.5779860019683838f, 0.45749199390411377f, 0.4885610044002533f, 0.8459640145301819f, 0.051560599356889725f, 0.2102849930524826f, 0.1447799950838089f, 0.9355409741401672f, 0.874222993850708f, 0.04910929873585701f, 0.8172299861907959f, 0.3561199903488159f, 0.056257400661706924f, 0.7630550265312195f, 0.3648209869861603f, 0.7316589951515198f, 0.7455220222473145f, 0.6480900049209595f, 0.5108889937400818f, 0.43094900250434875f, 0.16183599829673767f, 0.7354750037193298f, 0.2641820013523102f, 0.4765009880065918f, 0.46546098589897156f, 0.2581380009651184f, 0.7307689785957336f, 0.8993179798126221f, 0.17435899376869202f, 0.8688600063323975f, 0.7564240097999573f, 0.6857889890670776f, 0.2297070026397705f, 0.44377401471138f, 0.2751460075378418f, 0.6583260297775269f, 0.04868920147418976f, 0.045783400535583496f, 0.7876780033111572f, 0.6631749868392944f, 0.48176100850105286f, 0.5738980174064636f, 0.4395579993724823f, 0.5609800219535828f, 0.9862200021743774f, 0.8077520132064819f, 0.34172001481056213f, 0.9722620248794556f, 0.28400400280952454f, 0.3960070013999939f, 0.44043299555778503f, 0.20663000643253326f, 0.12101899832487106f, 0.02401909977197647f, 0.036369599401950836f, 0.9351500272750854f, 0.2833389937877655f, 0.9081599712371826f, 0.6948789954185486f, 0.28891000151634216f, 0.7681000232696533f, 0.521120011806488f, 0.4733169972896576f, 0.9334149956703186f, 0.14936800301074982f, 0.1794690042734146f, 0.922061026096344f, 0.6258789896965027f, 0.9165470004081726f, 0.11206699907779694f, 0.9746930003166199f, 0.5533030033111572f, 0.4408299922943115f, 0.7729330062866211f, 0.42364099621772766f, 0.09639369696378708f, 0.9373319745063782f, 0.30114099383354187f, 0.1644670069217682f, 0.9773610234260559f, 0.22435399889945984f, 0.848550021648407f, 0.728314995765686f, 0.5644869804382324f, 0.7334920167922974f, 0.9115960001945496f, 0.79442298412323f, 0.1754930019378662f, 0.1562650054693222f, 0.5119360089302063f, 0.5743770003318787f, 0.39449799060821533f, 0.522894024848938f, 0.09848739951848984f, 0.9217979907989502f, 0.8502029776573181f, 0.5966740250587463f, 0.03682040050625801f, 0.3262290060520172f, 0.2815980017185211f, 0.939939022064209f, 0.48739099502563477f, 0.5961670279502869f, 0.5499889850616455f, 0.09753979742527008f, 0.9832389950752258f, 0.993054986000061f, 0.06359480321407318f, 0.7499719858169556f, 0.17732200026512146f, 0.7713969945907593f, 0.7777280211448669f, 0.5318679809570312f, 0.31498000025749207f, 0.33478599786758423f, 0.6337890028953552f, 0.9592490196228027f, 0.5734559893608093f, 0.42987099289894104f, 0.23874600231647491f, 0.376446008682251f, 0.05550739914178848f, 0.5068920254707336f, 0.5652409791946411f, 0.8977620005607605f, 0.40904700756073f, 0.8900070190429688f, 0.394771009683609f, 0.9230409860610962f, 0.013342900201678276f, 0.9405459761619568f, 0.601826012134552f, 0.11788900196552277f, 0.341266006231308f, 0.778219997882843f, 0.9489480257034302f, 0.675557017326355f, 0.6205040216445923f, 0.2741830050945282f, 0.6973360180854797f, 0.17060400545597076f, 0.7266209721565247f, 0.5830860137939453f, 0.09142609685659409f, 0.6211529970169067f, 0.15802399814128876f, 0.4815239906311035f, 0.8015369772911072f, 0.6822429895401001f, 0.4694719910621643f, 0.06224739924073219f, 0.05873670056462288f, 0.9867150187492371f, 0.24073000252246857f, 0.4542880058288574f, 0.19933100044727325f, 0.830627977848053f, 0.8662930130958557f, 0.5044530034065247f, 0.6271399855613708f, 0.7601979970932007f, 0.8488320112228394f, 0.46738201379776f}),
                            .dimensions = {2, 6, 7, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // dummy9
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x6x7x3_to_2x14x13x3_float16_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x6x7x3_to_2x14x13x3_float16_all_inputs_as_internal", get_test_model_half_pixel_centers_2x6x7x3_to_2x14x13x3_float16_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x6x7x3_to_2x14x13x3_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.597099f, 0.717064f, 0.696989f, 0.37401f, 0.587701f, 0.52613f, 0.45983f, 0.834707f, 0.825826f, 0.188192f, 0.0581832f, 0.489487f, 0.897472f, 0.912709f, 0.338215f, 0.114233f, 0.975378f, 0.189915f, 0.885871f, 0.0123631f, 0.682109f, 0.0637571f, 0.021166f, 0.990439f, 0.405466f, 0.245107f, 0.235285f, 0.670532f, 0.721946f, 0.561132f, 0.00936949f, 0.0526415f, 0.447197f, 0.755065f, 0.597531f, 0.195151f, 0.269797f, 0.551208f, 0.954087f, 0.223795f, 0.397895f, 0.924789f, 0.635592f, 0.297786f, 0.0237268f, 0.101017f, 0.491771f, 0.715934f, 0.546113f, 0.782431f, 0.822342f, 0.454632f, 0.584726f, 0.427173f, 0.130061f, 0.63263f, 0.0130227f, 0.469195f, 0.689757f, 0.02537f, 0.630514f, 0.985893f, 0.257825f, 0.0865368f, 0.465077f, 0.950239f, 0.221726f, 0.349166f, 0.799219f, 0.181399f, 0.312473f, 0.0402693f, 0.533386f, 0.581182f, 0.600382f, 0.186105f, 0.322734f, 0.460414f, 0.37009f, 0.452929f, 0.387669f, 0.883785f, 0.291214f, 0.403792f, 0.288797f, 0.114932f, 0.441901f, 0.369459f, 0.00886202f, 0.577986f, 0.457492f, 0.488561f, 0.845964f, 0.0515606f, 0.210285f, 0.14478f, 0.935541f, 0.874223f, 0.0491093f, 0.81723f, 0.35612f, 0.0562574f, 0.763055f, 0.364821f, 0.731659f, 0.745522f, 0.64809f, 0.510889f, 0.430949f, 0.161836f, 0.735475f, 0.264182f, 0.476501f, 0.465461f, 0.258138f, 0.730769f, 0.899318f, 0.174359f, 0.86886f, 0.756424f, 0.685789f, 0.229707f, 0.443774f, 0.275146f, 0.658326f, 0.0486892f, 0.0457834f, 0.787678f, 0.663175f, 0.481761f, 0.573898f, 0.439558f, 0.56098f, 0.98622f, 0.807752f, 0.34172f, 0.972262f, 0.284004f, 0.396007f, 0.440433f, 0.20663f, 0.121019f, 0.0240191f, 0.0363696f, 0.93515f, 0.283339f, 0.90816f, 0.694879f, 0.28891f, 0.7681f, 0.52112f, 0.473317f, 0.933415f, 0.149368f, 0.179469f, 0.922061f, 0.625879f, 0.916547f, 0.112067f, 0.974693f, 0.553303f, 0.44083f, 0.772933f, 0.423641f, 0.0963937f, 0.937332f, 0.301141f, 0.164467f, 0.977361f, 0.224354f, 0.84855f, 0.728315f, 0.564487f, 0.733492f, 0.911596f, 0.794423f, 0.175493f, 0.156265f, 0.511936f, 0.574377f, 0.394498f, 0.522894f, 0.0984874f, 0.921798f, 0.850203f, 0.596674f, 0.0368204f, 0.326229f, 0.281598f, 0.939939f, 0.487391f, 0.596167f, 0.549989f, 0.0975398f, 0.983239f, 0.993055f, 0.0635948f, 0.749972f, 0.177322f, 0.771397f, 0.777728f, 0.531868f, 0.31498f, 0.334786f, 0.633789f, 0.959249f, 0.573456f, 0.429871f, 0.238746f, 0.376446f, 0.0555074f, 0.506892f, 0.565241f, 0.897762f, 0.409047f, 0.890007f, 0.394771f, 0.923041f, 0.0133429f, 0.940546f, 0.601826f, 0.117889f, 0.341266f, 0.77822f, 0.948948f, 0.675557f, 0.620504f, 0.274183f, 0.697336f, 0.170604f, 0.726621f, 0.583086f, 0.0914261f, 0.621153f, 0.158024f, 0.481524f, 0.801537f, 0.682243f, 0.469472f, 0.0622474f, 0.0587367f, 0.986715f, 0.24073f, 0.454288f, 0.199331f, 0.830628f, 0.866293f, 0.504453f, 0.62714f, 0.760198f, 0.848832f, 0.467382f}),
                            .dimensions = {2, 6, 7, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({13}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({14}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.597099f, 0.717064f, 0.696989f, 0.528456f, 0.67726f, 0.644417f, 0.408331f, 0.607603f, 0.552416f, 0.407018f, 0.682704f, 0.641398f, 0.453228f, 0.815707f, 0.802773f, 0.334458f, 0.476311f, 0.670593f, 0.188192f, 0.0581834f, 0.489487f, 0.570112f, 0.518313f, 0.408033f, 0.837223f, 0.91753f, 0.326807f, 0.415478f, 0.951275f, 0.246953f, 0.232946f, 0.827222f, 0.265637f, 0.648444f, 0.308675f, 0.530665f, 0.885871f, 0.0123631f, 0.682109f, 0.520907f, 0.61765f, 0.738911f, 0.477091f, 0.593376f, 0.660655f, 0.400412f, 0.550896f, 0.523708f, 0.42136f, 0.64639f, 0.601285f, 0.481359f, 0.797072f, 0.764672f, 0.338876f, 0.467272f, 0.647443f, 0.162646f, 0.0573917f, 0.483445f, 0.547367f, 0.493703f, 0.394239f, 0.820153f, 0.871307f, 0.316339f, 0.42133f, 0.896668f, 0.306273f, 0.2372f, 0.784422f, 0.363343f, 0.589802f, 0.32816f, 0.588256f, 0.791288f, 0.067439f, 0.716777f, 0.292332f, 0.319408f, 0.864675f, 0.322995f, 0.341724f, 0.70937f, 0.376654f, 0.380775f, 0.437585f, 0.464387f, 0.537448f, 0.480948f, 0.56575f, 0.741169f, 0.650369f, 0.352128f, 0.440154f, 0.577995f, 0.0860078f, 0.0550167f, 0.465321f, 0.479133f, 0.419873f, 0.352859f, 0.768945f, 0.732637f, 0.284935f, 0.438884f, 0.732846f, 0.484231f, 0.24996f, 0.656022f, 0.656461f, 0.413876f, 0.386614f, 0.76103f, 0.507542f, 0.232667f, 0.820783f, 0.0637571f, 0.021166f, 0.990439f, 0.168898f, 0.0900708f, 0.758084f, 0.352896f, 0.210654f, 0.351462f, 0.507414f, 0.428506f, 0.360611f, 0.650142f, 0.685266f, 0.536067f, 0.36538f, 0.413036f, 0.508547f, 0.00936967f, 0.0526416f, 0.447197f, 0.410898f, 0.346044f, 0.31148f, 0.717737f, 0.593968f, 0.253531f, 0.456438f, 0.569025f, 0.662189f, 0.262719f, 0.527622f, 0.94958f, 0.237949f, 0.445068f, 0.933804f, 0.223795f, 0.397895f, 0.924789f, 0.308829f, 0.139718f, 0.576134f, 0.298417f, 0.204672f, 0.534639f, 0.280194f, 0.318343f, 0.462024f, 0.406612f, 0.503531f, 0.530432f, 0.590885f, 0.717326f, 0.655248f, 0.424742f, 0.532242f, 0.564865f, 0.200196f, 0.280678f, 0.438615f, 0.35474f, 0.459391f, 0.265489f, 0.477056f, 0.612421f, 0.150863f, 0.406004f, 0.611351f, 0.387231f, 0.361845f, 0.616633f, 0.568817f, 0.384918f, 0.637799f, 0.613445f, 0.398103f, 0.649894f, 0.638947f, 0.553902f, 0.258269f, 0.161828f, 0.427935f, 0.319274f, 0.311195f, 0.207493f, 0.426031f, 0.572587f, 0.305809f, 0.578555f, 0.700253f, 0.531627f, 0.749386f, 0.77443f, 0.484104f, 0.651447f, 0.621183f, 0.391023f, 0.508714f, 0.430033f, 0.298582f, 0.572738f, 0.219499f, 0.236375f, 0.630874f, 0.048195f, 0.35557f, 0.653676f, 0.112274f, 0.460971f, 0.705645f, 0.188053f, 0.531887f, 0.83053f, 0.293086f, 0.572411f, 0.901893f, 0.353105f, 0.478719f, 0.345583f, 0.288445f, 0.373115f, 0.378028f, 0.427302f, 0.188308f, 0.434805f, 0.670301f, 0.253353f, 0.526846f, 0.685562f, 0.418339f, 0.632993f, 0.609726f, 0.458166f, 0.618414f, 0.542478f, 0.477133f, 0.583714f, 0.476661f, 0.29887f, 0.562377f, 0.295839f, 0.168751f, 0.550089f, 0.139929f, 0.327492f, 0.59209f, 0.133486f, 0.481187f, 0.647526f, 0.155137f, 0.622262f, 0.736545f, 0.247023f, 0.702877f, 0.787413f, 0.29953f, 0.24341f, 0.417279f, 0.685521f, 0.226126f, 0.408858f, 0.713184f, 0.19588f, 0.394121f, 0.761592f, 0.225071f, 0.41177f, 0.578614f, 0.278036f, 0.442374f, 0.30308f, 0.389579f, 0.509261f, 0.396261f, 0.510885f, 0.582195f, 0.550894f, 0.327381f, 0.490162f, 0.433345f, 0.187655f, 0.419685f, 0.328863f, 0.310593f, 0.478549f, 0.302783f, 0.461947f, 0.51584f, 0.296145f, 0.68434f, 0.499202f, 0.338108f, 0.811422f, 0.489694f, 0.362087f, 0.115431f, 0.415056f, 0.877619f, 0.154631f, 0.379824f, 0.843772f, 0.223231f, 0.318167f, 0.784538f, 0.234373f, 0.314811f, 0.532135f, 0.222532f, 0.334776f, 0.202464f, 0.333324f, 0.425583f, 0.33072f, 0.464554f, 0.528197f, 0.535296f, 0.372269f, 0.459986f, 0.463336f, 0.303998f, 0.404409f, 0.396939f, 0.379814f, 0.424645f, 0.363914f, 0.500517f, 0.417965f, 0.357295f, 0.73344f, 0.343997f, 0.41669f, 0.866538f, 0.30173f, 0.45063f, 0.202114f, 0.264994f, 0.65976f, 0.234124f, 0.231059f, 0.663772f, 0.29014f, 0.171674f, 0.670794f, 0.318846f, 0.254086f, 0.606595f, 0.336627f, 0.393218f, 0.513907f, 0.301731f, 0.392854f, 0.42653f, 0.258057f, 0.369241f, 0.340038f, 0.44991f, 0.513888f, 0.278301f, 0.615219f, 0.619389f, 0.223299f, 0.621274f, 0.490018f, 0.208711f, 0.654706f, 0.387712f, 0.25873f, 0.756582f, 0.35307f, 0.470267f, 0.814797f, 0.333275f, 0.591144f, 0.288797f, 0.114932f, 0.441901f, 0.313616f, 0.0822949f, 0.483773f, 0.35705f, 0.0251804f, 0.55705f, 0.403318f, 0.193361f, 0.681055f, 0.45072f, 0.451661f, 0.82535f, 0.270139f, 0.360126f, 0.522341f, 0.0515609f, 0.210285f, 0.14478f, 0.52755f, 0.56779f, 0.093265f, 0.92644f, 0.834369f, 0.0496592f, 0.862734f, 0.55539f, 0.0535081f, 0.808896f, 0.357459f, 0.160165f, 0.779725f, 0.362144f, 0.523843f, 0.763055f, 0.364821f, 0.731659f, 0.484536f, 0.343428f, 0.471467f, 0.457236f, 0.260657f, 0.52501f, 0.409462f, 0.115808f, 0.61871f, 0.387671f, 0.231718f, 0.65987f, 0.376273f, 0.451933f, 0.680014f, 0.266391f, 0.460296f, 0.583782f, 0.140094f, 0.43335f, 0.468154f, 0.392754f, 0.669506f, 0.405741f, 0.62098f, 0.82808f, 0.342252f, 0.702599f, 0.521167f, 0.272301f, 0.72906f, 0.330968f, 0.255662f, 0.617627f, 0.432558f, 0.372305f, 0.553951f, 0.490609f, 0.438957f, 0.680275f, 0.571925f, 0.501034f, 0.600857f, 0.439019f, 0.566247f, 0.461874f, 0.206435f, 0.68037f, 0.372023f, 0.270075f, 0.638685f, 0.301826f, 0.452205f, 0.534677f, 0.262642f, 0.560466f, 0.645222f, 0.228627f, 0.656414f, 0.791527f, 0.257958f, 0.771221f, 0.718217f, 0.31552f, 0.821791f, 0.634844f, 0.542464f, 0.486943f, 0.491093f, 0.649225f, 0.304478f, 0.351159f, 0.45553f, 0.502972f, 0.220766f, 0.344847f, 0.616397f, 0.146256f, 0.745522f, 0.64809f, 0.510889f, 0.64873f, 0.498473f, 0.579992f, 0.479344f, 0.236644f, 0.700923f, 0.366808f, 0.282861f, 0.631623f, 0.27701f, 0.452296f, 0.486232f, 0.261392f, 0.593856f, 0.665703f, 0.258138f, 0.730769f, 0.899318f, 0.213026f, 0.805126f, 0.822375f, 0.2137f, 0.819694f, 0.732374f, 0.489085f, 0.475535f, 0.564024f, 0.622613f, 0.295648f, 0.382992f, 0.401498f, 0.526443f, 0.170254f, 0.275146f, 0.658326f, 0.0486892f, 0.0457834f, 0.787678f, 0.663175f, 0.17993f, 0.7219f, 0.59437f, 0.414688f, 0.606787f, 0.473961f, 0.51223f, 0.732484f, 0.581171f, 0.554887f, 0.954503f, 0.77943f, 0.459783f, 0.979778f, 0.566022f, 0.34172f, 0.972261f, 0.284004f, 0.370952f, 0.685892f, 0.242341f, 0.374854f, 0.408401f, 0.193533f, 0.226783f, 0.184178f, 0.101854f, 0.24627f, 0.0639145f, 0.170491f, 0.684648f, 0.203548f, 0.639917f, 0.93515f, 0.283339f, 0.90816f, 0.138511f, 0.716425f, 0.678164f, 0.245857f, 0.66815f, 0.626455f, 0.433711f, 0.583667f, 0.535964f, 0.493074f, 0.679314f, 0.630868f, 0.501041f, 0.847013f, 0.79993f, 0.446857f, 0.914047f, 0.563479f, 0.382315f, 0.964302f, 0.259442f, 0.434202f, 0.690901f, 0.24902f, 0.458328f, 0.427676f, 0.225076f, 0.31589f, 0.225511f, 0.119999f, 0.325121f, 0.112612f, 0.161402f, 0.71352f, 0.222875f, 0.569003f, 0.935462f, 0.285882f, 0.801918f, 0.416695f, 0.502668f, 0.723132f, 0.443636f, 0.5069f, 0.722711f, 0.490781f, 0.514307f, 0.721973f, 0.435606f, 0.519806f, 0.779958f, 0.339502f, 0.524543f, 0.861432f, 0.408076f, 0.716853f, 0.555848f, 0.504097f, 0.940425f, 0.185755f, 0.623952f, 0.705927f, 0.269057f, 0.70875f, 0.485502f, 0.319706f, 0.583212f, 0.349511f, 0.174434f, 0.561673f, 0.258703f, 0.134134f, 0.800134f, 0.280854f, 0.356262f, 0.936397f, 0.293511f, 0.483193f, 0.694879f, 0.28891f, 0.7681f, 0.641415f, 0.345651f, 0.818966f, 0.547852f, 0.444947f, 0.907982f, 0.378138f, 0.360298f, 0.929048f, 0.177964f, 0.202073f, 0.922934f, 0.369296f, 0.519659f, 0.548218f, 0.625879f, 0.916547f, 0.112067f, 0.813702f, 0.720954f, 0.289094f, 0.959173f, 0.543329f, 0.414335f, 0.850533f, 0.473511f, 0.228869f, 0.798225f, 0.404795f, 0.106867f, 0.886748f, 0.338833f, 0.143521f, 0.937332f, 0.301141f, 0.164467f, 0.815943f, 0.261243f, 0.802579f, 0.75255f, 0.338519f, 0.816473f, 0.641614f, 0.473752f, 0.840787f, 0.558425f, 0.485709f, 0.75326f, 0.486336f, 0.448357f, 0.620998f, 0.452304f, 0.581538f, 0.467379f, 0.424616f, 0.743142f, 0.3102f, 0.58692f, 0.633904f, 0.301537f, 0.734553f, 0.545361f, 0.295396f, 0.794157f, 0.580998f, 0.304381f, 0.792835f, 0.561136f, 0.29601f, 0.639193f, 0.402527f, 0.244246f, 0.551399f, 0.311893f, 0.214666f, 0.937007f, 0.233576f, 0.837057f, 0.863686f, 0.331387f, 0.813979f, 0.735375f, 0.502557f, 0.773591f, 0.738712f, 0.61112f, 0.577473f, 0.794707f, 0.694641f, 0.319062f, 0.535312f, 0.643418f, 0.38654f, 0.223352f, 0.569737f, 0.508333f, 0.360138f, 0.546853f, 0.313981f, 0.509933f, 0.547394f, 0.176456f, 0.737782f, 0.688486f, 0.379893f, 0.787444f, 0.717478f, 0.485153f, 0.391639f, 0.466221f, 0.34497f, 0.165465f, 0.322645f, 0.264865f, 0.966669f, 0.299507f, 0.776441f, 0.877652f, 0.339989f, 0.785181f, 0.721873f, 0.410833f, 0.800478f, 0.776405f, 0.490512f, 0.625918f, 0.915062f, 0.573727f, 0.375416f, 0.578291f, 0.585824f, 0.474791f, 0.162281f, 0.586067f, 0.632477f, 0.308455f, 0.520064f, 0.381298f, 0.464959f, 0.495634f, 0.19862f, 0.683447f, 0.720641f, 0.426949f, 0.733296f, 0.792109f, 0.546757f, 0.361548f, 0.479734f, 0.395264f, 0.149121f, 0.301234f, 0.308697f, 0.950631f, 0.412237f, 0.668276f, 0.843034f, 0.356458f, 0.743232f, 0.654738f, 0.258843f, 0.874405f, 0.742802f, 0.246895f, 0.78648f, 0.941409f, 0.269214f, 0.610916f, 0.601254f, 0.468493f, 0.647586f, 0.171306f, 0.697265f, 0.719628f, 0.344321f, 0.523406f, 0.476052f, 0.509808f, 0.416977f, 0.291335f, 0.630132f, 0.715128f, 0.459775f, 0.654768f, 0.825885f, 0.544591f, 0.440188f, 0.468157f, 0.420344f, 0.317571f, 0.263741f, 0.349346f, 0.813592f, 0.490177f, 0.591749f, 0.74777f, 0.383058f, 0.708109f, 0.632582f, 0.1956f, 0.91174f, 0.7181f, 0.159092f, 0.844851f, 0.8839f, 0.182963f, 0.669753f, 0.620864f, 0.445203f, 0.662613f, 0.286354f, 0.747172f, 0.683465f, 0.403892f, 0.550087f, 0.542958f, 0.515033f, 0.421903f, 0.430845f, 0.587788f, 0.707126f, 0.489095f, 0.613344f, 0.805958f, 0.514667f, 0.520904f, 0.43881f, 0.458542f, 0.468081f, 0.229012f, 0.426471f, 0.434549f, 0.498534f, 0.578495f, 0.531216f, 0.429922f, 0.686639f, 0.700383f, 0.30985f, 0.875891f, 0.711202f, 0.382914f, 0.698839f, 0.658681f, 0.533233f, 0.375266f, 0.633765f, 0.609996f, 0.362104f, 0.61345f, 0.674499f, 0.400677f, 0.510875f, 0.623447f, 0.55417f, 0.441007f, 0.593996f, 0.663944f, 0.567386f, 0.694145f, 0.511401f, 0.646126f, 0.678623f, 0.429228f, 0.605769f, 0.373922f, 0.522976f, 0.582708f, 0.199808f, 0.576546f, 0.0555074f, 0.506892f, 0.565241f, 0.314663f, 0.476786f, 0.665169f, 0.768185f, 0.4241f, 0.840043f, 0.704304f, 0.606737f, 0.552828f, 0.433463f, 0.883503f, 0.0807784f, 0.646667f, 0.774788f, 0.0615947f, 0.940546f, 0.601826f, 0.117889f, 0.617857f, 0.696808f, 0.565382f, 0.366981f, 0.766088f, 0.897043f, 0.546984f, 0.681164f, 0.533708f, 0.678907f, 0.551288f, 0.343789f, 0.690635f, 0.309035f, 0.587409f, 0.697336f, 0.170604f, 0.726621f, 0.281613f, 0.328835f, 0.589203f, 0.373649f, 0.363073f, 0.670092f, 0.534713f, 0.42299f, 0.811647f, 0.556594f, 0.551088f, 0.537557f, 0.522801f, 0.706459f, 0.0972087f, 0.538583f, 0.74625f, 0.0971787f, 0.562627f, 0.766779f, 0.170535f, 0.469515f, 0.63935f, 0.562376f, 0.417981f, 0.533251f, 0.861871f, 0.615918f, 0.555136f, 0.607292f, 0.75222f, 0.553922f, 0.454692f, 0.734438f, 0.494964f, 0.557036f, 0.724277f, 0.461273f, 0.615519f, 0.507718f, 0.150778f, 0.613165f, 0.432636f, 0.249361f, 0.675015f, 0.301242f, 0.42188f, 0.783251f, 0.408884f, 0.495439f, 0.522285f, 0.612139f, 0.529414f, 0.113639f, 0.430499f, 0.717713f, 0.132763f, 0.184709f, 0.931731f, 0.223181f, 0.321173f, 0.581892f, 0.559369f, 0.468981f, 0.300414f, 0.826699f, 0.684852f, 0.429108f, 0.680876f, 0.825533f, 0.556557f, 0.565594f, 0.778242f, 0.680893f, 0.526663f, 0.751218f, 0.751942f, 0.504416f, 0.583086f, 0.0914261f, 0.621153f, 0.452298f, 0.211456f, 0.676656f, 0.223418f, 0.421509f, 0.773785f, 0.359647f, 0.476889f, 0.517195f, 0.641919f, 0.470399f, 0.119116f, 0.394471f, 0.7082f, 0.144624f, 0.0587368f, 0.986715f, 0.24073f, 0.271726f, 0.562739f, 0.558367f, 0.485981f, 0.222802f, 0.814975f, 0.70783f, 0.387099f, 0.705405f, 0.849971f, 0.557435f, 0.602562f, 0.792843f, 0.742869f, 0.516539f, 0.760198f, 0.848832f, 0.467382f}),
                            .dimensions = {2, 14, 13, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x6x7x3_to_2x14x13x3_relaxed = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x6x7x3_to_2x14x13x3_relaxed", get_test_model_half_pixel_centers_2x6x7x3_to_2x14x13x3_relaxed());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_half_pixel_centers_2x6x7x3_to_2x14x13x3_relaxed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {2, 6, 7, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({13}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({14}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output02
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.597099f, 0.717064f, 0.696989f, 0.528456f, 0.67726f, 0.644417f, 0.408331f, 0.607603f, 0.552416f, 0.407018f, 0.682704f, 0.641398f, 0.453228f, 0.815707f, 0.802773f, 0.334458f, 0.476311f, 0.670593f, 0.188192f, 0.0581834f, 0.489487f, 0.570112f, 0.518313f, 0.408033f, 0.837223f, 0.91753f, 0.326807f, 0.415478f, 0.951275f, 0.246953f, 0.232946f, 0.827222f, 0.265637f, 0.648444f, 0.308675f, 0.530665f, 0.885871f, 0.0123631f, 0.682109f, 0.520907f, 0.61765f, 0.738911f, 0.477091f, 0.593376f, 0.660655f, 0.400412f, 0.550896f, 0.523708f, 0.42136f, 0.64639f, 0.601285f, 0.481359f, 0.797072f, 0.764672f, 0.338876f, 0.467272f, 0.647443f, 0.162646f, 0.0573917f, 0.483445f, 0.547367f, 0.493703f, 0.394239f, 0.820153f, 0.871307f, 0.316339f, 0.42133f, 0.896668f, 0.306273f, 0.2372f, 0.784422f, 0.363343f, 0.589802f, 0.32816f, 0.588256f, 0.791288f, 0.067439f, 0.716777f, 0.292332f, 0.319408f, 0.864675f, 0.322995f, 0.341724f, 0.70937f, 0.376654f, 0.380775f, 0.437585f, 0.464387f, 0.537448f, 0.480948f, 0.56575f, 0.741169f, 0.650369f, 0.352128f, 0.440154f, 0.577995f, 0.0860078f, 0.0550167f, 0.465321f, 0.479133f, 0.419873f, 0.352859f, 0.768945f, 0.732637f, 0.284935f, 0.438884f, 0.732846f, 0.484231f, 0.24996f, 0.656022f, 0.656461f, 0.413876f, 0.386614f, 0.76103f, 0.507542f, 0.232667f, 0.820783f, 0.0637571f, 0.021166f, 0.990439f, 0.168898f, 0.0900708f, 0.758084f, 0.352896f, 0.210654f, 0.351462f, 0.507414f, 0.428506f, 0.360611f, 0.650142f, 0.685266f, 0.536067f, 0.36538f, 0.413036f, 0.508547f, 0.00936967f, 0.0526416f, 0.447197f, 0.410898f, 0.346044f, 0.31148f, 0.717737f, 0.593968f, 0.253531f, 0.456438f, 0.569025f, 0.662189f, 0.262719f, 0.527622f, 0.94958f, 0.237949f, 0.445068f, 0.933804f, 0.223795f, 0.397895f, 0.924789f, 0.308829f, 0.139718f, 0.576134f, 0.298417f, 0.204672f, 0.534639f, 0.280194f, 0.318343f, 0.462024f, 0.406612f, 0.503531f, 0.530432f, 0.590885f, 0.717326f, 0.655248f, 0.424742f, 0.532242f, 0.564865f, 0.200196f, 0.280678f, 0.438615f, 0.35474f, 0.459391f, 0.265489f, 0.477056f, 0.612421f, 0.150863f, 0.406004f, 0.611351f, 0.387231f, 0.361845f, 0.616633f, 0.568817f, 0.384918f, 0.637799f, 0.613445f, 0.398103f, 0.649894f, 0.638947f, 0.553902f, 0.258269f, 0.161828f, 0.427935f, 0.319274f, 0.311195f, 0.207493f, 0.426031f, 0.572587f, 0.305809f, 0.578555f, 0.700253f, 0.531627f, 0.749386f, 0.77443f, 0.484104f, 0.651447f, 0.621183f, 0.391023f, 0.508714f, 0.430033f, 0.298582f, 0.572738f, 0.219499f, 0.236375f, 0.630874f, 0.048195f, 0.35557f, 0.653676f, 0.112274f, 0.460971f, 0.705645f, 0.188053f, 0.531887f, 0.83053f, 0.293086f, 0.572411f, 0.901893f, 0.353105f, 0.478719f, 0.345583f, 0.288445f, 0.373115f, 0.378028f, 0.427302f, 0.188308f, 0.434805f, 0.670301f, 0.253353f, 0.526846f, 0.685562f, 0.418339f, 0.632993f, 0.609726f, 0.458166f, 0.618414f, 0.542478f, 0.477133f, 0.583714f, 0.476661f, 0.29887f, 0.562377f, 0.295839f, 0.168751f, 0.550089f, 0.139929f, 0.327492f, 0.59209f, 0.133486f, 0.481187f, 0.647526f, 0.155137f, 0.622262f, 0.736545f, 0.247023f, 0.702877f, 0.787413f, 0.29953f, 0.24341f, 0.417279f, 0.685521f, 0.226126f, 0.408858f, 0.713184f, 0.19588f, 0.394121f, 0.761592f, 0.225071f, 0.41177f, 0.578614f, 0.278036f, 0.442374f, 0.30308f, 0.389579f, 0.509261f, 0.396261f, 0.510885f, 0.582195f, 0.550894f, 0.327381f, 0.490162f, 0.433345f, 0.187655f, 0.419685f, 0.328863f, 0.310593f, 0.478549f, 0.302783f, 0.461947f, 0.51584f, 0.296145f, 0.68434f, 0.499202f, 0.338108f, 0.811422f, 0.489694f, 0.362087f, 0.115431f, 0.415056f, 0.877619f, 0.154631f, 0.379824f, 0.843772f, 0.223231f, 0.318167f, 0.784538f, 0.234373f, 0.314811f, 0.532135f, 0.222532f, 0.334776f, 0.202464f, 0.333324f, 0.425583f, 0.33072f, 0.464554f, 0.528197f, 0.535296f, 0.372269f, 0.459986f, 0.463336f, 0.303998f, 0.404409f, 0.396939f, 0.379814f, 0.424645f, 0.363914f, 0.500517f, 0.417965f, 0.357295f, 0.73344f, 0.343997f, 0.41669f, 0.866538f, 0.30173f, 0.45063f, 0.202114f, 0.264994f, 0.65976f, 0.234124f, 0.231059f, 0.663772f, 0.29014f, 0.171674f, 0.670794f, 0.318846f, 0.254086f, 0.606595f, 0.336627f, 0.393218f, 0.513907f, 0.301731f, 0.392854f, 0.42653f, 0.258057f, 0.369241f, 0.340038f, 0.44991f, 0.513888f, 0.278301f, 0.615219f, 0.619389f, 0.223299f, 0.621274f, 0.490018f, 0.208711f, 0.654706f, 0.387712f, 0.25873f, 0.756582f, 0.35307f, 0.470267f, 0.814797f, 0.333275f, 0.591144f, 0.288797f, 0.114932f, 0.441901f, 0.313616f, 0.0822949f, 0.483773f, 0.35705f, 0.0251804f, 0.55705f, 0.403318f, 0.193361f, 0.681055f, 0.45072f, 0.451661f, 0.82535f, 0.270139f, 0.360126f, 0.522341f, 0.0515609f, 0.210285f, 0.14478f, 0.52755f, 0.56779f, 0.093265f, 0.92644f, 0.834369f, 0.0496592f, 0.862734f, 0.55539f, 0.0535081f, 0.808896f, 0.357459f, 0.160165f, 0.779725f, 0.362144f, 0.523843f, 0.763055f, 0.364821f, 0.731659f, 0.484536f, 0.343428f, 0.471467f, 0.457236f, 0.260657f, 0.52501f, 0.409462f, 0.115808f, 0.61871f, 0.387671f, 0.231718f, 0.65987f, 0.376273f, 0.451933f, 0.680014f, 0.266391f, 0.460296f, 0.583782f, 0.140094f, 0.43335f, 0.468154f, 0.392754f, 0.669506f, 0.405741f, 0.62098f, 0.82808f, 0.342252f, 0.702599f, 0.521167f, 0.272301f, 0.72906f, 0.330968f, 0.255662f, 0.617627f, 0.432558f, 0.372305f, 0.553951f, 0.490609f, 0.438957f, 0.680275f, 0.571925f, 0.501034f, 0.600857f, 0.439019f, 0.566247f, 0.461874f, 0.206435f, 0.68037f, 0.372023f, 0.270075f, 0.638685f, 0.301826f, 0.452205f, 0.534677f, 0.262642f, 0.560466f, 0.645222f, 0.228627f, 0.656414f, 0.791527f, 0.257958f, 0.771221f, 0.718217f, 0.31552f, 0.821791f, 0.634844f, 0.542464f, 0.486943f, 0.491093f, 0.649225f, 0.304478f, 0.351159f, 0.45553f, 0.502972f, 0.220766f, 0.344847f, 0.616397f, 0.146256f, 0.745522f, 0.64809f, 0.510889f, 0.64873f, 0.498473f, 0.579992f, 0.479344f, 0.236644f, 0.700923f, 0.366808f, 0.282861f, 0.631623f, 0.27701f, 0.452296f, 0.486232f, 0.261392f, 0.593856f, 0.665703f, 0.258138f, 0.730769f, 0.899318f, 0.213026f, 0.805126f, 0.822375f, 0.2137f, 0.819694f, 0.732374f, 0.489085f, 0.475535f, 0.564024f, 0.622613f, 0.295648f, 0.382992f, 0.401498f, 0.526443f, 0.170254f, 0.275146f, 0.658326f, 0.0486892f, 0.0457834f, 0.787678f, 0.663175f, 0.17993f, 0.7219f, 0.59437f, 0.414688f, 0.606787f, 0.473961f, 0.51223f, 0.732484f, 0.581171f, 0.554887f, 0.954503f, 0.77943f, 0.459783f, 0.979778f, 0.566022f, 0.34172f, 0.972261f, 0.284004f, 0.370952f, 0.685892f, 0.242341f, 0.374854f, 0.408401f, 0.193533f, 0.226783f, 0.184178f, 0.101854f, 0.24627f, 0.0639145f, 0.170491f, 0.684648f, 0.203548f, 0.639917f, 0.93515f, 0.283339f, 0.90816f, 0.138511f, 0.716425f, 0.678164f, 0.245857f, 0.66815f, 0.626455f, 0.433711f, 0.583667f, 0.535964f, 0.493074f, 0.679314f, 0.630868f, 0.501041f, 0.847013f, 0.79993f, 0.446857f, 0.914047f, 0.563479f, 0.382315f, 0.964302f, 0.259442f, 0.434202f, 0.690901f, 0.24902f, 0.458328f, 0.427676f, 0.225076f, 0.31589f, 0.225511f, 0.119999f, 0.325121f, 0.112612f, 0.161402f, 0.71352f, 0.222875f, 0.569003f, 0.935462f, 0.285882f, 0.801918f, 0.416695f, 0.502668f, 0.723132f, 0.443636f, 0.5069f, 0.722711f, 0.490781f, 0.514307f, 0.721973f, 0.435606f, 0.519806f, 0.779958f, 0.339502f, 0.524543f, 0.861432f, 0.408076f, 0.716853f, 0.555848f, 0.504097f, 0.940425f, 0.185755f, 0.623952f, 0.705927f, 0.269057f, 0.70875f, 0.485502f, 0.319706f, 0.583212f, 0.349511f, 0.174434f, 0.561673f, 0.258703f, 0.134134f, 0.800134f, 0.280854f, 0.356262f, 0.936397f, 0.293511f, 0.483193f, 0.694879f, 0.28891f, 0.7681f, 0.641415f, 0.345651f, 0.818966f, 0.547852f, 0.444947f, 0.907982f, 0.378138f, 0.360298f, 0.929048f, 0.177964f, 0.202073f, 0.922934f, 0.369296f, 0.519659f, 0.548218f, 0.625879f, 0.916547f, 0.112067f, 0.813702f, 0.720954f, 0.289094f, 0.959173f, 0.543329f, 0.414335f, 0.850533f, 0.473511f, 0.228869f, 0.798225f, 0.404795f, 0.106867f, 0.886748f, 0.338833f, 0.143521f, 0.937332f, 0.301141f, 0.164467f, 0.815943f, 0.261243f, 0.802579f, 0.75255f, 0.338519f, 0.816473f, 0.641614f, 0.473752f, 0.840787f, 0.558425f, 0.485709f, 0.75326f, 0.486336f, 0.448357f, 0.620998f, 0.452304f, 0.581538f, 0.467379f, 0.424616f, 0.743142f, 0.3102f, 0.58692f, 0.633904f, 0.301537f, 0.734553f, 0.545361f, 0.295396f, 0.794157f, 0.580998f, 0.304381f, 0.792835f, 0.561136f, 0.29601f, 0.639193f, 0.402527f, 0.244246f, 0.551399f, 0.311893f, 0.214666f, 0.937007f, 0.233576f, 0.837057f, 0.863686f, 0.331387f, 0.813979f, 0.735375f, 0.502557f, 0.773591f, 0.738712f, 0.61112f, 0.577473f, 0.794707f, 0.694641f, 0.319062f, 0.535312f, 0.643418f, 0.38654f, 0.223352f, 0.569737f, 0.508333f, 0.360138f, 0.546853f, 0.313981f, 0.509933f, 0.547394f, 0.176456f, 0.737782f, 0.688486f, 0.379893f, 0.787444f, 0.717478f, 0.485153f, 0.391639f, 0.466221f, 0.34497f, 0.165465f, 0.322645f, 0.264865f, 0.966669f, 0.299507f, 0.776441f, 0.877652f, 0.339989f, 0.785181f, 0.721873f, 0.410833f, 0.800478f, 0.776405f, 0.490512f, 0.625918f, 0.915062f, 0.573727f, 0.375416f, 0.578291f, 0.585824f, 0.474791f, 0.162281f, 0.586067f, 0.632477f, 0.308455f, 0.520064f, 0.381298f, 0.464959f, 0.495634f, 0.19862f, 0.683447f, 0.720641f, 0.426949f, 0.733296f, 0.792109f, 0.546757f, 0.361548f, 0.479734f, 0.395264f, 0.149121f, 0.301234f, 0.308697f, 0.950631f, 0.412237f, 0.668276f, 0.843034f, 0.356458f, 0.743232f, 0.654738f, 0.258843f, 0.874405f, 0.742802f, 0.246895f, 0.78648f, 0.941409f, 0.269214f, 0.610916f, 0.601254f, 0.468493f, 0.647586f, 0.171306f, 0.697265f, 0.719628f, 0.344321f, 0.523406f, 0.476052f, 0.509808f, 0.416977f, 0.291335f, 0.630132f, 0.715128f, 0.459775f, 0.654768f, 0.825885f, 0.544591f, 0.440188f, 0.468157f, 0.420344f, 0.317571f, 0.263741f, 0.349346f, 0.813592f, 0.490177f, 0.591749f, 0.74777f, 0.383058f, 0.708109f, 0.632582f, 0.1956f, 0.91174f, 0.7181f, 0.159092f, 0.844851f, 0.8839f, 0.182963f, 0.669753f, 0.620864f, 0.445203f, 0.662613f, 0.286354f, 0.747172f, 0.683465f, 0.403892f, 0.550087f, 0.542958f, 0.515033f, 0.421903f, 0.430845f, 0.587788f, 0.707126f, 0.489095f, 0.613344f, 0.805958f, 0.514667f, 0.520904f, 0.43881f, 0.458542f, 0.468081f, 0.229012f, 0.426471f, 0.434549f, 0.498534f, 0.578495f, 0.531216f, 0.429922f, 0.686639f, 0.700383f, 0.30985f, 0.875891f, 0.711202f, 0.382914f, 0.698839f, 0.658681f, 0.533233f, 0.375266f, 0.633765f, 0.609996f, 0.362104f, 0.61345f, 0.674499f, 0.400677f, 0.510875f, 0.623447f, 0.55417f, 0.441007f, 0.593996f, 0.663944f, 0.567386f, 0.694145f, 0.511401f, 0.646126f, 0.678623f, 0.429228f, 0.605769f, 0.373922f, 0.522976f, 0.582708f, 0.199808f, 0.576546f, 0.0555074f, 0.506892f, 0.565241f, 0.314663f, 0.476786f, 0.665169f, 0.768185f, 0.4241f, 0.840043f, 0.704304f, 0.606737f, 0.552828f, 0.433463f, 0.883503f, 0.0807784f, 0.646667f, 0.774788f, 0.0615947f, 0.940546f, 0.601826f, 0.117889f, 0.617857f, 0.696808f, 0.565382f, 0.366981f, 0.766088f, 0.897043f, 0.546984f, 0.681164f, 0.533708f, 0.678907f, 0.551288f, 0.343789f, 0.690635f, 0.309035f, 0.587409f, 0.697336f, 0.170604f, 0.726621f, 0.281613f, 0.328835f, 0.589203f, 0.373649f, 0.363073f, 0.670092f, 0.534713f, 0.42299f, 0.811647f, 0.556594f, 0.551088f, 0.537557f, 0.522801f, 0.706459f, 0.0972087f, 0.538583f, 0.74625f, 0.0971787f, 0.562627f, 0.766779f, 0.170535f, 0.469515f, 0.63935f, 0.562376f, 0.417981f, 0.533251f, 0.861871f, 0.615918f, 0.555136f, 0.607292f, 0.75222f, 0.553922f, 0.454692f, 0.734438f, 0.494964f, 0.557036f, 0.724277f, 0.461273f, 0.615519f, 0.507718f, 0.150778f, 0.613165f, 0.432636f, 0.249361f, 0.675015f, 0.301242f, 0.42188f, 0.783251f, 0.408884f, 0.495439f, 0.522285f, 0.612139f, 0.529414f, 0.113639f, 0.430499f, 0.717713f, 0.132763f, 0.184709f, 0.931731f, 0.223181f, 0.321173f, 0.581892f, 0.559369f, 0.468981f, 0.300414f, 0.826699f, 0.684852f, 0.429108f, 0.680876f, 0.825533f, 0.556557f, 0.565594f, 0.778242f, 0.680893f, 0.526663f, 0.751218f, 0.751942f, 0.504416f, 0.583086f, 0.0914261f, 0.621153f, 0.452298f, 0.211456f, 0.676656f, 0.223418f, 0.421509f, 0.773785f, 0.359647f, 0.476889f, 0.517195f, 0.641919f, 0.470399f, 0.119116f, 0.394471f, 0.7082f, 0.144624f, 0.0587368f, 0.986715f, 0.24073f, 0.271726f, 0.562739f, 0.558367f, 0.485981f, 0.222802f, 0.814975f, 0.70783f, 0.387099f, 0.705405f, 0.849971f, 0.557435f, 0.602562f, 0.792843f, 0.742869f, 0.516539f, 0.760198f, 0.848832f, 0.467382f}),
                            .dimensions = {2, 14, 13, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input02_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.597099f, 0.717064f, 0.696989f, 0.37401f, 0.587701f, 0.52613f, 0.45983f, 0.834707f, 0.825826f, 0.188192f, 0.0581832f, 0.489487f, 0.897472f, 0.912709f, 0.338215f, 0.114233f, 0.975378f, 0.189915f, 0.885871f, 0.0123631f, 0.682109f, 0.0637571f, 0.021166f, 0.990439f, 0.405466f, 0.245107f, 0.235285f, 0.670532f, 0.721946f, 0.561132f, 0.00936949f, 0.0526415f, 0.447197f, 0.755065f, 0.597531f, 0.195151f, 0.269797f, 0.551208f, 0.954087f, 0.223795f, 0.397895f, 0.924789f, 0.635592f, 0.297786f, 0.0237268f, 0.101017f, 0.491771f, 0.715934f, 0.546113f, 0.782431f, 0.822342f, 0.454632f, 0.584726f, 0.427173f, 0.130061f, 0.63263f, 0.0130227f, 0.469195f, 0.689757f, 0.02537f, 0.630514f, 0.985893f, 0.257825f, 0.0865368f, 0.465077f, 0.950239f, 0.221726f, 0.349166f, 0.799219f, 0.181399f, 0.312473f, 0.0402693f, 0.533386f, 0.581182f, 0.600382f, 0.186105f, 0.322734f, 0.460414f, 0.37009f, 0.452929f, 0.387669f, 0.883785f, 0.291214f, 0.403792f, 0.288797f, 0.114932f, 0.441901f, 0.369459f, 0.00886202f, 0.577986f, 0.457492f, 0.488561f, 0.845964f, 0.0515606f, 0.210285f, 0.14478f, 0.935541f, 0.874223f, 0.0491093f, 0.81723f, 0.35612f, 0.0562574f, 0.763055f, 0.364821f, 0.731659f, 0.745522f, 0.64809f, 0.510889f, 0.430949f, 0.161836f, 0.735475f, 0.264182f, 0.476501f, 0.465461f, 0.258138f, 0.730769f, 0.899318f, 0.174359f, 0.86886f, 0.756424f, 0.685789f, 0.229707f, 0.443774f, 0.275146f, 0.658326f, 0.0486892f, 0.0457834f, 0.787678f, 0.663175f, 0.481761f, 0.573898f, 0.439558f, 0.56098f, 0.98622f, 0.807752f, 0.34172f, 0.972262f, 0.284004f, 0.396007f, 0.440433f, 0.20663f, 0.121019f, 0.0240191f, 0.0363696f, 0.93515f, 0.283339f, 0.90816f, 0.694879f, 0.28891f, 0.7681f, 0.52112f, 0.473317f, 0.933415f, 0.149368f, 0.179469f, 0.922061f, 0.625879f, 0.916547f, 0.112067f, 0.974693f, 0.553303f, 0.44083f, 0.772933f, 0.423641f, 0.0963937f, 0.937332f, 0.301141f, 0.164467f, 0.977361f, 0.224354f, 0.84855f, 0.728315f, 0.564487f, 0.733492f, 0.911596f, 0.794423f, 0.175493f, 0.156265f, 0.511936f, 0.574377f, 0.394498f, 0.522894f, 0.0984874f, 0.921798f, 0.850203f, 0.596674f, 0.0368204f, 0.326229f, 0.281598f, 0.939939f, 0.487391f, 0.596167f, 0.549989f, 0.0975398f, 0.983239f, 0.993055f, 0.0635948f, 0.749972f, 0.177322f, 0.771397f, 0.777728f, 0.531868f, 0.31498f, 0.334786f, 0.633789f, 0.959249f, 0.573456f, 0.429871f, 0.238746f, 0.376446f, 0.0555074f, 0.506892f, 0.565241f, 0.897762f, 0.409047f, 0.890007f, 0.394771f, 0.923041f, 0.0133429f, 0.940546f, 0.601826f, 0.117889f, 0.341266f, 0.77822f, 0.948948f, 0.675557f, 0.620504f, 0.274183f, 0.697336f, 0.170604f, 0.726621f, 0.583086f, 0.0914261f, 0.621153f, 0.158024f, 0.481524f, 0.801537f, 0.682243f, 0.469472f, 0.0622474f, 0.0587367f, 0.986715f, 0.24073f, 0.454288f, 0.199331f, 0.830628f, 0.866293f, 0.504453f, 0.62714f, 0.760198f, 0.848832f, 0.467382f}),
                            .dimensions = {2, 6, 7, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_half_pixel_centers_2x6x7x3_to_2x14x13x3_relaxed_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_half_pixel_centers_2x6x7x3_to_2x14x13x3_relaxed_all_inputs_as_internal", get_test_model_half_pixel_centers_2x6x7x3_to_2x14x13x3_relaxed_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_1x1() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f}),
                            .dimensions = {1, 1, 1, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_1x1 = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_1x1", get_test_model_align_corners_2x2_to_1x1());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_1x1_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f}),
                            .dimensions = {1, 1, 1, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input03_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_1x1_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_1x1_all_inputs_as_internal", get_test_model_align_corners_2x2_to_1x1_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_1x1_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 2.0f, 3.0f, 4.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output_width3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f}),
                            .dimensions = {1, 1, 1, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_1x1_float16 = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_1x1_float16", get_test_model_align_corners_2x2_to_1x1_float16());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_1x1_float16_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output_width3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f}),
                            .dimensions = {1, 1, 1, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // input03_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 2.0f, 3.0f, 4.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // dummy12
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_1x1_float16_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_1x1_float16_all_inputs_as_internal", get_test_model_align_corners_2x2_to_1x1_float16_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_1x1_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f}),
                            .dimensions = {1, 1, 1, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_1x1_relaxed = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_1x1_relaxed", get_test_model_align_corners_2x2_to_1x1_relaxed());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_1x1_relaxed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f}),
                            .dimensions = {1, 1, 1, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input03_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy13
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_1x1_relaxed_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_1x1_relaxed_all_inputs_as_internal", get_test_model_align_corners_2x2_to_1x1_relaxed_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_1x1_quant8() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130, 132, 134, 136}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // output_width3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130}),
                            .dimensions = {1, 1, 1, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_1x1_quant8 = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_1x1_quant8", get_test_model_align_corners_2x2_to_1x1_quant8());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_1x1_quant8_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // output_width3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130}),
                            .dimensions = {1, 1, 1, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // input03_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130, 132, 134, 136}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy14
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_1x1_quant8_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_1x1_quant8_all_inputs_as_internal", get_test_model_align_corners_2x2_to_1x1_quant8_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_1x1_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2, 4, 6, 8}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // output_width3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2}),
                            .dimensions = {1, 1, 1, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_1x1_quant8_signed = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_1x1_quant8_signed", get_test_model_align_corners_2x2_to_1x1_quant8_signed());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_1x1_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // output_width3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output03
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2}),
                            .dimensions = {1, 1, 1, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // input03_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2, 4, 6, 8}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // dummy15
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_1x1_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_1x1_quant8_signed_all_inputs_as_internal", get_test_model_align_corners_2x2_to_1x1_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_3x3() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 1.5f, 2.0f, 2.0f, 2.5f, 3.0f, 3.0f, 3.5f, 4.0f}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_3x3 = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_3x3", get_test_model_align_corners_2x2_to_3x3());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_3x3_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 1.5f, 2.0f, 2.0f, 2.5f, 3.0f, 3.0f, 3.5f, 4.0f}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input04_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy16
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_3x3_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_3x3_all_inputs_as_internal", get_test_model_align_corners_2x2_to_3x3_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_3x3_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 2.0f, 3.0f, 4.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output_width4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 1.5f, 2.0f, 2.0f, 2.5f, 3.0f, 3.0f, 3.5f, 4.0f}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_3x3_float16 = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_3x3_float16", get_test_model_align_corners_2x2_to_3x3_float16());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_3x3_float16_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output_width4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 1.5f, 2.0f, 2.0f, 2.5f, 3.0f, 3.0f, 3.5f, 4.0f}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // input04_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 2.0f, 3.0f, 4.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // dummy17
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_3x3_float16_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_3x3_float16_all_inputs_as_internal", get_test_model_align_corners_2x2_to_3x3_float16_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_3x3_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 1.5f, 2.0f, 2.0f, 2.5f, 3.0f, 3.0f, 3.5f, 4.0f}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_3x3_relaxed = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_3x3_relaxed", get_test_model_align_corners_2x2_to_3x3_relaxed());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_3x3_relaxed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 1.5f, 2.0f, 2.0f, 2.5f, 3.0f, 3.0f, 3.5f, 4.0f}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input04_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy18
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_3x3_relaxed_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_3x3_relaxed_all_inputs_as_internal", get_test_model_align_corners_2x2_to_3x3_relaxed_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_3x3_quant8() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130, 132, 134, 136}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // output_width4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130, 131, 132, 132, 133, 134, 134, 135, 136}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_3x3_quant8 = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_3x3_quant8", get_test_model_align_corners_2x2_to_3x3_quant8());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_3x3_quant8_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // output_width4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130, 131, 132, 132, 133, 134, 134, 135, 136}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // input04_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130, 132, 134, 136}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy19
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_3x3_quant8_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_3x3_quant8_all_inputs_as_internal", get_test_model_align_corners_2x2_to_3x3_quant8_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_3x3_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2, 4, 6, 8}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // output_width4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2, 3, 4, 4, 5, 6, 6, 7, 8}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_3x3_quant8_signed = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_3x3_quant8_signed", get_test_model_align_corners_2x2_to_3x3_quant8_signed());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_2x2_to_3x3_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // output_width4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output04
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2, 3, 4, 4, 5, 6, 6, 7, 8}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // input04_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2, 4, 6, 8}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // dummy20
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
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
                        }},
                .operations = {{
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_2x2_to_3x3_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_2x2_to_3x3_quant8_signed_all_inputs_as_internal", get_test_model_align_corners_2x2_to_3x3_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_3x3_to_2x2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 3.0f, 7.0f, 9.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_3x3_to_2x2 = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_3x3_to_2x2", get_test_model_align_corners_3x3_to_2x2());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_3x3_to_2x2_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 3.0f, 7.0f, 9.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input05_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy21
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
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
                        }},
                .operations = {{
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_3x3_to_2x2_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_3x3_to_2x2_all_inputs_as_internal", get_test_model_align_corners_3x3_to_2x2_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_3x3_to_2x2_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output_width5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 3.0f, 7.0f, 9.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_3x3_to_2x2_float16 = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_3x3_to_2x2_float16", get_test_model_align_corners_3x3_to_2x2_float16());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_3x3_to_2x2_float16_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output_width5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 3.0f, 7.0f, 9.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // input05_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // dummy22
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
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
                        }},
                .operations = {{
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_3x3_to_2x2_float16_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_3x3_to_2x2_float16_all_inputs_as_internal", get_test_model_align_corners_3x3_to_2x2_float16_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_3x3_to_2x2_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 3.0f, 7.0f, 9.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_3x3_to_2x2_relaxed = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_3x3_to_2x2_relaxed", get_test_model_align_corners_3x3_to_2x2_relaxed());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_3x3_to_2x2_relaxed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 3.0f, 7.0f, 9.0f}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input05_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy23
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
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
                        }},
                .operations = {{
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_3x3_to_2x2_relaxed_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_3x3_to_2x2_relaxed_all_inputs_as_internal", get_test_model_align_corners_3x3_to_2x2_relaxed_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_3x3_to_2x2_quant8() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130, 132, 134, 136, 138, 140, 142, 144, 146}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // output_width5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130, 134, 142, 146}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_3x3_to_2x2_quant8 = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_3x3_to_2x2_quant8", get_test_model_align_corners_3x3_to_2x2_quant8());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_3x3_to_2x2_quant8_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // output_width5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130, 134, 142, 146}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // input05_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130, 132, 134, 136, 138, 140, 142, 144, 146}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy24
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                        }},
                .operations = {{
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_3x3_to_2x2_quant8_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_3x3_to_2x2_quant8_all_inputs_as_internal", get_test_model_align_corners_3x3_to_2x2_quant8_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_3x3_to_2x2_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2, 4, 6, 8, 10, 12, 14, 16, 18}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // output_width5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2, 6, 14, 18}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_3x3_to_2x2_quant8_signed = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_3x3_to_2x2_quant8_signed", get_test_model_align_corners_3x3_to_2x2_quant8_signed());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_3x3_to_2x2_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // output_width5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output05
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2, 6, 14, 18}),
                            .dimensions = {1, 2, 2, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // input05_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2, 4, 6, 8, 10, 12, 14, 16, 18}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // dummy25
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
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
                        }},
                .operations = {{
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_3x3_to_2x2_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_3x3_to_2x2_quant8_signed_all_inputs_as_internal", get_test_model_align_corners_3x3_to_2x2_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_4x4_to_3x3() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f}),
                            .dimensions = {1, 4, 4, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.5f, 4.0f, 7.0f, 8.5f, 10.0f, 13.0f, 14.5f, 16.0f}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_4x4_to_3x3 = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_4x4_to_3x3", get_test_model_align_corners_4x4_to_3x3());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_4x4_to_3x3_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {1, 4, 4, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.5f, 4.0f, 7.0f, 8.5f, 10.0f, 13.0f, 14.5f, 16.0f}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input06_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f}),
                            .dimensions = {1, 4, 4, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy26
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
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
                        }},
                .operations = {{
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_4x4_to_3x3_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_4x4_to_3x3_all_inputs_as_internal", get_test_model_align_corners_4x4_to_3x3_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_4x4_to_3x3_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f}),
                            .dimensions = {1, 4, 4, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output_width6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 2.5f, 4.0f, 7.0f, 8.5f, 10.0f, 13.0f, 14.5f, 16.0f}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_4x4_to_3x3_float16 = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_4x4_to_3x3_float16", get_test_model_align_corners_4x4_to_3x3_float16());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_4x4_to_3x3_float16_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({}),
                            .dimensions = {1, 4, 4, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // output_width6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 2.5f, 4.0f, 7.0f, 8.5f, 10.0f, 13.0f, 14.5f, 16.0f}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // input06_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f}),
                            .dimensions = {1, 4, 4, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // dummy27
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_4x4_to_3x3_float16_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_4x4_to_3x3_float16_all_inputs_as_internal", get_test_model_align_corners_4x4_to_3x3_float16_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_4x4_to_3x3_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f}),
                            .dimensions = {1, 4, 4, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.5f, 4.0f, 7.0f, 8.5f, 10.0f, 13.0f, 14.5f, 16.0f}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_4x4_to_3x3_relaxed = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_4x4_to_3x3_relaxed", get_test_model_align_corners_4x4_to_3x3_relaxed());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_4x4_to_3x3_relaxed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {1, 4, 4, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // output_width6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.5f, 4.0f, 7.0f, 8.5f, 10.0f, 13.0f, 14.5f, 16.0f}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // input06_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f}),
                            .dimensions = {1, 4, 4, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy28
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
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
                        }},
                .operations = {{
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_4x4_to_3x3_relaxed_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_4x4_to_3x3_relaxed_all_inputs_as_internal", get_test_model_align_corners_4x4_to_3x3_relaxed_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_4x4_to_3x3_quant8() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130, 132, 134, 136, 138, 140, 142, 144, 146, 148, 150, 152, 154, 156, 158, 160}),
                            .dimensions = {1, 4, 4, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // output_width6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130, 133, 136, 142, 145, 148, 154, 157, 160}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_4x4_to_3x3_quant8 = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_4x4_to_3x3_quant8", get_test_model_align_corners_4x4_to_3x3_quant8());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_4x4_to_3x3_quant8_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {1, 4, 4, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // output_width6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130, 133, 136, 142, 145, 148, 154, 157, 160}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // input06_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({130, 132, 134, 136, 138, 140, 142, 144, 146, 148, 150, 152, 154, 156, 158, 160}),
                            .dimensions = {1, 4, 4, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy29
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_4x4_to_3x3_quant8_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_4x4_to_3x3_quant8_all_inputs_as_internal", get_test_model_align_corners_4x4_to_3x3_quant8_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_4x4_to_3x3_quant8_signed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0},
                .operands = {{ // input06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32}),
                            .dimensions = {1, 4, 4, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // output_width6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2, 5, 8, 14, 17, 20, 26, 29, 32}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_4x4_to_3x3_quant8_signed = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_4x4_to_3x3_quant8_signed", get_test_model_align_corners_4x4_to_3x3_quant8_signed());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_align_corners_4x4_to_3x3_quant8_signed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {7},
                .operands = {{ // input06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({}),
                            .dimensions = {1, 4, 4, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // output_width6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // output_height6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // output06
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2, 5, 8, 14, 17, 20, 26, 29, 32}),
                            .dimensions = {1, 3, 3, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // input06_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32}),
                            .dimensions = {1, 4, 4, 1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
                        }, { // dummy30
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.5f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
                            .zeroPoint = 0
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
                        }},
                .operations = {{
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_3,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_align_corners_4x4_to_3x3_quant8_signed_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_align_corners_4x4_to_3x3_quant8_signed_all_inputs_as_internal", get_test_model_align_corners_4x4_to_3x3_quant8_signed_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_shape_default_values_nhwc() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // shape
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 4.0f, 6.0f, 10.0f, 9.0f, 10.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 4.0f, 5.0f, 8.0f, 6.0f, 10.0f, 7.0f, 8.0f, 9.0f, 12.0f, 10.0f, 14.0f, 9.0f, 10.0f, 11.0f, 14.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 3, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_0,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_shape_default_values_nhwc = TestModelManager::get().add("resize_bilinear_v1_3_shape_default_values_nhwc", get_test_model_shape_default_values_nhwc());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_shape_default_values_nhwc_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // shape
                .inputIndexes = {7},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 4.0f, 5.0f, 8.0f, 6.0f, 10.0f, 7.0f, 8.0f, 9.0f, 12.0f, 10.0f, 14.0f, 9.0f, 10.0f, 11.0f, 14.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 3, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 4.0f, 6.0f, 10.0f, 9.0f, 10.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy31
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
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
                        }},
                .operations = {{
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_0,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_shape_default_values_nhwc_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_shape_default_values_nhwc_all_inputs_as_internal", get_test_model_shape_default_values_nhwc_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_shape_default_values_nhwc_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = { // shape
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 4.0f, 6.0f, 10.0f, 9.0f, 10.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 4.0f, 5.0f, 8.0f, 6.0f, 10.0f, 7.0f, 8.0f, 9.0f, 12.0f, 10.0f, 14.0f, 9.0f, 10.0f, 11.0f, 14.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 3, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_shape_default_values_nhwc_relaxed = TestModelManager::get().add("resize_bilinear_v1_3_shape_default_values_nhwc_relaxed", get_test_model_shape_default_values_nhwc_relaxed());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_shape_default_values_nhwc_relaxed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = { // shape
                .inputIndexes = {7},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 4.0f, 5.0f, 8.0f, 6.0f, 10.0f, 7.0f, 8.0f, 9.0f, 12.0f, 10.0f, 14.0f, 9.0f, 10.0f, 11.0f, 14.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 3, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 4.0f, 6.0f, 10.0f, 9.0f, 10.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy32
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
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
                        }},
                .operations = {{
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_shape_default_values_nhwc_relaxed_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_shape_default_values_nhwc_relaxed_all_inputs_as_internal", get_test_model_shape_default_values_nhwc_relaxed_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_shape_default_values_nhwc_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // shape
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({3.0f, 4.0f, 6.0f, 10.0f, 9.0f, 10.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({3.0f, 4.0f, 5.0f, 8.0f, 6.0f, 10.0f, 7.0f, 8.0f, 9.0f, 12.0f, 10.0f, 14.0f, 9.0f, 10.0f, 11.0f, 14.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 3, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_shape_default_values_nhwc_float16 = TestModelManager::get().add("resize_bilinear_v1_3_shape_default_values_nhwc_float16", get_test_model_shape_default_values_nhwc_float16());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_shape_default_values_nhwc_float16_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // shape
                .inputIndexes = {7},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({3.0f, 4.0f, 5.0f, 8.0f, 6.0f, 10.0f, 7.0f, 8.0f, 9.0f, 12.0f, 10.0f, 14.0f, 9.0f, 10.0f, 11.0f, 14.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 3, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({3.0f, 4.0f, 6.0f, 10.0f, 9.0f, 10.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // dummy33
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
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
                        }},
                .operations = {{
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_shape_default_values_nhwc_float16_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_shape_default_values_nhwc_float16_all_inputs_as_internal", get_test_model_shape_default_values_nhwc_float16_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_shape_default_values_nhwc_quant8() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // shape
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({12, 16, 24, 40, 36, 40, 48, 64}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({12, 16, 20, 32, 24, 40, 28, 32, 36, 48, 40, 56, 36, 40, 44, 56, 48, 64}),
                            .dimensions = {1, 3, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_shape_default_values_nhwc_quant8 = TestModelManager::get().add("resize_bilinear_v1_3_shape_default_values_nhwc_quant8", get_test_model_shape_default_values_nhwc_quant8());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_shape_default_values_nhwc_quant8_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // shape
                .inputIndexes = {7},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({12, 16, 20, 32, 24, 40, 28, 32, 36, 48, 40, 56, 36, 40, 44, 56, 48, 64}),
                            .dimensions = {1, 3, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({12, 16, 24, 40, 36, 40, 48, 64}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // dummy34
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
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
                        }},
                .operations = {{
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_shape_default_values_nhwc_quant8_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_shape_default_values_nhwc_quant8_all_inputs_as_internal", get_test_model_shape_default_values_nhwc_quant8_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_shape_default_values_nchw() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // shape
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 6.0f, 9.0f, 12.0f, 4.0f, 10.0f, 10.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 5.0f, 6.0f, 7.0f, 9.0f, 10.0f, 9.0f, 11.0f, 12.0f, 4.0f, 8.0f, 10.0f, 8.0f, 12.0f, 14.0f, 10.0f, 14.0f, 16.0f}),
                            .dimensions = {1, 2, 3, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_shape_default_values_nchw = TestModelManager::get().add("resize_bilinear_v1_3_shape_default_values_nchw", get_test_model_shape_default_values_nchw());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_shape_default_values_nchw_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // shape
                .inputIndexes = {7},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 5.0f, 6.0f, 7.0f, 9.0f, 10.0f, 9.0f, 11.0f, 12.0f, 4.0f, 8.0f, 10.0f, 8.0f, 12.0f, 14.0f, 10.0f, 14.0f, 16.0f}),
                            .dimensions = {1, 2, 3, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 6.0f, 9.0f, 12.0f, 4.0f, 10.0f, 10.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy35
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
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
                        }},
                .operations = {{
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_shape_default_values_nchw_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_shape_default_values_nchw_all_inputs_as_internal", get_test_model_shape_default_values_nchw_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_shape_default_values_nchw_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = { // shape
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 6.0f, 9.0f, 12.0f, 4.0f, 10.0f, 10.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 5.0f, 6.0f, 7.0f, 9.0f, 10.0f, 9.0f, 11.0f, 12.0f, 4.0f, 8.0f, 10.0f, 8.0f, 12.0f, 14.0f, 10.0f, 14.0f, 16.0f}),
                            .dimensions = {1, 2, 3, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_shape_default_values_nchw_relaxed = TestModelManager::get().add("resize_bilinear_v1_3_shape_default_values_nchw_relaxed", get_test_model_shape_default_values_nchw_relaxed());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_shape_default_values_nchw_relaxed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = { // shape
                .inputIndexes = {7},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 5.0f, 6.0f, 7.0f, 9.0f, 10.0f, 9.0f, 11.0f, 12.0f, 4.0f, 8.0f, 10.0f, 8.0f, 12.0f, 14.0f, 10.0f, 14.0f, 16.0f}),
                            .dimensions = {1, 2, 3, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 6.0f, 9.0f, 12.0f, 4.0f, 10.0f, 10.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy36
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
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
                        }},
                .operations = {{
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_shape_default_values_nchw_relaxed_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_shape_default_values_nchw_relaxed_all_inputs_as_internal", get_test_model_shape_default_values_nchw_relaxed_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_shape_default_values_nchw_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // shape
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({3.0f, 6.0f, 9.0f, 12.0f, 4.0f, 10.0f, 10.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({3.0f, 5.0f, 6.0f, 7.0f, 9.0f, 10.0f, 9.0f, 11.0f, 12.0f, 4.0f, 8.0f, 10.0f, 8.0f, 12.0f, 14.0f, 10.0f, 14.0f, 16.0f}),
                            .dimensions = {1, 2, 3, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_shape_default_values_nchw_float16 = TestModelManager::get().add("resize_bilinear_v1_3_shape_default_values_nchw_float16", get_test_model_shape_default_values_nchw_float16());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_shape_default_values_nchw_float16_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // shape
                .inputIndexes = {7},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({3.0f, 5.0f, 6.0f, 7.0f, 9.0f, 10.0f, 9.0f, 11.0f, 12.0f, 4.0f, 8.0f, 10.0f, 8.0f, 12.0f, 14.0f, 10.0f, 14.0f, 16.0f}),
                            .dimensions = {1, 2, 3, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({3.0f, 6.0f, 9.0f, 12.0f, 4.0f, 10.0f, 10.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // dummy37
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
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
                        }},
                .operations = {{
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_shape_default_values_nchw_float16_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_shape_default_values_nchw_float16_all_inputs_as_internal", get_test_model_shape_default_values_nchw_float16_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_shape_default_values_nchw_quant8() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // shape
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({12, 24, 36, 48, 16, 40, 40, 64}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({12, 20, 24, 28, 36, 40, 36, 44, 48, 16, 32, 40, 32, 48, 56, 40, 56, 64}),
                            .dimensions = {1, 2, 3, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_shape_default_values_nchw_quant8 = TestModelManager::get().add("resize_bilinear_v1_3_shape_default_values_nchw_quant8", get_test_model_shape_default_values_nchw_quant8());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_shape_default_values_nchw_quant8_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // shape
                .inputIndexes = {7},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({3}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({12, 20, 24, 28, 36, 40, 36, 44, 48, 16, 32, 40, 32, 48, 56, 40, 56, 64}),
                            .dimensions = {1, 2, 3, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({12, 24, 36, 48, 16, 40, 40, 64}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // dummy38
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
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
                        }},
                .operations = {{
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_shape_default_values_nchw_quant8_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_shape_default_values_nchw_quant8_all_inputs_as_internal", get_test_model_shape_default_values_nchw_quant8_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_scale_default_values_nhwc() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // scale
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 4.0f, 6.0f, 10.0f, 9.0f, 10.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 4.0f, 5.0f, 8.0f, 6.0f, 10.0f, 7.0f, 8.0f, 9.0f, 12.0f, 10.0f, 14.0f, 9.0f, 10.0f, 11.0f, 14.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 3, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_scale_default_values_nhwc = TestModelManager::get().add("resize_bilinear_v1_3_scale_default_values_nhwc", get_test_model_scale_default_values_nhwc());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_scale_default_values_nhwc_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // scale
                .inputIndexes = {7},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 4.0f, 5.0f, 8.0f, 6.0f, 10.0f, 7.0f, 8.0f, 9.0f, 12.0f, 10.0f, 14.0f, 9.0f, 10.0f, 11.0f, 14.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 3, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 4.0f, 6.0f, 10.0f, 9.0f, 10.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy39
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_scale_default_values_nhwc_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_scale_default_values_nhwc_all_inputs_as_internal", get_test_model_scale_default_values_nhwc_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_scale_default_values_nhwc_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = { // scale
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 4.0f, 6.0f, 10.0f, 9.0f, 10.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 4.0f, 5.0f, 8.0f, 6.0f, 10.0f, 7.0f, 8.0f, 9.0f, 12.0f, 10.0f, 14.0f, 9.0f, 10.0f, 11.0f, 14.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 3, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_scale_default_values_nhwc_relaxed = TestModelManager::get().add("resize_bilinear_v1_3_scale_default_values_nhwc_relaxed", get_test_model_scale_default_values_nhwc_relaxed());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_scale_default_values_nhwc_relaxed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = { // scale
                .inputIndexes = {7},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 4.0f, 5.0f, 8.0f, 6.0f, 10.0f, 7.0f, 8.0f, 9.0f, 12.0f, 10.0f, 14.0f, 9.0f, 10.0f, 11.0f, 14.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 3, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 4.0f, 6.0f, 10.0f, 9.0f, 10.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy40
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // param44
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_scale_default_values_nhwc_relaxed_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_scale_default_values_nhwc_relaxed_all_inputs_as_internal", get_test_model_scale_default_values_nhwc_relaxed_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_scale_default_values_nhwc_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // scale
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({3.0f, 4.0f, 6.0f, 10.0f, 9.0f, 10.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.600000023841858f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.600000023841858f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({3.0f, 4.0f, 5.0f, 8.0f, 6.0f, 10.0f, 7.0f, 8.0f, 9.0f, 12.0f, 10.0f, 14.0f, 9.0f, 10.0f, 11.0f, 14.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 3, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_scale_default_values_nhwc_float16 = TestModelManager::get().add("resize_bilinear_v1_3_scale_default_values_nhwc_float16", get_test_model_scale_default_values_nhwc_float16());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_scale_default_values_nhwc_float16_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // scale
                .inputIndexes = {7},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.600000023841858f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.600000023841858f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({3.0f, 4.0f, 5.0f, 8.0f, 6.0f, 10.0f, 7.0f, 8.0f, 9.0f, 12.0f, 10.0f, 14.0f, 9.0f, 10.0f, 11.0f, 14.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 3, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({3.0f, 4.0f, 6.0f, 10.0f, 9.0f, 10.0f, 12.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // dummy41
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // param45
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_scale_default_values_nhwc_float16_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_scale_default_values_nhwc_float16_all_inputs_as_internal", get_test_model_scale_default_values_nhwc_float16_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_scale_default_values_nhwc_quant8() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // scale
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({12, 16, 24, 40, 36, 40, 48, 64}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({12, 16, 20, 32, 24, 40, 28, 32, 36, 48, 40, 56, 36, 40, 44, 56, 48, 64}),
                            .dimensions = {1, 3, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_scale_default_values_nhwc_quant8 = TestModelManager::get().add("resize_bilinear_v1_3_scale_default_values_nhwc_quant8", get_test_model_scale_default_values_nhwc_quant8());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_scale_default_values_nhwc_quant8_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // scale
                .inputIndexes = {7},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({12, 16, 20, 32, 24, 40, 28, 32, 36, 48, 40, 56, 36, 40, 44, 56, 48, 64}),
                            .dimensions = {1, 3, 3, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({12, 16, 24, 40, 36, 40, 48, 64}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // dummy42
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // param46
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_scale_default_values_nhwc_quant8_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_scale_default_values_nhwc_quant8_all_inputs_as_internal", get_test_model_scale_default_values_nhwc_quant8_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_scale_default_values_nchw() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // scale
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 6.0f, 9.0f, 12.0f, 4.0f, 10.0f, 10.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 5.0f, 6.0f, 7.0f, 9.0f, 10.0f, 9.0f, 11.0f, 12.0f, 4.0f, 8.0f, 10.0f, 8.0f, 12.0f, 14.0f, 10.0f, 14.0f, 16.0f}),
                            .dimensions = {1, 2, 3, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_scale_default_values_nchw = TestModelManager::get().add("resize_bilinear_v1_3_scale_default_values_nchw", get_test_model_scale_default_values_nchw());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_scale_default_values_nchw_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // scale
                .inputIndexes = {7},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 5.0f, 6.0f, 7.0f, 9.0f, 10.0f, 9.0f, 11.0f, 12.0f, 4.0f, 8.0f, 10.0f, 8.0f, 12.0f, 14.0f, 10.0f, 14.0f, 16.0f}),
                            .dimensions = {1, 2, 3, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 6.0f, 9.0f, 12.0f, 4.0f, 10.0f, 10.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy43
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // param47
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_scale_default_values_nchw_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_scale_default_values_nchw_all_inputs_as_internal", get_test_model_scale_default_values_nchw_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_scale_default_values_nchw_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = { // scale
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 6.0f, 9.0f, 12.0f, 4.0f, 10.0f, 10.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 5.0f, 6.0f, 7.0f, 9.0f, 10.0f, 9.0f, 11.0f, 12.0f, 4.0f, 8.0f, 10.0f, 8.0f, 12.0f, 14.0f, 10.0f, 14.0f, 16.0f}),
                            .dimensions = {1, 2, 3, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_scale_default_values_nchw_relaxed = TestModelManager::get().add("resize_bilinear_v1_3_scale_default_values_nchw_relaxed", get_test_model_scale_default_values_nchw_relaxed());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_scale_default_values_nchw_relaxed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = { // scale
                .inputIndexes = {7},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 5.0f, 6.0f, 7.0f, 9.0f, 10.0f, 9.0f, 11.0f, 12.0f, 4.0f, 8.0f, 10.0f, 8.0f, 12.0f, 14.0f, 10.0f, 14.0f, 16.0f}),
                            .dimensions = {1, 2, 3, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({3.0f, 6.0f, 9.0f, 12.0f, 4.0f, 10.0f, 10.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy44
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // param48
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_scale_default_values_nchw_relaxed_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_scale_default_values_nchw_relaxed_all_inputs_as_internal", get_test_model_scale_default_values_nchw_relaxed_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_scale_default_values_nchw_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // scale
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({3.0f, 6.0f, 9.0f, 12.0f, 4.0f, 10.0f, 10.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.600000023841858f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.600000023841858f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({3.0f, 5.0f, 6.0f, 7.0f, 9.0f, 10.0f, 9.0f, 11.0f, 12.0f, 4.0f, 8.0f, 10.0f, 8.0f, 12.0f, 14.0f, 10.0f, 14.0f, 16.0f}),
                            .dimensions = {1, 2, 3, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_scale_default_values_nchw_float16 = TestModelManager::get().add("resize_bilinear_v1_3_scale_default_values_nchw_float16", get_test_model_scale_default_values_nchw_float16());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_scale_default_values_nchw_float16_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // scale
                .inputIndexes = {7},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.600000023841858f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.600000023841858f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({3.0f, 5.0f, 6.0f, 7.0f, 9.0f, 10.0f, 9.0f, 11.0f, 12.0f, 4.0f, 8.0f, 10.0f, 8.0f, 12.0f, 14.0f, 10.0f, 14.0f, 16.0f}),
                            .dimensions = {1, 2, 3, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({3.0f, 6.0f, 9.0f, 12.0f, 4.0f, 10.0f, 10.0f, 16.0f}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // dummy45
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // param49
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_scale_default_values_nchw_float16_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_scale_default_values_nchw_float16_all_inputs_as_internal", get_test_model_scale_default_values_nchw_float16_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_scale_default_values_nchw_quant8() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // scale
                .inputIndexes = {0},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({12, 24, 36, 48, 16, 40, 40, 64}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({12, 20, 24, 28, 36, 40, 36, 44, 48, 16, 32, 40, 32, 48, 56, 40, 56, 64}),
                            .dimensions = {1, 2, 3, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_scale_default_values_nchw_quant8 = TestModelManager::get().add("resize_bilinear_v1_3_scale_default_values_nchw_quant8", get_test_model_scale_default_values_nchw_quant8());

}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::resize_bilinear_v1_3 {

const TestModel& get_test_model_scale_default_values_nchw_quant8_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = { // scale
                .inputIndexes = {7},
                .operands = {{ // op1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.6f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // layout7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({true}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // align_corners7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // half_pixel_centers7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<bool8>({false}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::BOOL,
                            .zeroPoint = 0
                        }, { // op4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({12, 20, 24, 28, 36, 40, 36, 44, 48, 16, 32, 40, 32, 48, 56, 40, 56, 64}),
                            .dimensions = {1, 2, 3, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // op1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({12, 24, 36, 48, 16, 40, 40, 64}),
                            .dimensions = {1, 2, 2, 2},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // dummy46
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.25f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // param50
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
                            .inputs = {7, 8, 9},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5},
                            .outputs = {6},
                            .type = TestOperationType::RESIZE_BILINEAR
                        }},
                .outputIndexes = {6}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_scale_default_values_nchw_quant8_all_inputs_as_internal = TestModelManager::get().add("resize_bilinear_v1_3_scale_default_values_nchw_quant8_all_inputs_as_internal", get_test_model_scale_default_values_nchw_quant8_all_inputs_as_internal());

}  // namespace generated_tests::resize_bilinear_v1_3

