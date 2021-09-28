// Generated from box_with_nms_limit_gaussian.mod.py
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;

namespace generated_tests::box_with_nms_limit_gaussian {

const TestModel& get_test_model() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2},
                .operands = {{ // scores
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.9f, 0.95f, 0.75f, 0.8f, 0.7f, 0.85f, 0.6f, 0.9f, 0.95f, 0.9f, 0.65f, 0.9f, 0.8f, 0.85f, 0.8f, 0.6f, 0.6f, 0.2f, 0.6f, 0.8f, 0.4f, 0.9f, 0.55f, 0.6f, 0.9f, 0.75f, 0.7f, 0.8f, 0.7f, 0.85f, 0.9f, 0.95f, 0.75f, 0.8f, 0.85f, 0.8f, 0.6f, 0.9f, 0.95f, 0.6f, 0.6f, 0.2f, 0.5f, 0.9f, 0.8f, 0.9f, 0.75f, 0.7f, 0.9f, 0.65f, 0.9f, 0.9f, 0.55f, 0.6f, 0.6f, 0.8f, 0.4f}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // roi
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 1.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 2.0f, 2.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 3.0f, 3.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 4.0f, 4.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 5.0f, 5.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 6.0f, 6.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 7.0f, 7.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 8.0f, 8.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 9.0f, 9.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 2.0f, 2.0f, 11.0f, 11.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 1.0f, 1.0f, 10.0f, 10.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 5.0f, 5.0f, 14.0f, 14.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 3.0f, 3.0f, 12.0f, 12.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 6.0f, 6.0f, 15.0f, 15.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 2.0f, 2.0f, 0.0f, 0.0f, 2.0f, 2.0f, 9.0f, 9.0f, 18.0f, 18.0f, 9.0f, 9.0f, 19.0f, 19.0f, 9.0f, 9.0f, 19.0f, 19.0f, 4.0f, 4.0f, 13.0f, 13.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 8.0f, 8.0f, 17.0f, 17.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 7.0f, 7.0f, 16.0f, 16.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // batchSplit
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param5
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
                            .data = TestBuffer::createFromVector<float>({0.95f, 0.7879927f, 0.52485234f, 0.47400165f, 0.95f, 0.6894936f, 0.4812244f, 0.42367333f, 0.95f, 0.89983034f, 0.7879927f, 0.52485234f, 0.47400165f, 0.95f, 0.8f, 0.6894936f, 0.4811337f, 0.42367333f}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // roiOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f, 0.0f, 10.0f, 10.0f, 6.0f, 6.0f, 16.0f, 16.0f, 2.0f, 2.0f, 12.0f, 12.0f, 8.0f, 8.0f, 18.0f, 18.0f, 2.0f, 2.0f, 12.0f, 12.0f, 8.0f, 8.0f, 18.0f, 18.0f, 0.0f, 0.0f, 10.0f, 10.0f, 4.0f, 4.0f, 14.0f, 14.0f, 1.0f, 1.0f, 11.0f, 11.0f, 0.0f, 0.0f, 2.0f, 2.0f, 7.0f, 7.0f, 17.0f, 17.0f, 3.0f, 3.0f, 13.0f, 13.0f, 9.0f, 9.0f, 19.0f, 19.0f, 3.0f, 3.0f, 13.0f, 13.0f, 0.0f, 0.0f, 2.0f, 2.0f, 9.0f, 9.0f, 19.0f, 19.0f, 1.0f, 1.0f, 11.0f, 11.0f, 5.0f, 5.0f, 15.0f, 15.0f}),
                            .dimensions = {18, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // classesOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model = TestModelManager::get().add("box_with_nms_limit_gaussian", get_test_model());

}  // namespace generated_tests::box_with_nms_limit_gaussian

namespace generated_tests::box_with_nms_limit_gaussian {

const TestModel& get_test_model_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2, 13, 16},
                .operands = {{ // scores
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // roi
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // batchSplit
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param5
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
                            .data = TestBuffer::createFromVector<float>({0.95f, 0.7879927f, 0.52485234f, 0.47400165f, 0.95f, 0.6894936f, 0.4812244f, 0.42367333f, 0.95f, 0.89983034f, 0.7879927f, 0.52485234f, 0.47400165f, 0.95f, 0.8f, 0.6894936f, 0.4811337f, 0.42367333f}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // roiOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f, 0.0f, 10.0f, 10.0f, 6.0f, 6.0f, 16.0f, 16.0f, 2.0f, 2.0f, 12.0f, 12.0f, 8.0f, 8.0f, 18.0f, 18.0f, 2.0f, 2.0f, 12.0f, 12.0f, 8.0f, 8.0f, 18.0f, 18.0f, 0.0f, 0.0f, 10.0f, 10.0f, 4.0f, 4.0f, 14.0f, 14.0f, 1.0f, 1.0f, 11.0f, 11.0f, 0.0f, 0.0f, 2.0f, 2.0f, 7.0f, 7.0f, 17.0f, 17.0f, 3.0f, 3.0f, 13.0f, 13.0f, 9.0f, 9.0f, 19.0f, 19.0f, 3.0f, 3.0f, 13.0f, 13.0f, 0.0f, 0.0f, 2.0f, 2.0f, 9.0f, 9.0f, 19.0f, 19.0f, 1.0f, 1.0f, 11.0f, 11.0f, 5.0f, 5.0f, 15.0f, 15.0f}),
                            .dimensions = {18, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // classesOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // scores_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.9f, 0.95f, 0.75f, 0.8f, 0.7f, 0.85f, 0.6f, 0.9f, 0.95f, 0.9f, 0.65f, 0.9f, 0.8f, 0.85f, 0.8f, 0.6f, 0.6f, 0.2f, 0.6f, 0.8f, 0.4f, 0.9f, 0.55f, 0.6f, 0.9f, 0.75f, 0.7f, 0.8f, 0.7f, 0.85f, 0.9f, 0.95f, 0.75f, 0.8f, 0.85f, 0.8f, 0.6f, 0.9f, 0.95f, 0.6f, 0.6f, 0.2f, 0.5f, 0.9f, 0.8f, 0.9f, 0.75f, 0.7f, 0.9f, 0.65f, 0.9f, 0.9f, 0.55f, 0.6f, 0.6f, 0.8f, 0.4f}),
                            .dimensions = {19, 3},
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
                        }, { // roi_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 1.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 2.0f, 2.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 3.0f, 3.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 4.0f, 4.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 5.0f, 5.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 6.0f, 6.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 7.0f, 7.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 8.0f, 8.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 9.0f, 9.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 2.0f, 2.0f, 11.0f, 11.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 1.0f, 1.0f, 10.0f, 10.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 5.0f, 5.0f, 14.0f, 14.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 3.0f, 3.0f, 12.0f, 12.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 6.0f, 6.0f, 15.0f, 15.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 2.0f, 2.0f, 0.0f, 0.0f, 2.0f, 2.0f, 9.0f, 9.0f, 18.0f, 18.0f, 9.0f, 9.0f, 19.0f, 19.0f, 9.0f, 9.0f, 19.0f, 19.0f, 4.0f, 4.0f, 13.0f, 13.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 8.0f, 8.0f, 17.0f, 17.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 7.0f, 7.0f, 16.0f, 16.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f}),
                            .dimensions = {19, 12},
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
                            .inputs = {13, 14, 15},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {16, 17, 18},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal = TestModelManager::get().add("box_with_nms_limit_gaussian_all_inputs_as_internal", get_test_model_all_inputs_as_internal());

}  // namespace generated_tests::box_with_nms_limit_gaussian

namespace generated_tests::box_with_nms_limit_gaussian {

const TestModel& get_test_model_relaxed() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0, 1, 2},
                .operands = {{ // scores
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.9f, 0.95f, 0.75f, 0.8f, 0.7f, 0.85f, 0.6f, 0.9f, 0.95f, 0.9f, 0.65f, 0.9f, 0.8f, 0.85f, 0.8f, 0.6f, 0.6f, 0.2f, 0.6f, 0.8f, 0.4f, 0.9f, 0.55f, 0.6f, 0.9f, 0.75f, 0.7f, 0.8f, 0.7f, 0.85f, 0.9f, 0.95f, 0.75f, 0.8f, 0.85f, 0.8f, 0.6f, 0.9f, 0.95f, 0.6f, 0.6f, 0.2f, 0.5f, 0.9f, 0.8f, 0.9f, 0.75f, 0.7f, 0.9f, 0.65f, 0.9f, 0.9f, 0.55f, 0.6f, 0.6f, 0.8f, 0.4f}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // roi
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 1.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 2.0f, 2.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 3.0f, 3.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 4.0f, 4.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 5.0f, 5.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 6.0f, 6.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 7.0f, 7.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 8.0f, 8.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 9.0f, 9.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 2.0f, 2.0f, 11.0f, 11.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 1.0f, 1.0f, 10.0f, 10.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 5.0f, 5.0f, 14.0f, 14.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 3.0f, 3.0f, 12.0f, 12.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 6.0f, 6.0f, 15.0f, 15.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 2.0f, 2.0f, 0.0f, 0.0f, 2.0f, 2.0f, 9.0f, 9.0f, 18.0f, 18.0f, 9.0f, 9.0f, 19.0f, 19.0f, 9.0f, 9.0f, 19.0f, 19.0f, 4.0f, 4.0f, 13.0f, 13.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 8.0f, 8.0f, 17.0f, 17.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 7.0f, 7.0f, 16.0f, 16.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // batchSplit
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param5
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
                            .data = TestBuffer::createFromVector<float>({0.95f, 0.7879927f, 0.52485234f, 0.47400165f, 0.95f, 0.6894936f, 0.4812244f, 0.42367333f, 0.95f, 0.89983034f, 0.7879927f, 0.52485234f, 0.47400165f, 0.95f, 0.8f, 0.6894936f, 0.4811337f, 0.42367333f}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // roiOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f, 0.0f, 10.0f, 10.0f, 6.0f, 6.0f, 16.0f, 16.0f, 2.0f, 2.0f, 12.0f, 12.0f, 8.0f, 8.0f, 18.0f, 18.0f, 2.0f, 2.0f, 12.0f, 12.0f, 8.0f, 8.0f, 18.0f, 18.0f, 0.0f, 0.0f, 10.0f, 10.0f, 4.0f, 4.0f, 14.0f, 14.0f, 1.0f, 1.0f, 11.0f, 11.0f, 0.0f, 0.0f, 2.0f, 2.0f, 7.0f, 7.0f, 17.0f, 17.0f, 3.0f, 3.0f, 13.0f, 13.0f, 9.0f, 9.0f, 19.0f, 19.0f, 3.0f, 3.0f, 13.0f, 13.0f, 0.0f, 0.0f, 2.0f, 2.0f, 9.0f, 9.0f, 19.0f, 19.0f, 1.0f, 1.0f, 11.0f, 11.0f, 5.0f, 5.0f, 15.0f, 15.0f}),
                            .dimensions = {18, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // classesOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_relaxed = TestModelManager::get().add("box_with_nms_limit_gaussian_relaxed", get_test_model_relaxed());

}  // namespace generated_tests::box_with_nms_limit_gaussian

namespace generated_tests::box_with_nms_limit_gaussian {

const TestModel& get_test_model_relaxed_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {2, 13, 16},
                .operands = {{ // scores
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // roi
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // batchSplit
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param5
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
                            .data = TestBuffer::createFromVector<float>({0.95f, 0.7879927f, 0.52485234f, 0.47400165f, 0.95f, 0.6894936f, 0.4812244f, 0.42367333f, 0.95f, 0.89983034f, 0.7879927f, 0.52485234f, 0.47400165f, 0.95f, 0.8f, 0.6894936f, 0.4811337f, 0.42367333f}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // roiOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f, 0.0f, 10.0f, 10.0f, 6.0f, 6.0f, 16.0f, 16.0f, 2.0f, 2.0f, 12.0f, 12.0f, 8.0f, 8.0f, 18.0f, 18.0f, 2.0f, 2.0f, 12.0f, 12.0f, 8.0f, 8.0f, 18.0f, 18.0f, 0.0f, 0.0f, 10.0f, 10.0f, 4.0f, 4.0f, 14.0f, 14.0f, 1.0f, 1.0f, 11.0f, 11.0f, 0.0f, 0.0f, 2.0f, 2.0f, 7.0f, 7.0f, 17.0f, 17.0f, 3.0f, 3.0f, 13.0f, 13.0f, 9.0f, 9.0f, 19.0f, 19.0f, 3.0f, 3.0f, 13.0f, 13.0f, 0.0f, 0.0f, 2.0f, 2.0f, 9.0f, 9.0f, 19.0f, 19.0f, 1.0f, 1.0f, 11.0f, 11.0f, 5.0f, 5.0f, 15.0f, 15.0f}),
                            .dimensions = {18, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // classesOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // scores_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.9f, 0.95f, 0.75f, 0.8f, 0.7f, 0.85f, 0.6f, 0.9f, 0.95f, 0.9f, 0.65f, 0.9f, 0.8f, 0.85f, 0.8f, 0.6f, 0.6f, 0.2f, 0.6f, 0.8f, 0.4f, 0.9f, 0.55f, 0.6f, 0.9f, 0.75f, 0.7f, 0.8f, 0.7f, 0.85f, 0.9f, 0.95f, 0.75f, 0.8f, 0.85f, 0.8f, 0.6f, 0.9f, 0.95f, 0.6f, 0.6f, 0.2f, 0.5f, 0.9f, 0.8f, 0.9f, 0.75f, 0.7f, 0.9f, 0.65f, 0.9f, 0.9f, 0.55f, 0.6f, 0.6f, 0.8f, 0.4f}),
                            .dimensions = {19, 3},
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
                        }, { // roi_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 1.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 2.0f, 2.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 3.0f, 3.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 4.0f, 4.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 5.0f, 5.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 6.0f, 6.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 7.0f, 7.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 8.0f, 8.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 9.0f, 9.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 2.0f, 2.0f, 11.0f, 11.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 1.0f, 1.0f, 10.0f, 10.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 5.0f, 5.0f, 14.0f, 14.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 3.0f, 3.0f, 12.0f, 12.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 6.0f, 6.0f, 15.0f, 15.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 2.0f, 2.0f, 0.0f, 0.0f, 2.0f, 2.0f, 9.0f, 9.0f, 18.0f, 18.0f, 9.0f, 9.0f, 19.0f, 19.0f, 9.0f, 9.0f, 19.0f, 19.0f, 4.0f, 4.0f, 13.0f, 13.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 8.0f, 8.0f, 17.0f, 17.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 7.0f, 7.0f, 16.0f, 16.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f}),
                            .dimensions = {19, 12},
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
                            .inputs = {13, 14, 15},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {16, 17, 18},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_relaxed_all_inputs_as_internal = TestModelManager::get().add("box_with_nms_limit_gaussian_relaxed_all_inputs_as_internal", get_test_model_relaxed_all_inputs_as_internal());

}  // namespace generated_tests::box_with_nms_limit_gaussian

namespace generated_tests::box_with_nms_limit_gaussian {

const TestModel& get_test_model_float16() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2},
                .operands = {{ // scores
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.8999999761581421f, 0.949999988079071f, 0.75f, 0.800000011920929f, 0.699999988079071f, 0.8500000238418579f, 0.6000000238418579f, 0.8999999761581421f, 0.949999988079071f, 0.8999999761581421f, 0.6499999761581421f, 0.8999999761581421f, 0.800000011920929f, 0.8500000238418579f, 0.800000011920929f, 0.6000000238418579f, 0.6000000238418579f, 0.20000000298023224f, 0.6000000238418579f, 0.800000011920929f, 0.4000000059604645f, 0.8999999761581421f, 0.550000011920929f, 0.6000000238418579f, 0.8999999761581421f, 0.75f, 0.699999988079071f, 0.800000011920929f, 0.699999988079071f, 0.8500000238418579f, 0.8999999761581421f, 0.949999988079071f, 0.75f, 0.800000011920929f, 0.8500000238418579f, 0.800000011920929f, 0.6000000238418579f, 0.8999999761581421f, 0.949999988079071f, 0.6000000238418579f, 0.6000000238418579f, 0.20000000298023224f, 0.5f, 0.8999999761581421f, 0.800000011920929f, 0.8999999761581421f, 0.75f, 0.699999988079071f, 0.8999999761581421f, 0.6499999761581421f, 0.8999999761581421f, 0.8999999761581421f, 0.550000011920929f, 0.6000000238418579f, 0.6000000238418579f, 0.800000011920929f, 0.4000000059604645f}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // roi
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 1.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 2.0f, 2.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 3.0f, 3.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 4.0f, 4.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 5.0f, 5.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 6.0f, 6.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 7.0f, 7.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 8.0f, 8.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 9.0f, 9.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 2.0f, 2.0f, 11.0f, 11.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 1.0f, 1.0f, 10.0f, 10.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 5.0f, 5.0f, 14.0f, 14.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 3.0f, 3.0f, 12.0f, 12.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 6.0f, 6.0f, 15.0f, 15.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 2.0f, 2.0f, 0.0f, 0.0f, 2.0f, 2.0f, 9.0f, 9.0f, 18.0f, 18.0f, 9.0f, 9.0f, 19.0f, 19.0f, 9.0f, 9.0f, 19.0f, 19.0f, 4.0f, 4.0f, 13.0f, 13.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 8.0f, 8.0f, 17.0f, 17.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 7.0f, 7.0f, 16.0f, 16.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // batchSplit
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.30000001192092896f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.4000000059604645f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // param4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // param5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.30000001192092896f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // scoresOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.949999988079071f, 0.7879927158355713f, 0.5248523354530334f, 0.4740016460418701f, 0.949999988079071f, 0.6894935965538025f, 0.48122438788414f, 0.4236733317375183f, 0.949999988079071f, 0.8998303413391113f, 0.7879927158355713f, 0.5248523354530334f, 0.4740016460418701f, 0.949999988079071f, 0.800000011920929f, 0.6894935965538025f, 0.48113369941711426f, 0.4236733317375183f}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // roiOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f, 0.0f, 10.0f, 10.0f, 6.0f, 6.0f, 16.0f, 16.0f, 2.0f, 2.0f, 12.0f, 12.0f, 8.0f, 8.0f, 18.0f, 18.0f, 2.0f, 2.0f, 12.0f, 12.0f, 8.0f, 8.0f, 18.0f, 18.0f, 0.0f, 0.0f, 10.0f, 10.0f, 4.0f, 4.0f, 14.0f, 14.0f, 1.0f, 1.0f, 11.0f, 11.0f, 0.0f, 0.0f, 2.0f, 2.0f, 7.0f, 7.0f, 17.0f, 17.0f, 3.0f, 3.0f, 13.0f, 13.0f, 9.0f, 9.0f, 19.0f, 19.0f, 3.0f, 3.0f, 13.0f, 13.0f, 0.0f, 0.0f, 2.0f, 2.0f, 9.0f, 9.0f, 19.0f, 19.0f, 1.0f, 1.0f, 11.0f, 11.0f, 5.0f, 5.0f, 15.0f, 15.0f}),
                            .dimensions = {18, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // classesOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_float16 = TestModelManager::get().add("box_with_nms_limit_gaussian_float16", get_test_model_float16());

}  // namespace generated_tests::box_with_nms_limit_gaussian

namespace generated_tests::box_with_nms_limit_gaussian {

const TestModel& get_test_model_float16_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2, 13, 16},
                .operands = {{ // scores
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // roi
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // batchSplit
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.30000001192092896f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.4000000059604645f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // param4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // param5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.30000001192092896f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // scoresOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.949999988079071f, 0.7879927158355713f, 0.5248523354530334f, 0.4740016460418701f, 0.949999988079071f, 0.6894935965538025f, 0.48122438788414f, 0.4236733317375183f, 0.949999988079071f, 0.8998303413391113f, 0.7879927158355713f, 0.5248523354530334f, 0.4740016460418701f, 0.949999988079071f, 0.800000011920929f, 0.6894935965538025f, 0.48113369941711426f, 0.4236733317375183f}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // roiOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f, 0.0f, 10.0f, 10.0f, 6.0f, 6.0f, 16.0f, 16.0f, 2.0f, 2.0f, 12.0f, 12.0f, 8.0f, 8.0f, 18.0f, 18.0f, 2.0f, 2.0f, 12.0f, 12.0f, 8.0f, 8.0f, 18.0f, 18.0f, 0.0f, 0.0f, 10.0f, 10.0f, 4.0f, 4.0f, 14.0f, 14.0f, 1.0f, 1.0f, 11.0f, 11.0f, 0.0f, 0.0f, 2.0f, 2.0f, 7.0f, 7.0f, 17.0f, 17.0f, 3.0f, 3.0f, 13.0f, 13.0f, 9.0f, 9.0f, 19.0f, 19.0f, 3.0f, 3.0f, 13.0f, 13.0f, 0.0f, 0.0f, 2.0f, 2.0f, 9.0f, 9.0f, 19.0f, 19.0f, 1.0f, 1.0f, 11.0f, 11.0f, 5.0f, 5.0f, 15.0f, 15.0f}),
                            .dimensions = {18, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // classesOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // scores_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.8999999761581421f, 0.949999988079071f, 0.75f, 0.800000011920929f, 0.699999988079071f, 0.8500000238418579f, 0.6000000238418579f, 0.8999999761581421f, 0.949999988079071f, 0.8999999761581421f, 0.6499999761581421f, 0.8999999761581421f, 0.800000011920929f, 0.8500000238418579f, 0.800000011920929f, 0.6000000238418579f, 0.6000000238418579f, 0.20000000298023224f, 0.6000000238418579f, 0.800000011920929f, 0.4000000059604645f, 0.8999999761581421f, 0.550000011920929f, 0.6000000238418579f, 0.8999999761581421f, 0.75f, 0.699999988079071f, 0.800000011920929f, 0.699999988079071f, 0.8500000238418579f, 0.8999999761581421f, 0.949999988079071f, 0.75f, 0.800000011920929f, 0.8500000238418579f, 0.800000011920929f, 0.6000000238418579f, 0.8999999761581421f, 0.949999988079071f, 0.6000000238418579f, 0.6000000238418579f, 0.20000000298023224f, 0.5f, 0.8999999761581421f, 0.800000011920929f, 0.8999999761581421f, 0.75f, 0.699999988079071f, 0.8999999761581421f, 0.6499999761581421f, 0.8999999761581421f, 0.8999999761581421f, 0.550000011920929f, 0.6000000238418579f, 0.6000000238418579f, 0.800000011920929f, 0.4000000059604645f}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // dummy4
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
                        }, { // roi_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 1.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 2.0f, 2.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 3.0f, 3.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 4.0f, 4.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 5.0f, 5.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 6.0f, 6.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 7.0f, 7.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 8.0f, 8.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 9.0f, 9.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 2.0f, 2.0f, 11.0f, 11.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 1.0f, 1.0f, 10.0f, 10.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 5.0f, 5.0f, 14.0f, 14.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 3.0f, 3.0f, 12.0f, 12.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 6.0f, 6.0f, 15.0f, 15.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 2.0f, 2.0f, 0.0f, 0.0f, 2.0f, 2.0f, 9.0f, 9.0f, 18.0f, 18.0f, 9.0f, 9.0f, 19.0f, 19.0f, 9.0f, 9.0f, 19.0f, 19.0f, 4.0f, 4.0f, 13.0f, 13.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 8.0f, 8.0f, 17.0f, 17.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 7.0f, 7.0f, 16.0f, 16.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // dummy5
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
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
                            .inputs = {13, 14, 15},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {16, 17, 18},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_float16_all_inputs_as_internal = TestModelManager::get().add("box_with_nms_limit_gaussian_float16_all_inputs_as_internal", get_test_model_float16_all_inputs_as_internal());

}  // namespace generated_tests::box_with_nms_limit_gaussian

namespace generated_tests::box_with_nms_limit_gaussian {

const TestModel& get_test_model_quant8() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2},
                .operands = {{ // scores
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({90, 95, 75, 80, 70, 85, 60, 90, 95, 90, 65, 90, 80, 85, 80, 60, 60, 20, 60, 80, 40, 90, 55, 60, 90, 75, 70, 80, 70, 85, 90, 95, 75, 80, 85, 80, 60, 90, 95, 60, 60, 20, 50, 90, 80, 90, 75, 70, 90, 65, 90, 90, 55, 60, 60, 80, 40}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // roi
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({8, 8, 80, 80, 0, 0, 80, 80, 0, 0, 80, 80, 16, 16, 88, 88, 8, 8, 88, 88, 8, 8, 88, 88, 24, 24, 96, 96, 16, 16, 96, 96, 16, 16, 96, 96, 32, 32, 104, 104, 24, 24, 104, 104, 24, 24, 104, 104, 40, 40, 112, 112, 32, 32, 112, 112, 32, 32, 112, 112, 48, 48, 120, 120, 40, 40, 120, 120, 40, 40, 120, 120, 56, 56, 128, 128, 48, 48, 128, 128, 48, 48, 128, 128, 64, 64, 136, 136, 56, 56, 136, 136, 56, 56, 136, 136, 72, 72, 144, 144, 64, 64, 144, 144, 64, 64, 144, 144, 16, 16, 88, 88, 16, 16, 96, 96, 16, 16, 96, 96, 8, 8, 80, 80, 8, 8, 88, 88, 8, 8, 88, 88, 40, 40, 112, 112, 40, 40, 120, 120, 40, 40, 120, 120, 24, 24, 96, 96, 24, 24, 104, 104, 24, 24, 104, 104, 48, 48, 120, 120, 48, 48, 128, 128, 48, 48, 128, 128, 0, 0, 8, 8, 0, 0, 16, 16, 0, 0, 16, 16, 72, 72, 144, 144, 72, 72, 152, 152, 72, 72, 152, 152, 32, 32, 104, 104, 32, 32, 112, 112, 32, 32, 112, 112, 64, 64, 136, 136, 64, 64, 144, 144, 64, 64, 144, 144, 56, 56, 128, 128, 56, 56, 136, 136, 56, 56, 136, 136}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param5
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
                            .data = TestBuffer::createFromVector<uint8_t>({95, 79, 52, 47, 95, 69, 48, 42, 95, 90, 79, 52, 47, 95, 80, 69, 48, 42}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // roiOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({0, 0, 80, 80, 48, 48, 128, 128, 16, 16, 96, 96, 64, 64, 144, 144, 16, 16, 96, 96, 64, 64, 144, 144, 0, 0, 80, 80, 32, 32, 112, 112, 8, 8, 88, 88, 0, 0, 16, 16, 56, 56, 136, 136, 24, 24, 104, 104, 72, 72, 152, 152, 24, 24, 104, 104, 0, 0, 16, 16, 72, 72, 152, 152, 8, 8, 88, 88, 40, 40, 120, 120}),
                            .dimensions = {18, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // classesOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8 = TestModelManager::get().add("box_with_nms_limit_gaussian_quant8", get_test_model_quant8());

}  // namespace generated_tests::box_with_nms_limit_gaussian

namespace generated_tests::box_with_nms_limit_gaussian {

const TestModel& get_test_model_quant8_all_inputs_as_internal() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {1, 2, 13},
                .operands = {{ // scores
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // roi
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({8, 8, 80, 80, 0, 0, 80, 80, 0, 0, 80, 80, 16, 16, 88, 88, 8, 8, 88, 88, 8, 8, 88, 88, 24, 24, 96, 96, 16, 16, 96, 96, 16, 16, 96, 96, 32, 32, 104, 104, 24, 24, 104, 104, 24, 24, 104, 104, 40, 40, 112, 112, 32, 32, 112, 112, 32, 32, 112, 112, 48, 48, 120, 120, 40, 40, 120, 120, 40, 40, 120, 120, 56, 56, 128, 128, 48, 48, 128, 128, 48, 48, 128, 128, 64, 64, 136, 136, 56, 56, 136, 136, 56, 56, 136, 136, 72, 72, 144, 144, 64, 64, 144, 144, 64, 64, 144, 144, 16, 16, 88, 88, 16, 16, 96, 96, 16, 16, 96, 96, 8, 8, 80, 80, 8, 8, 88, 88, 8, 8, 88, 88, 40, 40, 112, 112, 40, 40, 120, 120, 40, 40, 120, 120, 24, 24, 96, 96, 24, 24, 104, 104, 24, 24, 104, 104, 48, 48, 120, 120, 48, 48, 128, 128, 48, 48, 128, 128, 0, 0, 8, 8, 0, 0, 16, 16, 0, 0, 16, 16, 72, 72, 144, 144, 72, 72, 152, 152, 72, 72, 152, 152, 32, 32, 104, 104, 32, 32, 112, 112, 32, 32, 112, 112, 64, 64, 136, 136, 64, 64, 144, 144, 64, 64, 144, 144, 56, 56, 128, 128, 56, 56, 136, 136, 56, 56, 136, 136}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({-1}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param2
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({2}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::INT32,
                            .zeroPoint = 0
                        }, { // param3
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param4
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param5
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
                            .data = TestBuffer::createFromVector<uint8_t>({95, 79, 52, 47, 95, 69, 48, 42, 95, 90, 79, 52, 47, 95, 80, 69, 48, 42}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // roiOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({0, 0, 80, 80, 48, 48, 128, 128, 16, 16, 96, 96, 64, 64, 144, 144, 16, 16, 96, 96, 64, 64, 144, 144, 0, 0, 80, 80, 32, 32, 112, 112, 8, 8, 88, 88, 0, 0, 16, 16, 56, 56, 136, 136, 24, 24, 104, 104, 72, 72, 152, 152, 24, 24, 104, 104, 0, 0, 16, 16, 72, 72, 152, 152, 8, 8, 88, 88, 40, 40, 120, 120}),
                            .dimensions = {18, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // classesOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}),
                            .dimensions = {18},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // scores_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({90, 95, 75, 80, 70, 85, 60, 90, 95, 90, 65, 90, 80, 85, 80, 60, 60, 20, 60, 80, 40, 90, 55, 60, 90, 75, 70, 80, 70, 85, 90, 95, 75, 80, 85, 80, 60, 90, 95, 60, 60, 20, 50, 90, 80, 90, 75, 70, 90, 65, 90, 90, 55, 60, 60, 80, 40}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
                        }, { // dummy6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({0}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 0
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
                            .inputs = {13, 14, 15},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_all_inputs_as_internal = TestModelManager::get().add("box_with_nms_limit_gaussian_quant8_all_inputs_as_internal", get_test_model_quant8_all_inputs_as_internal());

}  // namespace generated_tests::box_with_nms_limit_gaussian

namespace generated_tests::box_with_nms_limit_gaussian {

const TestModel& get_test_model_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2},
                .operands = {{ // scores1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.9f, 0.95f, 0.75f, 0.8f, 0.7f, 0.85f, 0.6f, 0.9f, 0.95f, 0.9f, 0.65f, 0.9f, 0.8f, 0.85f, 0.8f, 0.6f, 0.6f, 0.2f, 0.6f, 0.8f, 0.4f, 0.9f, 0.55f, 0.6f, 0.9f, 0.75f, 0.7f, 0.8f, 0.7f, 0.85f, 0.9f, 0.95f, 0.75f, 0.8f, 0.85f, 0.8f, 0.6f, 0.9f, 0.95f, 0.6f, 0.6f, 0.2f, 0.5f, 0.9f, 0.8f, 0.9f, 0.75f, 0.7f, 0.9f, 0.65f, 0.9f, 0.9f, 0.55f, 0.6f, 0.6f, 0.8f, 0.4f}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // roi1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 1.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 2.0f, 2.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 3.0f, 3.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 4.0f, 4.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 5.0f, 5.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 6.0f, 6.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 7.0f, 7.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 8.0f, 8.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 9.0f, 9.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 2.0f, 2.0f, 11.0f, 11.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 1.0f, 1.0f, 10.0f, 10.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 5.0f, 5.0f, 14.0f, 14.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 3.0f, 3.0f, 12.0f, 12.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 6.0f, 6.0f, 15.0f, 15.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 2.0f, 2.0f, 0.0f, 0.0f, 2.0f, 2.0f, 9.0f, 9.0f, 18.0f, 18.0f, 9.0f, 9.0f, 19.0f, 19.0f, 9.0f, 9.0f, 19.0f, 19.0f, 4.0f, 4.0f, 13.0f, 13.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 8.0f, 8.0f, 17.0f, 17.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 7.0f, 7.0f, 16.0f, 16.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // batchSplit1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
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
                        }, { // param7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({5}),
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
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // scoresOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.95f, 0.7879927f, 0.52485234f, 0.95f, 0.6894936f, 0.95f, 0.89983034f, 0.7879927f, 0.95f, 0.8f}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // roiOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f, 0.0f, 10.0f, 10.0f, 6.0f, 6.0f, 16.0f, 16.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 8.0f, 8.0f, 18.0f, 18.0f, 1.0f, 1.0f, 11.0f, 11.0f, 0.0f, 0.0f, 2.0f, 2.0f, 7.0f, 7.0f, 17.0f, 17.0f, 3.0f, 3.0f, 13.0f, 13.0f, 0.0f, 0.0f, 2.0f, 2.0f}),
                            .dimensions = {10, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // classesOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 2, 2, 1, 1, 1, 2, 2}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 3, 3, 3, 3, 3}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_2 = TestModelManager::get().add("box_with_nms_limit_gaussian_2", get_test_model_2());

}  // namespace generated_tests::box_with_nms_limit_gaussian

namespace generated_tests::box_with_nms_limit_gaussian {

const TestModel& get_test_model_all_inputs_as_internal_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2, 13, 16},
                .operands = {{ // scores1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // roi1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // batchSplit1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
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
                        }, { // param7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({5}),
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
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // scoresOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.95f, 0.7879927f, 0.52485234f, 0.95f, 0.6894936f, 0.95f, 0.89983034f, 0.7879927f, 0.95f, 0.8f}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // roiOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f, 0.0f, 10.0f, 10.0f, 6.0f, 6.0f, 16.0f, 16.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 8.0f, 8.0f, 18.0f, 18.0f, 1.0f, 1.0f, 11.0f, 11.0f, 0.0f, 0.0f, 2.0f, 2.0f, 7.0f, 7.0f, 17.0f, 17.0f, 3.0f, 3.0f, 13.0f, 13.0f, 0.0f, 0.0f, 2.0f, 2.0f}),
                            .dimensions = {10, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // classesOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 2, 2, 1, 1, 1, 2, 2}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 3, 3, 3, 3, 3}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // scores1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.9f, 0.95f, 0.75f, 0.8f, 0.7f, 0.85f, 0.6f, 0.9f, 0.95f, 0.9f, 0.65f, 0.9f, 0.8f, 0.85f, 0.8f, 0.6f, 0.6f, 0.2f, 0.6f, 0.8f, 0.4f, 0.9f, 0.55f, 0.6f, 0.9f, 0.75f, 0.7f, 0.8f, 0.7f, 0.85f, 0.9f, 0.95f, 0.75f, 0.8f, 0.85f, 0.8f, 0.6f, 0.9f, 0.95f, 0.6f, 0.6f, 0.2f, 0.5f, 0.9f, 0.8f, 0.9f, 0.75f, 0.7f, 0.9f, 0.65f, 0.9f, 0.9f, 0.55f, 0.6f, 0.6f, 0.8f, 0.4f}),
                            .dimensions = {19, 3},
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
                        }, { // roi1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 1.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 2.0f, 2.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 3.0f, 3.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 4.0f, 4.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 5.0f, 5.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 6.0f, 6.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 7.0f, 7.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 8.0f, 8.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 9.0f, 9.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 2.0f, 2.0f, 11.0f, 11.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 1.0f, 1.0f, 10.0f, 10.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 5.0f, 5.0f, 14.0f, 14.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 3.0f, 3.0f, 12.0f, 12.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 6.0f, 6.0f, 15.0f, 15.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 2.0f, 2.0f, 0.0f, 0.0f, 2.0f, 2.0f, 9.0f, 9.0f, 18.0f, 18.0f, 9.0f, 9.0f, 19.0f, 19.0f, 9.0f, 9.0f, 19.0f, 19.0f, 4.0f, 4.0f, 13.0f, 13.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 8.0f, 8.0f, 17.0f, 17.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 7.0f, 7.0f, 16.0f, 16.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f}),
                            .dimensions = {19, 12},
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
                            .inputs = {13, 14, 15},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {16, 17, 18},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_all_inputs_as_internal_2 = TestModelManager::get().add("box_with_nms_limit_gaussian_all_inputs_as_internal_2", get_test_model_all_inputs_as_internal_2());

}  // namespace generated_tests::box_with_nms_limit_gaussian

namespace generated_tests::box_with_nms_limit_gaussian {

const TestModel& get_test_model_relaxed_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {0, 1, 2},
                .operands = {{ // scores1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.9f, 0.95f, 0.75f, 0.8f, 0.7f, 0.85f, 0.6f, 0.9f, 0.95f, 0.9f, 0.65f, 0.9f, 0.8f, 0.85f, 0.8f, 0.6f, 0.6f, 0.2f, 0.6f, 0.8f, 0.4f, 0.9f, 0.55f, 0.6f, 0.9f, 0.75f, 0.7f, 0.8f, 0.7f, 0.85f, 0.9f, 0.95f, 0.75f, 0.8f, 0.85f, 0.8f, 0.6f, 0.9f, 0.95f, 0.6f, 0.6f, 0.2f, 0.5f, 0.9f, 0.8f, 0.9f, 0.75f, 0.7f, 0.9f, 0.65f, 0.9f, 0.9f, 0.55f, 0.6f, 0.6f, 0.8f, 0.4f}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // roi1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 1.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 2.0f, 2.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 3.0f, 3.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 4.0f, 4.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 5.0f, 5.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 6.0f, 6.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 7.0f, 7.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 8.0f, 8.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 9.0f, 9.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 2.0f, 2.0f, 11.0f, 11.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 1.0f, 1.0f, 10.0f, 10.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 5.0f, 5.0f, 14.0f, 14.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 3.0f, 3.0f, 12.0f, 12.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 6.0f, 6.0f, 15.0f, 15.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 2.0f, 2.0f, 0.0f, 0.0f, 2.0f, 2.0f, 9.0f, 9.0f, 18.0f, 18.0f, 9.0f, 9.0f, 19.0f, 19.0f, 9.0f, 9.0f, 19.0f, 19.0f, 4.0f, 4.0f, 13.0f, 13.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 8.0f, 8.0f, 17.0f, 17.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 7.0f, 7.0f, 16.0f, 16.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // batchSplit1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
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
                        }, { // param7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({5}),
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
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // scoresOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.95f, 0.7879927f, 0.52485234f, 0.95f, 0.6894936f, 0.95f, 0.89983034f, 0.7879927f, 0.95f, 0.8f}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // roiOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f, 0.0f, 10.0f, 10.0f, 6.0f, 6.0f, 16.0f, 16.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 8.0f, 8.0f, 18.0f, 18.0f, 1.0f, 1.0f, 11.0f, 11.0f, 0.0f, 0.0f, 2.0f, 2.0f, 7.0f, 7.0f, 17.0f, 17.0f, 3.0f, 3.0f, 13.0f, 13.0f, 0.0f, 0.0f, 2.0f, 2.0f}),
                            .dimensions = {10, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // classesOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 2, 2, 1, 1, 1, 2, 2}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 3, 3, 3, 3, 3}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_relaxed_2 = TestModelManager::get().add("box_with_nms_limit_gaussian_relaxed_2", get_test_model_relaxed_2());

}  // namespace generated_tests::box_with_nms_limit_gaussian

namespace generated_tests::box_with_nms_limit_gaussian {

const TestModel& get_test_model_relaxed_all_inputs_as_internal_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = true,
        .main = {
                .inputIndexes = {2, 13, 16},
                .operands = {{ // scores1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // roi1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // batchSplit1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
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
                        }, { // param7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({5}),
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
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // scoresOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.95f, 0.7879927f, 0.52485234f, 0.95f, 0.6894936f, 0.95f, 0.89983034f, 0.7879927f, 0.95f, 0.8f}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // roiOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f, 0.0f, 10.0f, 10.0f, 6.0f, 6.0f, 16.0f, 16.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 8.0f, 8.0f, 18.0f, 18.0f, 1.0f, 1.0f, 11.0f, 11.0f, 0.0f, 0.0f, 2.0f, 2.0f, 7.0f, 7.0f, 17.0f, 17.0f, 3.0f, 3.0f, 13.0f, 13.0f, 0.0f, 0.0f, 2.0f, 2.0f}),
                            .dimensions = {10, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // classesOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 2, 2, 1, 1, 1, 2, 2}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 3, 3, 3, 3, 3}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // scores1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.9f, 0.95f, 0.75f, 0.8f, 0.7f, 0.85f, 0.6f, 0.9f, 0.95f, 0.9f, 0.65f, 0.9f, 0.8f, 0.85f, 0.8f, 0.6f, 0.6f, 0.2f, 0.6f, 0.8f, 0.4f, 0.9f, 0.55f, 0.6f, 0.9f, 0.75f, 0.7f, 0.8f, 0.7f, 0.85f, 0.9f, 0.95f, 0.75f, 0.8f, 0.85f, 0.8f, 0.6f, 0.9f, 0.95f, 0.6f, 0.6f, 0.2f, 0.5f, 0.9f, 0.8f, 0.9f, 0.75f, 0.7f, 0.9f, 0.65f, 0.9f, 0.9f, 0.55f, 0.6f, 0.6f, 0.8f, 0.4f}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
                            .zeroPoint = 0
                        }, { // dummy9
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT32,
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
                        }, { // roi1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({1.0f, 1.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 2.0f, 2.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 3.0f, 3.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 4.0f, 4.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 5.0f, 5.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 6.0f, 6.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 7.0f, 7.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 8.0f, 8.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 9.0f, 9.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 2.0f, 2.0f, 11.0f, 11.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 1.0f, 1.0f, 10.0f, 10.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 5.0f, 5.0f, 14.0f, 14.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 3.0f, 3.0f, 12.0f, 12.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 6.0f, 6.0f, 15.0f, 15.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 2.0f, 2.0f, 0.0f, 0.0f, 2.0f, 2.0f, 9.0f, 9.0f, 18.0f, 18.0f, 9.0f, 9.0f, 19.0f, 19.0f, 9.0f, 9.0f, 19.0f, 19.0f, 4.0f, 4.0f, 13.0f, 13.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 8.0f, 8.0f, 17.0f, 17.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 7.0f, 7.0f, 16.0f, 16.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f}),
                            .dimensions = {19, 12},
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
                            .inputs = {13, 14, 15},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {16, 17, 18},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::UNKNOWN,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_relaxed_all_inputs_as_internal_2 = TestModelManager::get().add("box_with_nms_limit_gaussian_relaxed_all_inputs_as_internal_2", get_test_model_relaxed_all_inputs_as_internal_2());

}  // namespace generated_tests::box_with_nms_limit_gaussian

namespace generated_tests::box_with_nms_limit_gaussian {

const TestModel& get_test_model_float16_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2},
                .operands = {{ // scores1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.8999999761581421f, 0.949999988079071f, 0.75f, 0.800000011920929f, 0.699999988079071f, 0.8500000238418579f, 0.6000000238418579f, 0.8999999761581421f, 0.949999988079071f, 0.8999999761581421f, 0.6499999761581421f, 0.8999999761581421f, 0.800000011920929f, 0.8500000238418579f, 0.800000011920929f, 0.6000000238418579f, 0.6000000238418579f, 0.20000000298023224f, 0.6000000238418579f, 0.800000011920929f, 0.4000000059604645f, 0.8999999761581421f, 0.550000011920929f, 0.6000000238418579f, 0.8999999761581421f, 0.75f, 0.699999988079071f, 0.800000011920929f, 0.699999988079071f, 0.8500000238418579f, 0.8999999761581421f, 0.949999988079071f, 0.75f, 0.800000011920929f, 0.8500000238418579f, 0.800000011920929f, 0.6000000238418579f, 0.8999999761581421f, 0.949999988079071f, 0.6000000238418579f, 0.6000000238418579f, 0.20000000298023224f, 0.5f, 0.8999999761581421f, 0.800000011920929f, 0.8999999761581421f, 0.75f, 0.699999988079071f, 0.8999999761581421f, 0.6499999761581421f, 0.8999999761581421f, 0.8999999761581421f, 0.550000011920929f, 0.6000000238418579f, 0.6000000238418579f, 0.800000011920929f, 0.4000000059604645f}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // roi1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 1.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 2.0f, 2.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 3.0f, 3.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 4.0f, 4.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 5.0f, 5.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 6.0f, 6.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 7.0f, 7.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 8.0f, 8.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 9.0f, 9.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 2.0f, 2.0f, 11.0f, 11.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 1.0f, 1.0f, 10.0f, 10.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 5.0f, 5.0f, 14.0f, 14.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 3.0f, 3.0f, 12.0f, 12.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 6.0f, 6.0f, 15.0f, 15.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 2.0f, 2.0f, 0.0f, 0.0f, 2.0f, 2.0f, 9.0f, 9.0f, 18.0f, 18.0f, 9.0f, 9.0f, 19.0f, 19.0f, 9.0f, 9.0f, 19.0f, 19.0f, 4.0f, 4.0f, 13.0f, 13.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 8.0f, 8.0f, 17.0f, 17.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 7.0f, 7.0f, 16.0f, 16.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // batchSplit1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.30000001192092896f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // param7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({5}),
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
                            .data = TestBuffer::createFromVector<_Float16>({0.4000000059604645f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // param10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // param11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.30000001192092896f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // scoresOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.949999988079071f, 0.7879927158355713f, 0.5248523354530334f, 0.949999988079071f, 0.6894935965538025f, 0.949999988079071f, 0.8998303413391113f, 0.7879927158355713f, 0.949999988079071f, 0.800000011920929f}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // roiOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f, 0.0f, 10.0f, 10.0f, 6.0f, 6.0f, 16.0f, 16.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 8.0f, 8.0f, 18.0f, 18.0f, 1.0f, 1.0f, 11.0f, 11.0f, 0.0f, 0.0f, 2.0f, 2.0f, 7.0f, 7.0f, 17.0f, 17.0f, 3.0f, 3.0f, 13.0f, 13.0f, 0.0f, 0.0f, 2.0f, 2.0f}),
                            .dimensions = {10, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // classesOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 2, 2, 1, 1, 1, 2, 2}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 3, 3, 3, 3, 3}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_float16_2 = TestModelManager::get().add("box_with_nms_limit_gaussian_float16_2", get_test_model_float16_2());

}  // namespace generated_tests::box_with_nms_limit_gaussian

namespace generated_tests::box_with_nms_limit_gaussian {

const TestModel& get_test_model_float16_all_inputs_as_internal_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {2, 13, 16},
                .operands = {{ // scores1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // roi1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // batchSplit1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // param6
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.30000001192092896f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // param7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({5}),
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
                            .data = TestBuffer::createFromVector<_Float16>({0.4000000059604645f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // param10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // param11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.30000001192092896f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT16,
                            .zeroPoint = 0
                        }, { // scoresOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.949999988079071f, 0.7879927158355713f, 0.5248523354530334f, 0.949999988079071f, 0.6894935965538025f, 0.949999988079071f, 0.8998303413391113f, 0.7879927158355713f, 0.949999988079071f, 0.800000011920929f}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // roiOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f, 0.0f, 10.0f, 10.0f, 6.0f, 6.0f, 16.0f, 16.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 8.0f, 8.0f, 18.0f, 18.0f, 1.0f, 1.0f, 11.0f, 11.0f, 0.0f, 0.0f, 2.0f, 2.0f, 7.0f, 7.0f, 17.0f, 17.0f, 3.0f, 3.0f, 13.0f, 13.0f, 0.0f, 0.0f, 2.0f, 2.0f}),
                            .dimensions = {10, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // classesOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 2, 2, 1, 1, 1, 2, 2}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 3, 3, 3, 3, 3}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // scores1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.8999999761581421f, 0.949999988079071f, 0.75f, 0.800000011920929f, 0.699999988079071f, 0.8500000238418579f, 0.6000000238418579f, 0.8999999761581421f, 0.949999988079071f, 0.8999999761581421f, 0.6499999761581421f, 0.8999999761581421f, 0.800000011920929f, 0.8500000238418579f, 0.800000011920929f, 0.6000000238418579f, 0.6000000238418579f, 0.20000000298023224f, 0.6000000238418579f, 0.800000011920929f, 0.4000000059604645f, 0.8999999761581421f, 0.550000011920929f, 0.6000000238418579f, 0.8999999761581421f, 0.75f, 0.699999988079071f, 0.800000011920929f, 0.699999988079071f, 0.8500000238418579f, 0.8999999761581421f, 0.949999988079071f, 0.75f, 0.800000011920929f, 0.8500000238418579f, 0.800000011920929f, 0.6000000238418579f, 0.8999999761581421f, 0.949999988079071f, 0.6000000238418579f, 0.6000000238418579f, 0.20000000298023224f, 0.5f, 0.8999999761581421f, 0.800000011920929f, 0.8999999761581421f, 0.75f, 0.699999988079071f, 0.8999999761581421f, 0.6499999761581421f, 0.8999999761581421f, 0.8999999761581421f, 0.550000011920929f, 0.6000000238418579f, 0.6000000238418579f, 0.800000011920929f, 0.4000000059604645f}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
                        }, { // dummy11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({0.0f}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_FLOAT16,
                            .zeroPoint = 0
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
                        }, { // roi1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<_Float16>({1.0f, 1.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 0.0f, 0.0f, 10.0f, 10.0f, 2.0f, 2.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 3.0f, 3.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 4.0f, 4.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 5.0f, 5.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 6.0f, 6.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 7.0f, 7.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 8.0f, 8.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f, 9.0f, 9.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 2.0f, 2.0f, 11.0f, 11.0f, 2.0f, 2.0f, 12.0f, 12.0f, 2.0f, 2.0f, 12.0f, 12.0f, 1.0f, 1.0f, 10.0f, 10.0f, 1.0f, 1.0f, 11.0f, 11.0f, 1.0f, 1.0f, 11.0f, 11.0f, 5.0f, 5.0f, 14.0f, 14.0f, 5.0f, 5.0f, 15.0f, 15.0f, 5.0f, 5.0f, 15.0f, 15.0f, 3.0f, 3.0f, 12.0f, 12.0f, 3.0f, 3.0f, 13.0f, 13.0f, 3.0f, 3.0f, 13.0f, 13.0f, 6.0f, 6.0f, 15.0f, 15.0f, 6.0f, 6.0f, 16.0f, 16.0f, 6.0f, 6.0f, 16.0f, 16.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 2.0f, 2.0f, 0.0f, 0.0f, 2.0f, 2.0f, 9.0f, 9.0f, 18.0f, 18.0f, 9.0f, 9.0f, 19.0f, 19.0f, 9.0f, 9.0f, 19.0f, 19.0f, 4.0f, 4.0f, 13.0f, 13.0f, 4.0f, 4.0f, 14.0f, 14.0f, 4.0f, 4.0f, 14.0f, 14.0f, 8.0f, 8.0f, 17.0f, 17.0f, 8.0f, 8.0f, 18.0f, 18.0f, 8.0f, 8.0f, 18.0f, 18.0f, 7.0f, 7.0f, 16.0f, 16.0f, 7.0f, 7.0f, 17.0f, 17.0f, 7.0f, 7.0f, 17.0f, 17.0f}),
                            .dimensions = {19, 12},
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
                            .inputs = {13, 14, 15},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {16, 17, 18},
                            .outputs = {1},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_float16_all_inputs_as_internal_2 = TestModelManager::get().add("box_with_nms_limit_gaussian_float16_all_inputs_as_internal_2", get_test_model_float16_all_inputs_as_internal_2());

}  // namespace generated_tests::box_with_nms_limit_gaussian

namespace generated_tests::box_with_nms_limit_gaussian {

const TestModel& get_test_model_quant8_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {0, 1, 2},
                .operands = {{ // scores1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({218, 223, 203, 208, 198, 213, 188, 218, 223, 218, 193, 218, 208, 213, 208, 188, 188, 148, 188, 208, 168, 218, 183, 188, 218, 203, 198, 208, 198, 213, 218, 223, 203, 208, 213, 208, 188, 218, 223, 188, 188, 148, 178, 218, 208, 218, 203, 198, 218, 193, 218, 218, 183, 188, 188, 208, 168}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // roi1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({8, 8, 80, 80, 0, 0, 80, 80, 0, 0, 80, 80, 16, 16, 88, 88, 8, 8, 88, 88, 8, 8, 88, 88, 24, 24, 96, 96, 16, 16, 96, 96, 16, 16, 96, 96, 32, 32, 104, 104, 24, 24, 104, 104, 24, 24, 104, 104, 40, 40, 112, 112, 32, 32, 112, 112, 32, 32, 112, 112, 48, 48, 120, 120, 40, 40, 120, 120, 40, 40, 120, 120, 56, 56, 128, 128, 48, 48, 128, 128, 48, 48, 128, 128, 64, 64, 136, 136, 56, 56, 136, 136, 56, 56, 136, 136, 72, 72, 144, 144, 64, 64, 144, 144, 64, 64, 144, 144, 16, 16, 88, 88, 16, 16, 96, 96, 16, 16, 96, 96, 8, 8, 80, 80, 8, 8, 88, 88, 8, 8, 88, 88, 40, 40, 112, 112, 40, 40, 120, 120, 40, 40, 120, 120, 24, 24, 96, 96, 24, 24, 104, 104, 24, 24, 104, 104, 48, 48, 120, 120, 48, 48, 128, 128, 48, 48, 128, 128, 0, 0, 8, 8, 0, 0, 16, 16, 0, 0, 16, 16, 72, 72, 144, 144, 72, 72, 152, 152, 72, 72, 152, 152, 32, 32, 104, 104, 32, 32, 112, 112, 32, 32, 112, 112, 64, 64, 136, 136, 64, 64, 144, 144, 64, 64, 144, 144, 56, 56, 128, 128, 56, 56, 136, 136, 56, 56, 136, 136}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
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
                        }, { // param7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({5}),
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
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // scoresOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({223, 207, 180, 223, 197, 223, 218, 207, 223, 208}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // roiOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({0, 0, 80, 80, 48, 48, 128, 128, 16, 16, 96, 96, 16, 16, 96, 96, 64, 64, 144, 144, 8, 8, 88, 88, 0, 0, 16, 16, 56, 56, 136, 136, 24, 24, 104, 104, 0, 0, 16, 16}),
                            .dimensions = {10, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // classesOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 2, 2, 1, 1, 1, 2, 2}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 3, 3, 3, 3, 3}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }},
                .operations = {{
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_2 = TestModelManager::get().add("box_with_nms_limit_gaussian_quant8_2", get_test_model_quant8_2());

}  // namespace generated_tests::box_with_nms_limit_gaussian

namespace generated_tests::box_with_nms_limit_gaussian {

const TestModel& get_test_model_quant8_all_inputs_as_internal_2() {
    static TestModel model = {
        .expectFailure = false,
        .expectedMultinomialDistributionTolerance = 0,
        .isRelaxed = false,
        .main = {
                .inputIndexes = {1, 2, 13},
                .operands = {{ // scores1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::TEMPORARY_VARIABLE,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // roi1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({8, 8, 80, 80, 0, 0, 80, 80, 0, 0, 80, 80, 16, 16, 88, 88, 8, 8, 88, 88, 8, 8, 88, 88, 24, 24, 96, 96, 16, 16, 96, 96, 16, 16, 96, 96, 32, 32, 104, 104, 24, 24, 104, 104, 24, 24, 104, 104, 40, 40, 112, 112, 32, 32, 112, 112, 32, 32, 112, 112, 48, 48, 120, 120, 40, 40, 120, 120, 40, 40, 120, 120, 56, 56, 128, 128, 48, 48, 128, 128, 48, 48, 128, 128, 64, 64, 136, 136, 56, 56, 136, 136, 56, 56, 136, 136, 72, 72, 144, 144, 64, 64, 144, 144, 64, 64, 144, 144, 16, 16, 88, 88, 16, 16, 96, 96, 16, 16, 96, 96, 8, 8, 80, 80, 8, 8, 88, 88, 8, 8, 88, 88, 40, 40, 112, 112, 40, 40, 120, 120, 40, 40, 120, 120, 24, 24, 96, 96, 24, 24, 104, 104, 24, 24, 104, 104, 48, 48, 120, 120, 48, 48, 128, 128, 48, 48, 128, 128, 0, 0, 8, 8, 0, 0, 16, 16, 0, 0, 16, 16, 72, 72, 144, 144, 72, 72, 152, 152, 72, 72, 152, 152, 32, 32, 104, 104, 32, 32, 112, 112, 32, 32, 112, 112, 64, 64, 136, 136, 64, 64, 144, 144, 64, 64, 144, 144, 56, 56, 128, 128, 56, 56, 136, 136, 56, 56, 136, 136}),
                            .dimensions = {19, 12},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // batchSplit1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}),
                            .dimensions = {19},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
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
                        }, { // param7
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({5}),
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
                            .data = TestBuffer::createFromVector<float>({0.4f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param10
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.5f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // param11
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<float>({0.3f}),
                            .dimensions = {},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.0f,
                            .type = TestOperandType::FLOAT32,
                            .zeroPoint = 0
                        }, { // scoresOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({223, 207, 180, 223, 197, 223, 218, 207, 223, 208}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // roiOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint16_t>({0, 0, 80, 80, 48, 48, 128, 128, 16, 16, 96, 96, 16, 16, 96, 96, 64, 64, 144, 144, 8, 8, 88, 88, 0, 0, 16, 16, 56, 56, 136, 136, 24, 24, 104, 104, 0, 0, 16, 16}),
                            .dimensions = {10, 4},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.125f,
                            .type = TestOperandType::TENSOR_QUANT16_ASYMM,
                            .zeroPoint = 0
                        }, { // classesOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 2, 2, 1, 1, 1, 2, 2}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // batchSplitOut1
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<int32_t>({1, 1, 1, 1, 1, 3, 3, 3, 3, 3}),
                            .dimensions = {10},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_OUTPUT,
                            .numberOfConsumers = 0,
                            .scale = 0.0f,
                            .type = TestOperandType::TENSOR_INT32,
                            .zeroPoint = 0
                        }, { // scores1_new
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({218, 223, 203, 208, 198, 213, 188, 218, 223, 218, 193, 218, 208, 213, 208, 188, 188, 148, 188, 208, 168, 218, 183, 188, 218, 203, 198, 208, 198, 213, 218, 223, 203, 208, 213, 208, 188, 218, 223, 188, 188, 148, 178, 218, 208, 218, 203, 198, 218, 193, 218, 218, 183, 188, 188, 208, 168}),
                            .dimensions = {19, 3},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::SUBGRAPH_INPUT,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
                        }, { // dummy13
                            .channelQuant = {},
                            .data = TestBuffer::createFromVector<uint8_t>({128}),
                            .dimensions = {1},
                            .isIgnored = false,
                            .lifetime = TestOperandLifeTime::CONSTANT_COPY,
                            .numberOfConsumers = 1,
                            .scale = 0.01f,
                            .type = TestOperandType::TENSOR_QUANT8_ASYMM,
                            .zeroPoint = 128
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
                            .inputs = {13, 14, 15},
                            .outputs = {0},
                            .type = TestOperationType::ADD
                        }, {
                            .inputs = {0, 1, 2, 3, 4, 5, 6, 7, 8},
                            .outputs = {9, 10, 11, 12},
                            .type = TestOperationType::BOX_WITH_NMS_LIMIT
                        }},
                .outputIndexes = {9, 10, 11, 12}
            },
        .minSupportedVersion = TestHalVersion::V1_2,
        .referenced = {}
    };
    return model;
}

const auto dummy_test_model_quant8_all_inputs_as_internal_2 = TestModelManager::get().add("box_with_nms_limit_gaussian_quant8_all_inputs_as_internal_2", get_test_model_quant8_all_inputs_as_internal_2());

}  // namespace generated_tests::box_with_nms_limit_gaussian

