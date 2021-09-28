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

// This file tests that various abnormal uses of ANeuralNetworks*_free() don't crash.
//
// Limitation: It doesn't set various combinations of properties on objects before
// freeing those objects.

#include "NeuralNetworks.h"

#include <gtest/gtest.h>

#include <vector>

namespace {

ANeuralNetworksModel* createUnfinishedModel() {
    ANeuralNetworksModel* model = nullptr;
    EXPECT_EQ(ANeuralNetworksModel_create(&model), ANEURALNETWORKS_NO_ERROR);

    uint32_t dimensions[] = {1};
    ANeuralNetworksOperandType type = {
            .type = ANEURALNETWORKS_TENSOR_FLOAT32, .dimensionCount = 1, .dimensions = dimensions};
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, &type), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, &type), ANEURALNETWORKS_NO_ERROR);

    const uint32_t inList[]{0};
    const uint32_t outList[]{1};
    EXPECT_EQ(
            ANeuralNetworksModel_addOperation(model, ANEURALNETWORKS_FLOOR, 1, inList, 1, outList),
            ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_identifyInputsAndOutputs(model, 1, inList, 1, outList),
              ANEURALNETWORKS_NO_ERROR);

    return model;
}

ANeuralNetworksModel* createFinishedModel() {
    ANeuralNetworksModel* const model = createUnfinishedModel();
    EXPECT_EQ(ANeuralNetworksModel_finish(model), ANEURALNETWORKS_NO_ERROR);
    return model;
}

std::vector<ANeuralNetworksDevice*> createDeviceList() {
    std::vector<ANeuralNetworksDevice*> devices;

    uint32_t numDevices = 0;
    EXPECT_EQ(ANeuralNetworks_getDeviceCount(&numDevices), ANEURALNETWORKS_NO_ERROR);
    for (uint32_t devIndex = 0; devIndex < numDevices; ++devIndex) {
        ANeuralNetworksDevice* device = nullptr;
        const int getDeviceStatus = ANeuralNetworks_getDevice(devIndex, &device);
        EXPECT_EQ(getDeviceStatus, ANEURALNETWORKS_NO_ERROR);
        if (getDeviceStatus == ANEURALNETWORKS_NO_ERROR) {
            devices.push_back(device);
        }
    }

    return devices;
}

TEST(Free, Null) {
    ANeuralNetworksBurst_free(nullptr);
    ANeuralNetworksCompilation_free(nullptr);
    ANeuralNetworksEvent_free(nullptr);
    ANeuralNetworksExecution_free(nullptr);
    ANeuralNetworksMemory_free(nullptr);
    ANeuralNetworksModel_free(nullptr);
}

TEST(Free, UnfinishedModel) {
    ANeuralNetworksModel* const model = createUnfinishedModel();
    ANeuralNetworksModel_free(model);
}

TEST(Free, UnfinishedCompilation) {
    ANeuralNetworksModel* const model = createFinishedModel();

    ANeuralNetworksCompilation* compilation = nullptr;
    ASSERT_EQ(ANeuralNetworksCompilation_create(model, &compilation), ANEURALNETWORKS_NO_ERROR);
    ANeuralNetworksCompilation_free(compilation);

    ANeuralNetworksModel_free(model);
}

TEST(Free, UnfinishedCompilationForDevices) {
    ANeuralNetworksModel* const model = createFinishedModel();

    const auto devices = createDeviceList();

    ANeuralNetworksCompilation* compilation = nullptr;
    ASSERT_EQ(ANeuralNetworksCompilation_createForDevices(model, devices.data(), devices.size(),
                                                          &compilation),
              ANEURALNETWORKS_NO_ERROR);
    ANeuralNetworksCompilation_free(compilation);

    ANeuralNetworksModel_free(model);
}

TEST(Free, UnscheduledExecution) {
    ANeuralNetworksModel* const model = createFinishedModel();

    ANeuralNetworksCompilation* compilation = nullptr;
    ASSERT_EQ(ANeuralNetworksCompilation_create(model, &compilation), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksCompilation_finish(compilation), ANEURALNETWORKS_NO_ERROR);

    ANeuralNetworksExecution* execution = nullptr;
    ASSERT_EQ(ANeuralNetworksExecution_create(compilation, &execution), ANEURALNETWORKS_NO_ERROR);
    ANeuralNetworksExecution_free(execution);

    ANeuralNetworksCompilation_free(compilation);

    ANeuralNetworksModel_free(model);
}

}  // namespace
