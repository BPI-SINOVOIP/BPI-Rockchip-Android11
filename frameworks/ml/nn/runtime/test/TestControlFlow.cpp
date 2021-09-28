/*
 * Copyright (C) 2019 The Android Open Source Project
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

#define LOG_TAG "TestControlFlow"

#include <android-base/logging.h>
#include <gtest/gtest.h>

#include "ControlFlow.h"
#include "TestNeuralNetworksWrapper.h"

namespace android {
namespace nn {
namespace {

using namespace test_wrapper;

constexpr uint64_t kMillisecondsInNanosecond = 1'000'000;
constexpr int32_t kNoActivation = ANEURALNETWORKS_FUSED_NONE;

class ControlFlowTest : public ::testing::Test {};

TEST_F(ControlFlowTest, InfiniteLoop) {
    // Expected result: execution aborted after the specified timeout.
    // Model: given n <= 1.0, never returns.
    //
    // i = 1.0
    // while i >= n:
    //     i = i + 1.0

    OperandType boolType(Type::TENSOR_BOOL8, {1});
    OperandType activationType(Type::INT32, {});
    OperandType counterType(Type::TENSOR_FLOAT32, {1});

    Model conditionModel;
    {
        uint32_t i = conditionModel.addOperand(&counterType);
        uint32_t n = conditionModel.addOperand(&counterType);
        uint32_t out = conditionModel.addOperand(&boolType);
        conditionModel.addOperation(ANEURALNETWORKS_GREATER_EQUAL, {i, n}, {out});
        conditionModel.identifyInputsAndOutputs({i, n}, {out});
        ASSERT_EQ(conditionModel.finish(), Result::NO_ERROR);
        ASSERT_TRUE(conditionModel.isValid());
    }

    Model bodyModel;
    {
        uint32_t i = bodyModel.addOperand(&counterType);
        uint32_t n = bodyModel.addOperand(&counterType);
        uint32_t one = bodyModel.addConstantOperand(&counterType, 1.0f);
        uint32_t noActivation = bodyModel.addConstantOperand(&activationType, kNoActivation);
        uint32_t iOut = bodyModel.addOperand(&counterType);
        bodyModel.addOperation(ANEURALNETWORKS_ADD, {i, one, noActivation}, {iOut});
        bodyModel.identifyInputsAndOutputs({i, n}, {iOut});
        ASSERT_EQ(bodyModel.finish(), Result::NO_ERROR);
        ASSERT_TRUE(bodyModel.isValid());
    }

    Model model;
    {
        uint32_t iInit = model.addConstantOperand(&counterType, 1.0f);
        uint32_t n = model.addOperand(&counterType);
        uint32_t conditionOperand = model.addModelOperand(&conditionModel);
        uint32_t bodyOperand = model.addModelOperand(&bodyModel);
        uint32_t iOut = model.addOperand(&counterType);
        model.addOperation(ANEURALNETWORKS_WHILE, {conditionOperand, bodyOperand, iInit, n},
                           {iOut});
        model.identifyInputsAndOutputs({n}, {iOut});
        ASSERT_EQ(model.finish(), Result::NO_ERROR);
        ASSERT_TRUE(model.isValid());
    }

    Compilation compilation(&model);
    ASSERT_EQ(compilation.finish(), Result::NO_ERROR);

    float input = 0;
    float output;
    Execution execution(&compilation);
    ASSERT_EQ(execution.setInput(0, &input), Result::NO_ERROR);
    ASSERT_EQ(execution.setOutput(0, &output), Result::NO_ERROR);
    ASSERT_EQ(execution.setLoopTimeout(1 * kMillisecondsInNanosecond), Result::NO_ERROR);
    Result result = execution.compute();
    ASSERT_TRUE(result == Result::MISSED_DEADLINE_TRANSIENT ||
                result == Result::MISSED_DEADLINE_PERSISTENT)
            << "result = " << static_cast<int>(result);
}

TEST_F(ControlFlowTest, GetLoopTimeouts) {
    uint64_t defaultTimeout = ANeuralNetworks_getDefaultLoopTimeout();
    uint64_t maximumTimeout = ANeuralNetworks_getMaximumLoopTimeout();
    ASSERT_EQ(defaultTimeout, operation_while::kTimeoutNsDefault);
    ASSERT_EQ(maximumTimeout, operation_while::kTimeoutNsMaximum);
}

}  // end namespace
}  // namespace nn
}  // namespace android
