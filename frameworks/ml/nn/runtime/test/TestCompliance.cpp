/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "GeneratedTestUtils.h"
#include "HalInterfaces.h"
#include "Memory.h"
#include "MemoryUtils.h"
#include "ModelBuilder.h"
#include "TestNeuralNetworksWrapper.h"
#include "Utils.h"

namespace android::nn::compliance_test {

using namespace hal;
using namespace test_helper;
using HidlModel = V1_3::Model;
using WrapperModel = test_wrapper::Model;
using WrapperOperandType = test_wrapper::OperandType;
using WrapperType = test_wrapper::Type;

// Tag for the compilance tests
class ComplianceTest : public ::testing::Test {};

// Creates a HIDL model from a wrapper model.
static HidlModel createHidlModel(const WrapperModel& wrapperModel) {
    auto modelBuilder = reinterpret_cast<const ModelBuilder*>(wrapperModel.getHandle());
    EXPECT_TRUE(modelBuilder->isFinished());
    EXPECT_TRUE(modelBuilder->isValid());
    return modelBuilder->makeHidlModel();
}

static void testAvailableSinceV1_3(const WrapperModel& wrapperModel) {
    HidlModel hidlModel = createHidlModel(wrapperModel);
    ASSERT_FALSE(compliantWithV1_2(hidlModel));
    ASSERT_FALSE(compliantWithV1_1(hidlModel));
    ASSERT_FALSE(compliantWithV1_0(hidlModel));
}

static void testAvailableSinceV1_2(const WrapperModel& wrapperModel) {
    HidlModel hidlModel = createHidlModel(wrapperModel);
    ASSERT_TRUE(compliantWithV1_2(hidlModel));
    ASSERT_FALSE(compliantWithV1_1(hidlModel));
    ASSERT_FALSE(compliantWithV1_0(hidlModel));
}

static void testAvailableSinceV1_1(const WrapperModel& wrapperModel) {
    HidlModel hidlModel = createHidlModel(wrapperModel);
    ASSERT_TRUE(compliantWithV1_2(hidlModel));
    ASSERT_TRUE(compliantWithV1_1(hidlModel));
    ASSERT_FALSE(compliantWithV1_0(hidlModel));
}

static void testAvailableSinceV1_0(const WrapperModel& wrapperModel) {
    HidlModel hidlModel = createHidlModel(wrapperModel);
    ASSERT_TRUE(compliantWithV1_2(hidlModel));
    ASSERT_TRUE(compliantWithV1_1(hidlModel));
    ASSERT_TRUE(compliantWithV1_0(hidlModel));
}

static void testAvailableSinceV1_2(const Request& request) {
    ASSERT_FALSE(compliantWithV1_0(request));
    ASSERT_TRUE(compliantWithV1_2(request));
}

static void testAvailableSinceV1_3(const Request& request) {
    ASSERT_FALSE(compliantWithV1_0(request));
    ASSERT_FALSE(compliantWithV1_2(request));
}

static const WrapperOperandType kTypeTensorFloat(WrapperType::TENSOR_FLOAT32, {1});
static const WrapperOperandType kTypeTensorFloatRank0(WrapperType::TENSOR_FLOAT32, {});
static const WrapperOperandType kTypeInt32(WrapperType::INT32, {});

const int32_t kNoActivation = ANEURALNETWORKS_FUSED_NONE;

TEST_F(ComplianceTest, Rank0TensorModelInput) {
    // A simple ADD operation: op1 ADD op2 = op3, with op1 and op2 of rank 0.
    WrapperModel model;
    auto op1 = model.addOperand(&kTypeTensorFloatRank0);
    auto op2 = model.addOperand(&kTypeTensorFloatRank0);
    auto op3 = model.addOperand(&kTypeTensorFloat);
    auto act = model.addConstantOperand(&kTypeInt32, kNoActivation);
    model.addOperation(ANEURALNETWORKS_ADD, {op1, op2, act}, {op3});
    model.identifyInputsAndOutputs({op1, op2}, {op3});
    ASSERT_TRUE(model.isValid());
    model.finish();
    testAvailableSinceV1_2(model);
}

TEST_F(ComplianceTest, Rank0TensorModelOutput) {
    // A simple ADD operation: op1 ADD op2 = op3, with op3 of rank 0.
    WrapperModel model;
    auto op1 = model.addOperand(&kTypeTensorFloat);
    auto op2 = model.addOperand(&kTypeTensorFloat);
    auto op3 = model.addOperand(&kTypeTensorFloatRank0);
    auto act = model.addConstantOperand(&kTypeInt32, kNoActivation);
    model.addOperation(ANEURALNETWORKS_ADD, {op1, op2, act}, {op3});
    model.identifyInputsAndOutputs({op1, op2}, {op3});
    ASSERT_TRUE(model.isValid());
    model.finish();
    testAvailableSinceV1_2(model);
}

TEST_F(ComplianceTest, Rank0TensorTemporaryVariable) {
    // Two ADD operations: op1 ADD op2 = op3, op3 ADD op4 = op5, with op3 of rank 0.
    WrapperModel model;
    auto op1 = model.addOperand(&kTypeTensorFloat);
    auto op2 = model.addOperand(&kTypeTensorFloat);
    auto op3 = model.addOperand(&kTypeTensorFloatRank0);
    auto op4 = model.addOperand(&kTypeTensorFloat);
    auto op5 = model.addOperand(&kTypeTensorFloat);
    auto act = model.addConstantOperand(&kTypeInt32, kNoActivation);
    model.addOperation(ANEURALNETWORKS_ADD, {op1, op2, act}, {op3});
    model.addOperation(ANEURALNETWORKS_ADD, {op3, op4, act}, {op5});
    model.identifyInputsAndOutputs({op1, op2, op4}, {op5});
    ASSERT_TRUE(model.isValid());
    model.finish();
    testAvailableSinceV1_2(model);
}

TEST_F(ComplianceTest, HardwareBufferModel) {
    const size_t memorySize = 20;
    AHardwareBuffer_Desc desc{
            .width = memorySize,
            .height = 1,
            .layers = 1,
            .format = AHARDWAREBUFFER_FORMAT_BLOB,
            .usage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,
    };

    AHardwareBuffer* buffer = nullptr;
    ASSERT_EQ(AHardwareBuffer_allocate(&desc, &buffer), 0);
    test_wrapper::Memory memory(buffer);
    ASSERT_TRUE(memory.isValid());

    // A simple ADD operation: op1 ADD op2 = op3, with op2 using a const hardware buffer.
    WrapperModel model;
    auto op1 = model.addOperand(&kTypeTensorFloat);
    auto op2 = model.addOperand(&kTypeTensorFloat);
    auto op3 = model.addOperand(&kTypeTensorFloat);
    auto act = model.addConstantOperand(&kTypeInt32, kNoActivation);
    model.setOperandValueFromMemory(op2, &memory, 0, sizeof(float));
    model.addOperation(ANEURALNETWORKS_ADD, {op1, op2, act}, {op3});
    model.identifyInputsAndOutputs({op1}, {op3});
    ASSERT_TRUE(model.isValid());
    model.finish();
    testAvailableSinceV1_2(model);

    AHardwareBuffer_release(buffer);
}

TEST_F(ComplianceTest, HardwareBufferRequest) {
    const auto [n, ahwb] = MemoryRuntimeAHWB::create(1024);
    ASSERT_EQ(n, ANEURALNETWORKS_NO_ERROR);
    Request::MemoryPool sharedMemoryPool, ahwbMemoryPool = ahwb->getMemoryPool();
    sharedMemoryPool.hidlMemory(allocateSharedMemory(1024));
    ASSERT_TRUE(sharedMemoryPool.hidlMemory().valid());
    ASSERT_TRUE(ahwbMemoryPool.hidlMemory().valid());

    // AHardwareBuffer as input.
    testAvailableSinceV1_2(Request{
            .inputs = {{.hasNoValue = false, .location = {.poolIndex = 0}, .dimensions = {}}},
            .outputs = {{.hasNoValue = false, .location = {.poolIndex = 1}, .dimensions = {}}},
            .pools = {ahwbMemoryPool, sharedMemoryPool},
    });

    // AHardwareBuffer as output.
    testAvailableSinceV1_2(Request{
            .inputs = {{.hasNoValue = false, .location = {.poolIndex = 0}, .dimensions = {}}},
            .outputs = {{.hasNoValue = false, .location = {.poolIndex = 1}, .dimensions = {}}},
            .pools = {sharedMemoryPool, ahwbMemoryPool},
    });
}

TEST_F(ComplianceTest, DeviceMemory) {
    Request::MemoryPool sharedMemoryPool, deviceMemoryPool;
    sharedMemoryPool.hidlMemory(allocateSharedMemory(1024));
    ASSERT_TRUE(sharedMemoryPool.hidlMemory().valid());
    deviceMemoryPool.token(1);

    // Device memory as input.
    testAvailableSinceV1_3(Request{
            .inputs = {{.hasNoValue = false, .location = {.poolIndex = 0}, .dimensions = {}}},
            .outputs = {{.hasNoValue = false, .location = {.poolIndex = 1}, .dimensions = {}}},
            .pools = {deviceMemoryPool, sharedMemoryPool},
    });

    // Device memory as output.
    testAvailableSinceV1_3(Request{
            .inputs = {{.hasNoValue = false, .location = {.poolIndex = 0}, .dimensions = {}}},
            .outputs = {{.hasNoValue = false, .location = {.poolIndex = 1}, .dimensions = {}}},
            .pools = {sharedMemoryPool, deviceMemoryPool},
    });
}

class GeneratedComplianceTest : public generated_tests::GeneratedTestBase {};

TEST_P(GeneratedComplianceTest, Test) {
    generated_tests::GeneratedModel model;
    generated_tests::createModel(testModel, &model);
    ASSERT_TRUE(model.isValid());
    model.finish();
    switch (testModel.minSupportedVersion) {
        case TestHalVersion::V1_0:
            testAvailableSinceV1_0(model);
            break;
        case TestHalVersion::V1_1:
            testAvailableSinceV1_1(model);
            break;
        case TestHalVersion::V1_2:
            testAvailableSinceV1_2(model);
            break;
        case TestHalVersion::V1_3:
            testAvailableSinceV1_3(model);
            break;
        case TestHalVersion::UNKNOWN:
            FAIL();
    }
}

INSTANTIATE_GENERATED_TEST(GeneratedComplianceTest, [](const TestModel& testModel) {
    return !testModel.expectFailure && testModel.minSupportedVersion != TestHalVersion::UNKNOWN;
});

}  // namespace android::nn::compliance_test
