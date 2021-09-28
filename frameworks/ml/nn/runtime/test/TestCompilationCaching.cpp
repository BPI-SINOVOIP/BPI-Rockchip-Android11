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

#include <android-base/scopeguard.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <numeric>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "HalInterfaces.h"
#include "Manager.h"
#include "SampleDriver.h"
#include "TestNeuralNetworksWrapper.h"

using namespace android::nn;
using namespace hal;
using Result = test_wrapper::Result;
using Type = test_wrapper::Type;
const Timing kBadTiming = {.timeOnDevice = UINT64_MAX, .timeInDriver = UINT64_MAX};
template <typename T>
using MQDescriptorSync = ::android::hardware::MQDescriptorSync<T>;

namespace android::hardware::neuralnetworks::V1_0 {

::std::ostream& operator<<(::std::ostream& os, ErrorStatus errorStatus) {
    return os << toString(errorStatus);
}

}  // namespace android::hardware::neuralnetworks::V1_0

namespace {

enum class HasCalledPrepareModel { NO, WITHOUT_CACHING, WITH_CACHING };

// Print HasCalledPrepareModel enum for better GTEST failure messages
std::ostream& operator<<(std::ostream& os, HasCalledPrepareModel hasCalledPrepareModel) {
    switch (hasCalledPrepareModel) {
        case HasCalledPrepareModel::NO:
            return os << "NO";
        case HasCalledPrepareModel::WITHOUT_CACHING:
            return os << "WITHOUT_CACHING";
        case HasCalledPrepareModel::WITH_CACHING:
            return os << "WITH_CACHING";
    }
    CHECK(false) << "HasCalledPrepareModel print called with invalid code "
                 << static_cast<int>(hasCalledPrepareModel);
    return os;
}

// Whether the driver is expected to be registered because it can pass initialization.
bool canDeviceBeRegistered(ErrorStatus error, uint32_t numModelCache, uint32_t numDataCache) {
    constexpr uint32_t maxNumCacheFiles =
            static_cast<uint32_t>(Constant::MAX_NUMBER_OF_CACHE_FILES);
    return error == ErrorStatus::NONE && numModelCache <= maxNumCacheFiles &&
           numDataCache <= maxNumCacheFiles;
}

// Whether the driver supports caching based on the returns from getNumberOfCacheFilesNeeded.
bool isCachingSupported(uint32_t numModelCache, uint32_t numDataCache) {
    return numModelCache != 0 || numDataCache != 0;
}

// This is an IDevice for testing purposes which overrides several methods from sample driver:
// - supports all the operations and is faster than cpu fallback.
// - overrides getNumberOfCacheFilesNeeded to report according to given parameters.
// - overrides prepareModelFromCache_1_3 to return error status according to
//   mErrorStatusPrepareFromCache.
// - produces CachingPreparedModel on prepareModel and prepareModelFromCache_1_3.
//
// The cache entry is written by prepareModel_1_3 and is checked later by
// CachingDriver::prepareModelFromCache_1_3.
//
// The CachingDriver has 2 flags mHasCalledPrepareModelFromCache and mHasCalledPrepareModel
// to check if the correct methods are invoked by the runtime.
class CachingDriver : public sample_driver::SampleDriver {
   private:
    static constexpr size_t kCacheSize = 256;

    class CachingPreparedModel : public IPreparedModel {
       public:
        CachingPreparedModel() = default;

        Return<V1_0::ErrorStatus> execute(const V1_0::Request&,
                                          const sp<V1_0::IExecutionCallback>&) override {
            return V1_0::ErrorStatus::DEVICE_UNAVAILABLE;
        }
        Return<V1_0::ErrorStatus> execute_1_2(const V1_0::Request&, MeasureTiming,
                                              const sp<V1_2::IExecutionCallback>&) override {
            return V1_0::ErrorStatus::DEVICE_UNAVAILABLE;
        }
        Return<V1_3::ErrorStatus> execute_1_3(const V1_3::Request&, MeasureTiming,
                                              const OptionalTimePoint&,
                                              const OptionalTimeoutDuration&,
                                              const sp<V1_3::IExecutionCallback>&) override {
            return V1_3::ErrorStatus::DEVICE_UNAVAILABLE;
        }
        Return<void> executeSynchronously(const V1_0::Request&, MeasureTiming,
                                          executeSynchronously_cb cb) override {
            cb(V1_0::ErrorStatus::DEVICE_UNAVAILABLE, {}, kBadTiming);
            return Void();
        }
        Return<void> executeSynchronously_1_3(const V1_3::Request&, MeasureTiming,
                                              const OptionalTimePoint&,
                                              const OptionalTimeoutDuration&,
                                              executeSynchronously_1_3_cb cb) override {
            cb(V1_3::ErrorStatus::DEVICE_UNAVAILABLE, {}, kBadTiming);
            return Void();
        }
        Return<void> configureExecutionBurst(const sp<V1_2::IBurstCallback>&,
                                             const MQDescriptorSync<V1_2::FmqRequestDatum>&,
                                             const MQDescriptorSync<V1_2::FmqResultDatum>&,
                                             configureExecutionBurst_cb cb) override {
            cb(V1_0::ErrorStatus::DEVICE_UNAVAILABLE, nullptr);
            return Void();
        }
        Return<void> executeFenced(const hal::Request&, const hidl_vec<hidl_handle>&, MeasureTiming,
                                   const OptionalTimePoint&, const OptionalTimeoutDuration&,
                                   const OptionalTimeoutDuration&, executeFenced_cb cb) {
            cb(ErrorStatus::DEVICE_UNAVAILABLE, hidl_handle(nullptr), nullptr);
            return Void();
        }
    };

   public:
    CachingDriver(std::string_view name, ErrorStatus errorStatusGetNumCacheFiles,
                  uint32_t numModelCache, uint32_t numDataCache,
                  ErrorStatus errorStatusPrepareFromCache)
        : SampleDriver(name.data()),
          mErrorStatusGetNumCacheFiles(errorStatusGetNumCacheFiles),
          mNumModelCache(numModelCache),
          mNumDataCache(numDataCache),
          mErrorStatusPrepareFromCache(errorStatusPrepareFromCache) {
        mModelCacheData.resize(kCacheSize);
        std::iota(mModelCacheData.begin(), mModelCacheData.end(), 0);
        mDataCacheData.resize(kCacheSize);
        std::iota(mDataCacheData.begin(), mDataCacheData.end(), 1);
    }
    ~CachingDriver() override {}

    // Reports faster than cpu.
    Return<void> getCapabilities_1_3(getCapabilities_1_3_cb cb) override {
        android::nn::initVLogMask();
        const PerformanceInfo kPerf = {.execTime = 0.1, .powerUsage = 0.1};
        Capabilities capabilities = {
                .relaxedFloat32toFloat16PerformanceScalar = kPerf,
                .relaxedFloat32toFloat16PerformanceTensor = kPerf,
                .operandPerformance = nonExtensionOperandPerformance<HalVersion::V1_3>(kPerf),
                .ifPerformance = kPerf,
                .whilePerformance = kPerf};
        cb(V1_3::ErrorStatus::NONE, capabilities);
        return Void();
    }

    // Reports supporting all operations.
    Return<void> getSupportedOperations_1_3(const Model& model,
                                            getSupportedOperations_1_3_cb cb) override {
        std::vector<bool> supported(model.main.operations.size(), true);
        cb(V1_3::ErrorStatus::NONE, supported);
        return Void();
    }

    // Reports according to mGetNumCacheFiles.
    Return<void> getNumberOfCacheFilesNeeded(getNumberOfCacheFilesNeeded_cb cb) override {
        cb(convertToV1_0(mErrorStatusGetNumCacheFiles), mNumModelCache, mNumDataCache);
        return Void();
    }

    // Generates CachingPreparedModel.
    // Writes the cache entry per mCacheXData and sets mHasCalledPrepareModel.
    Return<V1_3::ErrorStatus> prepareModel_1_3(
            const Model&, ExecutionPreference, Priority, const OptionalTimePoint&,
            const hidl_vec<hidl_handle>& modelCacheHandle,
            const hidl_vec<hidl_handle>& dataCacheHandle, const CacheToken&,
            const sp<V1_3::IPreparedModelCallback>& cb) override {
        checkNumberOfCacheHandles(modelCacheHandle.size(), dataCacheHandle.size());
        if (modelCacheHandle.size() != 0 || dataCacheHandle.size() != 0) {
            writeToCache(modelCacheHandle, mModelCacheData);
            writeToCache(dataCacheHandle, mDataCacheData);
            mHasCalledPrepareModel = HasCalledPrepareModel::WITH_CACHING;
        } else {
            mHasCalledPrepareModel = HasCalledPrepareModel::WITHOUT_CACHING;
        }
        cb->notify_1_3(V1_3::ErrorStatus::NONE, new CachingPreparedModel());
        return V1_3::ErrorStatus::NONE;
    }

    // Checks if the cache entry is correct, notifies error status according to
    // mErrorStatusPrepareFromCache, sets mHasCalledPrepareModelFromCache.
    Return<V1_3::ErrorStatus> prepareModelFromCache_1_3(
            const OptionalTimePoint&, const hidl_vec<hidl_handle>& modelCacheHandle,
            const hidl_vec<hidl_handle>& dataCacheHandle, const CacheToken&,
            const sp<V1_3::IPreparedModelCallback>& callback) override {
        readFromCache(modelCacheHandle, mModelCacheData);
        readFromCache(dataCacheHandle, mDataCacheData);
        mHasCalledPrepareModelFromCache = true;
        if (mErrorStatusPrepareFromCache == V1_3::ErrorStatus::NONE) {
            callback->notify_1_3(mErrorStatusPrepareFromCache, new CachingPreparedModel());
        } else {
            callback->notify_1_3(mErrorStatusPrepareFromCache, nullptr);
        }
        return V1_3::ErrorStatus::NONE;
    };

    bool hasCalledPrepareModelFromCache() const { return mHasCalledPrepareModelFromCache; }
    HasCalledPrepareModel hasCalledPrepareModel() const { return mHasCalledPrepareModel; }

   private:
    // Checks the number of cache files passed to the driver from runtime.
    void checkNumberOfCacheHandles(size_t modelCache, size_t dataCache) {
        if (isCachingSupported(mNumModelCache, mNumDataCache)) {
            if (modelCache != 0 || dataCache != 0) {
                ASSERT_EQ(modelCache, mNumModelCache);
                ASSERT_EQ(dataCache, mNumDataCache);
            }
        } else {
            ASSERT_EQ(modelCache, 0ul);
            ASSERT_EQ(dataCache, 0ul);
        }
    }

    void writeToCache(const hidl_vec<hidl_handle>& handles, const std::vector<uint8_t>& cache) {
        for (uint32_t i = 0; i < handles.size(); ++i) {
            ASSERT_EQ(handles[i]->numFds, 1);
            EXPECT_EQ(write(handles[i]->data[0], cache.data(), kCacheSize),
                      static_cast<ssize_t>(kCacheSize));
        }
    }

    void readFromCache(const hidl_vec<hidl_handle>& handles, const std::vector<uint8_t>& expected) {
        for (uint32_t i = 0; i < handles.size(); ++i) {
            ASSERT_EQ(handles[i]->numFds, 1);
            std::vector<uint8_t> actual(kCacheSize);
            EXPECT_EQ(read(handles[i]->data[0], actual.data(), kCacheSize),
                      static_cast<ssize_t>(kCacheSize));
            EXPECT_EQ(actual, expected);
        }
    }

    std::vector<uint8_t> mModelCacheData;
    std::vector<uint8_t> mDataCacheData;

    const ErrorStatus mErrorStatusGetNumCacheFiles;
    const uint32_t mNumModelCache;
    const uint32_t mNumDataCache;
    const ErrorStatus mErrorStatusPrepareFromCache;

    bool mHasCalledPrepareModelFromCache = false;
    HasCalledPrepareModel mHasCalledPrepareModel = HasCalledPrepareModel::NO;
};

void CreateBroadcastAddModel(test_wrapper::Model* model) {
    test_wrapper::OperandType matrixType(Type::TENSOR_FLOAT32, {2, 2});
    test_wrapper::OperandType vectorType(Type::TENSOR_FLOAT32, {2});
    test_wrapper::OperandType scalarType(Type::INT32, {});
    int32_t activation(ANEURALNETWORKS_FUSED_NONE);
    auto a = model->addOperand(&matrixType);
    auto b = model->addOperand(&vectorType);
    auto c = model->addOperand(&matrixType);
    auto d = model->addOperand(&scalarType);
    model->setOperandValue(d, &activation, sizeof(activation));
    model->addOperation(ANEURALNETWORKS_ADD, {a, b, d}, {c});
    model->identifyInputsAndOutputs({a, b}, {c});
    ASSERT_TRUE(model->isValid());
    ASSERT_EQ(model->finish(), Result::NO_ERROR);
}

void getDeviceWithName(std::string_view deviceName, const ANeuralNetworksDevice** outputDevice) {
    uint32_t numDevices = 0;
    ASSERT_EQ(ANeuralNetworks_getDeviceCount(&numDevices), ANEURALNETWORKS_NO_ERROR);
    EXPECT_GE(numDevices, (uint32_t)1);

    int numMatchingDevices = 0;
    for (uint32_t i = 0; i < numDevices; i++) {
        ANeuralNetworksDevice* device = nullptr;
        ASSERT_EQ(ANeuralNetworks_getDevice(i, &device), ANEURALNETWORKS_NO_ERROR);

        const char* buffer = nullptr;
        ASSERT_EQ(ANeuralNetworksDevice_getName(device, &buffer), ANEURALNETWORKS_NO_ERROR);
        if (deviceName == buffer) {
            *outputDevice = device;
            numMatchingDevices++;
        }
    }

    EXPECT_LE(numMatchingDevices, 1);
}

// Test device registration with a driver parameterized with
// - ErrorStatus returning from getNumberOfCacheFilesNeeded
// - Number of model cache files returning from getNumberOfCacheFilesNeeded
// - Number of data cache files returning from getNumberOfCacheFilesNeeded
using DeviceRegistrationTestParam = std::tuple<ErrorStatus, uint32_t, uint32_t>;

class DeviceRegistrationTest : public ::testing::TestWithParam<DeviceRegistrationTestParam> {
   protected:
    static constexpr std::string_view kDeviceName = "deviceTestCompilationCaching";
    const ErrorStatus kErrorStatusGetNumCacheFiles = std::get<0>(GetParam());
    const uint32_t kNumModelCache = std::get<1>(GetParam());
    const uint32_t kNumDataCache = std::get<2>(GetParam());
    const sp<CachingDriver> kDriver =
            new CachingDriver(kDeviceName, kErrorStatusGetNumCacheFiles, kNumModelCache,
                              kNumDataCache, ErrorStatus::NONE);
};

TEST_P(DeviceRegistrationTest, CachingFailure) {
    if (DeviceManager::get()->getUseCpuOnly()) {
        return;
    }

    DeviceManager::get()->forTest_registerDevice(kDeviceName.data(), kDriver);
    const auto cleanup = android::base::make_scope_guard(
            [] { DeviceManager::get()->forTest_reInitializeDeviceList(); });

    // get device
    const ANeuralNetworksDevice* device = nullptr;
    getDeviceWithName(kDeviceName, &device);

    // check if device registeration matches expectations
    const bool isDeviceRegistered = (device != nullptr);
    const bool expectDeviceToBeRegistered =
            canDeviceBeRegistered(kErrorStatusGetNumCacheFiles, kNumModelCache, kNumDataCache);
    ASSERT_EQ(isDeviceRegistered, expectDeviceToBeRegistered);
}

// Test model compilation with a driver parameterized with
// - Number of model cache files returning from getNumberOfCacheFilesNeeded
// - Number of data cache files returning from getNumberOfCacheFilesNeeded
// - ErrorStatus returning from prepareModelFromCache_1_3
using CompilationCachingTestParam = std::tuple<uint32_t, uint32_t, ErrorStatus>;

class CompilationCachingTest : public ::testing::TestWithParam<CompilationCachingTestParam> {
   protected:
    virtual void SetUp() override {
        char cacheDirTemp[] = "/data/local/tmp/TestCompilationCachingXXXXXX";
        char* cacheDir = mkdtemp(cacheDirTemp);
        ASSERT_NE(cacheDir, nullptr);
        mCacheDir = cacheDir;
        CreateBroadcastAddModel(&mModel);
    }

    virtual void TearDown() override {
        if (!::testing::Test::HasFailure()) {
            std::filesystem::remove_all(mCacheDir);
        }
    }

    void compileModel(const sp<CachingDriver>& driver, bool withToken) {
        DeviceManager::get()->forTest_registerDevice(kDeviceName.data(), driver);
        const auto cleanup = android::base::make_scope_guard(
                [] { DeviceManager::get()->forTest_reInitializeDeviceList(); });

        // Get a handle to the single driver device matching kDeviceName.
        const ANeuralNetworksDevice* device = nullptr;
        getDeviceWithName(kDeviceName, &device);
        ASSERT_NE(device, nullptr);

        // Compile the model with the device.
        ANeuralNetworksCompilation* compilation = nullptr;
        ASSERT_EQ(ANeuralNetworksCompilation_createForDevices(mModel.getHandle(), &device, 1,
                                                              &compilation),
                  ANEURALNETWORKS_NO_ERROR);
        if (withToken) {
            ASSERT_EQ(ANeuralNetworksCompilation_setCaching(compilation, mCacheDir.c_str(),
                                                            kToken.data()),
                      ANEURALNETWORKS_NO_ERROR);
        }
        ASSERT_EQ(ANeuralNetworksCompilation_finish(compilation), ANEURALNETWORKS_NO_ERROR);
    }

    void createCache() {
        sp<CachingDriver> driver = new CachingDriver(kDeviceName, ErrorStatus::NONE, kNumModelCache,
                                                     kNumDataCache, ErrorStatus::NONE);
        compileModel(driver, /*withToken=*/true);
    }

    static constexpr std::string_view kDeviceName = "deviceTestCompilationCaching";
    const uint32_t kNumModelCache = std::get<0>(GetParam());
    const uint32_t kNumDataCache = std::get<1>(GetParam());
    const ErrorStatus kErrorStatusPrepareFromCache = std::get<2>(GetParam());
    const bool kIsCachingSupported = isCachingSupported(kNumModelCache, kNumDataCache);
    test_wrapper::Model mModel;
    std::string mCacheDir;
    const CacheToken kToken{};
};

TEST_P(CompilationCachingTest, TokenProvidedAndCacheNotExist) {
    if (DeviceManager::get()->getUseCpuOnly()) {
        return;
    }
    sp<CachingDriver> driver = new CachingDriver(kDeviceName, ErrorStatus::NONE, kNumModelCache,
                                                 kNumDataCache, kErrorStatusPrepareFromCache);
    compileModel(driver, /*withToken=*/true);

    // When cache file does not exist, the runtime should never call prepareModelFromCache_1_3.
    EXPECT_FALSE(driver->hasCalledPrepareModelFromCache());

    // The runtime should call prepareModel_1_3. It should request caching iff caching supported.
    EXPECT_EQ(driver->hasCalledPrepareModel(), kIsCachingSupported
                                                       ? HasCalledPrepareModel::WITH_CACHING
                                                       : HasCalledPrepareModel::WITHOUT_CACHING);
}

TEST_P(CompilationCachingTest, TokenProvidedAndCacheExist) {
    if (DeviceManager::get()->getUseCpuOnly()) {
        return;
    }
    createCache();
    sp<CachingDriver> driver = new CachingDriver(kDeviceName, ErrorStatus::NONE, kNumModelCache,
                                                 kNumDataCache, kErrorStatusPrepareFromCache);
    compileModel(driver, /*withToken=*/true);

    // When cache files exist, the runtime should call prepareModelFromCache_1_3 iff caching
    // supported.
    EXPECT_EQ(driver->hasCalledPrepareModelFromCache(), kIsCachingSupported);

    HasCalledPrepareModel expectHasCalledPrepareModel;
    if (kIsCachingSupported) {
        if (kErrorStatusPrepareFromCache == ErrorStatus::NONE) {
            // The runtime should not call prepareModel_1_3 iff caching supported and
            // prepareModelFromCache_1_3 succeeds.
            expectHasCalledPrepareModel = HasCalledPrepareModel::NO;
        } else {
            // The runtime should call prepareModel_1_3 and request caching iff caching supported
            // but prepareModelFromCache_1_3 fails.
            expectHasCalledPrepareModel = HasCalledPrepareModel::WITH_CACHING;
        }
    } else {
        // The runtime should call prepareModel_1_3 without caching iff caching not supported.
        expectHasCalledPrepareModel = HasCalledPrepareModel::WITHOUT_CACHING;
    }
    EXPECT_EQ(driver->hasCalledPrepareModel(), expectHasCalledPrepareModel);
}

TEST_P(CompilationCachingTest, TokenNotProvided) {
    if (DeviceManager::get()->getUseCpuOnly()) {
        return;
    }
    sp<CachingDriver> driver = new CachingDriver(kDeviceName, ErrorStatus::NONE, kNumModelCache,
                                                 kNumDataCache, kErrorStatusPrepareFromCache);
    compileModel(driver, /*withToken=*/false);

    // When no NDK token is provided by the client, the runtime should never call
    // prepareModelFromCache_1_3 or request caching with prepareModel_1_3.
    EXPECT_FALSE(driver->hasCalledPrepareModelFromCache());
    EXPECT_EQ(driver->hasCalledPrepareModel(), HasCalledPrepareModel::WITHOUT_CACHING);
}

static const auto kErrorStatusGetNumCacheFilesChoices =
        testing::Values(ErrorStatus::NONE, ErrorStatus::DEVICE_UNAVAILABLE);
static const auto kNumCacheChoices =
        testing::Values(0ul, 1ul, static_cast<uint32_t>(Constant::MAX_NUMBER_OF_CACHE_FILES),
                        static_cast<uint32_t>(Constant::MAX_NUMBER_OF_CACHE_FILES) + 1);
static const auto kNumValidCacheChoices =
        testing::Values(0ul, 1ul, static_cast<uint32_t>(Constant::MAX_NUMBER_OF_CACHE_FILES));
static const auto kErrorStatusPrepareFromCacheChoices =
        testing::Values(ErrorStatus::NONE, ErrorStatus::GENERAL_FAILURE,
                        ErrorStatus::DEVICE_UNAVAILABLE, ErrorStatus::INVALID_ARGUMENT);

INSTANTIATE_TEST_CASE_P(TestCompilationCaching, DeviceRegistrationTest,
                        testing::Combine(kErrorStatusGetNumCacheFilesChoices, kNumCacheChoices,
                                         kNumCacheChoices));

INSTANTIATE_TEST_CASE_P(TestCompilationCaching, CompilationCachingTest,
                        testing::Combine(kNumValidCacheChoices, kNumValidCacheChoices,
                                         kErrorStatusPrepareFromCacheChoices));

}  // namespace
