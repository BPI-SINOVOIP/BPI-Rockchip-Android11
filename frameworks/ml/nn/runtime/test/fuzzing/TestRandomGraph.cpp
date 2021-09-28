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

#include <android-base/properties.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "GeneratedTestUtils.h"
#include "TestHarness.h"
#include "TestNeuralNetworksWrapper.h"
#include "fuzzing/OperationManager.h"
#include "fuzzing/RandomGraphGenerator.h"
#include "fuzzing/RandomGraphGeneratorUtils.h"

#ifndef NNTEST_CTS
#include <memunreachable/memunreachable.h>

#include <vector>

#include "HalInterfaces.h"
#include "Manager.h"
#include "SampleDriverFull.h"

using android::nn::sample_driver::SampleDriverFull;
using namespace android::nn::hal;

#endif

namespace android {
namespace nn {
namespace fuzzing_test {

using namespace test_helper;
using test_wrapper::Result;
constexpr char kRefDeviceName[] = "nnapi-reference";

#ifndef NNTEST_CTS
class TestDriverV1_2 : public SampleDriverFull {
   public:
    TestDriverV1_2() : SampleDriverFull(name, {.execTime = 0.9f, .powerUsage = 0.9f}) {}
    static constexpr char name[] = "TestDriverV1_2";
};

// Like SampleDriverFull, but implementing 1.1
class TestDriverV1_1 : public V1_1::IDevice {
   public:
    TestDriverV1_1()
        : mDriverV1_2(new SampleDriverFull(name, {.execTime = 0.8f, .powerUsage = 0.8f})) {}
    static constexpr char name[] = "TestDriverV1_1";
    Return<void> getCapabilities_1_1(getCapabilities_1_1_cb _hidl_cb) override {
        return mDriverV1_2->getCapabilities_1_1(_hidl_cb);
    }
    Return<void> getSupportedOperations_1_1(const V1_1::Model& model,
                                            getSupportedOperations_1_1_cb _hidl_cb) override {
        return mDriverV1_2->getSupportedOperations_1_1(model, _hidl_cb);
    }
    Return<V1_0::ErrorStatus> prepareModel_1_1(
            const V1_1::Model& model, ExecutionPreference preference,
            const sp<V1_0::IPreparedModelCallback>& actualCallback) override {
        return mDriverV1_2->prepareModel_1_1(model, preference, actualCallback);
    }
    Return<DeviceStatus> getStatus() override { return mDriverV1_2->getStatus(); }
    Return<void> getCapabilities(getCapabilities_cb _hidl_cb) override {
        return mDriverV1_2->getCapabilities(_hidl_cb);
    }
    Return<void> getSupportedOperations(const V1_0::Model& model,
                                        getSupportedOperations_cb _hidl_cb) override {
        return mDriverV1_2->getSupportedOperations(model, _hidl_cb);
    }
    Return<V1_0::ErrorStatus> prepareModel(
            const V1_0::Model& model,
            const sp<V1_0::IPreparedModelCallback>& actualCallback) override {
        return mDriverV1_2->prepareModel(model, actualCallback);
    }

   private:
    const sp<V1_2::IDevice> mDriverV1_2;
};

// Like SampleDriverFull, but implementing 1.0
class TestDriverV1_0 : public V1_0::IDevice {
   public:
    TestDriverV1_0()
        : mDriverV1_2(new SampleDriverFull(name, {.execTime = 0.7f, .powerUsage = 0.7f})) {}
    static constexpr char name[] = "TestDriverV1_0";
    Return<void> getCapabilities(getCapabilities_cb _hidl_cb) override {
        return mDriverV1_2->getCapabilities(_hidl_cb);
    }
    Return<void> getSupportedOperations(const V1_0::Model& model,
                                        getSupportedOperations_cb _hidl_cb) override {
        return mDriverV1_2->getSupportedOperations(model, _hidl_cb);
    }
    Return<V1_0::ErrorStatus> prepareModel(
            const V1_0::Model& model,
            const sp<V1_0::IPreparedModelCallback>& actualCallback) override {
        return mDriverV1_2->prepareModel(model, actualCallback);
    }
    Return<DeviceStatus> getStatus() override { return mDriverV1_2->getStatus(); }

   private:
    const sp<V1_2::IDevice> mDriverV1_2;
};

template <class T_TestDriver>
std::shared_ptr<Device> makeTestDevice() {
    return DeviceManager::forTest_makeDriverDevice(T_TestDriver::name, new T_TestDriver);
}

#endif

// NN API fuzzer logging setting comes from system property debug.nn.fuzzer.log and
// debug.nn.fuzzer.dumpspec.
// * setprop debug.nn.fuzzer.log 1 : enable logging.
// * setprop debug.nn.fuzzer.log 0 : silence logging.
// * setprop debug.nn.fuzzer.dumpspec 1 : dump the randomly generated graph to a spec file.
// * setprop debug.nn.fuzzer.dumpspec 0 : do not dump the graph.
//
// Logs and spec files are dumped to /data/local/tmp/${testname}.{log,mod.py},
// e.g. for test case TestRandomGraph/RandomGraphTest/Large/0,
//      log : /data/local/tmp/TestRandomGraph_RandomGraphTest_Large_0.log
//      spec: /data/local/tmp/TestRandomGraph_RandomGraphTest_Large_0.mod.py
//
class RandomGraphTest : public ::testing::TestWithParam<uint32_t> {
   public:
    static void SetUpTestCase() {
#ifndef NNTEST_CTS
        mEnableLog = ::android::base::GetProperty("debug.nn.fuzzer.log", "") == "1";
        mDumpSpec = ::android::base::GetProperty("debug.nn.fuzzer.dumpspec", "") == "1";
        mDetectMemoryLeak = ::android::base::GetProperty("debug.nn.fuzzer.detectleak", "") == "1";

        mStandardDevices = DeviceManager::get()->forTest_getDevices();
        mSyntheticDevices.push_back(makeTestDevice<TestDriverV1_2>());
        mSyntheticDevices.push_back(makeTestDevice<TestDriverV1_1>());
        mSyntheticDevices.push_back(makeTestDevice<TestDriverV1_0>());
#endif
        mVndkVersion = ::android::base::GetIntProperty("ro.vndk.version", __ANDROID_API_FUTURE__);

        // Get all the devices and device names.
        mStandardDevicesFeatureLevel = __ANDROID_API_FUTURE__;
        uint32_t numDevices = 0;
        ASSERT_EQ(ANeuralNetworks_getDeviceCount(&numDevices), ANEURALNETWORKS_NO_ERROR);
        for (uint32_t i = 0; i < numDevices; i++) {
            ANeuralNetworksDevice* device = nullptr;
            const char* name = nullptr;
            int64_t featureLevel;
            ASSERT_EQ(ANeuralNetworks_getDevice(i, &device), ANEURALNETWORKS_NO_ERROR);
            ASSERT_EQ(ANeuralNetworksDevice_getName(device, &name), ANEURALNETWORKS_NO_ERROR);
            ASSERT_EQ(ANeuralNetworksDevice_getFeatureLevel(device, &featureLevel),
                      ANEURALNETWORKS_NO_ERROR);
            mDevices.emplace(name, device);
            mStandardDevicesFeatureLevel = std::min(mStandardDevicesFeatureLevel, featureLevel);
        }
    }

   protected:
    virtual void SetUp() override {
        // Initialize logging.
        const ::testing::TestInfo* const testInfo =
                ::testing::UnitTest::GetInstance()->current_test_info();
        mTestName = mTestName + testInfo->test_case_name() + "_" + testInfo->name();
        std::replace(mTestName.begin(), mTestName.end(), '/', '_');
        if (mEnableLog) NN_FUZZER_LOG_INIT("/data/local/tmp/" + mTestName + ".log");
    }

    virtual void TearDown() override {
        NN_FUZZER_LOG_CLOSE;
        // Dump test results on failure for debugging.
        if (::testing::Test::HasFailure() || mDumpSpec) {
            dumpTestResults();
        }
#ifndef NNTEST_CTS
        if (mDetectMemoryLeak) {
            ASSERT_TRUE(NoLeaks());
        }
#endif
    }

    bool shouldSkipTest(int64_t featureLevel) {
        static const std::set<std::string> kDisabledTests = {
                // In this test, the RGG produces a non-sensible graph with extreme large output
                // gain and highly clamped output range.
                // TODO: Currently quantized buffer values are uniformly distributed within
                //       [0, 255]. We should investigate on a better buffer value generation
                //       algorithm that represents the real-world cases.
                "TestRandomGraph_SingleOperationTest_CONV_2D_V1_2_40",
                "TestRandomGraph_SingleOperationTest_DEPTHWISE_CONV_2D_V1_0_32",
        };
        if (kDisabledTests.find(mTestName) != kDisabledTests.end()) return true;
        for (const auto& op : mTestModel.main.operations) {
            // Skip if testing BATCH_TO_SPACE_ND with batch dimension == 1.
            if (op.type == TestOperationType::BATCH_TO_SPACE_ND &&
                mTestModel.main.operands[op.inputs[0]].dimensions[0] == 1 &&
                featureLevel <= __ANDROID_API_Q__) {
                return true;
            }
            // L2_NORMALIZATION on axis of all zeros is undefined before R.
            if (op.type == TestOperationType::L2_NORMALIZATION &&
                featureLevel <= __ANDROID_API_Q__) {
                return true;
            }
            // Skip the following operations for 1.2 and earlier devices.
            if ((op.type == TestOperationType::ADD || op.type == TestOperationType::SUB ||
                 op.type == TestOperationType::MAXIMUM || op.type == TestOperationType::MINIMUM ||
                 op.type == TestOperationType::ROI_ALIGN) &&
                mTestModel.main.operands[op.inputs[0]].type ==
                        TestOperandType::TENSOR_QUANT8_ASYMM &&
                featureLevel <= __ANDROID_API_Q__) {
                return true;
            }
            // Skip the following operations when the VNDK version is earlier than R.
            if (mVndkVersion < __ANDROID_API_R__ &&
                op.type == TestOperationType::HEATMAP_MAX_KEYPOINT) {
                return true;
            }
        }
        return false;
    }

    // Compute the golden output results of the test model on nnapi-reference. If possible, the
    // golden results will be computed from an equivalent float32 model to avoid bias avoid bias
    // from quantized CPU implementation.
    void computeGoldenResults() {
        SCOPED_TRACE("computeGoldenResults");

        // Convert the test model to an equivalent float32 model if possible.
        auto fpModel = convertToFloat32Model(mTestModel);
        const TestModel& goldenModel = fpModel.has_value() ? fpModel.value() : mTestModel;

        // Create model.
        generated_tests::GeneratedModel model;
        generated_tests::createModel(goldenModel, &model);
        ASSERT_TRUE(model.isValid());
        ASSERT_EQ(model.finish(), Result::NO_ERROR);

        // Create compilation for nnapi-reference.
        ASSERT_TRUE(mDevices.find(kRefDeviceName) != mDevices.end());
        const auto refDevice = mDevices[kRefDeviceName];
        auto [result, compilation] = test_wrapper::Compilation::createForDevice(&model, refDevice);
        ASSERT_EQ(result, Result::NO_ERROR);
        ASSERT_EQ(compilation.finish(), Result::NO_ERROR);

        // Create request.
        test_wrapper::Execution execution(&compilation);
        std::vector<TestBuffer> outputs;
        generated_tests::createRequest(goldenModel, &execution, &outputs);

        // Compute result.
        ASSERT_EQ(execution.compute(), Result::NO_ERROR);

        if (fpModel.has_value()) {
            // Quantize the execution results as golden values.
            setExpectedOutputsFromFloat32Results(outputs, &mTestModel);
        } else {
            for (uint32_t i = 0; i < outputs.size(); i++) {
                auto outputIndex = mTestModel.main.outputIndexes[i];
                mTestModel.main.operands[outputIndex].data = outputs[i];
            }
        }
    }

    // Compile and execute the generated graph on a device selected by name.
    void computeAndVerifyResultsForDevice(const test_wrapper::Model* model, uint32_t numOps,
                                          const std::string& name) {
        SCOPED_TRACE("Device: " + name);
        std::cout << "[          ] - RUN:  " << name << "\n";
        ASSERT_TRUE(mDevices.find(name) != mDevices.end());
        const auto device = mDevices[name];

        // Check if the device fully supports the graph.
        constexpr int kMaxNumberOperations = 1000;
        ASSERT_TRUE(numOps <= kMaxNumberOperations);
        bool supported[kMaxNumberOperations] = {false};
        ASSERT_EQ(ANeuralNetworksModel_getSupportedOperationsForDevices(model->getHandle(), &device,
                                                                        1, supported),
                  ANEURALNETWORKS_NO_ERROR);
        if (!std::all_of(supported, supported + numOps, [](bool v) { return v; })) {
            std::cout << "[          ]   SKIP: " << name << " does not support the graph.\n";
            return;
        }

        // Since this test is introduced in Android Q, we only check the accuracy of output results
        // if the device has feature level >= Q (API level 29). For pre-Q devices, we allow
        // them to produce less accurate results, but must not hang or crash.
        int64_t featureLevel;
        ASSERT_EQ(ANeuralNetworksDevice_getFeatureLevel(device, &featureLevel),
                  ANEURALNETWORKS_NO_ERROR);
        if (shouldSkipTest(featureLevel)) return;

        // Create compilation for device.
        auto [result, compilation] = test_wrapper::Compilation::createForDevice(model, device);
        ASSERT_EQ(result, Result::NO_ERROR);
        Result compileReturn = compilation.finish();
        // Even if the model is fully supported, the compilation may still fail, e.g. each operation
        // is supported, but model is too big (too many operations and/or too-large constants) for
        // device.
        if (compileReturn == Result::OP_FAILED) {
            std::cout << "[          ]   SKIP: " << name << " failed at compilation step.\n";
            return;
        }
        ASSERT_EQ(compileReturn, Result::NO_ERROR);

        // Create request.
        test_wrapper::Execution execution(&compilation);
        std::vector<TestBuffer> outputs;
        generated_tests::createRequest(mTestModel, &execution, &outputs);

        // Compute result.
        Result executeReturn = execution.compute();
        // Even if the model is fully supported and the compilation succeeds, the execution may
        // still fail, e.g. there may be operand shapes that are unknown until execution time, and
        // at execution time turn out to be too big.
        if (executeReturn == Result::OP_FAILED) {
            std::cout << "[          ]   SKIP: " << name << " failed at execution step.\n";
            return;
        }
        ASSERT_EQ(executeReturn, Result::NO_ERROR);

        if (featureLevel >= __ANDROID_API_Q__) {
            checkResults(mTestModel, outputs, mCriteria);
            mResults.emplace_back(name, std::move(outputs));
        }
    }

    // Compile and execute the generated graph normally (i.e., allow runtime to
    // distribute across devices).
    void computeAndVerifyResults(const std::string& name, const test_wrapper::Model* model,
                                 bool shouldCheckResults) {
        // Because we're not using the introspection/control API, the CpuDevice
        // is available as a fallback, and hence we assume that compilation and
        // execution will succeed.
        SCOPED_TRACE(name);
        std::cout << "[          ] - RUN:  " << name << "\n";

        // Create compilation.
        test_wrapper::Compilation compilation(model);
        ASSERT_EQ(compilation.finish(), Result::NO_ERROR);

        // Create request.
        test_wrapper::Execution execution(&compilation);
        std::vector<TestBuffer> outputs;
        generated_tests::createRequest(mTestModel, &execution, &outputs);

        // Compute and verify result.
        ASSERT_EQ(execution.compute(), Result::NO_ERROR);
        if (shouldCheckResults) {
            checkResults(mTestModel, outputs, mCriteria);
            mResults.emplace_back(name, std::move(outputs));
        }
    }

    // Main test entrance.
    void testRandomGraph(uint32_t numOperations, uint32_t dimensionRange) {
        // Generate a random graph.
        RandomGraph graph;
        ASSERT_TRUE(graph.generate(kSeed, numOperations, dimensionRange));

        // Create a model from the random graph.
        mTestModel = graph.createTestModel();

        generated_tests::GeneratedModel model;
        generated_tests::createModel(mTestModel, &model);
        ASSERT_TRUE(model.isValid());
        ASSERT_EQ(model.finish(), Result::NO_ERROR);

        // Compute reference results.
        computeGoldenResults();

        // Compute on each available device.
        for (auto& pair : mDevices) {
            computeAndVerifyResultsForDevice(&model, numOperations, pair.first);
        }

        if (numOperations > 1) {
            if (!shouldSkipTest(mStandardDevicesFeatureLevel)) {
                // Compute normally (i.e., allow runtime to distribute across devices).
                computeAndVerifyResults("Compute normally", &model,
                                        mStandardDevicesFeatureLevel >= __ANDROID_API_Q__);
            }

#ifndef NNTEST_CTS
            {
                // Stress partitioner by allowing runtime to distribute across
                // three synthetic devices.  The synthetic devices use the
                // CpuExecutor for execution, so we always check results, even
                // though some are of feature level < __ANDROID_API_Q__: In this
                // case, we don't take feature level as an indication of
                // reliability, as we do with real devices.
                DeviceManager::get()->forTest_setDevices(mSyntheticDevices);
                computeAndVerifyResults("Compute across synthetic devices", &model, true);
                DeviceManager::get()->forTest_setDevices(mStandardDevices);
            }
#endif
        }
    }

    void dumpTestResults() {
        std::ofstream os("/data/local/tmp/" + mTestName + ".mod.py");
        ASSERT_TRUE(os.is_open());
        os << "# Generated from " << mTestName << ". Do not edit.\n\n";
        SpecDumper dumper(mTestModel, os);
        dumper.dumpTestModel();
        for (const auto& [name, results] : mResults) {
            dumper.dumpResults(name, results);
        }
    }

    enum GraphSize : uint32_t { SINGLE = 1, SMALL = 5, LARGE = 40 };
    enum DimensionRange : uint32_t { NARROW = 10, WIDE = 1000 };

    static bool mEnableLog;
    static bool mDumpSpec;
    static bool mDetectMemoryLeak;
    static std::map<std::string, ANeuralNetworksDevice*> mDevices;

    const uint32_t kSeed = GetParam();
    std::string mTestName;
    TestModel mTestModel;
    AccuracyCriteria mCriteria;

    // A vector of {name, output_results}.
    std::vector<std::pair<std::string, std::vector<TestBuffer>>> mResults;

    static int mVndkVersion;
    static int64_t mStandardDevicesFeatureLevel;  // minimum across all devices
#ifndef NNTEST_CTS
    static std::vector<std::shared_ptr<Device>> mStandardDevices;
    static std::vector<std::shared_ptr<Device>> mSyntheticDevices;
#endif
};

bool RandomGraphTest::mEnableLog = false;
bool RandomGraphTest::mDumpSpec = false;
bool RandomGraphTest::mDetectMemoryLeak = false;
std::map<std::string, ANeuralNetworksDevice*> RandomGraphTest::mDevices;

int RandomGraphTest::mVndkVersion = __ANDROID_API_FUTURE__;
int64_t RandomGraphTest::mStandardDevicesFeatureLevel;
#ifndef NNTEST_CTS
std::vector<std::shared_ptr<Device>> RandomGraphTest::mStandardDevices;
std::vector<std::shared_ptr<Device>> RandomGraphTest::mSyntheticDevices;
#endif

// Single-op graph with dimensions in range [1, 1000].
class SingleOperationTest : public RandomGraphTest {};
#define TEST_SINGLE_OPERATION(operation, halVersion, criteria)               \
    TEST_P(SingleOperationTest, operation##_##halVersion) {                  \
        OperationFilter filter = {.opcodes = {TestOperationType::operation}, \
                                  .versions = {TestHalVersion::halVersion}}; \
        OperationManager::get()->applyFilter(filter);                        \
        mCriteria = (criteria);                                              \
        testRandomGraph(GraphSize::SINGLE, DimensionRange::WIDE);            \
    }

// TODO: Adjust the accuracy criteria based on testing.
// We define three sets of accuracy criteria for single-operation tests.

// This is for operations that only copy buffers around without any computation on buffer values.
// Most of these operations fall into categories of reshape or selection, e.g. RESHAPE, GATHER.
// Additionally, operations with only logical or comparison arithmetic also use this criteria, e.g.
// EQUAL, ARGMAX, TOPK_V2.
const AccuracyCriteria kStrictCriteria = {
        .float32 = {.bias = 1e-7f, .mse = 1e-10f, .atol = 1e-6f, .rtol = 1e-6f},
        .float16 = {.bias = 1e-4f, .mse = 1e-8f, .atol = 1e-3f, .rtol = 1e-3f},
        .int32 = {.atol = 1},
        .quant8Asymm = {.bias = 0.1f, .mse = 0.1f, .atol = 1},
        .quant8AsymmSigned = {.bias = 0.1f, .mse = 0.1f, .atol = 1},
        .quant8Symm = {.bias = 0.1f, .mse = 0.1f, .atol = 1},
        .quant16Asymm = {.bias = 0.1f, .mse = 0.1f, .atol = 1},
        .quant16Symm = {.bias = 0.1f, .mse = 0.1f, .atol = 1},
};

// This is for operations that only do simple and single computation on buffer values, such as
// addition, multiplication, or requantization. Most of these operations fall into categories of
// broadcast or elementwise, e.g ADD, FLOOR.
const AccuracyCriteria kMediumCriteria = {
        .float32 = {.bias = 1e-6f, .mse = 1e-8f, .atol = 1e-5f, .rtol = 1e-5f},
        .float16 = {.bias = 1e-3f, .mse = 1e-5f, .atol = 1e-2f, .rtol = 1e-2f},
        .int32 = {.atol = 1},
        .quant8Asymm = {.bias = 1.2, .mse = 1.2, .atol = 2},
        .quant8AsymmSigned = {.bias = 1.2, .mse = 1.2, .atol = 2},
        .quant8Symm = {.bias = 1.2, .mse = 1.2, .atol = 2},
        .quant16Asymm = {.bias = 1.2, .mse = 1.2, .atol = 2},
        .quant16Symm = {.bias = 1.2, .mse = 1.2, .atol = 2},
};

// This is for operations that involve sophisticated computations on buffer values, either a single
// but complex transformation, e.g. LOGISTIC, or multiple transformations with accumulated errors,
// e.g. L2_NORMALIZATION, REDUCE_*.
const AccuracyCriteria kRelaxedCriteria = {
        .float32 = {.bias = 3e-5f, .mse = 1e-6f, .atol = 1e-3f, .rtol = 1e-3f},
        .float16 = {.bias = 5e-3f, .mse = 1e-3f, .atol = 1.0f, .rtol = 1.0f},
        .int32 = {.atol = 1},
        .quant8Asymm = {.bias = 1.5, .mse = 1.5, .atol = 10},
        .quant8AsymmSigned = {.bias = 1.5, .mse = 1.5, .atol = 10},
        .quant8Symm = {.bias = 1.5, .mse = 1.5, .atol = 10},
        .quant16Asymm = {.bias = 1.5, .mse = 1.5, .atol = 10},
        .quant16Symm = {.bias = 1.5, .mse = 1.5, .atol = 10},
};

// This is for convolution operations with potentially large kernel size.
const AccuracyCriteria kConvCriteria = {
        .float32 = {.bias = 4e-4f, .mse = 1e-5f, .atol = 2e-2f, .rtol = 2e-2f},
        .float16 = {.bias = 5e-2f, .mse = 1e-2f, .atol = 1.0f, .rtol = 1.0f},
        .int32 = {.atol = 1},
        .quant8Asymm = {.bias = 1.5, .mse = 1.5, .atol = 10},
        .quant8AsymmSigned = {.bias = 1.5, .mse = 1.5, .atol = 10},
        .quant8Symm = {.bias = 1.5, .mse = 1.5, .atol = 10},
        .quant16Asymm = {.bias = 1.5, .mse = 1.5, .atol = 10},
        .quant16Symm = {.bias = 1.5, .mse = 1.5, .atol = 10},
};

/*-- NNAPI 1.0 Operations ---------------------------------------------------*/

// TODO: The following 1.0 operation signatures are currently not defined:
// - ANEURALNETWORKS_LSH_PROJECTION
// - ANEURALNETWORKS_LSTM
// - ANEURALNETWORKS_RNN
// - ANEURALNETWORKS_SVDF

TEST_SINGLE_OPERATION(ADD, V1_0, kMediumCriteria);
TEST_SINGLE_OPERATION(MUL, V1_0, kMediumCriteria);
TEST_SINGLE_OPERATION(FLOOR, V1_0, kMediumCriteria);
TEST_SINGLE_OPERATION(LOGISTIC, V1_0, kRelaxedCriteria);
TEST_SINGLE_OPERATION(RELU, V1_0, kMediumCriteria);
TEST_SINGLE_OPERATION(RELU1, V1_0, kMediumCriteria);
TEST_SINGLE_OPERATION(RELU6, V1_0, kMediumCriteria);
TEST_SINGLE_OPERATION(TANH, V1_0, kRelaxedCriteria);
TEST_SINGLE_OPERATION(SOFTMAX, V1_0, kRelaxedCriteria);
TEST_SINGLE_OPERATION(L2_NORMALIZATION, V1_0, kRelaxedCriteria);
TEST_SINGLE_OPERATION(LOCAL_RESPONSE_NORMALIZATION, V1_0, kRelaxedCriteria);
TEST_SINGLE_OPERATION(AVERAGE_POOL_2D, V1_0, kRelaxedCriteria);
TEST_SINGLE_OPERATION(L2_POOL_2D, V1_0, kRelaxedCriteria);
TEST_SINGLE_OPERATION(MAX_POOL_2D, V1_0, kRelaxedCriteria);
TEST_SINGLE_OPERATION(CONV_2D, V1_0, kConvCriteria);
TEST_SINGLE_OPERATION(DEPTHWISE_CONV_2D, V1_0, kConvCriteria);
TEST_SINGLE_OPERATION(CONCATENATION, V1_0, kMediumCriteria);
TEST_SINGLE_OPERATION(RESIZE_BILINEAR, V1_0, kRelaxedCriteria);
TEST_SINGLE_OPERATION(DEPTH_TO_SPACE, V1_0, kStrictCriteria);
TEST_SINGLE_OPERATION(SPACE_TO_DEPTH, V1_0, kStrictCriteria);
TEST_SINGLE_OPERATION(EMBEDDING_LOOKUP, V1_0, kStrictCriteria);
TEST_SINGLE_OPERATION(HASHTABLE_LOOKUP, V1_0, kStrictCriteria);
TEST_SINGLE_OPERATION(FULLY_CONNECTED, V1_0, kRelaxedCriteria);
TEST_SINGLE_OPERATION(RESHAPE, V1_0, kStrictCriteria);
TEST_SINGLE_OPERATION(DEQUANTIZE, V1_0, kMediumCriteria);

/*-- NNAPI 1.1 Operations ---------------------------------------------------*/

TEST_SINGLE_OPERATION(SUB, V1_1, kMediumCriteria);
TEST_SINGLE_OPERATION(DIV, V1_1, kRelaxedCriteria);
TEST_SINGLE_OPERATION(BATCH_TO_SPACE_ND, V1_1, kStrictCriteria);
TEST_SINGLE_OPERATION(SPACE_TO_BATCH_ND, V1_1, kStrictCriteria);
TEST_SINGLE_OPERATION(MEAN, V1_1, kRelaxedCriteria);
TEST_SINGLE_OPERATION(PAD, V1_1, kStrictCriteria);
TEST_SINGLE_OPERATION(TRANSPOSE, V1_1, kStrictCriteria);
TEST_SINGLE_OPERATION(SQUEEZE, V1_1, kStrictCriteria);
TEST_SINGLE_OPERATION(STRIDED_SLICE, V1_1, kStrictCriteria);

/*-- NNAPI 1.0 and 1.1 Operations with Extended Behavior in 1.2 -------------*/

TEST_SINGLE_OPERATION(ADD, V1_2, kMediumCriteria);
TEST_SINGLE_OPERATION(MUL, V1_2, kMediumCriteria);
TEST_SINGLE_OPERATION(SUB, V1_2, kMediumCriteria);
TEST_SINGLE_OPERATION(DIV, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(FLOOR, V1_2, kMediumCriteria);
TEST_SINGLE_OPERATION(LOGISTIC, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(RELU, V1_2, kMediumCriteria);
TEST_SINGLE_OPERATION(RELU1, V1_2, kMediumCriteria);
TEST_SINGLE_OPERATION(RELU6, V1_2, kMediumCriteria);
TEST_SINGLE_OPERATION(TANH, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(CONCATENATION, V1_2, kMediumCriteria);
TEST_SINGLE_OPERATION(DEPTH_TO_SPACE, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(SPACE_TO_DEPTH, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(BATCH_TO_SPACE_ND, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(SPACE_TO_BATCH_ND, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(FULLY_CONNECTED, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(RESHAPE, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(MEAN, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(PAD, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(TRANSPOSE, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(CONV_2D, V1_2, kConvCriteria);
TEST_SINGLE_OPERATION(DEPTHWISE_CONV_2D, V1_2, kConvCriteria);
TEST_SINGLE_OPERATION(AVERAGE_POOL_2D, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(L2_POOL_2D, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(MAX_POOL_2D, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(RESIZE_BILINEAR, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(SOFTMAX, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(L2_NORMALIZATION, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(LOCAL_RESPONSE_NORMALIZATION, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(DEQUANTIZE, V1_2, kMediumCriteria);
TEST_SINGLE_OPERATION(SQUEEZE, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(STRIDED_SLICE, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(EMBEDDING_LOOKUP, V1_2, kStrictCriteria);

/*-- NNAPI 1.2 Operations ---------------------------------------------------*/

// TODO: The following 1.2 operation signatures are currently not defined:
// - ANEURALNETWORKS_AXIS_ALIGNED_BBOX_TRANSFORM
// - ANEURALNETWORKS_BIDIRECTIONAL_SEQUENCE_LSTM
// - ANEURALNETWORKS_BIDIRECTIONAL_SEQUENCE_RNN
// - ANEURALNETWORKS_BOX_WITH_NMS_LIMIT
// - ANEURALNETWORKS_DETECTION_POSTPROCESSING
// - ANEURALNETWORKS_GENERATE_PROPOSALS
// - ANEURALNETWORKS_QUANTIZED_16BIT_LSTM
// - ANEURALNETWORKS_RANDOM_MULTINOMIAL
// - ANEURALNETWORKS_UNIDIRECTIONAL_SEQUENCE_LSTM
// - ANEURALNETWORKS_UNIDIRECTIONAL_SEQUENCE_RNN

TEST_SINGLE_OPERATION(ABS, V1_2, kMediumCriteria);
TEST_SINGLE_OPERATION(EXP, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(LOG, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(NEG, V1_2, kMediumCriteria);
TEST_SINGLE_OPERATION(RSQRT, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(SIN, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(SQRT, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(ARGMAX, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(ARGMIN, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(EQUAL, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(GREATER, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(GREATER_EQUAL, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(LESS, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(LESS_EQUAL, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(LOGICAL_AND, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(LOGICAL_NOT, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(LOGICAL_OR, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(NOT_EQUAL, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(MAXIMUM, V1_2, kMediumCriteria);
TEST_SINGLE_OPERATION(MINIMUM, V1_2, kMediumCriteria);
TEST_SINGLE_OPERATION(POW, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(PRELU, V1_2, kMediumCriteria);
TEST_SINGLE_OPERATION(REDUCE_ALL, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(REDUCE_ANY, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(REDUCE_MAX, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(REDUCE_MIN, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(REDUCE_PROD, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(REDUCE_SUM, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(CHANNEL_SHUFFLE, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(INSTANCE_NORMALIZATION, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(LOG_SOFTMAX, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(GROUPED_CONV_2D, V1_2, kConvCriteria);
TEST_SINGLE_OPERATION(TRANSPOSE_CONV_2D, V1_2, kConvCriteria);
TEST_SINGLE_OPERATION(RESIZE_NEAREST_NEIGHBOR, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(PAD_V2, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(QUANTIZE, V1_2, kMediumCriteria);
TEST_SINGLE_OPERATION(CAST, V1_2, kMediumCriteria);
TEST_SINGLE_OPERATION(EXPAND_DIMS, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(TILE, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(GATHER, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(SELECT, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(TOPK_V2, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(SLICE, V1_2, kStrictCriteria);
TEST_SINGLE_OPERATION(SPLIT, V1_2, kMediumCriteria);
TEST_SINGLE_OPERATION(ROI_ALIGN, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(ROI_POOLING, V1_2, kRelaxedCriteria);
TEST_SINGLE_OPERATION(HEATMAP_MAX_KEYPOINT, V1_2, kRelaxedCriteria);

/*-- NNAPI 1.0, 1.1, and 1.2 Operations with Extended Behavior in 1.3 -------------*/

TEST_SINGLE_OPERATION(ADD, V1_3, kMediumCriteria);
TEST_SINGLE_OPERATION(AVERAGE_POOL_2D, V1_3, kRelaxedCriteria);
TEST_SINGLE_OPERATION(CONCATENATION, V1_3, kMediumCriteria);
TEST_SINGLE_OPERATION(CONV_2D, V1_3, kConvCriteria);
TEST_SINGLE_OPERATION(DEPTHWISE_CONV_2D, V1_3, kConvCriteria);
TEST_SINGLE_OPERATION(DEPTH_TO_SPACE, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(DEQUANTIZE, V1_3, kMediumCriteria);
TEST_SINGLE_OPERATION(EMBEDDING_LOOKUP, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(FULLY_CONNECTED, V1_3, kRelaxedCriteria);
TEST_SINGLE_OPERATION(L2_NORMALIZATION, V1_3, kRelaxedCriteria);
TEST_SINGLE_OPERATION(LOGISTIC, V1_3, kRelaxedCriteria);
TEST_SINGLE_OPERATION(MAX_POOL_2D, V1_3, kRelaxedCriteria);
TEST_SINGLE_OPERATION(MUL, V1_3, kMediumCriteria);
TEST_SINGLE_OPERATION(RELU, V1_3, kMediumCriteria);
TEST_SINGLE_OPERATION(RELU1, V1_3, kMediumCriteria);
TEST_SINGLE_OPERATION(RELU6, V1_3, kMediumCriteria);
TEST_SINGLE_OPERATION(RESHAPE, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(RESIZE_BILINEAR, V1_3, kRelaxedCriteria);
TEST_SINGLE_OPERATION(SOFTMAX, V1_3, kRelaxedCriteria);
TEST_SINGLE_OPERATION(SPACE_TO_DEPTH, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(TANH, V1_3, kRelaxedCriteria);
TEST_SINGLE_OPERATION(BATCH_TO_SPACE_ND, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(DIV, V1_3, kMediumCriteria);
TEST_SINGLE_OPERATION(MEAN, V1_3, kRelaxedCriteria);
TEST_SINGLE_OPERATION(PAD, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(SPACE_TO_BATCH_ND, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(SQUEEZE, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(STRIDED_SLICE, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(SUB, V1_3, kMediumCriteria);
TEST_SINGLE_OPERATION(TRANSPOSE, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(ABS, V1_3, kMediumCriteria);
TEST_SINGLE_OPERATION(ARGMAX, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(ARGMIN, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(CAST, V1_3, kMediumCriteria);
TEST_SINGLE_OPERATION(CHANNEL_SHUFFLE, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(EQUAL, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(EXPAND_DIMS, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(GATHER, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(GREATER, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(GREATER_EQUAL, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(GROUPED_CONV_2D, V1_3, kConvCriteria);
TEST_SINGLE_OPERATION(HEATMAP_MAX_KEYPOINT, V1_3, kRelaxedCriteria);
TEST_SINGLE_OPERATION(LESS, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(LESS_EQUAL, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(MAXIMUM, V1_3, kMediumCriteria);
TEST_SINGLE_OPERATION(MINIMUM, V1_3, kMediumCriteria);
TEST_SINGLE_OPERATION(NOT_EQUAL, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(PAD_V2, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(PRELU, V1_3, kMediumCriteria);
TEST_SINGLE_OPERATION(QUANTIZE, V1_3, kMediumCriteria);
TEST_SINGLE_OPERATION(REDUCE_MAX, V1_3, kRelaxedCriteria);
TEST_SINGLE_OPERATION(REDUCE_MIN, V1_3, kRelaxedCriteria);
TEST_SINGLE_OPERATION(ROI_ALIGN, V1_3, kRelaxedCriteria);
TEST_SINGLE_OPERATION(ROI_POOLING, V1_3, kRelaxedCriteria);
TEST_SINGLE_OPERATION(SELECT, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(SLICE, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(SPLIT, V1_3, kMediumCriteria);
TEST_SINGLE_OPERATION(TILE, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(TOPK_V2, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(TRANSPOSE_CONV_2D, V1_3, kConvCriteria);
TEST_SINGLE_OPERATION(RESIZE_NEAREST_NEIGHBOR, V1_3, kRelaxedCriteria);

/*-- NNAPI 1.3 Operations ---------------------------------------------------*/

// TODO: The following 1.3 operation signatures are currently not defined:
// - ANEURALNETWORKS_QUANTIZED_LSTM
// - ANEURALNETWORKS_IF
// - ANEURALNETWORKS_WHILE

TEST_SINGLE_OPERATION(ELU, V1_3, kMediumCriteria);
TEST_SINGLE_OPERATION(HARD_SWISH, V1_3, kMediumCriteria);
TEST_SINGLE_OPERATION(FILL, V1_3, kStrictCriteria);
TEST_SINGLE_OPERATION(RANK, V1_3, kStrictCriteria);

const AccuracyCriteria kSmallGraphCriteria = {
        .float32 = {.bias = 4e-4f, .mse = 1e-5f, .atol = 1e-2f, .rtol = 1e-2f},
        .float16 = {.bias = 5e-2f, .mse = 1e-2f, .atol = 1.0f, .rtol = 1.0f},
        .int32 = {.atol = 1},
        .quant8Asymm = {.bias = 2, .mse = 2, .atol = 12},
        .quant8AsymmSigned = {.bias = 2, .mse = 2, .atol = 12},
        .quant8Symm = {.bias = 2, .mse = 2, .atol = 12},
        .quant16Asymm = {.bias = 2, .mse = 2, .atol = 12},
        .quant16Symm = {.bias = 2, .mse = 2, .atol = 12},
};

const AccuracyCriteria kLargeGraphCriteria = {
        .float32 = {.bias = 1e-2f, .mse = 1e-4f, .atol = 1e-1f, .rtol = 1e-1f},
        .float16 = {.bias = 1e-1f, .mse = 5e-2f, .atol = 1.0f, .rtol = 1.0f},
        .int32 = {.atol = 1},
        .quant8Asymm = {.bias = 2, .mse = 2, .atol = 12},
        .quant8AsymmSigned = {.bias = 2, .mse = 2, .atol = 12},
        .quant8Symm = {.bias = 2, .mse = 2, .atol = 12},
        .quant16Asymm = {.bias = 2, .mse = 2, .atol = 12},
        .quant16Symm = {.bias = 2, .mse = 2, .atol = 12},
};

// Due to the limitation of the random graph generator, graphs generated with mixed-type or
// mixed-rank operations are likely to result in a disconnected network. Thus, we filter the
// operation signatures by primary data type and rank first, then generate random graph tests for
// each combination.
//
// Two parameterized tests are created for each filter:
// * 5-op graph with dimensions in range [1, 1000].
// * 40-op graph with dimensions in range [1, 10].
//
#define TEST_RANDOM_GRAPH_WITH_DATA_TYPE_AND_RANK(dataType, rank)                             \
    TEST_P(RandomGraphTest, SmallGraph_##dataType##_Rank##rank) {                             \
        OperationFilter filter = {.dataTypes = {TestOperandType::dataType}, .ranks = {rank}}; \
        OperationManager::get()->applyFilter(filter);                                         \
        mCriteria = kSmallGraphCriteria;                                                      \
        testRandomGraph(GraphSize::SMALL, DimensionRange::WIDE);                              \
    }                                                                                         \
    TEST_P(RandomGraphTest, LargeGraph_##dataType##_Rank##rank) {                             \
        OperationFilter filter = {.dataTypes = {TestOperandType::dataType}, .ranks = {rank}}; \
        OperationManager::get()->applyFilter(filter);                                         \
        mCriteria = kLargeGraphCriteria;                                                      \
        testRandomGraph(GraphSize::LARGE, DimensionRange::NARROW);                            \
    }

// Random graph test with TENSOR_QUANT8_ASYMM as the primary data type is currently not defined.
// The generated graph with TENSOR_QUANT8_ASYMM as the primary data type will likely to result in
// disconnected graphs due to the mismatch between quantized parameters.

TEST_RANDOM_GRAPH_WITH_DATA_TYPE_AND_RANK(TENSOR_FLOAT32, 4);
TEST_RANDOM_GRAPH_WITH_DATA_TYPE_AND_RANK(TENSOR_FLOAT32, 3);
TEST_RANDOM_GRAPH_WITH_DATA_TYPE_AND_RANK(TENSOR_FLOAT32, 2);
TEST_RANDOM_GRAPH_WITH_DATA_TYPE_AND_RANK(TENSOR_FLOAT32, 1);

TEST_RANDOM_GRAPH_WITH_DATA_TYPE_AND_RANK(TENSOR_FLOAT16, 4);
TEST_RANDOM_GRAPH_WITH_DATA_TYPE_AND_RANK(TENSOR_FLOAT16, 3);
TEST_RANDOM_GRAPH_WITH_DATA_TYPE_AND_RANK(TENSOR_FLOAT16, 2);
TEST_RANDOM_GRAPH_WITH_DATA_TYPE_AND_RANK(TENSOR_FLOAT16, 1);

TEST_RANDOM_GRAPH_WITH_DATA_TYPE_AND_RANK(TENSOR_INT32, 4);
TEST_RANDOM_GRAPH_WITH_DATA_TYPE_AND_RANK(TENSOR_INT32, 3);
TEST_RANDOM_GRAPH_WITH_DATA_TYPE_AND_RANK(TENSOR_INT32, 2);
TEST_RANDOM_GRAPH_WITH_DATA_TYPE_AND_RANK(TENSOR_INT32, 1);

TEST_RANDOM_GRAPH_WITH_DATA_TYPE_AND_RANK(TENSOR_BOOL8, 4);
TEST_RANDOM_GRAPH_WITH_DATA_TYPE_AND_RANK(TENSOR_BOOL8, 3);
TEST_RANDOM_GRAPH_WITH_DATA_TYPE_AND_RANK(TENSOR_BOOL8, 2);
TEST_RANDOM_GRAPH_WITH_DATA_TYPE_AND_RANK(TENSOR_BOOL8, 1);

INSTANTIATE_TEST_CASE_P(TestRandomGraph, SingleOperationTest, ::testing::Range(0u, 50u));
INSTANTIATE_TEST_CASE_P(TestRandomGraph, RandomGraphTest, ::testing::Range(0u, 50u));

}  // namespace fuzzing_test
}  // namespace nn
}  // namespace android
