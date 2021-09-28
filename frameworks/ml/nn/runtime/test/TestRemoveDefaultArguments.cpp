/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "GeneratedTestUtils.h"
#include "Manager.h"
#include "SampleDriverPartial.h"
#include "TestNeuralNetworksWrapper.h"
#include "Utils.h"

namespace generated_tests::avg_pool_v1_2 {
const test_helper::TestModel& get_test_model_nhwc();
const test_helper::TestModel& get_test_model_nchw();
const test_helper::TestModel& get_test_model_nhwc_5();
}  // namespace generated_tests::avg_pool_v1_2

namespace generated_tests::conv2d_dilation {
const test_helper::TestModel& get_test_model_nhwc();
const test_helper::TestModel& get_test_model_nchw();
const test_helper::TestModel& get_test_model_valid_padding_nhwc();
}  // namespace generated_tests::conv2d_dilation

namespace generated_tests::depthwise_conv2d_dilation {
const test_helper::TestModel& get_test_model_nhwc();
const test_helper::TestModel& get_test_model_valid_padding_nhwc();
}  // namespace generated_tests::depthwise_conv2d_dilation

namespace generated_tests::depth_to_space_v1_2 {
const test_helper::TestModel& get_test_model_nhwc();
}  // namespace generated_tests::depth_to_space_v1_2

namespace generated_tests::l2_normalization_axis {
const test_helper::TestModel& get_test_model_dim4_axis3_neg();
}  // namespace generated_tests::l2_normalization_axis

namespace generated_tests::l2_pool_v1_2 {
const test_helper::TestModel& get_test_model_nhwc();
const test_helper::TestModel& get_test_model_nhwc_2();
}  // namespace generated_tests::l2_pool_v1_2

namespace generated_tests::local_response_normalization_v1_2 {
const test_helper::TestModel& get_test_model_axis_dim2_axis1_neg();
}  // namespace generated_tests::local_response_normalization_v1_2

namespace generated_tests::max_pool_v1_2 {
const test_helper::TestModel& get_test_model_nhwc();
const test_helper::TestModel& get_test_model_nhwc_4();
}  // namespace generated_tests::max_pool_v1_2

namespace generated_tests::resize_bilinear_v1_2 {
const test_helper::TestModel& get_test_model_shape_nhwc();
}  // namespace generated_tests::resize_bilinear_v1_2

namespace generated_tests::resize_bilinear_v1_3 {
const test_helper::TestModel& get_test_model_align_corners_2x2_to_1x1();
}  // namespace generated_tests::resize_bilinear_v1_3

namespace generated_tests::softmax_v1_2 {
const test_helper::TestModel& get_test_model_axis_quant8_dim1_axis0_neg();
}  // namespace generated_tests::softmax_v1_2

namespace generated_tests::space_to_depth_v1_2 {
const test_helper::TestModel& get_test_model_nhwc();
}  // namespace generated_tests::space_to_depth_v1_2

namespace generated_tests::batch_to_space_v1_2 {
const test_helper::TestModel& get_test_model_nhwc();
}  // namespace generated_tests::batch_to_space_v1_2

namespace generated_tests::space_to_batch_v1_2 {
const test_helper::TestModel& get_test_model_nhwc();
}  // namespace generated_tests::space_to_batch_v1_2

namespace generated_tests::resize_nearest_neighbor_v1_3 {
const test_helper::TestModel& get_test_model_align_corners_2x2_to_1x1();
}  // namespace generated_tests::resize_nearest_neighbor_v1_3

namespace android::nn {
namespace {

using namespace hal;
using sample_driver::SampleDriverPartial;
using Result = test_wrapper::Result;
using WrapperOperandType = test_wrapper::OperandType;
using WrapperCompilation = test_wrapper::Compilation;
using WrapperType = test_wrapper::Type;
using WrapperModel = test_wrapper::Model;

const char* kTestDriverName = "nnapi-test";

// A driver that rejects operations based solely on the input count.
class TestDriver : public SampleDriverPartial {
   public:
    TestDriver() : SampleDriverPartial(kTestDriverName) {}

    Return<void> getCapabilities_1_3(getCapabilities_1_3_cb cb) override {
        cb(V1_3::ErrorStatus::NONE, {/* Dummy zero-filled capabilities. */});
        return Void();
    }

    void setSupportedInputCount(uint32_t count) { mSupportedInputCount = count; }

   private:
    std::vector<bool> getSupportedOperationsImpl(const Model& model) const override {
        std::vector<bool> supported(model.main.operations.size());
        std::transform(model.main.operations.begin(), model.main.operations.end(),
                       supported.begin(), [this](const Operation& operation) {
                           SCOPED_TRACE("operation = " + toString(operation.type));
                           EXPECT_EQ(operation.inputs.size(), mSupportedInputCount);
                           return operation.inputs.size() == mSupportedInputCount;
                       });
        return supported;
    }

    uint32_t mSupportedInputCount = std::numeric_limits<uint32_t>::max();
};

class TestRemoveDefaultArguments : public ::testing::Test {
    virtual void SetUp() {
        // skip the tests if useCpuOnly = 1
        if (DeviceManager::get()->getUseCpuOnly()) {
            GTEST_SKIP();
        }
        mTestDriver = new TestDriver();
        DeviceManager::get()->forTest_registerDevice(kTestDriverName, mTestDriver);
        mTestDevice = getDeviceByName(kTestDriverName);
        ASSERT_NE(mTestDevice, nullptr);
    }

    virtual void TearDown() { DeviceManager::get()->forTest_reInitializeDeviceList(); }

   protected:
    void test(const test_helper::TestModel& testModel, uint32_t originalInputCount,
              uint32_t expectedInputCount) {
        ASSERT_EQ(testModel.main.operations.size(), 1u);
        ASSERT_EQ(testModel.main.operations[0].inputs.size(), originalInputCount);

        mTestDriver->setSupportedInputCount(expectedInputCount);

        generated_tests::GeneratedModel model;
        generated_tests::createModel(testModel, &model);
        ASSERT_TRUE(model.isValid());
        ASSERT_EQ(model.finish(), Result::NO_ERROR);

        auto [result, compilation] = WrapperCompilation::createForDevice(&model, mTestDevice);
        ASSERT_EQ(result, Result::NO_ERROR);
        ASSERT_EQ(compilation.finish(), Result::NO_ERROR);
    }

   private:
    ANeuralNetworksDevice* getDeviceByName(const std::string& name) {
        ANeuralNetworksDevice* result = nullptr;
        uint32_t numDevices = 0;
        EXPECT_EQ(ANeuralNetworks_getDeviceCount(&numDevices), ANEURALNETWORKS_NO_ERROR);
        EXPECT_GE(numDevices, 1u);
        for (uint32_t i = 0; i < numDevices; i++) {
            ANeuralNetworksDevice* device = nullptr;
            EXPECT_EQ(ANeuralNetworks_getDevice(i, &device), ANEURALNETWORKS_NO_ERROR);
            const char* buffer = nullptr;
            EXPECT_EQ(ANeuralNetworksDevice_getName(device, &buffer), ANEURALNETWORKS_NO_ERROR);
            if (name == buffer) {
                EXPECT_EQ(result, nullptr) << "multiple devices named " << name;
                result = device;
            }
        }
        return result;
    }

    sp<TestDriver> mTestDriver;
    ANeuralNetworksDevice* mTestDevice;
};

TEST_F(TestRemoveDefaultArguments, AVERAGE_POOL_2D_11_inputs_to_10_inputs) {
    const uint32_t originalInputCount = 11;
    const uint32_t expectedInputCount = 10;
    test(::generated_tests::avg_pool_v1_2::get_test_model_nhwc(), originalInputCount,
         expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, AVERAGE_POOL_2D_11_inputs_no_default_values) {
    const uint32_t originalInputCount = 11;
    const uint32_t expectedInputCount = 11;
    test(::generated_tests::avg_pool_v1_2::get_test_model_nchw(), originalInputCount,
         expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, AVERAGE_POOL_2D_8_inputs_to_7_inputs) {
    const uint32_t originalInputCount = 8;
    const uint32_t expectedInputCount = 7;
    test(::generated_tests::avg_pool_v1_2::get_test_model_nhwc_5(), originalInputCount,
         expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, CONV_2D_13_inputs_to_10_inputs) {
    const uint32_t originalInputCount = 13;
    const uint32_t expectedInputCount = 10;
    test(::generated_tests::conv2d_dilation::get_test_model_nhwc(), originalInputCount,
         expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, CONV_2D_13_inputs_to_11_inputs) {
    const uint32_t originalInputCount = 13;
    const uint32_t expectedInputCount = 11;
    test(::generated_tests::conv2d_dilation::get_test_model_nchw(), originalInputCount,
         expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, CONV_2D_10_inputs_to_7_inputs) {
    const uint32_t originalInputCount = 10;
    const uint32_t expectedInputCount = 7;
    test(::generated_tests::conv2d_dilation::get_test_model_valid_padding_nhwc(),
         originalInputCount, expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, DEPTHWISE_CONV_3D_14_inputs_to_11_inputs) {
    const uint32_t originalInputCount = 14;
    const uint32_t expectedInputCount = 11;
    test(::generated_tests::depthwise_conv2d_dilation::get_test_model_nhwc(), originalInputCount,
         expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, DEPTHWISE_CONV_2D_11_inputs_to_8_inputs) {
    const uint32_t originalInputCount = 11;
    const uint32_t expectedInputCount = 8;
    test(::generated_tests::depthwise_conv2d_dilation::get_test_model_valid_padding_nhwc(),
         originalInputCount, expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, DEPTH_TO_SPACE_3_inputs_to_2_inputs) {
    const uint32_t originalInputCount = 3;
    const uint32_t expectedInputCount = 2;
    test(::generated_tests::depth_to_space_v1_2::get_test_model_nhwc(), originalInputCount,
         expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, L2_NORMALIZATION_2_inputs_to_1_input) {
    const uint32_t originalInputCount = 2;
    const uint32_t expectedInputCount = 1;
    test(::generated_tests::l2_normalization_axis::get_test_model_dim4_axis3_neg(),
         originalInputCount, expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, L2_POOL_2D_11_inputs_to_10_inputs) {
    const uint32_t originalInputCount = 11;
    const uint32_t expectedInputCount = 10;
    test(::generated_tests::l2_pool_v1_2::get_test_model_nhwc(), originalInputCount,
         expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, L2_POOL_2D_8_inputs_to_7_inputs) {
    const uint32_t originalInputCount = 8;
    const uint32_t expectedInputCount = 7;
    test(::generated_tests::l2_pool_v1_2::get_test_model_nhwc_2(), originalInputCount,
         expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, LOCAL_RESPONSE_NORMALIZATION_6_inputs_to_5_inputs) {
    const uint32_t originalInputCount = 6;
    const uint32_t expectedInputCount = 5;
    test(::generated_tests::local_response_normalization_v1_2::get_test_model_axis_dim2_axis1_neg(),
         originalInputCount, expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, MAX_POOL_2D_11_inputs_to_10_inputs) {
    const uint32_t originalInputCount = 11;
    const uint32_t expectedInputCount = 10;
    test(::generated_tests::max_pool_v1_2::get_test_model_nhwc(), originalInputCount,
         expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, MAX_POOL_2D_8_inputs_to_7_inputs) {
    const uint32_t originalInputCount = 8;
    const uint32_t expectedInputCount = 7;
    test(::generated_tests::max_pool_v1_2::get_test_model_nhwc_4(), originalInputCount,
         expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, RESIZE_BILINEAR_by_shape_6_inputs_to_5_inputs) {
    const uint32_t originalInputCount = 6;
    const uint32_t expectedInputCount = 5;
    test(::generated_tests::resize_bilinear_v1_3::get_test_model_align_corners_2x2_to_1x1(),
         originalInputCount, expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, RESIZE_BILINEAR_by_shape_4_inputs_to_3_inputs) {
    const uint32_t originalInputCount = 4;
    const uint32_t expectedInputCount = 3;
    test(::generated_tests::resize_bilinear_v1_2::get_test_model_shape_nhwc(), originalInputCount,
         expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, SOFTMAX_3_inputs_to_2_inputs) {
    const uint32_t originalInputCount = 3;
    const uint32_t expectedInputCount = 2;
    test(::generated_tests::softmax_v1_2::get_test_model_axis_quant8_dim1_axis0_neg(),
         originalInputCount, expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, SPACE_TO_DEPTH_3_inputs_to_2_inputs) {
    const uint32_t originalInputCount = 3;
    const uint32_t expectedInputCount = 2;
    test(::generated_tests::space_to_depth_v1_2::get_test_model_nhwc(), originalInputCount,
         expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, BATCH_TO_SPACE_ND_3_inputs_to_2_inputs) {
    const uint32_t originalInputCount = 3;
    const uint32_t expectedInputCount = 2;
    test(::generated_tests::batch_to_space_v1_2::get_test_model_nhwc(), originalInputCount,
         expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, SPACE_TO_BATCH_ND_4_inputs_to_3_inputs) {
    const uint32_t originalInputCount = 4;
    const uint32_t expectedInputCount = 3;
    test(::generated_tests::space_to_batch_v1_2::get_test_model_nhwc(), originalInputCount,
         expectedInputCount);
}

TEST_F(TestRemoveDefaultArguments, RESIZE_NEAREST_NEIGHBOR_by_shape_6_inputs_to_5_inputs) {
    const uint32_t originalInputCount = 6;
    const uint32_t expectedInputCount = 5;
    test(::generated_tests::resize_nearest_neighbor_v1_3::get_test_model_align_corners_2x2_to_1x1(),
         originalInputCount, expectedInputCount);
}

}  // namespace
}  // namespace android::nn
