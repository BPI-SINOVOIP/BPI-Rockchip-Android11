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

#include <android-base/logging.h>
#include <android-base/scopeguard.h>
#include <android/sharedmem.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

#include <algorithm>
#include <future>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "NeuralNetworks.h"
#include "NeuralNetworksOEM.h"

#ifndef NNTEST_ONLY_PUBLIC_API
#include "NeuralNetworksExtensions.h"
#include "TypeManager.h"
#endif

// This file tests all the validations done by the Neural Networks API.

namespace {

constexpr uint64_t kShortWaitInNanoseconds = 1'000'000'000;  // 1 second

class ValidationTest : public ::testing::Test {
   protected:
    virtual void SetUp() {}
};

class ValidationTestModel : public ValidationTest {
   protected:
    virtual void SetUp() {
        ValidationTest::SetUp();
        ASSERT_EQ(ANeuralNetworksModel_create(&mModel), ANEURALNETWORKS_NO_ERROR);
    }
    virtual void TearDown() {
        ANeuralNetworksModel_free(mModel);
        ValidationTest::TearDown();
    }

    uint32_t addScalarOperand(int32_t type = ANEURALNETWORKS_INT32) {
        ANeuralNetworksOperandType operandType = {
                .type = type, .dimensionCount = 0, .dimensions = nullptr};
        EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, &operandType), ANEURALNETWORKS_NO_ERROR);
        return mNumOperands++;
    }

    uint32_t addOperand(const ANeuralNetworksOperandType& operandType) {
        EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, &operandType), ANEURALNETWORKS_NO_ERROR);
        return mNumOperands++;
    }

    uint32_t addTensorOperand(int32_t type = ANEURALNETWORKS_TENSOR_FLOAT32) {
        return addTensorOperand(type, {2});
    }

    uint32_t addTensorOperand(int32_t type, const std::vector<uint32_t>& dimensions) {
        ANeuralNetworksOperandType operandType = {
                .type = type,
                .dimensionCount = static_cast<uint32_t>(dimensions.size()),
                .dimensions = dimensions.data(),
        };
        return addOperand(operandType);
    }

    int addOperation(ANeuralNetworksOperationType type, const std::vector<uint32_t>& inputs,
                     const std::vector<uint32_t>& outputs) {
        ++mNumOperations;
        return ANeuralNetworksModel_addOperation(mModel, type, inputs.size(), inputs.data(),
                                                 outputs.size(), outputs.data());
    }
    int identifyInputsAndOutputs(const std::vector<uint32_t>& inputs,
                                 const std::vector<uint32_t>& outputs) {
        return ANeuralNetworksModel_identifyInputsAndOutputs(mModel, inputs.size(), inputs.data(),
                                                             outputs.size(), outputs.data());
    }
    int modelFinish() { return ANeuralNetworksModel_finish(mModel); }

    virtual void createModel() {
        addTensorOperand();
        addTensorOperand();
        addScalarOperand();
        addTensorOperand();
        const std::vector<uint32_t> inList = {0, 1, 2};
        const std::vector<uint32_t> outList = {3};
        ASSERT_EQ(addOperation(ANEURALNETWORKS_ADD, inList, outList), ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(identifyInputsAndOutputs(inList, outList), ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(modelFinish(), ANEURALNETWORKS_NO_ERROR);
    }

    uint32_t mNumOperands = 0;
    uint32_t mNumOperations = 0;
    ANeuralNetworksModel* mModel = nullptr;

    const uint32_t kDummyDimensionValue = 1;
    const ANeuralNetworksOperandType kInvalidTensorType1{
            .type = ANEURALNETWORKS_TENSOR_FLOAT32,
            // dimensionCount must be consistent with dimensions.
            .dimensionCount = 1,
            .dimensions = nullptr,
    };
    const ANeuralNetworksOperandType kInvalidTensorType2{
            .type = ANEURALNETWORKS_TENSOR_FLOAT32,
            // dimensionCount must be consistent with dimensions.
            .dimensionCount = 0,
            .dimensions = &kDummyDimensionValue,
    };
};

#ifndef NNTEST_ONLY_PUBLIC_API
constexpr const char* kTestExtensionName = "com.android.test_extension";
constexpr int32_t kTestExtensionTensorType = ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL;

class ValidationTestModelExtensions : public ValidationTestModel {
   protected:
    virtual void SetUp() {
        ValidationTestModel::SetUp();
        EXPECT_TRUE(::android::nn::TypeManager::get()->forTest_registerExtension({
                .name = kTestExtensionName,
                .operandTypes =
                        {
                                {
                                        .type = kTestExtensionTensorType,
                                        .isTensor = true,
                                        .byteSize = 1,
                                },
                        },
        }));
    }

    virtual void TearDown() {
        ::android::nn::TypeManager::get()->forTest_reset();
        ValidationTestModel::TearDown();
    }

    int32_t getExtensionOperandType(uint16_t typeWithinExtension) {
        int32_t result;
        EXPECT_EQ(ANeuralNetworksModel_getExtensionOperandType(mModel, kTestExtensionName,
                                                               typeWithinExtension, &result),
                  ANEURALNETWORKS_NO_ERROR);
        return result;
    }
};
#endif

class ValidationTestIdentify : public ValidationTestModel {
    virtual void SetUp() {
        ValidationTestModel::SetUp();

        uint32_t dimensions[]{1};
        ANeuralNetworksOperandType tensorType{.type = ANEURALNETWORKS_TENSOR_FLOAT32,
                                              .dimensionCount = 1,
                                              .dimensions = dimensions};
        ANeuralNetworksOperandType scalarType{
                .type = ANEURALNETWORKS_INT32, .dimensionCount = 0, .dimensions = nullptr};
        ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &scalarType), ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(addOperation(ANEURALNETWORKS_ADD, {0, 1, 2}, {3}), ANEURALNETWORKS_NO_ERROR);
    }
    virtual void TearDown() { ValidationTestModel::TearDown(); }
};

class ValidationTestCompilation : public ValidationTestModel {
   protected:
    virtual void SetUp() {
        ValidationTestModel::SetUp();
        createModel();
        ASSERT_EQ(ANeuralNetworksCompilation_create(mModel, &mCompilation),
                  ANEURALNETWORKS_NO_ERROR);
    }

    virtual void TearDown() {
        ANeuralNetworksCompilation_free(mCompilation);
        ValidationTestModel::TearDown();
    }

    ANeuralNetworksCompilation* mCompilation = nullptr;
};

class ValidationTestExecution : public ValidationTestCompilation {
   protected:
    virtual void SetUp() {
        ValidationTestCompilation::SetUp();

        ASSERT_EQ(ANeuralNetworksCompilation_finish(mCompilation), ANEURALNETWORKS_NO_ERROR);

        ASSERT_EQ(ANeuralNetworksExecution_create(mCompilation, &mExecution),
                  ANEURALNETWORKS_NO_ERROR);
    }
    virtual void TearDown() {
        ANeuralNetworksExecution_free(mExecution);
        ValidationTestCompilation::TearDown();
    }
    ANeuralNetworksExecution* mExecution = nullptr;
};

class ValidationTestBurst : public ValidationTestExecution {
   protected:
    virtual void SetUp() {
        ValidationTestExecution::SetUp();

        ASSERT_EQ(ANeuralNetworksBurst_create(mCompilation, &mBurst), ANEURALNETWORKS_NO_ERROR);
    }
    virtual void TearDown() {
        ANeuralNetworksBurst_free(mBurst);
        ValidationTestExecution::TearDown();
    }
    ANeuralNetworksBurst* mBurst = nullptr;
};

class ValidationTestMemoryDesc : public ValidationTestCompilation {
   protected:
    virtual void SetUp() {
        ValidationTestCompilation::SetUp();
        ASSERT_EQ(ANeuralNetworksMemoryDesc_create(&mDesc), ANEURALNETWORKS_NO_ERROR);
    }
    virtual void TearDown() {
        ANeuralNetworksMemoryDesc_free(mDesc);
        for (auto* memory : mMemories) ANeuralNetworksMemory_free(memory);
        for (int fd : mFds) close(fd);
        ValidationTestCompilation::TearDown();
    }

    ANeuralNetworksMemory* createAshmem(uint32_t size) {
        int fd = ASharedMemory_create("nnMemory", size);
        EXPECT_GT(fd, 0);
        mFds.push_back(fd);
        ANeuralNetworksMemory* ashmem = nullptr;
        EXPECT_EQ(ANeuralNetworksMemory_createFromFd(size, PROT_READ | PROT_WRITE, fd, 0, &ashmem),
                  ANEURALNETWORKS_NO_ERROR);
        mMemories.push_back(ashmem);
        return ashmem;
    }

    ANeuralNetworksMemoryDesc* mDesc = nullptr;
    std::vector<ANeuralNetworksMemory*> mMemories;
    std::vector<int> mFds;
};

class ValidationTestExecutionDeviceMemory : public ValidationTest {
   protected:
    virtual void SetUp() {
        ValidationTest::SetUp();
        ASSERT_EQ(ANeuralNetworksModel_create(&mModel), ANEURALNETWORKS_NO_ERROR);
        createModel(mModel, /*dimensionsUnspecified=*/false, /*isValid=*/true);
        ASSERT_EQ(ANeuralNetworksCompilation_create(mModel, &mCompilation),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksCompilation_finish(mCompilation), ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksExecution_create(mCompilation, &mExecution),
                  ANEURALNETWORKS_NO_ERROR);

        ASSERT_EQ(ANeuralNetworksModel_create(&mModelDynamic), ANEURALNETWORKS_NO_ERROR);
        createModel(mModelDynamic, /*dimensionsUnspecified=*/true, /*isValid=*/true);
        ASSERT_EQ(ANeuralNetworksCompilation_create(mModelDynamic, &mCompilationDynamic),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksCompilation_finish(mCompilationDynamic), ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksExecution_create(mCompilationDynamic, &mExecutionDynamic),
                  ANEURALNETWORKS_NO_ERROR);

        ASSERT_EQ(ANeuralNetworksModel_create(&mInitModel), ANEURALNETWORKS_NO_ERROR);
        createModel(mInitModel, /*dimensionsUnspecified=*/false, /*isValid=*/true);
        ASSERT_EQ(ANeuralNetworksCompilation_create(mInitModel, &mInitCompilation),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksCompilation_finish(mInitCompilation), ANEURALNETWORKS_NO_ERROR);

        ASSERT_EQ(ANeuralNetworksModel_create(&mDeinitModel), ANEURALNETWORKS_NO_ERROR);
        createModel(mDeinitModel, /*dimensionsUnspecified=*/false, /*isValid=*/false);
        ASSERT_EQ(ANeuralNetworksCompilation_create(mDeinitModel, &mDeinitCompilation),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksCompilation_finish(mDeinitCompilation), ANEURALNETWORKS_NO_ERROR);
    }
    virtual void TearDown() {
        ANeuralNetworksExecution_free(mExecution);
        ANeuralNetworksCompilation_free(mCompilation);
        ANeuralNetworksModel_free(mModel);
        ANeuralNetworksExecution_free(mExecutionDynamic);
        ANeuralNetworksCompilation_free(mCompilationDynamic);
        ANeuralNetworksModel_free(mModelDynamic);

        ANeuralNetworksCompilation_free(mInitCompilation);
        ANeuralNetworksModel_free(mInitModel);
        ANeuralNetworksCompilation_free(mDeinitCompilation);
        ANeuralNetworksModel_free(mDeinitModel);

        ValidationTest::TearDown();
    }

    void addScalarOperand(ANeuralNetworksModel* model) {
        ANeuralNetworksOperandType operandType = {
                .type = ANEURALNETWORKS_INT32, .dimensionCount = 0, .dimensions = nullptr};
        EXPECT_EQ(ANeuralNetworksModel_addOperand(model, &operandType), ANEURALNETWORKS_NO_ERROR);
    }

    void addTensorOperand(ANeuralNetworksModel* model, bool dimensionsUnspecified) {
        uint32_t dimension = dimensionsUnspecified ? 0 : 1;
        ANeuralNetworksOperandType operandType = {
                .type = ANEURALNETWORKS_TENSOR_FLOAT32,
                .dimensionCount = 1,
                .dimensions = &dimension,
        };
        EXPECT_EQ(ANeuralNetworksModel_addOperand(model, &operandType), ANEURALNETWORKS_NO_ERROR);
    }

    void createModel(ANeuralNetworksModel* model, bool dimensionsUnspecified, bool isValid) {
        const float constData = 0;
        const uint32_t actData = isValid ? 0 : 999;

        addTensorOperand(model, dimensionsUnspecified);
        addTensorOperand(model, /*dimensionsUnspecified=*/false);
        addScalarOperand(model);
        addTensorOperand(model, dimensionsUnspecified);

        ANeuralNetworksModel_setOperandValue(model, 1, &constData, sizeof(float));
        ANeuralNetworksModel_setOperandValue(model, 2, &actData, sizeof(uint32_t));

        uint32_t inList[] = {0, 1, 2}, outList[] = {3};
        ASSERT_EQ(ANeuralNetworksModel_addOperation(model, ANEURALNETWORKS_ADD, 3, inList, 1,
                                                    outList),
                  ANEURALNETWORKS_NO_ERROR);
        uint32_t inputList[] = {0}, outputList[] = {3};
        ASSERT_EQ(ANeuralNetworksModel_identifyInputsAndOutputs(model, 1, inputList, 1, outputList),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksModel_finish(model), ANEURALNETWORKS_NO_ERROR);
    }

    void executeWithMemoryAsInput(ANeuralNetworksCompilation* compilation,
                                  ANeuralNetworksMemory* memory, int expectedResult) {
        float data = 0;
        ANeuralNetworksExecution* execution = nullptr;
        ASSERT_EQ(ANeuralNetworksExecution_create(compilation, &execution),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksExecution_setInputFromMemory(execution, 0, nullptr, memory, 0, 0),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksExecution_setOutput(execution, 0, nullptr, &data, sizeof(float)),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksExecution_compute(execution), expectedResult);
        ANeuralNetworksExecution_free(execution);
    }

    void executeWithMemoryAsOutput(ANeuralNetworksCompilation* compilation,
                                   ANeuralNetworksMemory* memory, int expectedResult) {
        const float data = 0;
        ANeuralNetworksExecution* execution = nullptr;
        ASSERT_EQ(ANeuralNetworksExecution_create(compilation, &execution),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksExecution_setInput(execution, 0, nullptr, &data, sizeof(float)),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksExecution_setOutputFromMemory(execution, 0, nullptr, memory, 0, 0),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksExecution_compute(execution), expectedResult);
        ANeuralNetworksExecution_free(execution);
    }

    ANeuralNetworksModel* mModel = nullptr;
    ANeuralNetworksCompilation* mCompilation = nullptr;
    ANeuralNetworksExecution* mExecution = nullptr;

    ANeuralNetworksModel* mModelDynamic = nullptr;
    ANeuralNetworksCompilation* mCompilationDynamic = nullptr;
    ANeuralNetworksExecution* mExecutionDynamic = nullptr;

    ANeuralNetworksModel* mInitModel = nullptr;
    ANeuralNetworksCompilation* mInitCompilation = nullptr;
    ANeuralNetworksModel* mDeinitModel = nullptr;
    ANeuralNetworksCompilation* mDeinitCompilation = nullptr;
};

TEST_F(ValidationTest, CreateModel) {
    EXPECT_EQ(ANeuralNetworksModel_create(nullptr), ANEURALNETWORKS_UNEXPECTED_NULL);
}

TEST_F(ValidationTestModel, AddOperand) {
    ANeuralNetworksOperandType floatType{
            .type = ANEURALNETWORKS_FLOAT32, .dimensionCount = 0, .dimensions = nullptr};
    EXPECT_EQ(ANeuralNetworksModel_addOperand(nullptr, &floatType),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, nullptr), ANEURALNETWORKS_UNEXPECTED_NULL);

    ANeuralNetworksOperandType quant8TypeInvalidScale{
            .type = ANEURALNETWORKS_TENSOR_QUANT8_ASYMM,
            .dimensionCount = 0,
            .dimensions = nullptr,
            // Scale has to be non-negative
            .scale = -1.0f,
            .zeroPoint = 0,
    };
    EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, &quant8TypeInvalidScale),
              ANEURALNETWORKS_BAD_DATA);

    ANeuralNetworksOperandType quant8TypeInvalidZeroPoint{
            .type = ANEURALNETWORKS_TENSOR_QUANT8_ASYMM,
            .dimensionCount = 0,
            .dimensions = nullptr,
            .scale = 1.0f,
            // zeroPoint has to be in [0, 255]
            .zeroPoint = -1,
    };
    EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, &quant8TypeInvalidZeroPoint),
              ANEURALNETWORKS_BAD_DATA);

    const uint32_t dim = 2;
    ANeuralNetworksOperandType invalidScalarType{
            .type = ANEURALNETWORKS_INT32,
            // a scalar type must have 0 dimensions.
            .dimensionCount = 1,
            .dimensions = &dim,
    };
    EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, &invalidScalarType),
              ANEURALNETWORKS_BAD_DATA);

    EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, &kInvalidTensorType1),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, &kInvalidTensorType2),
              ANEURALNETWORKS_BAD_DATA);

    modelFinish();
    // This should fail, as the model is already finished.
    EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, &floatType), ANEURALNETWORKS_BAD_STATE);
}

TEST_F(ValidationTestModel, SetOperandSymmPerChannelQuantParams) {
    const int32_t operandIndex = addTensorOperand(ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL);

    float scales[2] = {1.0, 2.0};
    ANeuralNetworksSymmPerChannelQuantParams channelQuant = {
            .channelDim = 0,
            .scaleCount = 2,
            .scales = scales,
    };

    EXPECT_EQ(ANeuralNetworksModel_setOperandSymmPerChannelQuantParams(nullptr, operandIndex,
                                                                       &channelQuant),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(
            ANeuralNetworksModel_setOperandSymmPerChannelQuantParams(mModel, operandIndex, nullptr),
            ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksModel_setOperandSymmPerChannelQuantParams(mModel, operandIndex + 1,
                                                                       &channelQuant),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksModel_setOperandSymmPerChannelQuantParams(mModel, operandIndex,
                                                                       &channelQuant),
              ANEURALNETWORKS_NO_ERROR);
}

#ifndef NNTEST_ONLY_PUBLIC_API
TEST_F(ValidationTestModelExtensions, AddOperand_UnknownPrefix) {
    ANeuralNetworksOperandType type = {.type = -1};
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &type), ANEURALNETWORKS_BAD_DATA);
}

TEST_F(ValidationTestModelExtensions, SetOperandSymmPerChannelQuantParams_ExtensionOperand) {
    const int32_t operandIndex =
            addTensorOperand(getExtensionOperandType(kTestExtensionTensorType));

    float scales[2] = {1.0, 2.0};
    ANeuralNetworksSymmPerChannelQuantParams channelQuant = {
            .channelDim = 0,
            .scaleCount = 2,
            .scales = scales,
    };

    EXPECT_EQ(ANeuralNetworksModel_setOperandSymmPerChannelQuantParams(mModel, operandIndex,
                                                                       &channelQuant),
              ANEURALNETWORKS_BAD_DATA);
}

TEST_F(ValidationTestModelExtensions, SetOperandExtensionData) {
    const int32_t operandIndex =
            addTensorOperand(getExtensionOperandType(kTestExtensionTensorType));
    const int32_t data = 42;
    const size_t dataLength = sizeof(data);
    EXPECT_EQ(
            ANeuralNetworksModel_setOperandExtensionData(nullptr, operandIndex, &data, dataLength),
            ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(
            ANeuralNetworksModel_setOperandExtensionData(mModel, operandIndex, nullptr, dataLength),
            ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksModel_setOperandExtensionData(mModel, operandIndex, &data, 0),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksModel_setOperandExtensionData(mModel, operandIndex + 1, &data,
                                                           dataLength),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksModel_setOperandExtensionData(mModel, operandIndex, &data, dataLength),
              ANEURALNETWORKS_NO_ERROR);
}

TEST_F(ValidationTestModelExtensions, SetOperandExtensionData_Empty) {
    const int32_t operandIndex =
            addTensorOperand(getExtensionOperandType(kTestExtensionTensorType));
    EXPECT_EQ(ANeuralNetworksModel_setOperandExtensionData(mModel, operandIndex, nullptr, 0),
              ANEURALNETWORKS_NO_ERROR);
}

TEST_F(ValidationTestModelExtensions, SetOperandExtensionData_NonExtensionOperand) {
    const int32_t operandIndex = addTensorOperand();
    const int32_t data = 42;
    const size_t dataLength = sizeof(data);
    EXPECT_EQ(ANeuralNetworksModel_setOperandExtensionData(mModel, operandIndex, &data, dataLength),
              ANEURALNETWORKS_BAD_DATA);
}

TEST_F(ValidationTestModelExtensions, SetOperandValue_UnspecifiedDimension) {
    const uint32_t dimensions[2] = {3, 0};
    ANeuralNetworksOperandType type = {
            .type = getExtensionOperandType(kTestExtensionTensorType),
            .dimensionCount = 2,
            .dimensions = dimensions,
    };
    const int32_t operandIndex = addOperand(type);
    char buffer[20];
    EXPECT_EQ(ANeuralNetworksModel_setOperandValue(mModel, operandIndex, buffer, sizeof(buffer)),
              ANEURALNETWORKS_BAD_DATA);
}

TEST_F(ValidationTestModelExtensions, SetOperandValue_UnspecifiedRank) {
    ANeuralNetworksOperandType type = {
            .type = getExtensionOperandType(kTestExtensionTensorType),
            .dimensionCount = 0,
            .dimensions = nullptr,
    };
    const int32_t operandIndex = addOperand(type);
    char buffer[20];
    EXPECT_EQ(ANeuralNetworksModel_setOperandValue(mModel, operandIndex, buffer, sizeof(buffer)),
              ANEURALNETWORKS_BAD_DATA);
}

TEST_F(ValidationTestModelExtensions, AddOperandDimensionProductOverflow) {
    uint32_t dimensions[] = {5, 4, 4, 786433, 5, 3, 16777216, 4, 5};
    ANeuralNetworksOperandType operandType = {
            .type = getExtensionOperandType(kTestExtensionTensorType),
            .dimensionCount = std::size(dimensions),
            .dimensions = dimensions,
    };
    // This should fail, as the operand type's dimension product overflows uint32_t.
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &operandType), ANEURALNETWORKS_BAD_DATA);
}
#endif

TEST_F(ValidationTestModel, SetOptionalOperand) {
    ANeuralNetworksOperandType floatType{
            .type = ANEURALNETWORKS_FLOAT32, .dimensionCount = 0, .dimensions = nullptr};
    EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, &floatType), ANEURALNETWORKS_NO_ERROR);

    EXPECT_EQ(ANeuralNetworksModel_setOperandValue(mModel, 0, nullptr, 0),
              ANEURALNETWORKS_NO_ERROR);
}

TEST_F(ValidationTestModel, SetOperandValue) {
    ANeuralNetworksOperandType floatType{
            .type = ANEURALNETWORKS_FLOAT32, .dimensionCount = 0, .dimensions = nullptr};
    EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, &floatType), ANEURALNETWORKS_NO_ERROR);

    char buffer[20];
    EXPECT_EQ(ANeuralNetworksModel_setOperandValue(nullptr, 0, buffer, sizeof(buffer)),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksModel_setOperandValue(mModel, 0, nullptr, sizeof(buffer)),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    // This should fail, since buffer is not the size of a float32.
    EXPECT_EQ(ANeuralNetworksModel_setOperandValue(mModel, 0, buffer, sizeof(buffer)),
              ANEURALNETWORKS_BAD_DATA);

    // This should succeed.
    EXPECT_EQ(ANeuralNetworksModel_setOperandValue(mModel, 0, buffer, sizeof(float)),
              ANEURALNETWORKS_NO_ERROR);

    // This should fail, as this operand does not exist.
    EXPECT_EQ(ANeuralNetworksModel_setOperandValue(mModel, 1, buffer, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    modelFinish();
    // This should fail, as the model is already finished.
    EXPECT_EQ(ANeuralNetworksModel_setOperandValue(mModel, 0, buffer, sizeof(float)),
              ANEURALNETWORKS_BAD_STATE);
}

TEST_F(ValidationTestModel, SetOperandValueFromMemory) {
    uint32_t dimensions[]{1};
    ANeuralNetworksOperandType floatType{
            .type = ANEURALNETWORKS_TENSOR_FLOAT32, .dimensionCount = 1, .dimensions = dimensions};
    EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, &floatType), ANEURALNETWORKS_NO_ERROR);

    const size_t memorySize = 20;
    int memoryFd = ASharedMemory_create("nnMemory", memorySize);
    ASSERT_GT(memoryFd, 0);

    ANeuralNetworksMemory* memory;
    EXPECT_EQ(ANeuralNetworksMemory_createFromFd(memorySize, PROT_READ | PROT_WRITE, memoryFd, 0,
                                                 &memory),
              ANEURALNETWORKS_NO_ERROR);

    EXPECT_EQ(ANeuralNetworksModel_setOperandValueFromMemory(nullptr, 0, memory, 0, sizeof(float)),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksModel_setOperandValueFromMemory(mModel, 0, nullptr, 0, sizeof(float)),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    // This should fail, since the operand does not exist.
    EXPECT_EQ(ANeuralNetworksModel_setOperandValueFromMemory(mModel, -1, memory, 0, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, since memory is not the size of a float32.
    EXPECT_EQ(ANeuralNetworksModel_setOperandValueFromMemory(mModel, 0, memory, 0, memorySize),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, as this operand does not exist.
    EXPECT_EQ(ANeuralNetworksModel_setOperandValueFromMemory(mModel, 1, memory, 0, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, since offset is larger than memorySize.
    EXPECT_EQ(ANeuralNetworksModel_setOperandValueFromMemory(mModel, 0, memory, memorySize + 1,
                                                             sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, since requested size is larger than the memory.
    EXPECT_EQ(ANeuralNetworksModel_setOperandValueFromMemory(mModel, 0, memory, memorySize - 3,
                                                             sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    modelFinish();
    // This should fail, as the model is already finished.
    EXPECT_EQ(ANeuralNetworksModel_setOperandValueFromMemory(mModel, 0, memory, 0, sizeof(float)),
              ANEURALNETWORKS_BAD_STATE);

    // close memory
    close(memoryFd);
}

TEST_F(ValidationTestModel, SetOperandValueFromAHardwareBuffer) {
    uint32_t dimensions[]{1};
    ANeuralNetworksOperandType quant8Type{.type = ANEURALNETWORKS_TENSOR_QUANT8_ASYMM,
                                          .dimensionCount = 1,
                                          .dimensions = dimensions,
                                          .scale = 1.0,
                                          .zeroPoint = 0};
    EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, &quant8Type), ANEURALNETWORKS_NO_ERROR);

    AHardwareBuffer_Desc desc{
            .width = 16,
            .height = 16,
            .layers = 1,
            .format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
            .usage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,
    };

    AHardwareBuffer* buffer = nullptr;
    ASSERT_EQ(AHardwareBuffer_allocate(&desc, &buffer), 0);

    ANeuralNetworksMemory* memory;
    EXPECT_EQ(ANeuralNetworksMemory_createFromAHardwareBuffer(buffer, &memory),
              ANEURALNETWORKS_NO_ERROR);

    // This should fail, since non-BLOB AHardwareBuffer is not allowed.
    EXPECT_EQ(ANeuralNetworksModel_setOperandValueFromMemory(mModel, 0, memory, 0, sizeof(uint8_t)),
              ANEURALNETWORKS_BAD_DATA);

    AHardwareBuffer_release(buffer);
}

TEST_F(ValidationTestModel, SetOperandValueFromAHardwareBufferBlob) {
    uint32_t dimensions[]{1};
    ANeuralNetworksOperandType floatType{
            .type = ANEURALNETWORKS_TENSOR_FLOAT32, .dimensionCount = 1, .dimensions = dimensions};
    EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, &floatType), ANEURALNETWORKS_NO_ERROR);

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

    ANeuralNetworksMemory* memory;
    EXPECT_EQ(ANeuralNetworksMemory_createFromAHardwareBuffer(buffer, &memory),
              ANEURALNETWORKS_NO_ERROR);

    // This should fail, since offset is larger than memorySize.
    EXPECT_EQ(ANeuralNetworksModel_setOperandValueFromMemory(mModel, 0, memory, memorySize + 1,
                                                             sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, since requested size is larger than the memory.
    EXPECT_EQ(ANeuralNetworksModel_setOperandValueFromMemory(mModel, 0, memory, memorySize - 3,
                                                             sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    AHardwareBuffer_release(buffer);
}

TEST_F(ValidationTestModel, SetOperandValueFromModel) {
    uint32_t dimensions[] = {2};
    ANeuralNetworksOperandType tensorType = {
            .type = ANEURALNETWORKS_TENSOR_FLOAT32,
            .dimensionCount = std::size(dimensions),
            .dimensions = dimensions,
    };
    ANeuralNetworksOperandType scalarType = {.type = ANEURALNETWORKS_INT32};
    ANeuralNetworksOperandType modelType = {.type = ANEURALNETWORKS_MODEL};

    ANeuralNetworksModel* valueModel = nullptr;
    ASSERT_EQ(ANeuralNetworksModel_create(&valueModel), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksModel_addOperand(valueModel, &tensorType), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksModel_addOperand(valueModel, &tensorType), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksModel_addOperand(valueModel, &scalarType), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksModel_addOperand(valueModel, &tensorType), ANEURALNETWORKS_NO_ERROR);
    uint32_t inList[3] = {0, 1, 2};
    uint32_t outList[1] = {3};
    ASSERT_EQ(ANeuralNetworksModel_addOperation(valueModel, ANEURALNETWORKS_ADD, 3, inList, 1,
                                                outList),
              ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksModel_identifyInputsAndOutputs(valueModel, 3, inList, 1, outList),
              ANEURALNETWORKS_NO_ERROR);

    EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, &modelType), ANEURALNETWORKS_NO_ERROR);

    // This should fail, as the value model is not finished.
    EXPECT_EQ(ANeuralNetworksModel_setOperandValueFromModel(mModel, 0, valueModel),
              ANEURALNETWORKS_BAD_STATE);
    ANeuralNetworksModel_finish(valueModel);

    EXPECT_EQ(ANeuralNetworksModel_setOperandValueFromModel(nullptr, 0, valueModel),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksModel_setOperandValueFromModel(mModel, 0, nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    // This should fail, since the operand does not exist.
    EXPECT_EQ(ANeuralNetworksModel_setOperandValueFromModel(mModel, -1, valueModel),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, as this operand does not exist.
    EXPECT_EQ(ANeuralNetworksModel_setOperandValueFromModel(mModel, 1, valueModel),
              ANEURALNETWORKS_BAD_DATA);

    modelFinish();
    // This should fail, as the model is already finished.
    EXPECT_EQ(ANeuralNetworksModel_setOperandValueFromModel(mModel, 0, valueModel),
              ANEURALNETWORKS_BAD_STATE);

    ANeuralNetworksModel_free(valueModel);
}

TEST_F(ValidationTestModel, AddOEMOperand) {
    ANeuralNetworksOperandType OEMScalarType{
            .type = ANEURALNETWORKS_OEM_SCALAR, .dimensionCount = 0, .dimensions = nullptr};
    EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, &OEMScalarType), ANEURALNETWORKS_NO_ERROR);
    char buffer[20];
    EXPECT_EQ(ANeuralNetworksModel_setOperandValue(mModel, 0, buffer, sizeof(buffer)),
              ANEURALNETWORKS_NO_ERROR);

    const size_t kByteSizeOfOEMTensor = 4;
    uint32_t dimensions[]{kByteSizeOfOEMTensor};
    ANeuralNetworksOperandType OEMTensorType{
            .type = ANEURALNETWORKS_TENSOR_OEM_BYTE, .dimensionCount = 1, .dimensions = dimensions};
    EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, &OEMTensorType), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_setOperandValue(mModel, 1, buffer, kByteSizeOfOEMTensor),
              ANEURALNETWORKS_NO_ERROR);

    modelFinish();
    // This should fail, as the model is already finished.
    EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, &OEMTensorType), ANEURALNETWORKS_BAD_STATE);
}

TEST_F(ValidationTestModel, AddOperation) {
    uint32_t input = 0;
    uint32_t output = 0;
    EXPECT_EQ(ANeuralNetworksModel_addOperation(nullptr, ANEURALNETWORKS_AVERAGE_POOL_2D, 1, &input,
                                                1, &output),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksModel_addOperation(mModel, ANEURALNETWORKS_AVERAGE_POOL_2D, 0, nullptr,
                                                1, &output),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksModel_addOperation(mModel, ANEURALNETWORKS_AVERAGE_POOL_2D, 1, &input,
                                                0, nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    ANeuralNetworksOperationType invalidOp = -1;
    EXPECT_EQ(addOperation(invalidOp, {input}, {output}), ANEURALNETWORKS_BAD_DATA);

    modelFinish();
    // This should fail, as the model is already finished.
    EXPECT_EQ(addOperation(ANEURALNETWORKS_AVERAGE_POOL_2D, {input}, {output}),
              ANEURALNETWORKS_BAD_STATE);
}

TEST_F(ValidationTestModel, IdentifyInputsAndOutputs) {
    uint32_t input = 0;
    uint32_t output = 0;
    EXPECT_EQ(ANeuralNetworksModel_identifyInputsAndOutputs(nullptr, 1, &input, 1, &output),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksModel_identifyInputsAndOutputs(mModel, 0, nullptr, 1, &output),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksModel_identifyInputsAndOutputs(mModel, 1, &input, 0, nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    createModel();
    // This should fail, as the model is already finished.
    EXPECT_EQ(identifyInputsAndOutputs({input}, {output}), ANEURALNETWORKS_BAD_STATE);
}

TEST_F(ValidationTestModel, RelaxComputationFloat32toFloat16) {
    EXPECT_EQ(ANeuralNetworksModel_relaxComputationFloat32toFloat16(nullptr, true),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    createModel();
    // This should fail, as the model is already finished.
    EXPECT_EQ(ANeuralNetworksModel_relaxComputationFloat32toFloat16(mModel, true),
              ANEURALNETWORKS_BAD_STATE);
    EXPECT_EQ(ANeuralNetworksModel_relaxComputationFloat32toFloat16(mModel, false),
              ANEURALNETWORKS_BAD_STATE);
}

TEST_F(ValidationTestModel, Finish) {
    EXPECT_EQ(ANeuralNetworksModel_finish(nullptr), ANEURALNETWORKS_UNEXPECTED_NULL);
    createModel();
    EXPECT_EQ(modelFinish(), ANEURALNETWORKS_BAD_STATE);
}

TEST_F(ValidationTestModel, EmptyModel) {
    // An empty model is invalid
    EXPECT_EQ(modelFinish(), ANEURALNETWORKS_BAD_DATA);
}

TEST_F(ValidationTestModel, CreateCompilation) {
    ANeuralNetworksCompilation* compilation = nullptr;
    EXPECT_EQ(ANeuralNetworksCompilation_create(nullptr, &compilation),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksCompilation_create(mModel, nullptr), ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksCompilation_create(mModel, &compilation), ANEURALNETWORKS_BAD_STATE);
}

TEST_F(ValidationTestModel, CreateCompilationForDevices) {
    createModel();
    uint32_t numDevices = 0;
    EXPECT_EQ(ANeuralNetworks_getDeviceCount(&numDevices), ANEURALNETWORKS_NO_ERROR);

    if (numDevices > 0) {
        ANeuralNetworksDevice* device;
        EXPECT_EQ(ANeuralNetworks_getDevice(0, &device), ANEURALNETWORKS_NO_ERROR);
        ANeuralNetworksCompilation* compilation = nullptr;
        EXPECT_EQ(ANeuralNetworksCompilation_createForDevices(nullptr, &device, 1, &compilation),
                  ANEURALNETWORKS_UNEXPECTED_NULL);
        EXPECT_EQ(ANeuralNetworksCompilation_createForDevices(mModel, &device, 1, nullptr),
                  ANEURALNETWORKS_UNEXPECTED_NULL);

        // empty device list
        EXPECT_EQ(ANeuralNetworksCompilation_createForDevices(mModel, &device, 0, &compilation),
                  ANEURALNETWORKS_BAD_DATA);

        // duplicate devices in the list.
        ANeuralNetworksDevice* invalidDevices[2] = {device, device};
        EXPECT_EQ(ANeuralNetworksCompilation_createForDevices(mModel, invalidDevices, 2,
                                                              &compilation),
                  ANEURALNETWORKS_BAD_DATA);
        // nullptr in the list.
        invalidDevices[1] = nullptr;
        EXPECT_EQ(ANeuralNetworksCompilation_createForDevices(mModel, invalidDevices, 2,
                                                              &compilation),
                  ANEURALNETWORKS_UNEXPECTED_NULL);
    }

    ANeuralNetworksCompilation* compilation = nullptr;
    EXPECT_EQ(ANeuralNetworksCompilation_createForDevices(nullptr, nullptr, 1, &compilation),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksCompilation_createForDevices(mModel, nullptr, 1, nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksCompilation_createForDevices(mModel, nullptr, 1, &compilation),
              ANEURALNETWORKS_UNEXPECTED_NULL);
}

TEST_F(ValidationTestModel, GetSupportedOperationsForDevices) {
    createModel();
    uint32_t numDevices = 0;
    EXPECT_EQ(ANeuralNetworks_getDeviceCount(&numDevices), ANEURALNETWORKS_NO_ERROR);

    bool supportedOps[20];
    ASSERT_LE(mNumOperations, sizeof(supportedOps) / sizeof(supportedOps[0]));
    if (numDevices > 0) {
        ANeuralNetworksDevice* device;
        EXPECT_EQ(ANeuralNetworks_getDevice(0, &device), ANEURALNETWORKS_NO_ERROR);
        EXPECT_EQ(ANeuralNetworksModel_getSupportedOperationsForDevices(nullptr, &device, 1,
                                                                        supportedOps),
                  ANEURALNETWORKS_UNEXPECTED_NULL);
        EXPECT_EQ(
                ANeuralNetworksModel_getSupportedOperationsForDevices(mModel, &device, 1, nullptr),
                ANEURALNETWORKS_UNEXPECTED_NULL);

        // empty device list
        EXPECT_EQ(ANeuralNetworksModel_getSupportedOperationsForDevices(mModel, &device, 0,
                                                                        supportedOps),
                  ANEURALNETWORKS_BAD_DATA);

        // duplicate devices in the list.
        ANeuralNetworksDevice* invalidDevices[2] = {device, device};
        EXPECT_EQ(ANeuralNetworksModel_getSupportedOperationsForDevices(mModel, invalidDevices, 2,
                                                                        supportedOps),
                  ANEURALNETWORKS_BAD_DATA);
        // nullptr in the list.
        invalidDevices[1] = nullptr;
        EXPECT_EQ(ANeuralNetworksModel_getSupportedOperationsForDevices(mModel, invalidDevices, 2,
                                                                        supportedOps),
                  ANEURALNETWORKS_UNEXPECTED_NULL);
    }

    EXPECT_EQ(ANeuralNetworksModel_getSupportedOperationsForDevices(nullptr, nullptr, 1,
                                                                    supportedOps),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksModel_getSupportedOperationsForDevices(mModel, nullptr, 1, nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(
            ANeuralNetworksModel_getSupportedOperationsForDevices(mModel, nullptr, 1, supportedOps),
            ANEURALNETWORKS_UNEXPECTED_NULL);
}

TEST_F(ValidationTestModel, Cycle) {
    uint32_t dimensions[]{1};
    ANeuralNetworksOperandType tensorType{
            .type = ANEURALNETWORKS_TENSOR_FLOAT32, .dimensionCount = 1, .dimensions = dimensions};
    ANeuralNetworksOperandType scalarType{
            .type = ANEURALNETWORKS_INT32, .dimensionCount = 0, .dimensions = nullptr};

    // opnd0 = model input TENSOR_FLOAT32
    // opnd1 = model input TENSOR_FLOAT32
    // opnd2 = model input INT32
    // opnd3 = ADD(opnd0, opnd4, opnd2)
    // opnd4 = ADD(opnd1, opnd3, opnd2)
    // opnd5 = ADD(opnd4, opnd0, opnd2)  // model output
    //
    //            +-----+
    //            |     |
    //            v     |
    // 3 = ADD(0, 4, 2) |
    // |                |
    // +----------+     |
    //            |     |
    //            v     |
    // 4 = ADD(1, 3, 2) |
    // |                |
    // +----------------+
    // |
    // |
    // +-------+
    //         |
    //         v
    // 5 = ADD(4, 0, 2)

    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &scalarType), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);

    ASSERT_EQ(addOperation(ANEURALNETWORKS_ADD, {0, 4, 2}, {3}), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(addOperation(ANEURALNETWORKS_ADD, {1, 3, 2}, {4}), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(addOperation(ANEURALNETWORKS_ADD, {4, 0, 2}, {5}), ANEURALNETWORKS_NO_ERROR);

    ASSERT_EQ(identifyInputsAndOutputs({0, 1, 2}, {5}), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(modelFinish(), ANEURALNETWORKS_BAD_DATA);
}

TEST_F(ValidationTestModel, AcyclicReadBeforeWrite) {
    uint32_t dimensions[]{1};
    ANeuralNetworksOperandType tensorType{
            .type = ANEURALNETWORKS_TENSOR_FLOAT32, .dimensionCount = 1, .dimensions = dimensions};

    // opnd0 = TENSOR_FLOAT32   // model input
    // opnd1 = LOGISTIC(opnd2)  // model output
    // opnd2 = LOGISTIC(opnd0)
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);

    ASSERT_EQ(addOperation(ANEURALNETWORKS_LOGISTIC, {2}, {1}), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(addOperation(ANEURALNETWORKS_LOGISTIC, {0}, {2}), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(identifyInputsAndOutputs({0}, {1}), ANEURALNETWORKS_NO_ERROR);

    // This should succeed, because NN API doesn't require that operations be sorted.
    ASSERT_EQ(modelFinish(), ANEURALNETWORKS_NO_ERROR);
}

TEST_F(ValidationTestModel, MissingWrite) {
    uint32_t dimensions[]{1};
    ANeuralNetworksOperandType tensorType{
            .type = ANEURALNETWORKS_TENSOR_FLOAT32, .dimensionCount = 1, .dimensions = dimensions};

    // opnd0 = TENSOR_FLOAT32  // model input
    // opnd1 = TENSOR_FLOAT32  // never written
    // opnd2 = LOGISTIC(opnd1) // model output
    // opnd3 = LOGISTIC(opnd0) // model output
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);

    ASSERT_EQ(addOperation(ANEURALNETWORKS_LOGISTIC, {1}, {2}), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(addOperation(ANEURALNETWORKS_LOGISTIC, {0}, {3}), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(identifyInputsAndOutputs({0}, {2, 3}), ANEURALNETWORKS_NO_ERROR);

    ASSERT_EQ(modelFinish(), ANEURALNETWORKS_BAD_DATA);
}

TEST_F(ValidationTestModel, UnwrittenOperand) {
    uint32_t dimensions[]{1};
    ANeuralNetworksOperandType tensorType{
            .type = ANEURALNETWORKS_TENSOR_FLOAT32, .dimensionCount = 1, .dimensions = dimensions};

    // opnd0 = TENSOR_FLOAT32  // model input
    // opnd1 = TENSOR_FLOAT32  // never written
    // opnd2 = LOGISTIC(opnd0) // model output
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);

    ASSERT_EQ(addOperation(ANEURALNETWORKS_LOGISTIC, {0}, {2}), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(identifyInputsAndOutputs({0}, {2}), ANEURALNETWORKS_NO_ERROR);

    ASSERT_EQ(modelFinish(), ANEURALNETWORKS_BAD_DATA);
}

TEST_F(ValidationTestModel, MultipleWrite) {
    uint32_t dimensions[]{1};
    ANeuralNetworksOperandType tensorType{
            .type = ANEURALNETWORKS_TENSOR_FLOAT32, .dimensionCount = 1, .dimensions = dimensions};
    ANeuralNetworksOperandType scalarType{
            .type = ANEURALNETWORKS_INT32, .dimensionCount = 0, .dimensions = nullptr};

    // opnd0 = TENSOR_FLOAT32            // model input
    // opnd1 = INT32                     // model input
    // opnd2 = ADD(opnd0, opnd0, opnd1)  // model output; do this twice
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &scalarType), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &tensorType), ANEURALNETWORKS_NO_ERROR);

    for (int i = 0; i < 2; ++i) {
        SCOPED_TRACE(i);
        ASSERT_EQ(addOperation(ANEURALNETWORKS_ADD, {0, 0, 1}, {2}), ANEURALNETWORKS_NO_ERROR);
    }

    ASSERT_EQ(identifyInputsAndOutputs({0, 1}, {2}), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(modelFinish(), ANEURALNETWORKS_BAD_DATA);
}

TEST_F(ValidationTestIdentify, Ok) {
    ASSERT_EQ(identifyInputsAndOutputs({0, 1, 2}, {3}), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(modelFinish(), ANEURALNETWORKS_NO_ERROR);
}

TEST_F(ValidationTestIdentify, InputIsOutput) {
    ASSERT_EQ(identifyInputsAndOutputs({0, 1, 2}, {3, 0}), ANEURALNETWORKS_BAD_DATA);
}

TEST_F(ValidationTestIdentify, OutputIsInput) {
    ASSERT_EQ(identifyInputsAndOutputs({0, 1, 2, 3}, {3}), ANEURALNETWORKS_BAD_DATA);
}

TEST_F(ValidationTestIdentify, DuplicateInputs) {
    ASSERT_EQ(identifyInputsAndOutputs({0, 1, 2, 0}, {3}), ANEURALNETWORKS_BAD_DATA);
}

TEST_F(ValidationTestIdentify, DuplicateOutputs) {
    ASSERT_EQ(identifyInputsAndOutputs({0, 1, 2}, {3, 3}), ANEURALNETWORKS_BAD_DATA);
}

// Also see TEST_F(ValidationTestCompilationForDevices_1, SetPreference)
TEST_F(ValidationTestCompilation, SetPreference) {
    EXPECT_EQ(ANeuralNetworksCompilation_setPreference(nullptr, ANEURALNETWORKS_PREFER_LOW_POWER),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    EXPECT_EQ(ANeuralNetworksCompilation_setPreference(mCompilation, 40), ANEURALNETWORKS_BAD_DATA);
}

// Also see TEST_F(ValidationTestCompilationForDevices_1, SetCaching)
TEST_F(ValidationTestCompilation, SetCaching) {
    std::vector<uint8_t> token(ANEURALNETWORKS_BYTE_SIZE_OF_CACHE_TOKEN, 0);
    EXPECT_EQ(ANeuralNetworksCompilation_setCaching(nullptr, "/data/local/tmp", token.data()),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksCompilation_setCaching(mCompilation, nullptr, token.data()),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksCompilation_setCaching(mCompilation, "/data/local/tmp", nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);
}

TEST_F(ValidationTestCompilation, SetPriority) {
    EXPECT_EQ(ANeuralNetworksCompilation_setPriority(nullptr, ANEURALNETWORKS_PRIORITY_DEFAULT),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    // Test invalid values of priority.
    constexpr int kInvalidPriorities[] = {0,
                                          ANEURALNETWORKS_PRIORITY_LOW - 1,
                                          ANEURALNETWORKS_PRIORITY_LOW + 1,
                                          ANEURALNETWORKS_PRIORITY_MEDIUM - 1,
                                          ANEURALNETWORKS_PRIORITY_MEDIUM + 1,
                                          ANEURALNETWORKS_PRIORITY_HIGH - 1,
                                          ANEURALNETWORKS_PRIORITY_HIGH + 1};
    for (int invalidPriority : kInvalidPriorities) {
        EXPECT_EQ(ANeuralNetworksCompilation_setPriority(mCompilation, invalidPriority),
                  ANEURALNETWORKS_BAD_DATA);
    }
}

// Also see TEST_F(ValidationTestCompilationForDevices_1, SetTimeout)
// Also see TEST_F(ValidationTestCompilationForDevices_2, SetTimeout)
TEST_F(ValidationTestCompilation, SetTimeout) {
    EXPECT_EQ(ANeuralNetworksCompilation_setTimeout(nullptr, kShortWaitInNanoseconds),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    // Timeout can only be set on Compilations created from CompilationForDevices with one device
    // specified.
    EXPECT_EQ(ANeuralNetworksCompilation_setTimeout(mCompilation, kShortWaitInNanoseconds),
              ANEURALNETWORKS_BAD_DATA);
}

// Also see TEST_F(ValidationTestCompilationForDevices_1, CreateExecution)
TEST_F(ValidationTestCompilation, CreateExecution) {
    ANeuralNetworksExecution* execution = nullptr;
    EXPECT_EQ(ANeuralNetworksExecution_create(nullptr, &execution),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksExecution_create(mCompilation, nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksExecution_create(mCompilation, &execution), ANEURALNETWORKS_BAD_STATE);
}

// Also see TEST_F(ValidationTestCompilationForDevices_1, Finish)
TEST_F(ValidationTestCompilation, Finish) {
    EXPECT_EQ(ANeuralNetworksCompilation_finish(nullptr), ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksCompilation_finish(mCompilation), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksCompilation_setPreference(mCompilation,
                                                       ANEURALNETWORKS_PREFER_FAST_SINGLE_ANSWER),
              ANEURALNETWORKS_BAD_STATE);
    EXPECT_EQ(
            ANeuralNetworksCompilation_setPriority(mCompilation, ANEURALNETWORKS_PRIORITY_DEFAULT),
            ANEURALNETWORKS_BAD_STATE);
    EXPECT_EQ(ANeuralNetworksCompilation_setTimeout(mCompilation, kShortWaitInNanoseconds),
              ANEURALNETWORKS_BAD_STATE);
    std::vector<uint8_t> token(ANEURALNETWORKS_BYTE_SIZE_OF_CACHE_TOKEN, 0);
    EXPECT_EQ(ANeuralNetworksCompilation_setCaching(mCompilation, "/data/local/tmp", token.data()),
              ANEURALNETWORKS_BAD_STATE);
    EXPECT_EQ(ANeuralNetworksCompilation_finish(mCompilation), ANEURALNETWORKS_BAD_STATE);
}

// Also see TEST_F(ValidationTestCompilationForDevices_1, ExecutionSetTimeout)
// Also see TEST_F(ValidationTestCompilationForDevices_2, ExecutionSetTimeout)
TEST_F(ValidationTestCompilation, ExecutionSetTimeout) {
    EXPECT_EQ(ANeuralNetworksExecution_setTimeout(nullptr, kShortWaitInNanoseconds),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    ASSERT_EQ(ANeuralNetworksCompilation_finish(mCompilation), ANEURALNETWORKS_NO_ERROR);
    ANeuralNetworksExecution* execution;
    ASSERT_EQ(ANeuralNetworksExecution_create(mCompilation, &execution), ANEURALNETWORKS_NO_ERROR);
    // Timeout can only be set on Compilations created from CompilationForDevices with one device
    // specified.
    EXPECT_EQ(ANeuralNetworksExecution_setTimeout(execution, kShortWaitInNanoseconds),
              ANEURALNETWORKS_BAD_DATA);
    ANeuralNetworksExecution_free(execution);
}

// Also see TEST_F(ValidationTestCompilationForDevices_1, ExecutionTiming)
// Also see TEST_F(ValidationTestCompilationForDevices_2, ExecutionTiming)
TEST_F(ValidationTestCompilation, ExecutionTiming) {
    ASSERT_EQ(ANeuralNetworksCompilation_finish(mCompilation), ANEURALNETWORKS_NO_ERROR);
    ANeuralNetworksExecution* execution;
    ASSERT_EQ(ANeuralNetworksExecution_create(mCompilation, &execution), ANEURALNETWORKS_NO_ERROR);
    // Cannot setMeasureTiming() with Compilation rather than CompilationForDevices.
    EXPECT_EQ(ANeuralNetworksExecution_setMeasureTiming(execution, false),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksExecution_setMeasureTiming(execution, true), ANEURALNETWORKS_BAD_DATA);
}

// Also see TEST_F(ValidationTestCompilationForDevices_1, ExecutionTiming)
TEST_F(ValidationTestCompilation, ExecutionUsability) {
    ASSERT_EQ(ANeuralNetworksCompilation_finish(mCompilation), ANEURALNETWORKS_NO_ERROR);

    enum class ExecutionType : uint32_t { ASYNC, SYNC, BURST, FENCED };
    for (auto executionType :
         {ExecutionType::ASYNC, ExecutionType::SYNC, ExecutionType::BURST, ExecutionType::FENCED}) {
        SCOPED_TRACE(static_cast<uint32_t>(executionType));

        ANeuralNetworksExecution* execution;
        ASSERT_EQ(ANeuralNetworksExecution_create(mCompilation, &execution),
                  ANEURALNETWORKS_NO_ERROR);

        float in0[] = {0.0f, 0.0f}, in1[] = {1.0f, 1.0f}, out0[2];
        int in2 = 0;
        ASSERT_EQ(ANeuralNetworksExecution_setInput(execution, 0, nullptr, &in0, sizeof(in0)),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksExecution_setInput(execution, 1, nullptr, &in1, sizeof(in1)),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksExecution_setInput(execution, 2, nullptr, &in2, sizeof(in2)),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksExecution_setOutput(execution, 0, nullptr, &out0, sizeof(out0)),
                  ANEURALNETWORKS_NO_ERROR);

        const size_t memorySize = std::max(sizeof(in0), sizeof(out0));
        int memoryFd = ASharedMemory_create("nnMemory", memorySize);
        ASSERT_GT(memoryFd, 0);
        ANeuralNetworksMemory* memory;
        EXPECT_EQ(ANeuralNetworksMemory_createFromFd(memorySize, PROT_READ | PROT_WRITE, memoryFd,
                                                     0, &memory),
                  ANEURALNETWORKS_NO_ERROR);

        auto testTooLate = [this, execution, &in0, &out0, memory] {
            // Try a bunch of things that are impermissible if the execution has started.

            // Set loop timeout.
            ASSERT_EQ(ANeuralNetworksExecution_setLoopTimeout(execution, kShortWaitInNanoseconds),
                      ANEURALNETWORKS_BAD_STATE);

            // Set inputs and outputs.
            ASSERT_EQ(ANeuralNetworksExecution_setInput(execution, 0, nullptr, &in0, sizeof(in0)),
                      ANEURALNETWORKS_BAD_STATE);
            ASSERT_EQ(
                    ANeuralNetworksExecution_setOutput(execution, 0, nullptr, &out0, sizeof(out0)),
                    ANEURALNETWORKS_BAD_STATE);
            ASSERT_EQ(ANeuralNetworksExecution_setInputFromMemory(execution, 0, nullptr, memory, 0,
                                                                  sizeof(in0)),
                      ANEURALNETWORKS_BAD_STATE);
            ASSERT_EQ(ANeuralNetworksExecution_setOutputFromMemory(execution, 0, nullptr, memory, 0,
                                                                   sizeof(out0)),
                      ANEURALNETWORKS_BAD_STATE);

            // Reuse for asynchronous execution.
            {
                ANeuralNetworksEvent* event;
                ASSERT_EQ(ANeuralNetworksExecution_startCompute(execution, &event),
                          ANEURALNETWORKS_BAD_STATE);
            }

            // Reuse for synchronous execution.
            ASSERT_EQ(ANeuralNetworksExecution_compute(execution), ANEURALNETWORKS_BAD_STATE);

            // Reuse for burst execution.
            {
                ANeuralNetworksBurst* burst;
                ASSERT_EQ(ANeuralNetworksBurst_create(mCompilation, &burst),
                          ANEURALNETWORKS_NO_ERROR);
                ASSERT_EQ(ANeuralNetworksExecution_burstCompute(execution, burst),
                          ANEURALNETWORKS_BAD_STATE);
                ANeuralNetworksBurst_free(burst);
            }

            // Reuse for fenced execution.
            {
                ANeuralNetworksEvent* event;
                ASSERT_EQ(ANeuralNetworksExecution_startComputeWithDependencies(execution, nullptr,
                                                                                0, 0, &event),
                          ANEURALNETWORKS_BAD_STATE);
            }
        };

        // Compute.
        switch (executionType) {
            case ExecutionType::ASYNC: {
                ANeuralNetworksEvent* event;
                ASSERT_EQ(ANeuralNetworksExecution_startCompute(execution, &event),
                          ANEURALNETWORKS_NO_ERROR);
                testTooLate();
                ASSERT_EQ(ANeuralNetworksEvent_wait(event), ANEURALNETWORKS_NO_ERROR);
                testTooLate();
                ANeuralNetworksEvent_free(event);
                break;
            }
            case ExecutionType::SYNC: {
                ASSERT_EQ(ANeuralNetworksExecution_compute(execution), ANEURALNETWORKS_NO_ERROR);
                testTooLate();
                break;
            }
            case ExecutionType::BURST: {
                ANeuralNetworksBurst* burst;
                ASSERT_EQ(ANeuralNetworksBurst_create(mCompilation, &burst),
                          ANEURALNETWORKS_NO_ERROR);
                ASSERT_EQ(ANeuralNetworksExecution_burstCompute(execution, burst),
                          ANEURALNETWORKS_NO_ERROR);
                testTooLate();
                ANeuralNetworksBurst_free(burst);
                break;
            }
            case ExecutionType::FENCED: {
                ANeuralNetworksEvent* event;
                ASSERT_EQ(ANeuralNetworksExecution_startComputeWithDependencies(execution, nullptr,
                                                                                0, 0, &event),
                          ANEURALNETWORKS_NO_ERROR);
                testTooLate();
                ASSERT_EQ(ANeuralNetworksEvent_wait(event), ANEURALNETWORKS_NO_ERROR);
                testTooLate();
                ANeuralNetworksEvent_free(event);
                break;
            }
            default:
                FAIL() << "Unreachable";
        }
    }
}

TEST_F(ValidationTestExecution, SetLoopTimeout) {
    EXPECT_EQ(ANeuralNetworksExecution_setLoopTimeout(nullptr, kShortWaitInNanoseconds),
              ANEURALNETWORKS_UNEXPECTED_NULL);
}

TEST_F(ValidationTestExecution, SetInput) {
    char buffer[20];
    EXPECT_EQ(ANeuralNetworksExecution_setInput(nullptr, 0, nullptr, buffer, sizeof(float)),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksExecution_setInput(mExecution, 0, nullptr, nullptr, sizeof(float)),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    // This should fail, since memory is not the size of a float32.
    EXPECT_EQ(ANeuralNetworksExecution_setInput(mExecution, 0, nullptr, buffer, 20),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, as this operand does not exist.
    EXPECT_EQ(ANeuralNetworksExecution_setInput(mExecution, 999, nullptr, buffer, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, as this operand does not exist.
    EXPECT_EQ(ANeuralNetworksExecution_setInput(mExecution, -1, nullptr, buffer, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // These should fail, since the tensor types are invalid.
    EXPECT_EQ(ANeuralNetworksExecution_setInput(mExecution, 0, &kInvalidTensorType1, buffer,
                                                sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksExecution_setInput(mExecution, 0, &kInvalidTensorType2, buffer,
                                                sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // Cannot do this twice.
    EXPECT_EQ(ANeuralNetworksExecution_setInput(mExecution, 0, nullptr, buffer, 8),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setInput(mExecution, 0, nullptr, buffer, 8),
              ANEURALNETWORKS_BAD_STATE);
}

TEST_F(ValidationTestExecution, SetOutput) {
    char buffer[20];
    EXPECT_EQ(ANeuralNetworksExecution_setOutput(nullptr, 0, nullptr, buffer, sizeof(float)),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksExecution_setOutput(mExecution, 0, nullptr, nullptr, sizeof(float)),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    // This should fail, since memory is not the size of a float32.
    EXPECT_EQ(ANeuralNetworksExecution_setOutput(mExecution, 0, nullptr, buffer, 20),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, as this operand does not exist.
    EXPECT_EQ(ANeuralNetworksExecution_setOutput(mExecution, 999, nullptr, buffer, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, as this operand does not exist.
    EXPECT_EQ(ANeuralNetworksExecution_setOutput(mExecution, -1, nullptr, buffer, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // These should fail, since the tensor types are invalid.
    EXPECT_EQ(ANeuralNetworksExecution_setOutput(mExecution, 0, &kInvalidTensorType1, buffer,
                                                 sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksExecution_setOutput(mExecution, 0, &kInvalidTensorType2, buffer,
                                                 sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // Cannot do this twice.
    EXPECT_EQ(ANeuralNetworksExecution_setOutput(mExecution, 0, nullptr, buffer, 8),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setOutput(mExecution, 0, nullptr, buffer, 8),
              ANEURALNETWORKS_BAD_STATE);
}

TEST_F(ValidationTestExecution, SetInputFromMemory) {
    const size_t memorySize = 20;
    int memoryFd = ASharedMemory_create("nnMemory", memorySize);
    ASSERT_GT(memoryFd, 0);

    ANeuralNetworksMemory* memory;
    EXPECT_EQ(ANeuralNetworksMemory_createFromFd(memorySize, PROT_READ | PROT_WRITE, memoryFd, 0,
                                                 &memory),
              ANEURALNETWORKS_NO_ERROR);

    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(nullptr, 0, nullptr, memory, 0,
                                                          sizeof(float)),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(mExecution, 0, nullptr, nullptr, 0,
                                                          sizeof(float)),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    // This should fail, since the operand does not exist.
    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(mExecution, 999, nullptr, memory, 0,
                                                          sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, since the operand does not exist.
    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(mExecution, -1, nullptr, memory, 0,
                                                          sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, since memory is not the size of a float32.
    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(mExecution, 0, nullptr, memory, 0,
                                                          memorySize),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, since offset is larger than memorySize.
    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(mExecution, 0, nullptr, memory,
                                                          memorySize + 1, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, since requested size is larger than the memory.
    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(mExecution, 0, nullptr, memory,
                                                          memorySize - 3, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // These should fail, since the tensor types are invalid.
    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(mExecution, 0, &kInvalidTensorType1,
                                                          memory, 0, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(mExecution, 0, &kInvalidTensorType2,
                                                          memory, 0, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // Cannot do this twice.
    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(mExecution, 0, nullptr, memory, 0, 8),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(mExecution, 0, nullptr, memory, 0, 8),
              ANEURALNETWORKS_BAD_STATE);
    char buffer[memorySize];
    EXPECT_EQ(ANeuralNetworksExecution_setInput(mExecution, 0, nullptr, buffer, 8),
              ANEURALNETWORKS_BAD_STATE);

    // close memory
    close(memoryFd);
}

TEST_F(ValidationTestExecution, SetInputFromAHardwareBufferBlob) {
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

    ANeuralNetworksMemory* memory;
    EXPECT_EQ(ANeuralNetworksMemory_createFromAHardwareBuffer(buffer, &memory),
              ANEURALNETWORKS_NO_ERROR);

    // This should fail, since memory is not the size of a float32.
    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(mExecution, 0, nullptr, memory, 0,
                                                          memorySize),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, since offset is larger than memorySize.
    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(mExecution, 0, nullptr, memory,
                                                          memorySize + 1, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);
    // This should fail, since requested size is larger than the memory.
    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(mExecution, 0, nullptr, memory,
                                                          memorySize - 3, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // These should fail, since the tensor types are invalid.
    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(mExecution, 0, &kInvalidTensorType1,
                                                          memory, 0, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(mExecution, 0, &kInvalidTensorType2,
                                                          memory, 0, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    AHardwareBuffer_release(buffer);
}

TEST_F(ValidationTestExecution, SetOutputFromMemory) {
    ANeuralNetworksExecution* execution;
    EXPECT_EQ(ANeuralNetworksExecution_create(mCompilation, &execution), ANEURALNETWORKS_NO_ERROR);

    const size_t memorySize = 20;
    int memoryFd = ASharedMemory_create("nnMemory", memorySize);
    ASSERT_GT(memoryFd, 0);

    ANeuralNetworksMemory* memory;
    EXPECT_EQ(ANeuralNetworksMemory_createFromFd(memorySize, PROT_READ | PROT_WRITE, memoryFd, 0,
                                                 &memory),
              ANEURALNETWORKS_NO_ERROR);

    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(nullptr, 0, nullptr, memory, 0,
                                                           sizeof(float)),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(execution, 0, nullptr, nullptr, 0,
                                                           sizeof(float)),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    // This should fail, since the operand does not exist.
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(execution, 999, nullptr, memory, 0,
                                                           sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, since the operand does not exist.
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(execution, -1, nullptr, memory, 0,
                                                           sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, since memory is not the size of a float32.
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(execution, 0, nullptr, memory, 0,
                                                           memorySize),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, since offset is larger than memorySize.
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(execution, 0, nullptr, memory,
                                                           memorySize + 1, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, since requested size is larger than the memory.
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(execution, 0, nullptr, memory,
                                                           memorySize - 3, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // These should fail, since the tensor types are invalid.
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(execution, 0, &kInvalidTensorType1,
                                                           memory, 0, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(execution, 0, &kInvalidTensorType2,
                                                           memory, 0, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // Cannot do this twice.
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(execution, 0, nullptr, memory, 0, 8),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(execution, 0, nullptr, memory, 0, 8),
              ANEURALNETWORKS_BAD_STATE);
    char buffer[memorySize];
    EXPECT_EQ(ANeuralNetworksExecution_setOutput(execution, 0, nullptr, buffer, 8),
              ANEURALNETWORKS_BAD_STATE);

    // close memory
    close(memoryFd);
}

TEST_F(ValidationTestExecution, SetOutputFromAHardwareBufferBlob) {
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

    ANeuralNetworksMemory* memory;
    EXPECT_EQ(ANeuralNetworksMemory_createFromAHardwareBuffer(buffer, &memory),
              ANEURALNETWORKS_NO_ERROR);

    // This should fail, since memory is not the size of a float32.
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(mExecution, 0, nullptr, memory, 0,
                                                           memorySize),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, since offset is larger than memorySize.
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(mExecution, 0, nullptr, memory,
                                                           memorySize + 1, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // This should fail, since requested size is larger than the memory.
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(mExecution, 0, nullptr, memory,
                                                           memorySize - 3, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // These should fail, since the tensor types are invalid.
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(mExecution, 0, &kInvalidTensorType1,
                                                           memory, 0, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(mExecution, 0, &kInvalidTensorType2,
                                                           memory, 0, sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    AHardwareBuffer_release(buffer);
}

TEST_F(ValidationTestExecutionDeviceMemory, SetInputFromMemory) {
    ANeuralNetworksMemoryDesc* desc;
    ASSERT_EQ(ANeuralNetworksMemoryDesc_create(&desc), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(desc, mCompilation, 0, 1.0f),
              ANEURALNETWORKS_NO_ERROR);

    // The following output roles are for init/deinit of the device memory.
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(desc, mInitCompilation, 0, 1.0f),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(desc, mDeinitCompilation, 0, 1.0f),
              ANEURALNETWORKS_NO_ERROR);

    EXPECT_EQ(ANeuralNetworksMemoryDesc_finish(desc), ANEURALNETWORKS_NO_ERROR);

    ANeuralNetworksMemory* memory;
    EXPECT_EQ(ANeuralNetworksMemory_createFromDesc(desc, &memory), ANEURALNETWORKS_NO_ERROR);
    ANeuralNetworksMemoryDesc_free(desc);

    // Uninitialized memory as input.
    executeWithMemoryAsInput(mCompilation, memory, ANEURALNETWORKS_OP_FAILED);

    // The memory is deinitialized between setInputFromMemory and compute.
    {
        // Initialize device memory.
        executeWithMemoryAsOutput(mInitCompilation, memory, ANEURALNETWORKS_NO_ERROR);

        float data = 0;
        ANeuralNetworksExecution* execution = nullptr;
        ASSERT_EQ(ANeuralNetworksExecution_create(mCompilation, &execution),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksExecution_setInputFromMemory(execution, 0, nullptr, memory, 0, 0),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksExecution_setOutput(execution, 0, nullptr, &data, sizeof(float)),
                  ANEURALNETWORKS_NO_ERROR);

        // Deinitialize device memory.
        executeWithMemoryAsOutput(mDeinitCompilation, memory, ANEURALNETWORKS_OP_FAILED);

        // Uninitialized memory as input at compute time.
        ASSERT_EQ(ANeuralNetworksExecution_compute(execution), ANEURALNETWORKS_OP_FAILED);
        ANeuralNetworksExecution_free(execution);
    }

    // Initialize device memory.
    executeWithMemoryAsOutput(mInitCompilation, memory, ANEURALNETWORKS_NO_ERROR);

    // Bad offset and length.
    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(mExecution, 0, nullptr, memory, 1, 0),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(mExecution, 0, nullptr, memory, 0,
                                                          sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // Bad usage -- not configured for this role.
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(mExecution, 0, nullptr, memory, 0, 0),
              ANEURALNETWORKS_BAD_DATA);

    // Deinitialize device memory.
    executeWithMemoryAsOutput(mDeinitCompilation, memory, ANEURALNETWORKS_OP_FAILED);

    // Uninitialized memory as input.
    executeWithMemoryAsInput(mCompilation, memory, ANEURALNETWORKS_OP_FAILED);

    ANeuralNetworksMemory_free(memory);
}

TEST_F(ValidationTestExecutionDeviceMemory, SetOutputFromMemory) {
    ANeuralNetworksMemoryDesc* desc;
    ASSERT_EQ(ANeuralNetworksMemoryDesc_create(&desc), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(desc, mCompilation, 0, 1.0f),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_finish(desc), ANEURALNETWORKS_NO_ERROR);

    ANeuralNetworksMemory* memory;
    EXPECT_EQ(ANeuralNetworksMemory_createFromDesc(desc, &memory), ANEURALNETWORKS_NO_ERROR);
    ANeuralNetworksMemoryDesc_free(desc);

    // Bad offset and length.
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(mExecution, 0, nullptr, memory, 1, 0),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(mExecution, 0, nullptr, memory, 0,
                                                           sizeof(float)),
              ANEURALNETWORKS_BAD_DATA);

    // Bad usage -- not configured for this role.
    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(mExecution, 0, nullptr, memory, 0, 0),
              ANEURALNETWORKS_BAD_DATA);

    ANeuralNetworksMemory_free(memory);
}

TEST_F(ValidationTestExecutionDeviceMemory, SetInputFromMemory_DynamicShape) {
    uint32_t dimension = 1, badDimension = 2;
    ANeuralNetworksOperandType badType = {
            .type = ANEURALNETWORKS_TENSOR_FLOAT32,
            .dimensionCount = 1,
            .dimensions = &badDimension,
    };

    ANeuralNetworksMemoryDesc* desc;
    ASSERT_EQ(ANeuralNetworksMemoryDesc_create(&desc), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(desc, mCompilationDynamic, 0, 1.0f),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_setDimensions(desc, 1, &dimension),
              ANEURALNETWORKS_NO_ERROR);

    // The following output role is for init of the device memory.
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(desc, mInitCompilation, 0, 1.0f),
              ANEURALNETWORKS_NO_ERROR);

    EXPECT_EQ(ANeuralNetworksMemoryDesc_finish(desc), ANEURALNETWORKS_NO_ERROR);

    ANeuralNetworksMemory* memory;
    EXPECT_EQ(ANeuralNetworksMemory_createFromDesc(desc, &memory), ANEURALNETWORKS_NO_ERROR);
    ANeuralNetworksMemoryDesc_free(desc);

    // Initialize device memory.
    executeWithMemoryAsOutput(mInitCompilation, memory, ANEURALNETWORKS_NO_ERROR);

    // Incompatible dimensions between updated type and memory.
    EXPECT_EQ(ANeuralNetworksExecution_setInputFromMemory(mExecutionDynamic, 0, &badType, memory, 0,
                                                          0),
              ANEURALNETWORKS_BAD_DATA);

    ANeuralNetworksMemory_free(memory);
}

TEST_F(ValidationTestExecutionDeviceMemory, SetOutputFromMemory_DynamicShape) {
    uint32_t dimension = 1, badDimension = 2;
    ANeuralNetworksOperandType badType = {
            .type = ANEURALNETWORKS_TENSOR_FLOAT32,
            .dimensionCount = 1,
            .dimensions = &badDimension,
    };

    ANeuralNetworksMemoryDesc* desc;
    ASSERT_EQ(ANeuralNetworksMemoryDesc_create(&desc), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(desc, mCompilationDynamic, 0, 1.0f),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_setDimensions(desc, 1, &dimension),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_finish(desc), ANEURALNETWORKS_NO_ERROR);

    ANeuralNetworksMemory* memory;
    EXPECT_EQ(ANeuralNetworksMemory_createFromDesc(desc, &memory), ANEURALNETWORKS_NO_ERROR);
    ANeuralNetworksMemoryDesc_free(desc);

    // Incompatible dimensions between updated type and memory.
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(mExecutionDynamic, 0, &badType, memory,
                                                           0, 0),
              ANEURALNETWORKS_BAD_DATA);

    ANeuralNetworksMemory_free(memory);
}

TEST_F(ValidationTestExecution, Compute) {
    EXPECT_EQ(ANeuralNetworksExecution_compute(nullptr), ANEURALNETWORKS_UNEXPECTED_NULL);
}

TEST_F(ValidationTestExecution, StartCompute) {
    ANeuralNetworksExecution* execution;
    EXPECT_EQ(ANeuralNetworksExecution_create(mCompilation, &execution), ANEURALNETWORKS_NO_ERROR);

    ANeuralNetworksEvent* event;
    EXPECT_EQ(ANeuralNetworksExecution_startCompute(nullptr, &event),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksExecution_startCompute(execution, nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);
}

TEST_F(ValidationTestExecution, EventWait) {
    EXPECT_EQ(ANeuralNetworksEvent_wait(nullptr), ANEURALNETWORKS_UNEXPECTED_NULL);
}

TEST_F(ValidationTest, EventCreateFromSyncFenceFd) {
    ANeuralNetworksEvent* event;
    EXPECT_EQ(ANeuralNetworksEvent_createFromSyncFenceFd(-1, &event), ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksEvent_createFromSyncFenceFd(1, nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);
}

TEST_F(ValidationTest, EventGetSyncFenceFd) {
    int sync_fd = -1;
    EXPECT_EQ(ANeuralNetworksEvent_getSyncFenceFd(nullptr, &sync_fd),
              ANEURALNETWORKS_UNEXPECTED_NULL);
}

TEST_F(ValidationTestExecution, FencedExecution) {
    // Create a valid execution and event first.
    ANeuralNetworksExecution* execution1;
    EXPECT_EQ(ANeuralNetworksExecution_create(mCompilation, &execution1), ANEURALNETWORKS_NO_ERROR);
    float input0[] = {1.0f, 1.0f}, input1[] = {2.0f, 2.0f}, output0[2];
    int32_t input2[] = {0};
    EXPECT_EQ(ANeuralNetworksExecution_setInput(execution1, 0, nullptr, input0, sizeof(input0)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setInput(execution1, 1, nullptr, input1, sizeof(input1)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setInput(execution1, 2, nullptr, input2, sizeof(input2)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setOutput(execution1, 0, nullptr, output0, sizeof(output0)),
              ANEURALNETWORKS_NO_ERROR);
    ANeuralNetworksEvent* event1 = nullptr;
    EXPECT_EQ(ANeuralNetworksExecution_startComputeWithDependencies(execution1, nullptr, 0, 0,
                                                                    &event1),
              ANEURALNETWORKS_NO_ERROR);

    EXPECT_EQ(ANeuralNetworksEvent_getSyncFenceFd(event1, nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    // The subsequent execution will wait for the first execution to finish.
    ANeuralNetworksExecution* execution2;
    ANeuralNetworksEvent* event2 = nullptr;
    EXPECT_EQ(ANeuralNetworksExecution_create(mCompilation, &execution2), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(
            ANeuralNetworksExecution_startComputeWithDependencies(nullptr, &event1, 1, 0, &event2),
            ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksExecution_startComputeWithDependencies(execution2, nullptr, 1, 0,
                                                                    &event2),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksExecution_startComputeWithDependencies(execution2, &event1, 1, 0,
                                                                    nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    ANeuralNetworksEvent* wait_for_list[] = {event1, nullptr};
    EXPECT_EQ(ANeuralNetworksExecution_startComputeWithDependencies(execution2, wait_for_list, 2, 0,
                                                                    &event2),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    ANeuralNetworksEvent_free(event1);
    ANeuralNetworksExecution_free(execution1);
    ANeuralNetworksExecution_free(execution2);
}

TEST_F(ValidationTestExecution, GetOutputOperandRankAndDimensions) {
    ANeuralNetworksExecution* execution;
    EXPECT_EQ(ANeuralNetworksExecution_create(mCompilation, &execution), ANEURALNETWORKS_NO_ERROR);

    float input0[] = {1.0f, 1.0f}, input1[] = {2.0f, 2.0f}, output0[2];
    int32_t input2[] = {0};
    EXPECT_EQ(ANeuralNetworksExecution_setInput(execution, 0, nullptr, input0, sizeof(input0)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setInput(execution, 1, nullptr, input1, sizeof(input1)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setInput(execution, 2, nullptr, input2, sizeof(input2)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setOutput(execution, 0, nullptr, output0, sizeof(output0)),
              ANEURALNETWORKS_NO_ERROR);

    uint32_t rank, dims[4], expectedRank = 1, expectedDims = 2;
    // This should fail, since the execution has not yet started to compute.
    EXPECT_EQ(ANeuralNetworksExecution_getOutputOperandRank(execution, 0, &rank),
              ANEURALNETWORKS_BAD_STATE);
    EXPECT_EQ(ANeuralNetworksExecution_getOutputOperandDimensions(execution, 0, dims),
              ANEURALNETWORKS_BAD_STATE);

    ANeuralNetworksEvent* event;
    EXPECT_EQ(ANeuralNetworksExecution_startCompute(execution, &event), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksEvent_wait(event), ANEURALNETWORKS_NO_ERROR);

    // This should fail, since unexpected nullptr.
    EXPECT_EQ(ANeuralNetworksExecution_getOutputOperandRank(nullptr, 0, &rank),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksExecution_getOutputOperandDimensions(nullptr, 0, dims),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksExecution_getOutputOperandRank(execution, 0, nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksExecution_getOutputOperandDimensions(execution, 0, nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    // This should fail, since the operand does not exist.
    EXPECT_EQ(ANeuralNetworksExecution_getOutputOperandRank(execution, -1, &rank),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksExecution_getOutputOperandRank(execution, 999, &rank),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksExecution_getOutputOperandDimensions(execution, -1, dims),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksExecution_getOutputOperandDimensions(execution, 999, dims),
              ANEURALNETWORKS_BAD_DATA);

    EXPECT_EQ(ANeuralNetworksExecution_getOutputOperandRank(execution, 0, &rank),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_getOutputOperandDimensions(execution, 0, dims),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(rank, expectedRank);
    EXPECT_EQ(dims[0], expectedDims);
}

// Regression test for b/146044137.
class ValidationTestDimensionProductOverflow : public ValidationTestExecution {
   protected:
    void createModel() override {
        uint32_t dimensions[] = {5, 4, 4, 0, 5, 3, 0, 4, 5};
        ANeuralNetworksOperandType operandType = {
                .type = ANEURALNETWORKS_TENSOR_FLOAT32,
                .dimensionCount = std::size(dimensions),
                .dimensions = dimensions,
        };
        addOperand(operandType);
        addOperand(operandType);
        ASSERT_EQ(addOperation(ANEURALNETWORKS_ABS, {0}, {1}), ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(identifyInputsAndOutputs({0}, {1}), ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(modelFinish(), ANEURALNETWORKS_NO_ERROR);
    }
};

TEST_F(ValidationTestDimensionProductOverflow, SetInputOrOutput) {
    uint32_t dimensions[] = {5, 4, 4, 786433, 5, 3, 16777216, 4, 5};
    ANeuralNetworksOperandType operandType = {
            .type = ANEURALNETWORKS_TENSOR_FLOAT32,
            .dimensionCount = std::size(dimensions),
            .dimensions = dimensions,
    };
    uint8_t buffer[20];
    // This should fail, as the new operand type's dimension product overflows
    // uint32_t.
    EXPECT_EQ(
            ANeuralNetworksExecution_setInput(mExecution, 0, &operandType, buffer, sizeof(buffer)),
            ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(
            ANeuralNetworksExecution_setOutput(mExecution, 0, &operandType, buffer, sizeof(buffer)),
            ANEURALNETWORKS_BAD_DATA);
}

TEST_F(ValidationTestModel, AddOperandDimensionProductOverflow) {
    uint32_t dimensions[] = {5, 4, 4, 786433, 5, 3, 16777216, 4, 5};
    ANeuralNetworksOperandType operandType = {
            .type = ANEURALNETWORKS_TENSOR_FLOAT32,
            .dimensionCount = std::size(dimensions),
            .dimensions = dimensions,
    };
    // This should fail, as the operand type's dimension product overflows uint32_t.
    ASSERT_EQ(ANeuralNetworksModel_addOperand(mModel, &operandType), ANEURALNETWORKS_BAD_DATA);
}

class ValidationTestDimensionProductOverflow2 : public ValidationTestExecution {
   protected:
    void createModel() override {
        addTensorOperand(ANEURALNETWORKS_TENSOR_FLOAT32, {0, 1});
        addTensorOperand(ANEURALNETWORKS_TENSOR_FLOAT32, {0, 1});
        addTensorOperand(ANEURALNETWORKS_TENSOR_FLOAT32, {0});
        addScalarOperand(ANEURALNETWORKS_INT32);
        int32_t activation = 0;
        ASSERT_EQ(ANeuralNetworksModel_setOperandValue(mModel, 3, &activation, sizeof(activation)),
                  ANEURALNETWORKS_NO_ERROR);
        addTensorOperand(ANEURALNETWORKS_TENSOR_FLOAT32, {0, 0});
        ASSERT_EQ(addOperation(ANEURALNETWORKS_FULLY_CONNECTED, {0, 1, 2, 3}, {4}),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(identifyInputsAndOutputs({0, 1, 2}, {4}), ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(modelFinish(), ANEURALNETWORKS_NO_ERROR);
    }
};

TEST_F(ValidationTestDimensionProductOverflow2, DynamicOutputShapeOverflow) {
    constexpr uint32_t kLargeDim = 1 << 16;
    std::vector<float> inputData(kLargeDim), outputData(kLargeDim);
    const uint32_t inputDims[] = {kLargeDim, 1};
    const uint32_t biasDims[] = {kLargeDim};
    const ANeuralNetworksOperandType inputType = {
            .type = ANEURALNETWORKS_TENSOR_FLOAT32,
            .dimensionCount = std::size(inputDims),
            .dimensions = inputDims,
    };
    const ANeuralNetworksOperandType biasType = {
            .type = ANEURALNETWORKS_TENSOR_FLOAT32,
            .dimensionCount = std::size(biasDims),
            .dimensions = biasDims,
    };
    EXPECT_EQ(ANeuralNetworksExecution_setInput(mExecution, 0, &inputType, inputData.data(),
                                                inputData.size() * sizeof(float)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setInput(mExecution, 1, &inputType, inputData.data(),
                                                inputData.size() * sizeof(float)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setInput(mExecution, 2, &biasType, inputData.data(),
                                                inputData.size() * sizeof(float)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setOutput(mExecution, 0, nullptr, outputData.data(),
                                                 outputData.size() * sizeof(float)),
              ANEURALNETWORKS_NO_ERROR);

    // This should fail, because the deduced output data size overflows uint32_t.
    EXPECT_NE(ANeuralNetworksExecution_compute(mExecution), ANEURALNETWORKS_NO_ERROR);
}

TEST_F(ValidationTestBurst, BurstComputeNull) {
    EXPECT_EQ(ANeuralNetworksExecution_burstCompute(mExecution, nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksExecution_burstCompute(nullptr, mBurst),
              ANEURALNETWORKS_UNEXPECTED_NULL);
}

TEST_F(ValidationTestBurst, BurstComputeBadCompilation) {
    ANeuralNetworksCompilation* compilation;
    ASSERT_EQ(ANeuralNetworksCompilation_create(mModel, &compilation), ANEURALNETWORKS_NO_ERROR);
    // NOTE: ANeuralNetworksCompilation_finish not called

    ANeuralNetworksBurst* burst;
    EXPECT_EQ(ANeuralNetworksBurst_create(compilation, &burst), ANEURALNETWORKS_BAD_STATE);
}

TEST_F(ValidationTestBurst, BurstComputeDifferentCompilations) {
    ANeuralNetworksCompilation* secondCompilation;
    ASSERT_EQ(ANeuralNetworksCompilation_create(mModel, &secondCompilation),
              ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksCompilation_finish(secondCompilation), ANEURALNETWORKS_NO_ERROR);

    ANeuralNetworksExecution* execution;
    EXPECT_EQ(ANeuralNetworksExecution_create(secondCompilation, &execution),
              ANEURALNETWORKS_NO_ERROR);

    EXPECT_EQ(ANeuralNetworksExecution_burstCompute(execution, mBurst), ANEURALNETWORKS_BAD_DATA);

    ANeuralNetworksExecution_free(execution);
    ANeuralNetworksCompilation_free(secondCompilation);
}

TEST_F(ValidationTestBurst, BurstComputeConcurrent) {
    ANeuralNetworksExecution* secondExecution;
    EXPECT_EQ(ANeuralNetworksExecution_create(mCompilation, &secondExecution),
              ANEURALNETWORKS_NO_ERROR);

    // set inputs of first execution
    float inputA0[] = {1.0f, 1.0f}, inputA1[] = {2.0f, 2.0f}, outputA0[2];
    int32_t inputA2[] = {0};
    EXPECT_EQ(ANeuralNetworksExecution_setInput(mExecution, 0, nullptr, inputA0, sizeof(inputA0)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setInput(mExecution, 1, nullptr, inputA1, sizeof(inputA1)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setInput(mExecution, 2, nullptr, inputA2, sizeof(inputA2)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(
            ANeuralNetworksExecution_setOutput(mExecution, 0, nullptr, outputA0, sizeof(outputA0)),
            ANEURALNETWORKS_NO_ERROR);

    // set inputs of second execution
    float inputB0[] = {1.0f, 1.0f}, inputB1[] = {2.0f, 2.0f}, outputB0[2];
    int32_t inputB2[] = {0};
    EXPECT_EQ(ANeuralNetworksExecution_setInput(secondExecution, 0, nullptr, inputB0,
                                                sizeof(inputB0)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setInput(secondExecution, 1, nullptr, inputB1,
                                                sizeof(inputB1)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setInput(secondExecution, 2, nullptr, inputB2,
                                                sizeof(inputB2)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setOutput(secondExecution, 0, nullptr, outputB0,
                                                 sizeof(outputB0)),
              ANEURALNETWORKS_NO_ERROR);

    // Execute on the same burst concurrently. At least one result must be
    // ANEURALNETWORKS_NO_ERROR. One may return ANEURALNETWORKS_BAD_STATE if the
    // other is already executing on the burst.
    auto first = std::async(std::launch::async, [this] {
        return ANeuralNetworksExecution_burstCompute(mExecution, mBurst);
    });
    auto second = std::async(std::launch::async, [this, secondExecution] {
        return ANeuralNetworksExecution_burstCompute(secondExecution, mBurst);
    });

    const int result1 = first.get();
    const int result2 = second.get();
    EXPECT_TRUE(result1 == ANEURALNETWORKS_BAD_STATE || result1 == ANEURALNETWORKS_NO_ERROR);
    EXPECT_TRUE(result2 == ANEURALNETWORKS_BAD_STATE || result2 == ANEURALNETWORKS_NO_ERROR);
    EXPECT_TRUE(result1 == ANEURALNETWORKS_NO_ERROR || result2 == ANEURALNETWORKS_NO_ERROR);

    ANeuralNetworksExecution_free(secondExecution);
}

// The burst object maintains a local cache of memory objects. Because the burst
// is intended to live for multiple executions, and because memory might be
// created and freed for each execution, burst includes internal mechanisms to
// purge memory objects from its cache that have been freed by the NNAPI client.
// The following two test cases (FreeMemoryBeforeBurst and
// FreeBurstBeforeMemory) ensure that this internal cleanup is tested in both
// freeing orders.
//
// These two test cases explicitly create a new burst object and a new execution
// object so that the order of freeing can be specified. If these tests instead
// relied on the provided mExecution and mBurst, mBurst would always be freed
// before mExecution.

TEST_F(ValidationTestBurst, FreeMemoryBeforeBurst) {
    ANeuralNetworksBurst* burst;
    EXPECT_EQ(ANeuralNetworksBurst_create(mCompilation, &burst), ANEURALNETWORKS_NO_ERROR);

    // prepare data for execution
    float input0[] = {1.0f, 1.0f}, input1[] = {2.0f, 2.0f}, output0[2];
    int32_t input2[] = {0};

    const size_t memorySize = sizeof(output0);
    int memoryFd = ASharedMemory_create("nnMemory", memorySize);
    ASSERT_GT(memoryFd, 0);

    ANeuralNetworksMemory* memory;
    EXPECT_EQ(ANeuralNetworksMemory_createFromFd(memorySize, PROT_READ | PROT_WRITE, memoryFd, 0,
                                                 &memory),
              ANEURALNETWORKS_NO_ERROR);

    // create and configure execution
    ANeuralNetworksExecution* execution;
    EXPECT_EQ(ANeuralNetworksExecution_create(mCompilation, &execution), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setInput(execution, 0, nullptr, input0, sizeof(input0)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setInput(execution, 1, nullptr, input1, sizeof(input1)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setInput(execution, 2, nullptr, input2, sizeof(input2)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(execution, 0, nullptr, memory, 0,
                                                           sizeof(output0)),
              ANEURALNETWORKS_NO_ERROR);

    // preform execution to cache memory into burst
    EXPECT_EQ(ANeuralNetworksExecution_burstCompute(execution, burst), ANEURALNETWORKS_NO_ERROR);
    ANeuralNetworksExecution_free(execution);

    // free memory before burst
    ANeuralNetworksMemory_free(memory);
    ANeuralNetworksBurst_free(burst);

    // close memory
    close(memoryFd);
}

TEST_F(ValidationTestBurst, FreeBurstBeforeMemory) {
    ANeuralNetworksBurst* burst;
    EXPECT_EQ(ANeuralNetworksBurst_create(mCompilation, &burst), ANEURALNETWORKS_NO_ERROR);

    // prepare data for execution
    float input0[] = {1.0f, 1.0f}, input1[] = {2.0f, 2.0f}, output0[2];
    int32_t input2[] = {0};
    const size_t memorySize = sizeof(output0);
    int memoryFd = ASharedMemory_create("nnMemory", memorySize);
    ASSERT_GT(memoryFd, 0);

    ANeuralNetworksMemory* memory;
    EXPECT_EQ(ANeuralNetworksMemory_createFromFd(memorySize, PROT_READ | PROT_WRITE, memoryFd, 0,
                                                 &memory),
              ANEURALNETWORKS_NO_ERROR);

    // create and configure execution
    ANeuralNetworksExecution* execution;
    EXPECT_EQ(ANeuralNetworksExecution_create(mCompilation, &execution), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setInput(execution, 0, nullptr, input0, sizeof(input0)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setInput(execution, 1, nullptr, input1, sizeof(input1)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setInput(execution, 2, nullptr, input2, sizeof(input2)),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksExecution_setOutputFromMemory(execution, 0, nullptr, memory, 0,
                                                           sizeof(output0)),
              ANEURALNETWORKS_NO_ERROR);

    // preform execution to cache memory into burst
    EXPECT_EQ(ANeuralNetworksExecution_burstCompute(execution, burst), ANEURALNETWORKS_NO_ERROR);
    ANeuralNetworksExecution_free(execution);

    // free burst before memory
    ANeuralNetworksBurst_free(burst);
    ANeuralNetworksMemory_free(memory);

    // close memory
    close(memoryFd);
}

TEST(ValidationTestIntrospection, GetNumDevices) {
    uint32_t numDevices = 0;
    EXPECT_EQ(ANeuralNetworks_getDeviceCount(&numDevices), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworks_getDeviceCount(nullptr), ANEURALNETWORKS_UNEXPECTED_NULL);
}

TEST(ValidationTestIntrospection, GetDevice) {
    uint32_t numDevices = 0;
    EXPECT_EQ(ANeuralNetworks_getDeviceCount(&numDevices), ANEURALNETWORKS_NO_ERROR);

    ANeuralNetworksDevice* device = nullptr;
    for (uint32_t i = 0; i < numDevices; i++) {
        SCOPED_TRACE(i);
        EXPECT_EQ(ANeuralNetworks_getDevice(i, &device), ANEURALNETWORKS_NO_ERROR);
        EXPECT_NE(device, nullptr);
    }
    EXPECT_EQ(ANeuralNetworks_getDevice(0, nullptr), ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworks_getDevice(numDevices, &device), ANEURALNETWORKS_BAD_DATA);
}

static void deviceStringCheck(std::function<int(const ANeuralNetworksDevice*, const char**)> func) {
    uint32_t numDevices = 0;
    EXPECT_EQ(ANeuralNetworks_getDeviceCount(&numDevices), ANEURALNETWORKS_NO_ERROR);

    const char* buffer;
    for (uint32_t i = 0; i < numDevices; i++) {
        SCOPED_TRACE(i);
        ANeuralNetworksDevice* device;
        EXPECT_EQ(ANeuralNetworks_getDevice(i, &device), ANEURALNETWORKS_NO_ERROR);
        EXPECT_EQ(func(device, &buffer), ANEURALNETWORKS_NO_ERROR);
        EXPECT_EQ(func(device, nullptr), ANEURALNETWORKS_UNEXPECTED_NULL);
    }
    EXPECT_EQ(func(nullptr, &buffer), ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(func(nullptr, nullptr), ANEURALNETWORKS_UNEXPECTED_NULL);
}

TEST(ValidationTestIntrospection, DeviceGetName) {
    deviceStringCheck(ANeuralNetworksDevice_getName);
}

TEST(ValidationTestIntrospection, DeviceGetNameUnique) {
    uint32_t numDevices = 0;
    EXPECT_EQ(ANeuralNetworks_getDeviceCount(&numDevices), ANEURALNETWORKS_NO_ERROR);

    std::set<std::string> deviceNames;
    for (uint32_t i = 0; i < numDevices; i++) {
        ANeuralNetworksDevice* device = nullptr;
        EXPECT_EQ(ANeuralNetworks_getDevice(i, &device), ANEURALNETWORKS_NO_ERROR);
        const char* buffer = nullptr;
        EXPECT_EQ(ANeuralNetworksDevice_getName(device, &buffer), ANEURALNETWORKS_NO_ERROR);
        std::string name(buffer);
        EXPECT_EQ(deviceNames.count(name), (uint32_t)0);
        deviceNames.insert(name);
    }
}

TEST(ValidationTestIntrospection, DeviceGetVersion) {
    deviceStringCheck(ANeuralNetworksDevice_getVersion);
}

TEST(ValidationTestIntrospection, DeviceGetFeatureLevel) {
    uint32_t numDevices = 0;
    EXPECT_EQ(ANeuralNetworks_getDeviceCount(&numDevices), ANEURALNETWORKS_NO_ERROR);

    int64_t featureLevel;
    for (uint32_t i = 0; i < numDevices; i++) {
        SCOPED_TRACE(i);
        ANeuralNetworksDevice* device;
        EXPECT_EQ(ANeuralNetworks_getDevice(i, &device), ANEURALNETWORKS_NO_ERROR);
        EXPECT_EQ(ANeuralNetworksDevice_getFeatureLevel(device, &featureLevel),
                  ANEURALNETWORKS_NO_ERROR);
        EXPECT_EQ(ANeuralNetworksDevice_getFeatureLevel(device, nullptr),
                  ANEURALNETWORKS_UNEXPECTED_NULL);
    }
    EXPECT_EQ(ANeuralNetworksDevice_getFeatureLevel(nullptr, &featureLevel),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksDevice_getFeatureLevel(nullptr, nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);
}

TEST(ValidationTestIntrospection, DeviceGetType) {
    uint32_t numDevices = 0;
    EXPECT_EQ(ANeuralNetworks_getDeviceCount(&numDevices), ANEURALNETWORKS_NO_ERROR);

    int32_t validTypes[] = {ANEURALNETWORKS_DEVICE_UNKNOWN, ANEURALNETWORKS_DEVICE_OTHER,
                            ANEURALNETWORKS_DEVICE_CPU, ANEURALNETWORKS_DEVICE_GPU,
                            ANEURALNETWORKS_DEVICE_ACCELERATOR};
    int32_t deviceType;
    for (uint32_t i = 0; i < numDevices; i++) {
        SCOPED_TRACE(i);
        // Initialize the deviceType to be an invalid type.
        deviceType = -1;
        ANeuralNetworksDevice* device;
        EXPECT_EQ(ANeuralNetworks_getDevice(i, &device), ANEURALNETWORKS_NO_ERROR);
        EXPECT_EQ(ANeuralNetworksDevice_getType(device, &deviceType), ANEURALNETWORKS_NO_ERROR);
        EXPECT_TRUE(std::find(std::begin(validTypes), std::end(validTypes), deviceType) !=
                    std::end(validTypes));
        EXPECT_EQ(ANeuralNetworksDevice_getType(device, nullptr), ANEURALNETWORKS_UNEXPECTED_NULL);
    }
    EXPECT_EQ(ANeuralNetworksDevice_getType(nullptr, &deviceType), ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksDevice_getType(nullptr, nullptr), ANEURALNETWORKS_UNEXPECTED_NULL);
}

TEST(ValidationTestIntrospection, DeviceWait) {
    uint32_t numDevices = 0;
    EXPECT_EQ(ANeuralNetworks_getDeviceCount(&numDevices), ANEURALNETWORKS_NO_ERROR);

    for (uint32_t i = 0; i < numDevices; i++) {
        SCOPED_TRACE(i);
        ANeuralNetworksDevice* device;
        EXPECT_EQ(ANeuralNetworks_getDevice(i, &device), ANEURALNETWORKS_NO_ERROR);
        EXPECT_EQ(ANeuralNetworksDevice_wait(device), ANEURALNETWORKS_NO_ERROR);
    }
    EXPECT_EQ(ANeuralNetworksDevice_wait(nullptr), ANEURALNETWORKS_UNEXPECTED_NULL);
}

class ValidationTestCompilationForDevices_1 : public ValidationTestModel {
   protected:
    virtual void SetUp() override {
        ValidationTestModel::SetUp();
        createModel();

        uint32_t numDevices = 0;
        EXPECT_EQ(ANeuralNetworks_getDeviceCount(&numDevices), ANEURALNETWORKS_NO_ERROR);

        if (numDevices > 0) {
            EXPECT_EQ(ANeuralNetworks_getDevice(0, &mDevice), ANEURALNETWORKS_NO_ERROR);
            bool supported = false;
            ASSERT_EQ(mNumOperations, static_cast<uint32_t>(1));
            EXPECT_EQ(ANeuralNetworksModel_getSupportedOperationsForDevices(mModel, &mDevice, 1,
                                                                            &supported),
                      ANEURALNETWORKS_NO_ERROR);
            if (supported) {
                ASSERT_EQ(ANeuralNetworksCompilation_createForDevices(mModel, &mDevice, 1,
                                                                      &mCompilation),
                          ANEURALNETWORKS_NO_ERROR);
            }
        }
    }

    virtual void TearDown() {
        ANeuralNetworksCompilation_free(mCompilation);
        ValidationTestModel::TearDown();
    }

    ANeuralNetworksDevice* mDevice = nullptr;
    ANeuralNetworksCompilation* mCompilation = nullptr;
};

// Also see TEST_F(ValidationTestCompilation, SetPreference)
TEST_F(ValidationTestCompilationForDevices_1, SetPreference) {
    EXPECT_EQ(ANeuralNetworksCompilation_setPreference(nullptr, ANEURALNETWORKS_PREFER_LOW_POWER),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    if (!mCompilation) {
        return;
    }
    EXPECT_EQ(ANeuralNetworksCompilation_setPreference(mCompilation, 40), ANEURALNETWORKS_BAD_DATA);
}

// Also see TEST_F(ValidationTestCompilation, SetCaching)
TEST_F(ValidationTestCompilationForDevices_1, SetCaching) {
    std::vector<uint8_t> token(ANEURALNETWORKS_BYTE_SIZE_OF_CACHE_TOKEN, 0);
    EXPECT_EQ(ANeuralNetworksCompilation_setCaching(nullptr, "/data/local/tmp", token.data()),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    if (!mCompilation) {
        return;
    }
    EXPECT_EQ(ANeuralNetworksCompilation_setCaching(mCompilation, nullptr, token.data()),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksCompilation_setCaching(mCompilation, "/data/local/tmp", nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);
}

// Also see TEST_F(ValidationTestCompilation, CreateExecution)
TEST_F(ValidationTestCompilationForDevices_1, CreateExecution) {
    ANeuralNetworksExecution* execution = nullptr;
    EXPECT_EQ(ANeuralNetworksExecution_create(nullptr, &execution),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    if (!mCompilation) {
        return;
    }
    EXPECT_EQ(ANeuralNetworksExecution_create(mCompilation, nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksExecution_create(mCompilation, &execution), ANEURALNETWORKS_BAD_STATE);
}

// Also see TEST_F(ValidationTestCompilation, Finish)
TEST_F(ValidationTestCompilationForDevices_1, Finish) {
    EXPECT_EQ(ANeuralNetworksCompilation_finish(nullptr), ANEURALNETWORKS_UNEXPECTED_NULL);
    if (!mCompilation) {
        return;
    }
    EXPECT_EQ(ANeuralNetworksCompilation_finish(mCompilation), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksCompilation_setPreference(mCompilation,
                                                       ANEURALNETWORKS_PREFER_FAST_SINGLE_ANSWER),
              ANEURALNETWORKS_BAD_STATE);
    EXPECT_EQ(
            ANeuralNetworksCompilation_setPriority(mCompilation, ANEURALNETWORKS_PRIORITY_DEFAULT),
            ANEURALNETWORKS_BAD_STATE);
    EXPECT_EQ(ANeuralNetworksCompilation_setTimeout(mCompilation, kShortWaitInNanoseconds),
              ANEURALNETWORKS_BAD_STATE);
    std::vector<uint8_t> token(ANEURALNETWORKS_BYTE_SIZE_OF_CACHE_TOKEN, 0);
    EXPECT_EQ(ANeuralNetworksCompilation_setCaching(mCompilation, "/data/local/tmp", token.data()),
              ANEURALNETWORKS_BAD_STATE);
    EXPECT_EQ(ANeuralNetworksCompilation_finish(mCompilation), ANEURALNETWORKS_BAD_STATE);
}

// Also see TEST_F(ValidationTestCompilation, SetTimeout)
// Also see TEST_F(ValidationTestCompilationForDevices_2, SetTimeout)
TEST_F(ValidationTestCompilationForDevices_1, SetTimeout) {
    if (!mCompilation) {
        return;
    }

    EXPECT_EQ(ANeuralNetworksCompilation_setTimeout(mCompilation, kShortWaitInNanoseconds),
              ANEURALNETWORKS_NO_ERROR);

    // Attempt to finish
    const int n = ANeuralNetworksCompilation_finish(mCompilation);
    EXPECT_TRUE(n == ANEURALNETWORKS_NO_ERROR || n == ANEURALNETWORKS_MISSED_DEADLINE_TRANSIENT ||
                n == ANEURALNETWORKS_MISSED_DEADLINE_PERSISTENT);
}

TEST_F(ValidationTestCompilationForDevices_1, SetTimeoutMaximum) {
    if (!mCompilation) {
        return;
    }

    constexpr uint64_t duration = std::numeric_limits<uint64_t>::max();
    EXPECT_EQ(ANeuralNetworksCompilation_setTimeout(mCompilation, duration),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksCompilation_finish(mCompilation), ANEURALNETWORKS_NO_ERROR);
}

class ValidationTestCompilationForDevices_2 : public ValidationTestModel {
   protected:
    virtual void SetUp() override {
        ValidationTestModel::SetUp();
        createModel();

        uint32_t numDevices = 0;
        EXPECT_EQ(ANeuralNetworks_getDeviceCount(&numDevices), ANEURALNETWORKS_NO_ERROR);

        if (numDevices > 1) {
            EXPECT_EQ(ANeuralNetworks_getDevice(0, &mDevices[0]), ANEURALNETWORKS_NO_ERROR);
            EXPECT_EQ(ANeuralNetworks_getDevice(1, &mDevices[1]), ANEURALNETWORKS_NO_ERROR);
            bool supported = false;
            ASSERT_EQ(mNumOperations, static_cast<uint32_t>(1));
            EXPECT_EQ(ANeuralNetworksModel_getSupportedOperationsForDevices(mModel, mDevices, 2,
                                                                            &supported),
                      ANEURALNETWORKS_NO_ERROR);
            if (supported) {
                ASSERT_EQ(ANeuralNetworksCompilation_createForDevices(mModel, mDevices, 2,
                                                                      &mCompilation),
                          ANEURALNETWORKS_NO_ERROR);
            }
        }
    }

    virtual void TearDown() {
        ANeuralNetworksCompilation_free(mCompilation);
        ValidationTestModel::TearDown();
    }

    ANeuralNetworksDevice* mDevices[2] = {nullptr, nullptr};
    ANeuralNetworksCompilation* mCompilation = nullptr;
};

// Also see TEST_F(ValidationTestCompilation, SetTimeout)
// Also see TEST_F(ValidationTestCompilationForDevices_1, SetTimeout)
TEST_F(ValidationTestCompilationForDevices_2, SetTimeout) {
    EXPECT_EQ(ANeuralNetworksCompilation_setTimeout(nullptr, kShortWaitInNanoseconds),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    if (!mCompilation) {
        return;
    }
    // Timeouts can only be set on Compilations created from CompilationForDevices with one device
    // specified.
    EXPECT_EQ(ANeuralNetworksCompilation_setTimeout(mCompilation, kShortWaitInNanoseconds),
              ANEURALNETWORKS_BAD_DATA);
}

// Also see TEST_F(ValidationTestCompilation, ExecutionSetTimeout)
// Also see TEST_F(ValidationTestCompilationForDevices_1, ExecutionSetTimeout)
TEST_F(ValidationTestCompilationForDevices_2, ExecutionSetTimeout) {
    EXPECT_EQ(ANeuralNetworksExecution_setTimeout(nullptr, kShortWaitInNanoseconds),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    if (!mCompilation) {
        return;
    }
    ASSERT_EQ(ANeuralNetworksCompilation_finish(mCompilation), ANEURALNETWORKS_NO_ERROR);
    ANeuralNetworksExecution* execution;
    ASSERT_EQ(ANeuralNetworksExecution_create(mCompilation, &execution), ANEURALNETWORKS_NO_ERROR);
    // Timeouts can only be set on Compilations created from CompilationForDevices with one device
    // specified.
    EXPECT_EQ(ANeuralNetworksExecution_setTimeout(execution, kShortWaitInNanoseconds),
              ANEURALNETWORKS_BAD_DATA);
    ANeuralNetworksExecution_free(execution);
}

// Also see TEST_F(ValidationTestCompilation, ExecutionTiming)
// Also see TEST_F(ValidationTestCompilationForDevices_1, ExecutionTiming)
TEST_F(ValidationTestCompilationForDevices_2, ExecutionTiming) {
    if (!mCompilation) {
        return;
    }
    ASSERT_EQ(ANeuralNetworksCompilation_finish(mCompilation), ANEURALNETWORKS_NO_ERROR);
    ANeuralNetworksExecution* execution;
    ASSERT_EQ(ANeuralNetworksExecution_create(mCompilation, &execution), ANEURALNETWORKS_NO_ERROR);
    // Cannot setMeasureTiming() if there are two or more devices.
    EXPECT_EQ(ANeuralNetworksExecution_setMeasureTiming(execution, false),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksExecution_setMeasureTiming(execution, true), ANEURALNETWORKS_BAD_DATA);
}

class ValidationTestInvalidCompilation : public ValidationTestModel {
   protected:
    virtual void SetUp() override {
        ValidationTestModel::SetUp();

        // Create a model with an OEM operation
        uint32_t dimensions[]{1};
        ANeuralNetworksOperandType OEMTensorType{.type = ANEURALNETWORKS_TENSOR_OEM_BYTE,
                                                 .dimensionCount = 1,
                                                 .dimensions = dimensions};
        EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, &OEMTensorType),
                  ANEURALNETWORKS_NO_ERROR);
        EXPECT_EQ(ANeuralNetworksModel_addOperand(mModel, &OEMTensorType),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(addOperation(ANEURALNETWORKS_OEM_OPERATION, {0}, {1}), ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(identifyInputsAndOutputs({0}, {1}), ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(modelFinish(), ANEURALNETWORKS_NO_ERROR);

        // Find a device that cannot handle OEM operation and create compilation on that
        uint32_t numDevices = 0;
        EXPECT_EQ(ANeuralNetworks_getDeviceCount(&numDevices), ANEURALNETWORKS_NO_ERROR);
        for (uint32_t i = 0; i < numDevices; i++) {
            ANeuralNetworksDevice* device;
            EXPECT_EQ(ANeuralNetworks_getDevice(i, &device), ANEURALNETWORKS_NO_ERROR);
            bool supported = false;
            EXPECT_EQ(ANeuralNetworksModel_getSupportedOperationsForDevices(mModel, &device, 1,
                                                                            &supported),
                      ANEURALNETWORKS_NO_ERROR);
            if (!supported) {
                ASSERT_EQ(ANeuralNetworksCompilation_createForDevices(mModel, &device, 1,
                                                                      &mInvalidCompilation),
                          ANEURALNETWORKS_NO_ERROR);
                break;
            }
        }
        if (mInvalidCompilation) {
            ASSERT_EQ(ANeuralNetworksCompilation_finish(mInvalidCompilation),
                      ANEURALNETWORKS_BAD_DATA);
        }
    }

    virtual void TearDown() {
        ANeuralNetworksCompilation_free(mInvalidCompilation);
        ValidationTestModel::TearDown();
    }

    ANeuralNetworksCompilation* mInvalidCompilation = nullptr;
};

TEST_F(ValidationTestInvalidCompilation, CreateExecution) {
    if (!mInvalidCompilation) {
        return;
    }
    ANeuralNetworksExecution* execution = nullptr;
    EXPECT_EQ(ANeuralNetworksExecution_create(mInvalidCompilation, &execution),
              ANEURALNETWORKS_BAD_STATE);
    ANeuralNetworksExecution_free(execution);
}

TEST_F(ValidationTestInvalidCompilation, MemoryDescAddRole) {
    if (!mInvalidCompilation) {
        return;
    }
    ANeuralNetworksMemoryDesc* desc = nullptr;
    ASSERT_EQ(ANeuralNetworksMemoryDesc_create(&desc), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(desc, mInvalidCompilation, 0, 1.0f),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(desc, mInvalidCompilation, 0, 1.0f),
              ANEURALNETWORKS_BAD_DATA);
    ANeuralNetworksMemoryDesc_free(desc);
}

// Also see TEST_F(ValidationTestCompilation, ExecutionTiming)
// Also see TEST_F(ValidationTestCompilationForDevices_2, ExecutionTiming)
// Also see TEST_F(ValidationTestCompilation, ExecutionUsability)
TEST_F(ValidationTestCompilationForDevices_1, ExecutionTiming) {
    if (!mCompilation) {
        return;
    }
    ASSERT_EQ(ANeuralNetworksCompilation_finish(mCompilation), ANEURALNETWORKS_NO_ERROR);

    enum class ExecutionType : uint32_t { ASYNC, SYNC, BURST, FENCED };
    for (auto executionType :
         {ExecutionType::ASYNC, ExecutionType::SYNC, ExecutionType::BURST, ExecutionType::FENCED}) {
        SCOPED_TRACE(static_cast<uint32_t>(executionType));

        ANeuralNetworksExecution* execution;
        ASSERT_EQ(ANeuralNetworksExecution_create(mCompilation, &execution),
                  ANEURALNETWORKS_NO_ERROR);

        EXPECT_EQ(ANeuralNetworksExecution_setMeasureTiming(nullptr, false),
                  ANEURALNETWORKS_UNEXPECTED_NULL);
        EXPECT_EQ(ANeuralNetworksExecution_setMeasureTiming(nullptr, true),
                  ANEURALNETWORKS_UNEXPECTED_NULL);
        EXPECT_EQ(ANeuralNetworksExecution_setMeasureTiming(execution, false),
                  ANEURALNETWORKS_NO_ERROR);
        EXPECT_EQ(ANeuralNetworksExecution_setMeasureTiming(execution, true),
                  ANEURALNETWORKS_NO_ERROR);

        float in0[] = {0.0f, 0.0f}, in1[] = {1.0f, 1.0f}, out0[2];
        int in2 = 0;
        ASSERT_EQ(ANeuralNetworksExecution_setInput(execution, 0, nullptr, &in0, sizeof(in0)),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksExecution_setInput(execution, 1, nullptr, &in1, sizeof(in1)),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksExecution_setInput(execution, 2, nullptr, &in2, sizeof(in2)),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksExecution_setOutput(execution, 0, nullptr, &out0, sizeof(out0)),
                  ANEURALNETWORKS_NO_ERROR);

        // Cannot getDuration until the execution has finished.
        uint64_t duration;
        EXPECT_EQ(ANeuralNetworksExecution_getDuration(
                          execution, ANEURALNETWORKS_DURATION_ON_HARDWARE, &duration),
                  ANEURALNETWORKS_BAD_STATE);
        EXPECT_EQ(ANeuralNetworksExecution_getDuration(
                          execution, ANEURALNETWORKS_DURATION_IN_DRIVER, &duration),
                  ANEURALNETWORKS_BAD_STATE);

        auto testSetTimeoutTooLate = [execution] {
            // Cannot setTimeout if the execution has started.
            EXPECT_EQ(ANeuralNetworksExecution_setTimeout(execution, kShortWaitInNanoseconds),
                      ANEURALNETWORKS_BAD_STATE);
        };

        auto testMeasureTooLate = [execution] {
            // Cannot setMeasureTiming if the execution has started.
            EXPECT_EQ(ANeuralNetworksExecution_setMeasureTiming(execution, false),
                      ANEURALNETWORKS_BAD_STATE);
            EXPECT_EQ(ANeuralNetworksExecution_setMeasureTiming(execution, true),
                      ANEURALNETWORKS_BAD_STATE);
        };

        // Compute.
        switch (executionType) {
            case ExecutionType::ASYNC: {
                ANeuralNetworksEvent* event;
                ASSERT_EQ(ANeuralNetworksExecution_startCompute(execution, &event),
                          ANEURALNETWORKS_NO_ERROR);
                testMeasureTooLate();
                ASSERT_EQ(ANeuralNetworksEvent_wait(event), ANEURALNETWORKS_NO_ERROR);
                testSetTimeoutTooLate();
                testMeasureTooLate();
                ANeuralNetworksEvent_free(event);
                break;
            }
            case ExecutionType::SYNC: {
                ASSERT_EQ(ANeuralNetworksExecution_compute(execution), ANEURALNETWORKS_NO_ERROR);
                testSetTimeoutTooLate();
                testMeasureTooLate();
                break;
            }
            case ExecutionType::BURST: {
                ANeuralNetworksBurst* burst;
                ASSERT_EQ(ANeuralNetworksBurst_create(mCompilation, &burst),
                          ANEURALNETWORKS_NO_ERROR);
                ASSERT_EQ(ANeuralNetworksExecution_burstCompute(execution, burst),
                          ANEURALNETWORKS_NO_ERROR);
                testSetTimeoutTooLate();
                testMeasureTooLate();
                ANeuralNetworksBurst_free(burst);
                break;
            }
            case ExecutionType::FENCED: {
                ANeuralNetworksEvent* event = nullptr;
                ASSERT_EQ(ANeuralNetworksExecution_startComputeWithDependencies(execution, nullptr,
                                                                                0, 0, &event),
                          ANEURALNETWORKS_NO_ERROR);
                testMeasureTooLate();
                ASSERT_EQ(ANeuralNetworksEvent_wait(event), ANEURALNETWORKS_NO_ERROR);
                testSetTimeoutTooLate();
                testMeasureTooLate();
                ANeuralNetworksEvent_free(event);
                break;
            }
            default:
                FAIL() << "Unreachable";
        }

        auto testDuration = [](ANeuralNetworksExecution* e, int32_t durationCode,
                               bool nullDuration) {
            SCOPED_TRACE(e);
            SCOPED_TRACE(durationCode);
            SCOPED_TRACE(nullDuration);

            // Strictly speaking, a duration COULD have this value, but it is
            // exceedingly unlikely. We'll use it as an initial value that we expect
            // to be modified by getDuration().
            const uint64_t kBogusDuration = UINT64_MAX - 1;

            uint64_t duration = kBogusDuration;
            uint64_t* durationPtr = nullDuration ? nullptr : &duration;

            int expectedResultCode = ANEURALNETWORKS_NO_ERROR;
            if (e == nullptr | durationPtr == nullptr) {
                expectedResultCode = ANEURALNETWORKS_UNEXPECTED_NULL;
            } else if (durationCode < 0 ||
                       durationCode > ANEURALNETWORKS_FENCED_DURATION_IN_DRIVER) {
                expectedResultCode = ANEURALNETWORKS_BAD_DATA;
            }

            EXPECT_EQ(ANeuralNetworksExecution_getDuration(e, durationCode, durationPtr),
                      expectedResultCode);
            if (expectedResultCode == ANEURALNETWORKS_NO_ERROR) {
                EXPECT_NE(duration, kBogusDuration);
            }
        };

        std::vector<ANeuralNetworksExecution*> executions = {nullptr, execution};
        std::vector<int32_t> durationCodes = {-1,
                                              ANEURALNETWORKS_DURATION_ON_HARDWARE,
                                              ANEURALNETWORKS_DURATION_IN_DRIVER,
                                              ANEURALNETWORKS_FENCED_DURATION_ON_HARDWARE,
                                              ANEURALNETWORKS_FENCED_DURATION_IN_DRIVER,
                                              ANEURALNETWORKS_FENCED_DURATION_IN_DRIVER + 1};
        std::vector<bool> nullDurations = {false, true};
        for (auto e : executions) {
            for (auto d : durationCodes) {
                for (auto n : nullDurations) {
                    testDuration(e, d, n);
                }
            }
        }
    }
}

enum class TimeoutDurationType { SHORT, MAXIMUM };
uint64_t createTimeoutDuration(TimeoutDurationType type) {
    switch (type) {
        case TimeoutDurationType::SHORT:
            return kShortWaitInNanoseconds;
        case TimeoutDurationType::MAXIMUM:
            return std::numeric_limits<uint64_t>::max();
    }
    LOG(FATAL) << "Invalid TimeoutDurationType: " << static_cast<int>(type);
    return 0;
}

void runExecutionSetTimeoutTest(ANeuralNetworksCompilation* compilation,
                                TimeoutDurationType timeoutDurationType) {
    if (!compilation) {
        return;
    }
    ASSERT_EQ(ANeuralNetworksCompilation_finish(compilation), ANEURALNETWORKS_NO_ERROR);

    enum class ExecutionType : uint32_t { ASYNC, SYNC, BURST, FENCED };
    for (auto executionType :
         {ExecutionType::ASYNC, ExecutionType::SYNC, ExecutionType::BURST, ExecutionType::FENCED}) {
        SCOPED_TRACE(static_cast<uint32_t>(executionType));

        ANeuralNetworksExecution* execution;
        ASSERT_EQ(ANeuralNetworksExecution_create(compilation, &execution),
                  ANEURALNETWORKS_NO_ERROR);
        const auto scoped = android::base::make_scope_guard(
                [execution] { ANeuralNetworksExecution_free(execution); });

        float in0[] = {0.0f, 0.0f}, in1[] = {1.0f, 1.0f}, out0[2];
        int in2 = 0;
        ASSERT_EQ(ANeuralNetworksExecution_setInput(execution, 0, nullptr, &in0, sizeof(in0)),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksExecution_setInput(execution, 1, nullptr, &in1, sizeof(in1)),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksExecution_setInput(execution, 2, nullptr, &in2, sizeof(in2)),
                  ANEURALNETWORKS_NO_ERROR);
        ASSERT_EQ(ANeuralNetworksExecution_setOutput(execution, 0, nullptr, &out0, sizeof(out0)),
                  ANEURALNETWORKS_NO_ERROR);

        const uint64_t timeoutDuration = createTimeoutDuration(timeoutDurationType);
        EXPECT_EQ(ANeuralNetworksExecution_setTimeout(execution, timeoutDuration),
                  ANEURALNETWORKS_NO_ERROR);

        const auto checkResult = [timeoutDurationType](int n) {
            switch (timeoutDurationType) {
                case TimeoutDurationType::SHORT:
                    EXPECT_TRUE(n == ANEURALNETWORKS_NO_ERROR ||
                                n == ANEURALNETWORKS_MISSED_DEADLINE_TRANSIENT ||
                                n == ANEURALNETWORKS_MISSED_DEADLINE_PERSISTENT);
                    return;
                case TimeoutDurationType::MAXIMUM:
                    EXPECT_EQ(n, ANEURALNETWORKS_NO_ERROR);
                    return;
            }
            LOG(FATAL) << "Invalid TimeoutDurationType: " << static_cast<int>(timeoutDurationType);
        };

        // Compute.
        switch (executionType) {
            case ExecutionType::ASYNC: {
                ANeuralNetworksEvent* event = nullptr;
                EXPECT_EQ(ANeuralNetworksExecution_startCompute(execution, &event),
                          ANEURALNETWORKS_NO_ERROR);
                checkResult(ANeuralNetworksEvent_wait(event));
                ANeuralNetworksEvent_free(event);
                break;
            }
            case ExecutionType::SYNC: {
                checkResult(ANeuralNetworksExecution_compute(execution));
                break;
            }
            case ExecutionType::BURST: {
                ANeuralNetworksBurst* burst;
                ASSERT_EQ(ANeuralNetworksBurst_create(compilation, &burst),
                          ANEURALNETWORKS_NO_ERROR);
                checkResult(ANeuralNetworksExecution_burstCompute(execution, burst));
                ANeuralNetworksBurst_free(burst);
                break;
            }
            case ExecutionType::FENCED: {
                ANeuralNetworksEvent* event = nullptr;
                EXPECT_EQ(ANeuralNetworksExecution_startComputeWithDependencies(execution, nullptr,
                                                                                0, 0, &event),
                          ANEURALNETWORKS_NO_ERROR);
                checkResult(ANeuralNetworksEvent_wait(event));
                ANeuralNetworksEvent_free(event);
                break;
            }
            default:
                FAIL() << "Unreachable";
        }
    }
}

// Also see TEST_F(ValidationTestCompilation, ExecutionSetTimeout)
// Also see TEST_F(ValidationTestCompilationForDevices_2, ExecutionSetTimeout)
TEST_F(ValidationTestCompilationForDevices_1, ExecutionSetTimeout) {
    runExecutionSetTimeoutTest(mCompilation, TimeoutDurationType::SHORT);
}

TEST_F(ValidationTestCompilationForDevices_1, ExecutionSetTimeoutMaximum) {
    runExecutionSetTimeoutTest(mCompilation, TimeoutDurationType::MAXIMUM);
}

TEST_F(ValidationTest, CreateMemoryDesc) {
    EXPECT_EQ(ANeuralNetworksMemoryDesc_create(nullptr), ANEURALNETWORKS_UNEXPECTED_NULL);
}

TEST_F(ValidationTestMemoryDesc, AddInputRole) {
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(nullptr, mCompilation, 0, 1.0f),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(mDesc, nullptr, 0, 1.0f),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    // Unfinished compilation.
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(mDesc, mCompilation, 0, 1.0f),
              ANEURALNETWORKS_BAD_DATA);

    ASSERT_EQ(ANeuralNetworksCompilation_finish(mCompilation), ANEURALNETWORKS_NO_ERROR);

    // Index out of range.
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(mDesc, mCompilation, 999, 1.0f),
              ANEURALNETWORKS_BAD_DATA);

    // Invalid frequency.
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(mDesc, mCompilation, 0, 10.0f),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(mDesc, mCompilation, 0, 0.0f),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(mDesc, mCompilation, 0, -1.0f),
              ANEURALNETWORKS_BAD_DATA);

    // Specify the same operand twice.
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(mDesc, mCompilation, 0, 1.0f),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(mDesc, mCompilation, 0, 1.0f),
              ANEURALNETWORKS_BAD_DATA);

    // Attempting to modify a finished descriptor.
    EXPECT_EQ(ANeuralNetworksMemoryDesc_finish(mDesc), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(mDesc, mCompilation, 0, 1.0f),
              ANEURALNETWORKS_BAD_STATE);
}

TEST_F(ValidationTestMemoryDesc, AddOutputRole) {
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(nullptr, mCompilation, 0, 1.0f),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(mDesc, nullptr, 0, 1.0f),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    // Unfinished compilation.
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(mDesc, mCompilation, 0, 1.0f),
              ANEURALNETWORKS_BAD_DATA);

    ASSERT_EQ(ANeuralNetworksCompilation_finish(mCompilation), ANEURALNETWORKS_NO_ERROR);

    // Index out of range.
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(mDesc, mCompilation, 999, 1.0f),
              ANEURALNETWORKS_BAD_DATA);

    // Invalid frequency.
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(mDesc, mCompilation, 0, 10.0f),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(mDesc, mCompilation, 0, 0.0f),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(mDesc, mCompilation, 0, -1.0f),
              ANEURALNETWORKS_BAD_DATA);

    // Specify the same operand twice.
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(mDesc, mCompilation, 0, 1.0f),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(mDesc, mCompilation, 0, 1.0f),
              ANEURALNETWORKS_BAD_DATA);

    // Attempting to modify a finished descriptor.
    EXPECT_EQ(ANeuralNetworksMemoryDesc_finish(mDesc), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(mDesc, mCompilation, 0, 1.0f),
              ANEURALNETWORKS_BAD_STATE);
}

// Creates and compiles a single-operation ADD model with the given operand type.
// The caller is responsible to free the returned model and compilation.
static std::pair<ANeuralNetworksModel*, ANeuralNetworksCompilation*>
createAndCompileAddModelWithType(const ANeuralNetworksOperandType& type) {
    // OperandType for activation scalar.
    const ANeuralNetworksOperandType actType = {
            .type = ANEURALNETWORKS_INT32, .dimensionCount = 0, .dimensions = nullptr};

    ANeuralNetworksModel* model;
    EXPECT_EQ(ANeuralNetworksModel_create(&model), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, &type), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, &type), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, &actType), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, &type), ANEURALNETWORKS_NO_ERROR);

    const uint32_t inList[] = {0, 1, 2};
    const uint32_t outList[] = {3};
    EXPECT_EQ(ANeuralNetworksModel_addOperation(model, ANEURALNETWORKS_ADD, 3, inList, 1, outList),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_identifyInputsAndOutputs(model, 3, inList, 1, outList),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_finish(model), ANEURALNETWORKS_NO_ERROR);

    ANeuralNetworksCompilation* compilation;
    EXPECT_EQ(ANeuralNetworksCompilation_create(model, &compilation), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksCompilation_finish(compilation), ANEURALNETWORKS_NO_ERROR);
    return {model, compilation};
}

static void testIncompatibleOperands(const ANeuralNetworksCompilation* compilation,
                                     const ANeuralNetworksOperandType& badType) {
    const auto [badModel, badCompilation] = createAndCompileAddModelWithType(badType);
    {
        ANeuralNetworksMemoryDesc* desc = nullptr;
        EXPECT_EQ(ANeuralNetworksMemoryDesc_create(&desc), ANEURALNETWORKS_NO_ERROR);
        EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(desc, compilation, 0, 1.0f),
                  ANEURALNETWORKS_NO_ERROR);
        EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(desc, badCompilation, 0, 1.0f),
                  ANEURALNETWORKS_BAD_DATA);
        EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(desc, badCompilation, 0, 1.0f),
                  ANEURALNETWORKS_BAD_DATA);
        ANeuralNetworksMemoryDesc_free(desc);
    }
    {
        ANeuralNetworksMemoryDesc* desc = nullptr;
        EXPECT_EQ(ANeuralNetworksMemoryDesc_create(&desc), ANEURALNETWORKS_NO_ERROR);
        EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(desc, compilation, 0, 1.0f),
                  ANEURALNETWORKS_NO_ERROR);
        EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(desc, badCompilation, 0, 1.0f),
                  ANEURALNETWORKS_BAD_DATA);
        EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(desc, badCompilation, 0, 1.0f),
                  ANEURALNETWORKS_BAD_DATA);
        ANeuralNetworksMemoryDesc_free(desc);
    }
    ANeuralNetworksCompilation_free(badCompilation);
    ANeuralNetworksModel_free(badModel);
}

TEST_F(ValidationTestMemoryDesc, OperandMetadata) {
    const uint32_t dimensions[] = {2};
    const uint32_t rank = std::size(dimensions);
    const ANeuralNetworksOperandType floatBase = {.type = ANEURALNETWORKS_TENSOR_FLOAT32,
                                                  .dimensionCount = rank,
                                                  .dimensions = dimensions,
                                                  .scale = 0.0f,
                                                  .zeroPoint = 0};
    const ANeuralNetworksOperandType quantBase = {.type = ANEURALNETWORKS_TENSOR_QUANT8_ASYMM,
                                                  .dimensionCount = rank,
                                                  .dimensions = dimensions,
                                                  .scale = 1.0f,
                                                  .zeroPoint = 0};
    const auto [floatModel, floatCompilation] = createAndCompileAddModelWithType(floatBase);
    const auto [quantModel, quantCompilation] = createAndCompileAddModelWithType(quantBase);

    // Different data type.
    {
        SCOPED_TRACE("Data type");
        ANeuralNetworksOperandType wrongType = floatBase;
        wrongType.type = ANEURALNETWORKS_TENSOR_FLOAT16;
        testIncompatibleOperands(floatCompilation, wrongType);
    }

    // Different scale.
    {
        SCOPED_TRACE("Scale");
        ANeuralNetworksOperandType wrongScale = quantBase;
        wrongScale.scale = 0.5f;
        testIncompatibleOperands(quantCompilation, wrongScale);
    }

    // Different zero point.
    {
        SCOPED_TRACE("Zero point");
        ANeuralNetworksOperandType wrongZeroPoint = quantBase;
        wrongZeroPoint.zeroPoint = 128;
        testIncompatibleOperands(quantCompilation, wrongZeroPoint);
    }

    // Different rank.
    {
        SCOPED_TRACE("Rank");
        const uint32_t badDimensions[] = {2, 1};
        const uint32_t badRank = std::size(badDimensions);
        ANeuralNetworksOperandType wrongRank = quantBase;
        wrongRank.dimensionCount = badRank;
        wrongRank.dimensions = badDimensions;
        testIncompatibleOperands(quantCompilation, wrongRank);
    }

    // Different dimensions.
    {
        SCOPED_TRACE("Dimensions");
        const uint32_t badDimensions[] = {1};
        ANeuralNetworksOperandType wrongDims = quantBase;
        wrongDims.dimensions = badDimensions;
        testIncompatibleOperands(quantCompilation, wrongDims);
    }

    ANeuralNetworksCompilation_free(floatCompilation);
    ANeuralNetworksCompilation_free(quantCompilation);
    ANeuralNetworksModel_free(floatModel);
    ANeuralNetworksModel_free(quantModel);
}

// Creates and compiles a single-operation CONV_2D model with channel quant data type of the given
// scales. The caller is responsible to free the returned model and compilation.
static std::pair<ANeuralNetworksModel*, ANeuralNetworksCompilation*>
createAndCompileChannelQuantConvModel(const std::vector<float>& scales) {
    const uint32_t numChannels = scales.size();

    // OperandType for input and output.
    const uint32_t inoutDimensions[] = {1, 16, 16, numChannels};
    const ANeuralNetworksOperandType inoutType = {
            .type = ANEURALNETWORKS_TENSOR_QUANT8_ASYMM,
            .dimensionCount = std::size(inoutDimensions),
            .dimensions = inoutDimensions,
            .scale = 1.0f,
            .zeroPoint = 0,
    };

    // OperandType for filter.
    const uint32_t filterDimensions[] = {numChannels, 3, 3, numChannels};
    const ANeuralNetworksOperandType filterType = {
            .type = ANEURALNETWORKS_TENSOR_QUANT8_SYMM_PER_CHANNEL,
            .dimensionCount = std::size(filterDimensions),
            .dimensions = filterDimensions,
            .scale = 0.0f,
            .zeroPoint = 0,
    };

    // OperandType for bias.
    const uint32_t biasDimensions[] = {numChannels};
    const ANeuralNetworksOperandType biasType = {
            .type = ANEURALNETWORKS_TENSOR_INT32,
            .dimensionCount = std::size(biasDimensions),
            .dimensions = biasDimensions,
            .scale = 0.0f,
            .zeroPoint = 0,
    };

    // OperandType for scalars: implicit padding code, strides, activation.
    const ANeuralNetworksOperandType scalarType = {
            .type = ANEURALNETWORKS_INT32, .dimensionCount = 0, .dimensions = nullptr};

    ANeuralNetworksModel* model;
    EXPECT_EQ(ANeuralNetworksModel_create(&model), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, &inoutType), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, &filterType), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, &biasType), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, &scalarType), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, &scalarType), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, &scalarType), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, &scalarType), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_addOperand(model, &inoutType), ANEURALNETWORKS_NO_ERROR);

    // Set channel quant parameters for the filter tensor.
    const ANeuralNetworksSymmPerChannelQuantParams channelQuant = {
            .channelDim = 0,
            .scaleCount = numChannels,
            .scales = scales.data(),
    };
    EXPECT_EQ(ANeuralNetworksModel_setOperandSymmPerChannelQuantParams(model, 1, &channelQuant),
              ANEURALNETWORKS_NO_ERROR);

    const uint32_t inList[] = {0, 1, 2, 3, 4, 5, 6};
    const uint32_t outList[] = {7};
    EXPECT_EQ(ANeuralNetworksModel_addOperation(model, ANEURALNETWORKS_CONV_2D, std::size(inList),
                                                inList, std::size(outList), outList),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_identifyInputsAndOutputs(model, std::size(inList), inList,
                                                            std::size(outList), outList),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksModel_finish(model), ANEURALNETWORKS_NO_ERROR);

    ANeuralNetworksCompilation* compilation;
    EXPECT_EQ(ANeuralNetworksCompilation_create(model, &compilation), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksCompilation_finish(compilation), ANEURALNETWORKS_NO_ERROR);
    return {model, compilation};
}

TEST_F(ValidationTestMemoryDesc, ExtraParams) {
    // Create two compilations with conflict channel quant scales.
    const auto [model1, compilation1] = createAndCompileChannelQuantConvModel({1.0f, 1.0f});
    const auto [model2, compilation2] = createAndCompileChannelQuantConvModel({0.5f, 0.5f});

    ANeuralNetworksMemoryDesc* desc = nullptr;
    EXPECT_EQ(ANeuralNetworksMemoryDesc_create(&desc), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(desc, compilation1, 1, 1.0f),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(desc, compilation2, 1, 1.0f),
              ANEURALNETWORKS_BAD_DATA);
    ANeuralNetworksMemoryDesc_free(desc);

    ANeuralNetworksCompilation_free(compilation1);
    ANeuralNetworksCompilation_free(compilation2);
    ANeuralNetworksModel_free(model1);
    ANeuralNetworksModel_free(model2);
}

TEST_F(ValidationTestMemoryDesc, SetDimensions) {
    const uint32_t dimensions[] = {2};
    const uint32_t badDimensions[] = {3};
    const uint32_t rank = std::size(dimensions);
    const uint32_t badRankDimensions[] = {2, 1};
    const uint32_t badRank = std::size(badRankDimensions);

    EXPECT_EQ(ANeuralNetworksMemoryDesc_setDimensions(nullptr, rank, dimensions),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_setDimensions(mDesc, rank, nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    // Incompatible dimensions.
    EXPECT_EQ(ANeuralNetworksMemoryDesc_setDimensions(mDesc, rank, dimensions),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_setDimensions(mDesc, rank, badDimensions),
              ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_setDimensions(mDesc, badRank, badRankDimensions),
              ANEURALNETWORKS_BAD_DATA);

    // Attempting to modify a finished descriptor.
    EXPECT_EQ(ANeuralNetworksCompilation_finish(mCompilation), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(mDesc, mCompilation, 0, 1.0f),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_finish(mDesc), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_setDimensions(mDesc, rank, dimensions),
              ANEURALNETWORKS_BAD_STATE);
}

TEST_F(ValidationTestMemoryDesc, SetScalarDimensionsBeforeAddRole) {
    const uint32_t badDimensions[] = {2};
    const uint32_t badRank = std::size(badDimensions);

    // Set non-zero rank.
    EXPECT_EQ(ANeuralNetworksMemoryDesc_setDimensions(mDesc, badRank, badDimensions),
              ANEURALNETWORKS_NO_ERROR);

    // This should fail because input2 is a scalar.
    EXPECT_EQ(ANeuralNetworksCompilation_finish(mCompilation), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(mDesc, mCompilation, 2, 1.0f),
              ANEURALNETWORKS_BAD_DATA);
}

TEST_F(ValidationTestMemoryDesc, SetScalarDimensionsAfterAddRole) {
    const uint32_t badDimensions[] = {2};
    const uint32_t badRank = std::size(badDimensions);

    // Input2 is a scalar.
    EXPECT_EQ(ANeuralNetworksCompilation_finish(mCompilation), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(mDesc, mCompilation, 2, 1.0f),
              ANEURALNETWORKS_NO_ERROR);

    // This should fail because the rank is not zero.
    EXPECT_EQ(ANeuralNetworksMemoryDesc_setDimensions(mDesc, 0, nullptr), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_setDimensions(mDesc, badRank, badDimensions),
              ANEURALNETWORKS_BAD_DATA);
}

TEST_F(ValidationTestMemoryDesc, Finish) {
    EXPECT_EQ(ANeuralNetworksMemoryDesc_finish(nullptr), ANEURALNETWORKS_UNEXPECTED_NULL);

    // No usage is specified.
    EXPECT_EQ(ANeuralNetworksMemoryDesc_finish(mDesc), ANEURALNETWORKS_BAD_DATA);

    // Finish an already finished descriptor.
    EXPECT_EQ(ANeuralNetworksCompilation_finish(mCompilation), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(mDesc, mCompilation, 0, 1.0f),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_finish(mDesc), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_finish(mDesc), ANEURALNETWORKS_BAD_STATE);
}

TEST_F(ValidationTestMemoryDesc, CreateMemory) {
    ANeuralNetworksMemory* memory = nullptr;
    EXPECT_EQ(ANeuralNetworksMemory_createFromDesc(nullptr, &memory),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksMemory_createFromDesc(mDesc, nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    // Unfinished descriptor.
    EXPECT_EQ(ANeuralNetworksMemory_createFromDesc(mDesc, &memory), ANEURALNETWORKS_BAD_STATE);

    ANeuralNetworksMemory_free(memory);
}

TEST_F(ValidationTestMemoryDesc, MemoryCopying) {
    uint32_t goodSize = sizeof(float) * 2, badSize1 = sizeof(float), badSize2 = sizeof(float) * 4;
    ANeuralNetworksMemory* goodAshmem = createAshmem(goodSize);
    ANeuralNetworksMemory* badAshmem1 = createAshmem(badSize1);
    ANeuralNetworksMemory* badAshmem2 = createAshmem(badSize2);

    const uint32_t goodDimensions[] = {1, 2};
    const uint32_t badDimensions1[] = {2};
    const uint32_t badDimensions2[] = {2, 1};
    const ANeuralNetworksOperandType goodType = {.type = ANEURALNETWORKS_TENSOR_FLOAT32,
                                                 .dimensionCount = std::size(goodDimensions),
                                                 .dimensions = goodDimensions,
                                                 .scale = 0.0f,
                                                 .zeroPoint = 0};
    const ANeuralNetworksOperandType badType1 = {.type = ANEURALNETWORKS_TENSOR_FLOAT32,
                                                 .dimensionCount = std::size(badDimensions1),
                                                 .dimensions = badDimensions1,
                                                 .scale = 0.0f,
                                                 .zeroPoint = 0};
    const ANeuralNetworksOperandType badType2 = {.type = ANEURALNETWORKS_TENSOR_FLOAT32,
                                                 .dimensionCount = std::size(badDimensions2),
                                                 .dimensions = badDimensions2,
                                                 .scale = 0.0f,
                                                 .zeroPoint = 0};
    const auto [goodModel, goodCompilation] = createAndCompileAddModelWithType(goodType);
    const auto [badModel1, badCompilation1] = createAndCompileAddModelWithType(badType1);
    const auto [badModel2, badCompilation2] = createAndCompileAddModelWithType(badType2);

    ANeuralNetworksMemoryDesc* desc = nullptr;
    ANeuralNetworksMemory *goodDeviceMemory1 = nullptr, *goodDeviceMemory2 = nullptr;
    EXPECT_EQ(ANeuralNetworksMemoryDesc_create(&desc), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(desc, goodCompilation, 0, 1.0f),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_finish(desc), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemory_createFromDesc(desc, &goodDeviceMemory1),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemory_createFromDesc(desc, &goodDeviceMemory2),
              ANEURALNETWORKS_NO_ERROR);
    ANeuralNetworksMemoryDesc_free(desc);

    ANeuralNetworksMemory* badDeviceMemory1 = nullptr;
    EXPECT_EQ(ANeuralNetworksMemoryDesc_create(&desc), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(desc, badCompilation1, 0, 1.0f),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_finish(desc), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemory_createFromDesc(desc, &badDeviceMemory1),
              ANEURALNETWORKS_NO_ERROR);
    ANeuralNetworksMemoryDesc_free(desc);

    ANeuralNetworksMemory* badDeviceMemory2 = nullptr;
    EXPECT_EQ(ANeuralNetworksMemoryDesc_create(&desc), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(desc, badCompilation2, 0, 1.0f),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_finish(desc), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemory_createFromDesc(desc, &badDeviceMemory2),
              ANEURALNETWORKS_NO_ERROR);
    ANeuralNetworksMemoryDesc_free(desc);

    EXPECT_EQ(ANeuralNetworksMemory_copy(nullptr, goodDeviceMemory1),
              ANEURALNETWORKS_UNEXPECTED_NULL);
    EXPECT_EQ(ANeuralNetworksMemory_copy(goodDeviceMemory1, nullptr),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    // Ashmem -> Ashmem
    // Bad memory size.
    EXPECT_EQ(ANeuralNetworksMemory_copy(goodAshmem, badAshmem1), ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksMemory_copy(goodAshmem, badAshmem2), ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksMemory_copy(badAshmem1, goodAshmem), ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksMemory_copy(badAshmem2, goodAshmem), ANEURALNETWORKS_BAD_DATA);

    // Ashmem -> Device Memory
    // Bad memory size.
    EXPECT_EQ(ANeuralNetworksMemory_copy(badAshmem1, goodDeviceMemory1), ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksMemory_copy(badAshmem2, goodDeviceMemory1), ANEURALNETWORKS_BAD_DATA);

    // Device Memory -> Ashmem
    // Uninitialized source device memory.
    EXPECT_EQ(ANeuralNetworksMemory_copy(goodDeviceMemory1, goodAshmem), ANEURALNETWORKS_BAD_DATA);
    // Bad memory size.
    EXPECT_EQ(ANeuralNetworksMemory_copy(goodAshmem, goodDeviceMemory1), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemory_copy(goodDeviceMemory1, badAshmem1), ANEURALNETWORKS_BAD_DATA);
    // Uninitialized source device memory (after a failed copy).
    EXPECT_EQ(ANeuralNetworksMemory_copy(badAshmem1, goodDeviceMemory1), ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksMemory_copy(goodDeviceMemory1, goodAshmem), ANEURALNETWORKS_BAD_DATA);
    // Bad memory size.
    EXPECT_EQ(ANeuralNetworksMemory_copy(goodAshmem, goodDeviceMemory1), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemory_copy(goodDeviceMemory1, badAshmem2), ANEURALNETWORKS_BAD_DATA);

    // Device Memory -> Device Memory
    // Uninitialized source device memory.
    EXPECT_EQ(ANeuralNetworksMemory_copy(goodDeviceMemory2, goodDeviceMemory1),
              ANEURALNETWORKS_BAD_DATA);
    // Incompatible rank.
    EXPECT_EQ(ANeuralNetworksMemory_copy(goodAshmem, badDeviceMemory1), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemory_copy(badDeviceMemory1, goodDeviceMemory1),
              ANEURALNETWORKS_BAD_DATA);
    // Incompatible dimensions.
    EXPECT_EQ(ANeuralNetworksMemory_copy(goodAshmem, badDeviceMemory2), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemory_copy(badDeviceMemory2, goodDeviceMemory1),
              ANEURALNETWORKS_BAD_DATA);
    // Deinitialized source device memory.
    EXPECT_EQ(ANeuralNetworksMemory_copy(goodAshmem, goodDeviceMemory2), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemory_copy(badAshmem1, goodDeviceMemory2), ANEURALNETWORKS_BAD_DATA);
    EXPECT_EQ(ANeuralNetworksMemory_copy(goodDeviceMemory2, goodDeviceMemory1),
              ANEURALNETWORKS_BAD_DATA);

    ANeuralNetworksMemory_free(goodDeviceMemory1);
    ANeuralNetworksMemory_free(goodDeviceMemory2);
    ANeuralNetworksMemory_free(badDeviceMemory1);
    ANeuralNetworksMemory_free(badDeviceMemory2);
    ANeuralNetworksCompilation_free(goodCompilation);
    ANeuralNetworksCompilation_free(badCompilation1);
    ANeuralNetworksCompilation_free(badCompilation2);
    ANeuralNetworksModel_free(goodModel);
    ANeuralNetworksModel_free(badModel1);
    ANeuralNetworksModel_free(badModel2);
}

#ifndef NNTEST_ONLY_PUBLIC_API
TEST(ValidationTestDevice, GetExtensionSupport) {
    bool result;
    EXPECT_EQ(ANeuralNetworksDevice_getExtensionSupport(nullptr, kTestExtensionName, &result),
              ANEURALNETWORKS_UNEXPECTED_NULL);

    uint32_t numDevices = 0;
    EXPECT_EQ(ANeuralNetworks_getDeviceCount(&numDevices), ANEURALNETWORKS_NO_ERROR);

    for (uint32_t i = 0; i < numDevices; i++) {
        SCOPED_TRACE(i);
        ANeuralNetworksDevice* device;
        EXPECT_EQ(ANeuralNetworks_getDevice(i, &device), ANEURALNETWORKS_NO_ERROR);
        EXPECT_EQ(ANeuralNetworksDevice_getExtensionSupport(device, kTestExtensionName, nullptr),
                  ANEURALNETWORKS_UNEXPECTED_NULL);
        EXPECT_EQ(ANeuralNetworksDevice_getExtensionSupport(device, nullptr, &result),
                  ANEURALNETWORKS_UNEXPECTED_NULL);
        EXPECT_EQ(ANeuralNetworksDevice_getExtensionSupport(device, kTestExtensionName, &result),
                  ANEURALNETWORKS_NO_ERROR);
    }
}
#endif

}  // namespace
