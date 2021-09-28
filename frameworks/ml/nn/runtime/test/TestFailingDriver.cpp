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

#include <memory>
#include <vector>

#include "CompilationBuilder.h"
#include "ExecutionPlan.h"
#include "Manager.h"
#include "SampleDriverPartial.h"
#include "TestNeuralNetworksWrapper.h"

namespace android::nn {
namespace {

using namespace hal;
using sample_driver::SampleDriverPartial;
using Result = test_wrapper::Result;
using WrapperOperandType = test_wrapper::OperandType;
using WrapperCompilation = test_wrapper::Compilation;
using WrapperExecution = test_wrapper::Execution;
using WrapperType = test_wrapper::Type;
using WrapperModel = test_wrapper::Model;

class EmptyOperationResolver : public IOperationResolver {
   public:
    const OperationRegistration* findOperation(OperationType) const override { return nullptr; }
};

const char* kTestDriverName = "nnapi-test-sqrt-failing";

// A driver that only supports SQRT and fails during execution.
class FailingTestDriver : public SampleDriverPartial {
   public:
    // EmptyOperationResolver causes execution to fail.
    FailingTestDriver() : SampleDriverPartial(kTestDriverName, &mEmptyOperationResolver) {}

    Return<void> getCapabilities_1_3(getCapabilities_1_3_cb cb) override {
        cb(V1_3::ErrorStatus::NONE,
           {.operandPerformance = {{.type = OperandType::TENSOR_FLOAT32,
                                    .info = {.execTime = 0.1,  // Faster than CPU.
                                             .powerUsage = 0.1}}}});
        return Void();
    }

   private:
    std::vector<bool> getSupportedOperationsImpl(const Model& model) const override {
        std::vector<bool> supported(model.main.operations.size());
        std::transform(
                model.main.operations.begin(), model.main.operations.end(), supported.begin(),
                [](const Operation& operation) { return operation.type == OperationType::SQRT; });
        return supported;
    }

    const EmptyOperationResolver mEmptyOperationResolver;
};

class FailingDriverTest : public ::testing::Test {
    virtual void SetUp() {
        DeviceManager* deviceManager = DeviceManager::get();
        if (deviceManager->getUseCpuOnly() ||
            !DeviceManager::partitioningAllowsFallback(deviceManager->getPartitioning())) {
            GTEST_SKIP();
        }
        mTestDevice =
                DeviceManager::forTest_makeDriverDevice(kTestDriverName, new FailingTestDriver());
        deviceManager->forTest_setDevices({
                mTestDevice,
                DeviceManager::getCpuDevice(),
        });
    }

    virtual void TearDown() { DeviceManager::get()->forTest_reInitializeDeviceList(); }

   protected:
    std::shared_ptr<Device> mTestDevice;
};

// Regression test for b/152623150.
TEST_F(FailingDriverTest, FailAfterInterpretedWhile) {
    // Model:
    //     f = input0
    //     b = input1
    //     while CAST(b):  # Identity cast.
    //         f = CAST(f)
    //     # FailingTestDriver fails here. When partial CPU fallback happens,
    //     # it should not loop forever.
    //     output0 = SQRT(f)

    WrapperOperandType floatType(WrapperType::TENSOR_FLOAT32, {2});
    WrapperOperandType boolType(WrapperType::TENSOR_BOOL8, {1});

    WrapperModel conditionModel;
    {
        uint32_t f = conditionModel.addOperand(&floatType);
        uint32_t b = conditionModel.addOperand(&boolType);
        uint32_t out = conditionModel.addOperand(&boolType);
        conditionModel.addOperation(ANEURALNETWORKS_CAST, {b}, {out});
        conditionModel.identifyInputsAndOutputs({f, b}, {out});
        ASSERT_EQ(conditionModel.finish(), Result::NO_ERROR);
        ASSERT_TRUE(conditionModel.isValid());
    }

    WrapperModel bodyModel;
    {
        uint32_t f = bodyModel.addOperand(&floatType);
        uint32_t b = bodyModel.addOperand(&boolType);
        uint32_t out = bodyModel.addOperand(&floatType);
        bodyModel.addOperation(ANEURALNETWORKS_CAST, {f}, {out});
        bodyModel.identifyInputsAndOutputs({f, b}, {out});
        ASSERT_EQ(bodyModel.finish(), Result::NO_ERROR);
        ASSERT_TRUE(bodyModel.isValid());
    }

    WrapperModel model;
    {
        uint32_t fInput = model.addOperand(&floatType);
        uint32_t bInput = model.addOperand(&boolType);
        uint32_t fTmp = model.addOperand(&floatType);
        uint32_t fSqrt = model.addOperand(&floatType);
        uint32_t cond = model.addModelOperand(&conditionModel);
        uint32_t body = model.addModelOperand(&bodyModel);
        model.addOperation(ANEURALNETWORKS_WHILE, {cond, body, fInput, bInput}, {fTmp});
        model.addOperation(ANEURALNETWORKS_SQRT, {fTmp}, {fSqrt});
        model.identifyInputsAndOutputs({fInput, bInput}, {fSqrt});
        ASSERT_TRUE(model.isValid());
        ASSERT_EQ(model.finish(), Result::NO_ERROR);
    }

    WrapperCompilation compilation(&model);
    ASSERT_EQ(compilation.finish(), Result::NO_ERROR);

    const CompilationBuilder* compilationBuilder =
            reinterpret_cast<CompilationBuilder*>(compilation.getHandle());
    const ExecutionPlan& plan = compilationBuilder->forTest_getExecutionPlan();
    const std::vector<std::shared_ptr<LogicalStep>>& steps = plan.forTest_compoundGetSteps();
    ASSERT_EQ(steps.size(), 6u);
    ASSERT_TRUE(steps[0]->isWhile());
    ASSERT_TRUE(steps[1]->isExecution());
    ASSERT_EQ(steps[1]->executionStep()->getDevice(), DeviceManager::getCpuDevice());
    ASSERT_TRUE(steps[2]->isGoto());
    ASSERT_TRUE(steps[3]->isExecution());
    ASSERT_EQ(steps[3]->executionStep()->getDevice(), DeviceManager::getCpuDevice());
    ASSERT_TRUE(steps[4]->isGoto());
    ASSERT_TRUE(steps[5]->isExecution());
    ASSERT_EQ(steps[5]->executionStep()->getDevice(), mTestDevice);

    WrapperExecution execution(&compilation);
    const float fInput[] = {12 * 12, 5 * 5};
    const bool8 bInput = false;
    float fSqrt[] = {0, 0};
    ASSERT_EQ(execution.setInput(0, &fInput), Result::NO_ERROR);
    ASSERT_EQ(execution.setInput(1, &bInput), Result::NO_ERROR);
    ASSERT_EQ(execution.setOutput(0, &fSqrt), Result::NO_ERROR);
    ASSERT_EQ(execution.compute(), Result::NO_ERROR);
    ASSERT_EQ(fSqrt[0], 12);
    ASSERT_EQ(fSqrt[1], 5);
}

// Regression test for b/155923033.
TEST_F(FailingDriverTest, SimplePlan) {
    // Model:
    //     output0 = SQRT(input0)
    //
    // This results in a SIMPLE execution plan. When FailingTestDriver fails,
    // partial CPU fallback should complete the execution.

    WrapperOperandType floatType(WrapperType::TENSOR_FLOAT32, {2});

    WrapperModel model;
    {
        uint32_t fInput = model.addOperand(&floatType);
        uint32_t fSqrt = model.addOperand(&floatType);
        model.addOperation(ANEURALNETWORKS_SQRT, {fInput}, {fSqrt});
        model.identifyInputsAndOutputs({fInput}, {fSqrt});
        ASSERT_TRUE(model.isValid());
        ASSERT_EQ(model.finish(), Result::NO_ERROR);
    }

    WrapperCompilation compilation(&model);
    ASSERT_EQ(compilation.finish(), Result::NO_ERROR);

    const CompilationBuilder* compilationBuilder =
            reinterpret_cast<CompilationBuilder*>(compilation.getHandle());
    const ExecutionPlan& plan = compilationBuilder->forTest_getExecutionPlan();
    ASSERT_TRUE(plan.isSimple());

    WrapperExecution execution(&compilation);
    const float fInput[] = {12 * 12, 5 * 5};
    float fSqrt[] = {0, 0};
    ASSERT_EQ(execution.setInput(0, &fInput), Result::NO_ERROR);
    ASSERT_EQ(execution.setOutput(0, &fSqrt), Result::NO_ERROR);
    ASSERT_EQ(execution.compute(), Result::NO_ERROR);
    ASSERT_EQ(fSqrt[0], 12);
    ASSERT_EQ(fSqrt[1], 5);
}

}  // namespace
}  // namespace android::nn
