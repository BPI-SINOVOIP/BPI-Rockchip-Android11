/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "TestNeuralNetworksWrapper.h"

//#include <android-base/logging.h>
#include <gtest/gtest.h>

using namespace android::nn::test_wrapper;

namespace {

typedef float Matrix3x4[3][4];
typedef float Matrix4[4];

const int32_t kNoActivation = ANEURALNETWORKS_FUSED_NONE;

class TrivialTest : public ::testing::Test {
   protected:
    virtual void SetUp() {}

    const Matrix3x4 matrix1 = {{1.f, 2.f, 3.f, 4.f}, {5.f, 6.f, 7.f, 8.f}, {9.f, 10.f, 11.f, 12.f}};
    const Matrix3x4 matrix2 = {{100.f, 200.f, 300.f, 400.f},
                               {500.f, 600.f, 700.f, 800.f},
                               {900.f, 1000.f, 1100.f, 1200.f}};
    const Matrix4 matrix2b = {100.f, 200.f, 300.f, 400.f};
    const Matrix3x4 matrix3 = {
            {20.f, 30.f, 40.f, 50.f}, {21.f, 22.f, 23.f, 24.f}, {31.f, 32.f, 33.f, 34.f}};
    const Matrix3x4 expected2 = {{101.f, 202.f, 303.f, 404.f},
                                 {505.f, 606.f, 707.f, 808.f},
                                 {909.f, 1010.f, 1111.f, 1212.f}};
    const Matrix3x4 expected2b = {{101.f, 202.f, 303.f, 404.f},
                                  {105.f, 206.f, 307.f, 408.f},
                                  {109.f, 210.f, 311.f, 412.f}};
    const Matrix3x4 expected2c = {{100.f, 400.f, 900.f, 1600.f},
                                  {500.f, 1200.f, 2100.f, 3200.f},
                                  {900.f, 2000.f, 3300.f, 4800.f}};

    const Matrix3x4 expected3 = {{121.f, 232.f, 343.f, 454.f},
                                 {526.f, 628.f, 730.f, 832.f},
                                 {940.f, 1042.f, 1144.f, 1246.f}};
    const Matrix3x4 expected3b = {
            {22.f, 34.f, 46.f, 58.f}, {31.f, 34.f, 37.f, 40.f}, {49.f, 52.f, 55.f, 58.f}};
};

// Create a model that can add two tensors using a one node graph.
void CreateAddTwoTensorModel(Model* model) {
    OperandType matrixType(Type::TENSOR_FLOAT32, {3, 4});
    OperandType scalarType(Type::INT32, {});
    auto a = model->addOperand(&matrixType);
    auto b = model->addOperand(&matrixType);
    auto c = model->addOperand(&matrixType);
    auto d = model->addConstantOperand(&scalarType, kNoActivation);
    model->addOperation(ANEURALNETWORKS_ADD, {a, b, d}, {c});
    model->identifyInputsAndOutputs({a, b}, {c});
    ASSERT_TRUE(model->isValid());
    model->finish();
}

// Create a model that can add three tensors using a two node graph,
// with one tensor set as part of the model.
void CreateAddThreeTensorModel(Model* model, const Matrix3x4 bias) {
    OperandType matrixType(Type::TENSOR_FLOAT32, {3, 4});
    OperandType scalarType(Type::INT32, {});
    auto a = model->addOperand(&matrixType);
    auto b = model->addOperand(&matrixType);
    auto c = model->addOperand(&matrixType);
    auto d = model->addOperand(&matrixType);
    auto e = model->addOperand(&matrixType);
    auto f = model->addConstantOperand(&scalarType, kNoActivation);
    model->setOperandValue(e, bias, sizeof(Matrix3x4));
    model->addOperation(ANEURALNETWORKS_ADD, {a, c, f}, {b});
    model->addOperation(ANEURALNETWORKS_ADD, {b, e, f}, {d});
    model->identifyInputsAndOutputs({c, a}, {d});
    ASSERT_TRUE(model->isValid());
    model->finish();
}

// Check that the values are the same. This works only if dealing with integer
// value, otherwise we should accept values that are similar if not exact.
int CompareMatrices(const Matrix3x4& expected, const Matrix3x4& actual) {
    int errors = 0;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            if (expected[i][j] != actual[i][j]) {
                printf("expected[%d][%d] != actual[%d][%d], %f != %f\n", i, j, i, j,
                       static_cast<double>(expected[i][j]), static_cast<double>(actual[i][j]));
                errors++;
            }
        }
    }
    return errors;
}

TEST_F(TrivialTest, AddTwo) {
    Model modelAdd2;
    CreateAddTwoTensorModel(&modelAdd2);

    // Test the one node model.
    Matrix3x4 actual;
    memset(&actual, 0, sizeof(actual));
    Compilation compilation(&modelAdd2);
    compilation.finish();
    Execution execution(&compilation);
    ASSERT_EQ(execution.setInput(0, matrix1, sizeof(Matrix3x4)), Result::NO_ERROR);
    ASSERT_EQ(execution.setInput(1, matrix2, sizeof(Matrix3x4)), Result::NO_ERROR);
    ASSERT_EQ(execution.setOutput(0, actual, sizeof(Matrix3x4)), Result::NO_ERROR);
    ASSERT_EQ(execution.compute(), Result::NO_ERROR);
    ASSERT_EQ(CompareMatrices(expected2, actual), 0);
}

TEST_F(TrivialTest, AddTwoWithHardwareBufferInput) {
    Model modelAdd2;
    CreateAddTwoTensorModel(&modelAdd2);

    AHardwareBuffer_Desc desc{
            .width = sizeof(matrix1),
            .height = 1,
            .layers = 1,
            .format = AHARDWAREBUFFER_FORMAT_BLOB,
            .usage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,
    };
    AHardwareBuffer* matrix1Buffer = nullptr;
    ASSERT_EQ(AHardwareBuffer_allocate(&desc, &matrix1Buffer), 0);
    Memory matrix1Memory(matrix1Buffer);
    ASSERT_TRUE(matrix1Memory.isValid());

    // Test the one node model.
    Matrix3x4 actual;
    memset(&actual, 0, sizeof(actual));
    Compilation compilation(&modelAdd2);
    compilation.finish();
    Execution execution(&compilation);
    ASSERT_EQ(execution.setInputFromMemory(0, &matrix1Memory, 0, sizeof(Matrix3x4)),
              Result::NO_ERROR);
    ASSERT_EQ(execution.setInput(1, matrix2, sizeof(Matrix3x4)), Result::NO_ERROR);
    ASSERT_EQ(execution.setOutput(0, actual, sizeof(Matrix3x4)), Result::NO_ERROR);

    // Set the value for matrix1Buffer.
    void* bufferPtr = nullptr;
    ASSERT_EQ(AHardwareBuffer_lock(matrix1Buffer, desc.usage, -1, NULL, &bufferPtr), 0);
    memcpy((uint8_t*)bufferPtr, matrix1, sizeof(matrix1));
    int synFenceFd = -1;
    ASSERT_EQ(AHardwareBuffer_unlock(matrix1Buffer, &synFenceFd), 0);
    if (synFenceFd > 0) {
        // If valid sync fence is return by AHardwareBuffer_unlock, use
        // ANeuralNetworksExecution_startComputeWithDependencies
        ANeuralNetworksEvent* eventBufferUnlock;
        ANeuralNetworksEvent* eventToSignal;
        ASSERT_EQ(ANeuralNetworksEvent_createFromSyncFenceFd(synFenceFd, &eventBufferUnlock),
                  ANEURALNETWORKS_NO_ERROR);
        close(synFenceFd);
        ANeuralNetworksExecution* executionHandle = execution.getHandle();
        ASSERT_EQ(ANeuralNetworksExecution_startComputeWithDependencies(
                          executionHandle, &eventBufferUnlock, 1, 0, &eventToSignal),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksEvent_wait(eventToSignal), ANEURALNETWORKS_NO_ERROR);
        ANeuralNetworksEvent_free(eventBufferUnlock);
        ANeuralNetworksEvent_free(eventToSignal);
    } else {
        ASSERT_EQ(execution.compute(), Result::NO_ERROR);
    }

    ASSERT_EQ(CompareMatrices(expected2, actual), 0);
    AHardwareBuffer_release(matrix1Buffer);
}

TEST_F(TrivialTest, AddThree) {
    Model modelAdd3;
    CreateAddThreeTensorModel(&modelAdd3, matrix3);

    // Test the three node model.
    Matrix3x4 actual;
    memset(&actual, 0, sizeof(actual));
    Compilation compilation2(&modelAdd3);
    compilation2.finish();
    Execution execution2(&compilation2);
    ASSERT_EQ(execution2.setInput(0, matrix1, sizeof(Matrix3x4)), Result::NO_ERROR);
    ASSERT_EQ(execution2.setInput(1, matrix2, sizeof(Matrix3x4)), Result::NO_ERROR);
    ASSERT_EQ(execution2.setOutput(0, actual, sizeof(Matrix3x4)), Result::NO_ERROR);
    ASSERT_EQ(execution2.compute(), Result::NO_ERROR);
    ASSERT_EQ(CompareMatrices(expected3, actual), 0);

    // Test it a second time to make sure the model is reusable.
    memset(&actual, 0, sizeof(actual));
    Compilation compilation3(&modelAdd3);
    compilation3.finish();
    Execution execution3(&compilation3);
    ASSERT_EQ(execution3.setInput(0, matrix1, sizeof(Matrix3x4)), Result::NO_ERROR);
    ASSERT_EQ(execution3.setInput(1, matrix1, sizeof(Matrix3x4)), Result::NO_ERROR);
    ASSERT_EQ(execution3.setOutput(0, actual, sizeof(Matrix3x4)), Result::NO_ERROR);
    ASSERT_EQ(execution3.compute(), Result::NO_ERROR);
    ASSERT_EQ(CompareMatrices(expected3b, actual), 0);
}

TEST_F(TrivialTest, FencedAddThree) {
    Model modelAdd3;
    CreateAddThreeTensorModel(&modelAdd3, matrix3);
    Compilation compilation(&modelAdd3);
    compilation.finish();

    Matrix3x4 output1, output2;
    memset(&output1, 0, sizeof(output1));
    memset(&output2, 0, sizeof(output2));

    // Start the first execution
    Execution execution1(&compilation);
    ASSERT_EQ(execution1.setInput(0, matrix1, sizeof(Matrix3x4)), Result::NO_ERROR);
    ASSERT_EQ(execution1.setInput(1, matrix2, sizeof(Matrix3x4)), Result::NO_ERROR);
    ASSERT_EQ(execution1.setOutput(0, output1, sizeof(Matrix3x4)), Result::NO_ERROR);
    ANeuralNetworksEvent* event1;
    ANeuralNetworksExecution* execution1_handle = execution1.getHandle();
    ASSERT_EQ(ANeuralNetworksExecution_startComputeWithDependencies(execution1_handle, nullptr, 0,
                                                                    0, &event1),
              ANEURALNETWORKS_NO_ERROR);

    // Start the second execution which will wait for the first one.
    Execution execution2(&compilation);
    ASSERT_EQ(execution2.setInput(0, matrix1, sizeof(Matrix3x4)), Result::NO_ERROR);
    ASSERT_EQ(execution2.setInput(1, matrix1, sizeof(Matrix3x4)), Result::NO_ERROR);
    ASSERT_EQ(execution2.setOutput(0, output2, sizeof(Matrix3x4)), Result::NO_ERROR);
    ANeuralNetworksEvent* event2;
    ANeuralNetworksExecution* execution2_handle = execution2.getHandle();
    ASSERT_EQ(ANeuralNetworksExecution_startComputeWithDependencies(execution2_handle, &event1, 1,
                                                                    0, &event2),
              ANEURALNETWORKS_NO_ERROR);
    // Wait for the second event.
    ASSERT_EQ(ANeuralNetworksEvent_wait(event2), ANEURALNETWORKS_NO_ERROR);

    // Check the results for both executions.
    ASSERT_EQ(CompareMatrices(expected3, output1), 0);
    ASSERT_EQ(CompareMatrices(expected3b, output2), 0);

    // Free the event objects
    ANeuralNetworksEvent_free(event1);
    ANeuralNetworksEvent_free(event2);
}

TEST_F(TrivialTest, BroadcastAddTwo) {
    Model modelBroadcastAdd2;
    OperandType scalarType(Type::INT32, {});
    auto activation = modelBroadcastAdd2.addConstantOperand(&scalarType, kNoActivation);

    OperandType matrixType(Type::TENSOR_FLOAT32, {1, 1, 3, 4});
    OperandType matrixType2(Type::TENSOR_FLOAT32, {4});

    auto a = modelBroadcastAdd2.addOperand(&matrixType);
    auto b = modelBroadcastAdd2.addOperand(&matrixType2);
    auto c = modelBroadcastAdd2.addOperand(&matrixType);
    modelBroadcastAdd2.addOperation(ANEURALNETWORKS_ADD, {a, b, activation}, {c});
    modelBroadcastAdd2.identifyInputsAndOutputs({a, b}, {c});
    ASSERT_TRUE(modelBroadcastAdd2.isValid());
    modelBroadcastAdd2.finish();

    // Test the one node model.
    Matrix3x4 actual;
    memset(&actual, 0, sizeof(actual));
    Compilation compilation(&modelBroadcastAdd2);
    compilation.finish();
    Execution execution(&compilation);
    ASSERT_EQ(execution.setInput(0, matrix1, sizeof(Matrix3x4)), Result::NO_ERROR);
    ASSERT_EQ(execution.setInput(1, matrix2b, sizeof(Matrix4)), Result::NO_ERROR);
    ASSERT_EQ(execution.setOutput(0, actual, sizeof(Matrix3x4)), Result::NO_ERROR);
    ASSERT_EQ(execution.compute(), Result::NO_ERROR);
    ASSERT_EQ(CompareMatrices(expected2b, actual), 0);
}

TEST_F(TrivialTest, BroadcastMulTwo) {
    Model modelBroadcastMul2;
    OperandType scalarType(Type::INT32, {});
    auto activation = modelBroadcastMul2.addConstantOperand(&scalarType, kNoActivation);

    OperandType matrixType(Type::TENSOR_FLOAT32, {1, 1, 3, 4});
    OperandType matrixType2(Type::TENSOR_FLOAT32, {4});

    auto a = modelBroadcastMul2.addOperand(&matrixType);
    auto b = modelBroadcastMul2.addOperand(&matrixType2);
    auto c = modelBroadcastMul2.addOperand(&matrixType);
    modelBroadcastMul2.addOperation(ANEURALNETWORKS_MUL, {a, b, activation}, {c});
    modelBroadcastMul2.identifyInputsAndOutputs({a, b}, {c});
    ASSERT_TRUE(modelBroadcastMul2.isValid());
    modelBroadcastMul2.finish();

    // Test the one node model.
    Matrix3x4 actual;
    memset(&actual, 0, sizeof(actual));
    Compilation compilation(&modelBroadcastMul2);
    compilation.finish();
    Execution execution(&compilation);
    ASSERT_EQ(execution.setInput(0, matrix1, sizeof(Matrix3x4)), Result::NO_ERROR);
    ASSERT_EQ(execution.setInput(1, matrix2b, sizeof(Matrix4)), Result::NO_ERROR);
    ASSERT_EQ(execution.setOutput(0, actual, sizeof(Matrix3x4)), Result::NO_ERROR);
    ASSERT_EQ(execution.compute(), Result::NO_ERROR);
    ASSERT_EQ(CompareMatrices(expected2c, actual), 0);
}

}  // end namespace
