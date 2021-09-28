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
#include <android-base/properties.h>
#include <ftw.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "GeneratedTestUtils.h"
#include "TestHarness.h"
#include "TestNeuralNetworksWrapper.h"
#include "TestUtils.h"

// Systrace is not available from CTS tests due to platform layering
// constraints. We reuse the NNTEST_ONLY_PUBLIC_API flag, as that should also be
// the case for CTS (public APIs only).
#ifndef NNTEST_ONLY_PUBLIC_API
#include "Tracing.h"
#else
#define NNTRACE_FULL_RAW(...)
#define NNTRACE_APP(...)
#define NNTRACE_APP_SWITCH(...)
#endif

#ifdef NNTEST_CTS
#define NNTEST_COMPUTE_MODE
#endif

namespace android::nn::generated_tests {
using namespace test_wrapper;
using namespace test_helper;

class GeneratedTests : public GeneratedTestBase {
   protected:
    void SetUp() override;
    void TearDown() override;

    bool shouldSkipTest();

    std::optional<Compilation> compileModel(const Model& model);
    void executeWithCompilation(const Compilation& compilation, const TestModel& testModel);
    void executeOnce(const Model& model, const TestModel& testModel);
    void executeMultithreadedOwnCompilation(const Model& model, const TestModel& testModel);
    void executeMultithreadedSharedCompilation(const Model& model, const TestModel& testModel);
    // Test driver for those generated from ml/nn/runtime/test/spec
    void execute(const TestModel& testModel);

    // VNDK version of the device under test.
    static int mVndkVersion;

    std::string mCacheDir;
    std::vector<uint8_t> mToken;
    bool mTestCompilationCaching = false;
    bool mTestDynamicOutputShape = false;
    bool mExpectFailure = false;
    bool mTestQuantizationCoupling = false;
    bool mTestDeviceMemory = false;
};

int GeneratedTests::mVndkVersion = __ANDROID_API_FUTURE__;

// Tag for the dynamic output shape tests
class DynamicOutputShapeTest : public GeneratedTests {
   protected:
    DynamicOutputShapeTest() { mTestDynamicOutputShape = true; }
};

// Tag for the fenced execute tests
class FencedComputeTest : public GeneratedTests {};

// Tag for the generated validation tests
class GeneratedValidationTests : public GeneratedTests {
   protected:
    GeneratedValidationTests() { mExpectFailure = true; }
};

class QuantizationCouplingTest : public GeneratedTests {
   protected:
    QuantizationCouplingTest() { mTestQuantizationCoupling = true; }
};

class DeviceMemoryTest : public GeneratedTests {
   protected:
    DeviceMemoryTest() { mTestDeviceMemory = true; }
};

std::optional<Compilation> GeneratedTests::compileModel(const Model& model) {
    NNTRACE_APP(NNTRACE_PHASE_COMPILATION, "compileModel");
    if (mTestCompilationCaching) {
        // Compile the model twice with the same token, so that compilation caching will be
        // exercised if supported by the driver.
        // No invalid model will be passed to this branch.
        EXPECT_FALSE(mExpectFailure);
        Compilation compilation1(&model);
        EXPECT_EQ(compilation1.setCaching(mCacheDir, mToken), Result::NO_ERROR);
        EXPECT_EQ(compilation1.finish(), Result::NO_ERROR);
        Compilation compilation2(&model);
        EXPECT_EQ(compilation2.setCaching(mCacheDir, mToken), Result::NO_ERROR);
        EXPECT_EQ(compilation2.finish(), Result::NO_ERROR);
        return compilation2;
    } else {
        Compilation compilation(&model);
        Result result = compilation.finish();

        // For valid model, we check the compilation result == NO_ERROR.
        // For invalid model, the driver may fail at compilation or execution, so any result code is
        // permitted at this point.
        if (mExpectFailure && result != Result::NO_ERROR) return std::nullopt;
        EXPECT_EQ(result, Result::NO_ERROR);
        return compilation;
    }
}

static void computeWithPtrs(const TestModel& testModel, Execution* execution, Result* result,
                            std::vector<TestBuffer>* outputs) {
    {
        NNTRACE_APP(NNTRACE_PHASE_INPUTS_AND_OUTPUTS, "computeWithPtrs example");
        createRequest(testModel, execution, outputs);
    }
    *result = execution->compute();
}

static ANeuralNetworksMemory* createDeviceMemoryForInput(const Compilation& compilation,
                                                         uint32_t index) {
    ANeuralNetworksMemoryDesc* desc = nullptr;
    EXPECT_EQ(ANeuralNetworksMemoryDesc_create(&desc), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(desc, compilation.getHandle(), index, 1.0f),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_finish(desc), ANEURALNETWORKS_NO_ERROR);
    ANeuralNetworksMemory* memory = nullptr;
    ANeuralNetworksMemory_createFromDesc(desc, &memory);
    ANeuralNetworksMemoryDesc_free(desc);
    return memory;
}

static ANeuralNetworksMemory* createDeviceMemoryForOutput(const Compilation& compilation,
                                                          uint32_t index) {
    ANeuralNetworksMemoryDesc* desc = nullptr;
    EXPECT_EQ(ANeuralNetworksMemoryDesc_create(&desc), ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(desc, compilation.getHandle(), index, 1.0f),
              ANEURALNETWORKS_NO_ERROR);
    EXPECT_EQ(ANeuralNetworksMemoryDesc_finish(desc), ANEURALNETWORKS_NO_ERROR);
    ANeuralNetworksMemory* memory = nullptr;
    ANeuralNetworksMemory_createFromDesc(desc, &memory);
    ANeuralNetworksMemoryDesc_free(desc);
    return memory;
}

// Set result = Result::NO_ERROR and outputs = {} if the test should be skipped.
static void computeWithDeviceMemories(const Compilation& compilation, const TestModel& testModel,
                                      Execution* execution, Result* result,
                                      std::vector<TestBuffer>* outputs) {
    ASSERT_NE(execution, nullptr);
    ASSERT_NE(result, nullptr);
    ASSERT_NE(outputs, nullptr);
    outputs->clear();
    std::vector<Memory> inputMemories, outputMemories;

    {
        NNTRACE_APP(NNTRACE_PHASE_INPUTS_AND_OUTPUTS, "computeWithDeviceMemories example");
        // Model inputs.
        for (uint32_t i = 0; i < testModel.main.inputIndexes.size(); i++) {
            SCOPED_TRACE("Input index: " + std::to_string(i));
            const auto& operand = testModel.main.operands[testModel.main.inputIndexes[i]];
            // Omitted input.
            if (operand.data.size() == 0) {
                ASSERT_EQ(Result::NO_ERROR, execution->setInput(i, nullptr, 0));
                continue;
            }

            // Create device memory.
            ANeuralNetworksMemory* memory = createDeviceMemoryForInput(compilation, i);
            ASSERT_NE(memory, nullptr);
            auto& wrapperMemory = inputMemories.emplace_back(memory);

            // Copy data from TestBuffer to device memory.
            auto ashmem = TestAshmem::createFrom(operand.data);
            ASSERT_NE(ashmem, nullptr);
            ASSERT_EQ(ANeuralNetworksMemory_copy(ashmem->get()->get(), memory),
                      ANEURALNETWORKS_NO_ERROR);
            ASSERT_EQ(Result::NO_ERROR, execution->setInputFromMemory(i, &wrapperMemory, 0, 0));
        }

        // Model outputs.
        for (uint32_t i = 0; i < testModel.main.outputIndexes.size(); i++) {
            SCOPED_TRACE("Output index: " + std::to_string(i));
            ANeuralNetworksMemory* memory = createDeviceMemoryForOutput(compilation, i);
            ASSERT_NE(memory, nullptr);
            auto& wrapperMemory = outputMemories.emplace_back(memory);
            ASSERT_EQ(Result::NO_ERROR, execution->setOutputFromMemory(i, &wrapperMemory, 0, 0));
        }
    }

    *result = execution->compute();

    // Copy out output results.
    for (uint32_t i = 0; i < testModel.main.outputIndexes.size(); i++) {
        SCOPED_TRACE("Output index: " + std::to_string(i));
        const auto& operand = testModel.main.operands[testModel.main.outputIndexes[i]];
        const size_t bufferSize = operand.data.size();
        auto& output = outputs->emplace_back(bufferSize);

        auto ashmem = TestAshmem::createFrom(output);
        ASSERT_NE(ashmem, nullptr);
        ASSERT_EQ(ANeuralNetworksMemory_copy(outputMemories[i].get(), ashmem->get()->get()),
                  ANEURALNETWORKS_NO_ERROR);
        std::copy(ashmem->dataAs<uint8_t>(), ashmem->dataAs<uint8_t>() + bufferSize,
                  output.getMutable<uint8_t>());
    }
}

void GeneratedTests::executeWithCompilation(const Compilation& compilation,
                                            const TestModel& testModel) {
    NNTRACE_APP(NNTRACE_PHASE_EXECUTION, "executeWithCompilation example");

    Execution execution(&compilation);
    Result result;
    std::vector<TestBuffer> outputs;

    if (mTestDeviceMemory) {
        computeWithDeviceMemories(compilation, testModel, &execution, &result, &outputs);
    } else {
        computeWithPtrs(testModel, &execution, &result, &outputs);
    }

    if (result == Result::NO_ERROR && outputs.empty()) {
        return;
    }

    {
        NNTRACE_APP(NNTRACE_PHASE_RESULTS, "executeWithCompilation example");
        if (mExpectFailure) {
            ASSERT_NE(result, Result::NO_ERROR);
            return;
        } else {
            ASSERT_EQ(result, Result::NO_ERROR);
        }

        // Check output dimensions.
        for (uint32_t i = 0; i < testModel.main.outputIndexes.size(); i++) {
            const auto& output = testModel.main.operands[testModel.main.outputIndexes[i]];
            if (output.isIgnored) continue;
            std::vector<uint32_t> actualDimensions;
            ASSERT_EQ(Result::NO_ERROR, execution.getOutputOperandDimensions(i, &actualDimensions));
            ASSERT_EQ(output.dimensions, actualDimensions);
        }

        checkResults(testModel, outputs);
    }
}

void GeneratedTests::executeOnce(const Model& model, const TestModel& testModel) {
    NNTRACE_APP(NNTRACE_PHASE_OVERALL, "executeOnce");
    std::optional<Compilation> compilation = compileModel(model);
    // Early return if compilation fails. The compilation result code is checked in compileModel.
    if (!compilation) return;
    executeWithCompilation(compilation.value(), testModel);
}

void GeneratedTests::executeMultithreadedOwnCompilation(const Model& model,
                                                        const TestModel& testModel) {
    NNTRACE_APP(NNTRACE_PHASE_OVERALL, "executeMultithreadedOwnCompilation");
    SCOPED_TRACE("MultithreadedOwnCompilation");
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.push_back(std::thread([&]() { executeOnce(model, testModel); }));
    }
    std::for_each(threads.begin(), threads.end(), [](std::thread& t) { t.join(); });
}

void GeneratedTests::executeMultithreadedSharedCompilation(const Model& model,
                                                           const TestModel& testModel) {
    NNTRACE_APP(NNTRACE_PHASE_OVERALL, "executeMultithreadedSharedCompilation");
    SCOPED_TRACE("MultithreadedSharedCompilation");
    std::optional<Compilation> compilation = compileModel(model);
    // Early return if compilation fails. The ompilation result code is checked in compileModel.
    if (!compilation) return;
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.push_back(
                std::thread([&]() { executeWithCompilation(compilation.value(), testModel); }));
    }
    std::for_each(threads.begin(), threads.end(), [](std::thread& t) { t.join(); });
}

// Test driver for those generated from ml/nn/runtime/test/spec
void GeneratedTests::execute(const TestModel& testModel) {
    NNTRACE_APP(NNTRACE_PHASE_OVERALL, "execute");
    GeneratedModel model;
    createModel(testModel, mTestDynamicOutputShape, &model);
    if (testModel.expectFailure && !model.isValid()) {
        return;
    }
    ASSERT_EQ(model.finish(), Result::NO_ERROR);
    ASSERT_TRUE(model.isValid());
    auto executeInternal = [&testModel, &model, this]() {
        SCOPED_TRACE("TestCompilationCaching = " + std::to_string(mTestCompilationCaching));
#ifndef NNTEST_MULTITHREADED
        executeOnce(model, testModel);
#else   // defined(NNTEST_MULTITHREADED)
        executeMultithreadedOwnCompilation(model, testModel);
        executeMultithreadedSharedCompilation(model, testModel);
#endif  // !defined(NNTEST_MULTITHREADED)
    };
    mTestCompilationCaching = false;
    executeInternal();
    if (!mExpectFailure) {
        mTestCompilationCaching = true;
        executeInternal();
    }
}

bool GeneratedTests::shouldSkipTest() {
    // A map of {min VNDK version -> tests that should be skipped with earlier VNDK versions}.
    // The listed tests are added in a later release, but exercising old APIs. They should be
    // skipped if the device has a mixed build of system and vendor partitions.
    static const std::map<int, std::set<std::string>> kMapOfMinVndkVersionToTests = {
            {
                    __ANDROID_API_R__,
                    {
                            "add_broadcast_quant8_all_inputs_as_internal",
                    },
            },
    };
    for (const auto& [minVersion, names] : kMapOfMinVndkVersionToTests) {
        if (mVndkVersion < minVersion && names.count(kTestName) > 0) {
            return true;
        }
    }
    return false;
}

void GeneratedTests::SetUp() {
    GeneratedTestBase::SetUp();

    mVndkVersion = ::android::base::GetIntProperty("ro.vndk.version", __ANDROID_API_FUTURE__);
    if (shouldSkipTest()) {
        GTEST_SKIP();
        return;
    }

    char cacheDirTemp[] = "/data/local/tmp/TestCompilationCachingXXXXXX";
    char* cacheDir = mkdtemp(cacheDirTemp);
    ASSERT_NE(cacheDir, nullptr);
    mCacheDir = cacheDir;
    mToken = std::vector<uint8_t>(ANEURALNETWORKS_BYTE_SIZE_OF_CACHE_TOKEN, 0);
}

void GeneratedTests::TearDown() {
    if (!::testing::Test::HasFailure()) {
        // TODO: Switch to std::filesystem::remove_all once libc++fs is made available in CTS.
        // Remove the cache directory specified by path recursively.
        auto callback = [](const char* child, const struct stat*, int, struct FTW*) {
            return remove(child);
        };
        nftw(mCacheDir.c_str(), callback, 128, FTW_DEPTH | FTW_MOUNT | FTW_PHYS);
    }
    GeneratedTestBase::TearDown();
}

#ifdef NNTEST_COMPUTE_MODE
TEST_P(GeneratedTests, Sync) {
    const auto oldComputeMode = Execution::setComputeMode(Execution::ComputeMode::SYNC);
    execute(testModel);
    Execution::setComputeMode(oldComputeMode);
}

TEST_P(GeneratedTests, Async) {
    const auto oldComputeMode = Execution::setComputeMode(Execution::ComputeMode::ASYNC);
    execute(testModel);
    Execution::setComputeMode(oldComputeMode);
}

TEST_P(GeneratedTests, Burst) {
    const auto oldComputeMode = Execution::setComputeMode(Execution::ComputeMode::BURST);
    execute(testModel);
    Execution::setComputeMode(oldComputeMode);
}
#else
TEST_P(GeneratedTests, Test) {
    execute(testModel);
}
#endif

TEST_P(DynamicOutputShapeTest, Test) {
    execute(testModel);
}

TEST_P(GeneratedValidationTests, Test) {
    execute(testModel);
}

TEST_P(QuantizationCouplingTest, Test) {
    execute(convertQuant8AsymmOperandsToSigned(testModel));
}

TEST_P(DeviceMemoryTest, Test) {
    execute(testModel);
}

TEST_P(FencedComputeTest, Test) {
    const auto oldComputeMode = Execution::setComputeMode(Execution::ComputeMode::FENCED);
    execute(testModel);
    Execution::setComputeMode(oldComputeMode);
}

INSTANTIATE_GENERATED_TEST(GeneratedTests,
                           [](const TestModel& testModel) { return !testModel.expectFailure; });

INSTANTIATE_GENERATED_TEST(DynamicOutputShapeTest, [](const TestModel& testModel) {
    return !testModel.expectFailure && !testModel.hasScalarOutputs();
});

INSTANTIATE_GENERATED_TEST(GeneratedValidationTests, [](const TestModel& testModel) {
    return testModel.expectFailure && !testModel.isInfiniteLoopTimeoutTest();
});

INSTANTIATE_GENERATED_TEST(QuantizationCouplingTest, [](const TestModel& testModel) {
    return !testModel.expectFailure && testModel.main.operations.size() == 1 &&
           testModel.referenced.size() == 0 && testModel.hasQuant8CoupledOperands();
});

INSTANTIATE_GENERATED_TEST(DeviceMemoryTest, [](const TestModel& testModel) {
    return !testModel.expectFailure &&
           std::all_of(testModel.main.outputIndexes.begin(), testModel.main.outputIndexes.end(),
                       [&testModel](uint32_t index) {
                           return testModel.main.operands[index].data.size() > 0;
                       });
});

INSTANTIATE_GENERATED_TEST(FencedComputeTest, [](const TestModel& testModel) {
    return !testModel.expectFailure &&
           std::all_of(testModel.main.outputIndexes.begin(), testModel.main.outputIndexes.end(),
                       [&testModel](uint32_t index) {
                           return testModel.main.operands[index].data.size() > 0;
                       });
});

}  // namespace android::nn::generated_tests
