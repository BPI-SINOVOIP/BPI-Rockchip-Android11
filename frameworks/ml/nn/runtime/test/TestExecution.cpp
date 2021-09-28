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

#include <android-base/scopeguard.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "Callbacks.h"
#include "CompilationBuilder.h"
#include "HalInterfaces.h"
#include "Manager.h"
#include "ModelBuilder.h"
#include "NeuralNetworks.h"
#include "SampleDriver.h"
#include "TestNeuralNetworksWrapper.h"
#include "Utils.h"
#include "ValidateHal.h"

namespace android {

using namespace nn::hal;
using CompilationBuilder = nn::CompilationBuilder;
using Device = nn::Device;
using DeviceManager = nn::DeviceManager;
using HidlModel = V1_3::Model;
using PreparedModelCallback = nn::PreparedModelCallback;
using Result = nn::test_wrapper::Result;
using SampleDriver = nn::sample_driver::SampleDriver;
using WrapperCompilation = nn::test_wrapper::Compilation;
using WrapperEvent = nn::test_wrapper::Event;
using WrapperExecution = nn::test_wrapper::Execution;
using WrapperModel = nn::test_wrapper::Model;
using WrapperOperandType = nn::test_wrapper::OperandType;
using WrapperType = nn::test_wrapper::Type;
using nn::convertToV1_0;

template <typename T>
using MQDescriptorSync = hardware::MQDescriptorSync<T>;

namespace {

const Timing kBadTiming = {.timeOnDevice = UINT64_MAX, .timeInDriver = UINT64_MAX};

// Wraps the latest version of IPreparedModel to allow dummying up the execution status,
// and control when the execution finishes.
class TestPreparedModelLatest : public IPreparedModel {
   public:
    // If errorStatus is NONE, then execute behaves normally (and sends back
    // the actual execution status).  Otherwise, don't bother to execute, and
    // just send back errorStatus (as the execution status, not the launch
    // status).
    TestPreparedModelLatest(sp<V1_0::IPreparedModel> preparedModel, ErrorStatus errorStatus)
        : mPreparedModelV1_0(preparedModel),
          mPreparedModelV1_2(V1_2::IPreparedModel::castFrom(preparedModel).withDefault(nullptr)),
          mPreparedModelV1_3(V1_3::IPreparedModel::castFrom(preparedModel).withDefault(nullptr)),
          mErrorStatus(errorStatus) {}

    Return<V1_0::ErrorStatus> execute(const V1_0::Request& request,
                                      const sp<V1_0::IExecutionCallback>& callback) override {
        CHECK(mPreparedModelV1_0 != nullptr) << "V1_0 prepared model is nullptr.";
        std::thread([this, &request, &callback] {
            dummyExecution();
            if (mErrorStatus == ErrorStatus::NONE) {
                // Note that we lose the actual launch status.
                (void)mPreparedModelV1_0->execute(request, callback);
            } else {
                callback->notify(convertToV1_0(mErrorStatus));
            }
        }).detach();
        return V1_0::ErrorStatus::NONE;
    }

    Return<V1_0::ErrorStatus> execute_1_2(const V1_0::Request& request, MeasureTiming measure,
                                          const sp<V1_2::IExecutionCallback>& callback) override {
        CHECK(mPreparedModelV1_2 != nullptr) << "V1_2 prepared model is nullptr.";
        std::thread([this, &request, measure, &callback] {
            dummyExecution();
            if (mErrorStatus == ErrorStatus::NONE) {
                // Note that we lose the actual launch status.
                (void)mPreparedModelV1_2->execute_1_2(request, measure, callback);
            } else if (mErrorStatus == ErrorStatus::OUTPUT_INSUFFICIENT_SIZE) {
                OutputShape shape = {.dimensions = {1}, .isSufficient = false};
                callback->notify_1_2(convertToV1_0(mErrorStatus), {shape}, kBadTiming);
            } else {
                callback->notify_1_2(convertToV1_0(mErrorStatus), {}, kBadTiming);
            }
        }).detach();
        return V1_0::ErrorStatus::NONE;
    }

    Return<V1_3::ErrorStatus> execute_1_3(const V1_3::Request& request, MeasureTiming measure,
                                          const OptionalTimePoint& deadline,
                                          const OptionalTimeoutDuration& loopTimeoutDuration,
                                          const sp<V1_3::IExecutionCallback>& callback) override {
        CHECK(mPreparedModelV1_3 != nullptr) << "V1_3 prepared model is nullptr.";
        std::thread([this, &request, measure, &deadline, &loopTimeoutDuration, &callback] {
            dummyExecution();
            if (mErrorStatus == ErrorStatus::NONE) {
                // Note that we lose the actual launch status.
                (void)mPreparedModelV1_3->execute_1_3(request, measure, deadline,
                                                      loopTimeoutDuration, callback);
            } else if (mErrorStatus == ErrorStatus::OUTPUT_INSUFFICIENT_SIZE) {
                OutputShape shape = {.dimensions = {1}, .isSufficient = false};
                callback->notify_1_3(mErrorStatus, {shape}, kBadTiming);
            } else {
                callback->notify_1_3(mErrorStatus, {}, kBadTiming);
            }
        }).detach();
        return V1_3::ErrorStatus::NONE;
    }

    Return<void> executeSynchronously(const V1_0::Request& request, MeasureTiming measure,
                                      executeSynchronously_cb cb) override {
        CHECK(mPreparedModelV1_2 != nullptr) << "V1_2 prepared model is nullptr.";
        dummyExecution();
        if (mErrorStatus == ErrorStatus::NONE) {
            return mPreparedModelV1_2->executeSynchronously(request, measure, cb);
        } else if (mErrorStatus == ErrorStatus::OUTPUT_INSUFFICIENT_SIZE) {
            OutputShape shape = {.dimensions = {1}, .isSufficient = false};
            cb(convertToV1_0(mErrorStatus), {shape}, kBadTiming);
            return Void();
        } else {
            cb(convertToV1_0(mErrorStatus), {}, kBadTiming);
            return Void();
        }
    }

    Return<void> executeSynchronously_1_3(const V1_3::Request& request, MeasureTiming measure,
                                          const OptionalTimePoint& deadline,
                                          const OptionalTimeoutDuration& loopTimeoutDuration,
                                          executeSynchronously_1_3_cb cb) override {
        CHECK(mPreparedModelV1_3 != nullptr) << "V1_3 prepared model is nullptr.";
        dummyExecution();
        if (mErrorStatus == ErrorStatus::NONE) {
            return mPreparedModelV1_3->executeSynchronously_1_3(request, measure, deadline,
                                                                loopTimeoutDuration, cb);
        } else if (mErrorStatus == ErrorStatus::OUTPUT_INSUFFICIENT_SIZE) {
            OutputShape shape = {.dimensions = {1}, .isSufficient = false};
            cb(mErrorStatus, {shape}, kBadTiming);
            return Void();
        } else {
            cb(mErrorStatus, {}, kBadTiming);
            return Void();
        }
    }

    Return<void> configureExecutionBurst(
            const sp<V1_2::IBurstCallback>& callback,
            const MQDescriptorSync<V1_2::FmqRequestDatum>& requestChannel,
            const MQDescriptorSync<V1_2::FmqResultDatum>& resultChannel,
            configureExecutionBurst_cb cb) override {
        CHECK(mPreparedModelV1_2 != nullptr) << "V1_2 prepared model is nullptr.";
        if (mErrorStatus == ErrorStatus::NONE) {
            return mPreparedModelV1_2->configureExecutionBurst(callback, requestChannel,
                                                               resultChannel, cb);
        } else {
            cb(convertToV1_0(mErrorStatus), nullptr);
            return Void();
        }
    }

    // Note, due to the limitation of SampleDriver implementation, the call is
    // synchronous.  The test code that exercises this implementation of
    // SampleDriver is written with that in mind.  Therefore, this
    // implementation is synchronous also.  If the SampleDriver is updated to
    // return real sync fence, this must be updated.
    Return<void> executeFenced(const V1_3::Request& request, const hidl_vec<hidl_handle>& waitFor,
                               MeasureTiming measure, const OptionalTimePoint& deadline,
                               const OptionalTimeoutDuration& loopTimeoutDuration,
                               const OptionalTimeoutDuration& duration,
                               executeFenced_cb cb) override {
        CHECK(mPreparedModelV1_3 != nullptr) << "V1_3 prepared model is nullptr.";
        CHECK(mErrorStatus != ErrorStatus::OUTPUT_INSUFFICIENT_SIZE)
                << "executeFenced does not support dynamic output shape";
        dummyExecution();
        if (mErrorStatus == ErrorStatus::NONE) {
            return mPreparedModelV1_3->executeFenced(request, waitFor, measure, deadline,
                                                     loopTimeoutDuration, duration, cb);
        } else {
            // Due to the limitations of the SampleDriver, all failures look
            // like launch failures.  If the SampleDriver is updated to return
            // real sync fences, this must be updated.
            cb(mErrorStatus, hidl_handle(nullptr), nullptr);
        }
        return Void();
    }

    // We can place the TestPreparedModelLatest system in a "pause" mode where
    // no execution will complete until the system is taken out of that mode.
    // Initially, the system is not in that mode.
    static void pauseExecutions(bool v) { mPauseExecutions.store(v); }

    // This function is only guaranteed to work in the following pattern:
    // - pauseExecutions(true);
    // - // launch execution
    // - // thread A: waitForExecutionToBegin()
    // - // thread B: pauseExecutions(false);
    static void waitForExecutionToBegin() {
        CHECK(mPauseExecutions.load());
        while (mExecutionsInFlight.load()) {
        }
    }

   private:
    const sp<V1_0::IPreparedModel> mPreparedModelV1_0;
    const sp<V1_2::IPreparedModel> mPreparedModelV1_2;
    const sp<V1_3::IPreparedModel> mPreparedModelV1_3;
    ErrorStatus mErrorStatus;

    static std::atomic<bool> mPauseExecutions;
    static std::atomic<unsigned int> mExecutionsInFlight;

    static void dummyExecution() {
        CHECK_EQ(mExecutionsInFlight.fetch_add(1), 0u) << "We do not support concurrent executions";
        while (mPauseExecutions.load()) {
        }
        mExecutionsInFlight.fetch_sub(1);
    }
};
std::atomic<bool> TestPreparedModelLatest::mPauseExecutions = false;
std::atomic<unsigned int> TestPreparedModelLatest::mExecutionsInFlight = 0;

using TestPreparedModel13 = TestPreparedModelLatest;

// Like TestPreparedModelLatest, but implementing 1.2
class TestPreparedModel12 : public V1_2::IPreparedModel {
   public:
    TestPreparedModel12(sp<V1_0::IPreparedModel> preparedModel, ErrorStatus errorStatus)
        : mLatestPreparedModel(new TestPreparedModelLatest(preparedModel, errorStatus)) {}

    Return<V1_0::ErrorStatus> execute(const V1_0::Request& request,
                                      const sp<V1_0::IExecutionCallback>& callback) override {
        return mLatestPreparedModel->execute(request, callback);
    }

    Return<V1_0::ErrorStatus> execute_1_2(const V1_0::Request& request, MeasureTiming measure,
                                          const sp<V1_2::IExecutionCallback>& callback) override {
        return mLatestPreparedModel->execute_1_2(request, measure, callback);
    }

    Return<void> executeSynchronously(const V1_0::Request& request, MeasureTiming measure,
                                      executeSynchronously_cb cb) override {
        return mLatestPreparedModel->executeSynchronously(request, measure, cb);
    }

    Return<void> configureExecutionBurst(
            const sp<V1_2::IBurstCallback>& callback,
            const MQDescriptorSync<V1_2::FmqRequestDatum>& requestChannel,
            const MQDescriptorSync<V1_2::FmqResultDatum>& resultChannel,
            configureExecutionBurst_cb cb) override {
        return mLatestPreparedModel->configureExecutionBurst(callback, requestChannel,
                                                             resultChannel, cb);
    }

   private:
    const sp<IPreparedModel> mLatestPreparedModel;
};

// Like TestPreparedModelLatest, but implementing 1.0
class TestPreparedModel10 : public V1_0::IPreparedModel {
   public:
    TestPreparedModel10(sp<V1_0::IPreparedModel> preparedModel, ErrorStatus errorStatus)
        : mLatestPreparedModel(new TestPreparedModelLatest(preparedModel, errorStatus)) {}

    Return<V1_0::ErrorStatus> execute(const V1_0::Request& request,
                                      const sp<V1_0::IExecutionCallback>& callback) override {
        return mLatestPreparedModel->execute(request, callback);
    }

   private:
    const sp<IPreparedModel> mLatestPreparedModel;
};

// Behaves like SampleDriver, except that it produces wrapped IPreparedModel.
class TestDriver13 : public SampleDriver {
   public:
    // Allow dummying up the error status for execution of all models
    // prepared from this driver.  If errorStatus is NONE, then
    // execute behaves normally (and sends back the actual execution
    // status). Otherwise, don't bother to execute, and just send
    // back errorStatus (as the execution status, not the launch
    // status).
    TestDriver13(const std::string& name, ErrorStatus errorStatus)
        : SampleDriver(name.c_str()), mErrorStatus(errorStatus) {}

    Return<void> getCapabilities_1_3(getCapabilities_1_3_cb _hidl_cb) override {
        android::nn::initVLogMask();
        const PerformanceInfo kPerf = {.execTime = 0.75f, .powerUsage = 0.75f};
        Capabilities capabilities = {
                .relaxedFloat32toFloat16PerformanceScalar = kPerf,
                .relaxedFloat32toFloat16PerformanceTensor = kPerf,
                .operandPerformance =
                        nn::nonExtensionOperandPerformance<nn::HalVersion::V1_3>(kPerf),
                .ifPerformance = kPerf,
                .whilePerformance = kPerf};
        _hidl_cb(V1_3::ErrorStatus::NONE, capabilities);
        return Void();
    }

    Return<void> getSupportedOperations_1_3(const HidlModel& model,
                                            getSupportedOperations_1_3_cb cb) override {
        if (nn::validateModel(model)) {
            std::vector<bool> supported(model.main.operations.size(), true);
            cb(V1_3::ErrorStatus::NONE, supported);
        } else {
            cb(V1_3::ErrorStatus::INVALID_ARGUMENT, {});
        }
        return Void();
    }

    Return<V1_3::ErrorStatus> prepareModel_1_3(
            const HidlModel& model, ExecutionPreference preference, Priority priority,
            const OptionalTimePoint& deadline, const hidl_vec<hidl_handle>& modelCache,
            const hidl_vec<hidl_handle>& dataCache, const CacheToken& token,
            const sp<V1_3::IPreparedModelCallback>& actualCallback) override {
        sp<PreparedModelCallback> localCallback = new PreparedModelCallback;
        Return<V1_3::ErrorStatus> prepareModelReturn = SampleDriver::prepareModel_1_3(
                model, preference, priority, deadline, modelCache, dataCache, token, localCallback);
        if (!prepareModelReturn.isOkUnchecked()) {
            return prepareModelReturn;
        }
        if (prepareModelReturn != ErrorStatus::NONE) {
            actualCallback->notify_1_3(
                    localCallback->getStatus(),
                    V1_3::IPreparedModel::castFrom(localCallback->getPreparedModel()));
            return prepareModelReturn;
        }
        localCallback->wait();
        if (localCallback->getStatus() != ErrorStatus::NONE) {
            actualCallback->notify_1_3(
                    localCallback->getStatus(),
                    V1_3::IPreparedModel::castFrom(localCallback->getPreparedModel()));
        } else {
            actualCallback->notify_1_3(
                    V1_3::ErrorStatus::NONE,
                    new TestPreparedModel13(localCallback->getPreparedModel(), mErrorStatus));
        }
        return prepareModelReturn;
    }

    Return<V1_0::ErrorStatus> prepareModel_1_2(
            const V1_2::Model& model, ExecutionPreference preference,
            const hidl_vec<hidl_handle>& modelCache, const hidl_vec<hidl_handle>& dataCache,
            const CacheToken& token,
            const sp<V1_2::IPreparedModelCallback>& actualCallback) override {
        sp<PreparedModelCallback> localCallback = new PreparedModelCallback;
        Return<V1_0::ErrorStatus> prepareModelReturn = SampleDriver::prepareModel_1_2(
                model, preference, modelCache, dataCache, token, localCallback);
        if (!prepareModelReturn.isOkUnchecked()) {
            return prepareModelReturn;
        }
        if (prepareModelReturn != V1_0::ErrorStatus::NONE) {
            actualCallback->notify_1_2(
                    convertToV1_0(localCallback->getStatus()),
                    V1_2::IPreparedModel::castFrom(localCallback->getPreparedModel()));
            return prepareModelReturn;
        }
        localCallback->wait();
        if (localCallback->getStatus() != ErrorStatus::NONE) {
            actualCallback->notify_1_2(
                    convertToV1_0(localCallback->getStatus()),
                    V1_2::IPreparedModel::castFrom(localCallback->getPreparedModel()));
        } else {
            actualCallback->notify_1_2(
                    V1_0::ErrorStatus::NONE,
                    new TestPreparedModel12(localCallback->getPreparedModel(), mErrorStatus));
        }
        return prepareModelReturn;
    }

    Return<V1_0::ErrorStatus> prepareModel_1_1(
            const V1_1::Model& model, ExecutionPreference preference,
            const sp<V1_0::IPreparedModelCallback>& actualCallback) override {
        sp<PreparedModelCallback> localCallback = new PreparedModelCallback;
        Return<V1_0::ErrorStatus> prepareModelReturn =
                SampleDriver::prepareModel_1_1(model, preference, localCallback);
        if (!prepareModelReturn.isOkUnchecked()) {
            return prepareModelReturn;
        }
        if (prepareModelReturn != V1_0::ErrorStatus::NONE) {
            actualCallback->notify(convertToV1_0(localCallback->getStatus()),
                                   localCallback->getPreparedModel());
            return prepareModelReturn;
        }
        localCallback->wait();
        if (localCallback->getStatus() != ErrorStatus::NONE) {
            actualCallback->notify(convertToV1_0(localCallback->getStatus()),
                                   localCallback->getPreparedModel());
        } else {
            actualCallback->notify(
                    V1_0::ErrorStatus::NONE,
                    new TestPreparedModel10(localCallback->getPreparedModel(), mErrorStatus));
        }
        return prepareModelReturn;
    }

    Return<V1_0::ErrorStatus> prepareModel(
            const V1_0::Model& model,
            const sp<V1_0::IPreparedModelCallback>& actualCallback) override {
        return prepareModel_1_1(nn::convertToV1_1(model), ExecutionPreference::FAST_SINGLE_ANSWER,
                                actualCallback);
    }

   private:
    ErrorStatus mErrorStatus;
};

// Like TestDriver, but implementing 1.2
class TestDriver12 : public V1_2::IDevice {
   public:
    TestDriver12(const std::string& name, ErrorStatus errorStatus)
        : mLatestDriver(new TestDriver13(name, errorStatus)) {}
    Return<void> getCapabilities_1_2(getCapabilities_1_2_cb _hidl_cb) override {
        return mLatestDriver->getCapabilities_1_2(_hidl_cb);
    }
    Return<void> getCapabilities_1_1(getCapabilities_1_1_cb _hidl_cb) override {
        return mLatestDriver->getCapabilities_1_1(_hidl_cb);
    }
    Return<void> getCapabilities(getCapabilities_cb _hidl_cb) override {
        return mLatestDriver->getCapabilities(_hidl_cb);
    }
    Return<void> getSupportedOperations_1_2(const V1_2::Model& model,
                                            getSupportedOperations_1_2_cb _hidl_cb) override {
        return mLatestDriver->getSupportedOperations_1_2(model, _hidl_cb);
    }
    Return<void> getSupportedOperations_1_1(const V1_1::Model& model,
                                            getSupportedOperations_1_1_cb _hidl_cb) override {
        return mLatestDriver->getSupportedOperations_1_1(model, _hidl_cb);
    }
    Return<void> getSupportedOperations(const V1_0::Model& model,
                                        getSupportedOperations_cb _hidl_cb) override {
        return mLatestDriver->getSupportedOperations(model, _hidl_cb);
    }
    Return<V1_0::ErrorStatus> prepareModel_1_2(
            const V1_2::Model& model, ExecutionPreference preference,
            const hidl_vec<hidl_handle>& modelCache, const hidl_vec<hidl_handle>& dataCache,
            const CacheToken& token,
            const sp<V1_2::IPreparedModelCallback>& actualCallback) override {
        return mLatestDriver->prepareModel_1_2(model, preference, modelCache, dataCache, token,
                                               actualCallback);
    }
    Return<V1_0::ErrorStatus> prepareModel_1_1(
            const V1_1::Model& model, ExecutionPreference preference,
            const sp<V1_0::IPreparedModelCallback>& actualCallback) override {
        return mLatestDriver->prepareModel_1_1(model, preference, actualCallback);
    }
    Return<V1_0::ErrorStatus> prepareModel(
            const V1_0::Model& model,
            const sp<V1_0::IPreparedModelCallback>& actualCallback) override {
        return mLatestDriver->prepareModel(model, actualCallback);
    }
    Return<DeviceStatus> getStatus() override { return mLatestDriver->getStatus(); }
    Return<void> getVersionString(getVersionString_cb _hidl_cb) override {
        return mLatestDriver->getVersionString(_hidl_cb);
    }
    Return<void> getType(getType_cb _hidl_cb) override { return mLatestDriver->getType(_hidl_cb); }
    Return<void> getSupportedExtensions(getSupportedExtensions_cb _hidl_cb) {
        return mLatestDriver->getSupportedExtensions(_hidl_cb);
    }
    Return<void> getNumberOfCacheFilesNeeded(getNumberOfCacheFilesNeeded_cb _hidl_cb) {
        return mLatestDriver->getNumberOfCacheFilesNeeded(_hidl_cb);
    }
    Return<V1_0::ErrorStatus> prepareModelFromCache(
            const hidl_vec<hidl_handle>& modelCache, const hidl_vec<hidl_handle>& dataCache,
            const CacheToken& token, const sp<V1_2::IPreparedModelCallback>& callback) {
        return mLatestDriver->prepareModelFromCache(modelCache, dataCache, token, callback);
    }

   private:
    const sp<V1_3::IDevice> mLatestDriver;
};

// Like TestDriver, but implementing 1.1
class TestDriver11 : public V1_1::IDevice {
   public:
    TestDriver11(const std::string& name, ErrorStatus errorStatus)
        : mLatestDriver(new TestDriver13(name, errorStatus)) {}
    Return<void> getCapabilities_1_1(getCapabilities_1_1_cb _hidl_cb) override {
        return mLatestDriver->getCapabilities_1_1(_hidl_cb);
    }
    Return<void> getSupportedOperations_1_1(const V1_1::Model& model,
                                            getSupportedOperations_1_1_cb _hidl_cb) override {
        return mLatestDriver->getSupportedOperations_1_1(model, _hidl_cb);
    }
    Return<V1_0::ErrorStatus> prepareModel_1_1(
            const V1_1::Model& model, ExecutionPreference preference,
            const sp<V1_0::IPreparedModelCallback>& actualCallback) override {
        return mLatestDriver->prepareModel_1_1(model, preference, actualCallback);
    }
    Return<DeviceStatus> getStatus() override { return mLatestDriver->getStatus(); }
    Return<void> getCapabilities(getCapabilities_cb _hidl_cb) override {
        return mLatestDriver->getCapabilities(_hidl_cb);
    }
    Return<void> getSupportedOperations(const V1_0::Model& model,
                                        getSupportedOperations_cb _hidl_cb) override {
        return mLatestDriver->getSupportedOperations(model, _hidl_cb);
    }
    Return<V1_0::ErrorStatus> prepareModel(
            const V1_0::Model& model,
            const sp<V1_0::IPreparedModelCallback>& actualCallback) override {
        return mLatestDriver->prepareModel(model, actualCallback);
    }

   private:
    const sp<V1_3::IDevice> mLatestDriver;
};

// Like TestDriver, but implementing 1.0
class TestDriver10 : public V1_0::IDevice {
   public:
    TestDriver10(const std::string& name, ErrorStatus errorStatus)
        : mLatestDriver(new TestDriver13(name, errorStatus)) {}
    Return<void> getCapabilities(getCapabilities_cb _hidl_cb) override {
        return mLatestDriver->getCapabilities(_hidl_cb);
    }
    Return<void> getSupportedOperations(const V1_0::Model& model,
                                        getSupportedOperations_cb _hidl_cb) override {
        return mLatestDriver->getSupportedOperations(model, _hidl_cb);
    }
    Return<V1_0::ErrorStatus> prepareModel(
            const V1_0::Model& model,
            const sp<V1_0::IPreparedModelCallback>& actualCallback) override {
        return mLatestDriver->prepareModel(model, actualCallback);
    }
    Return<DeviceStatus> getStatus() override { return mLatestDriver->getStatus(); }

   private:
    const sp<V1_3::IDevice> mLatestDriver;
};

// This class adds some simple utilities on top of WrapperCompilation in order
// to provide access to certain features from CompilationBuilder that are not
// exposed by the base class.
template <typename DriverClass>
class TestCompilation : public WrapperCompilation {
   public:
    // Allow dummying up the error status for all executions from this
    // compilation.  If errorStatus is NONE, then execute behaves
    // normally (and sends back the actual execution status).
    // Otherwise, don't bother to execute, and just send back
    // errorStatus (as the execution status, not the launch status).
    TestCompilation(const WrapperModel* model, const std::string& deviceName,
                    ErrorStatus errorStatus) {
        std::vector<std::shared_ptr<Device>> devices;
        auto device = DeviceManager::forTest_makeDriverDevice(
                deviceName, new DriverClass(deviceName, errorStatus));
        devices.push_back(device);

        nn::ModelBuilder* m = reinterpret_cast<nn::ModelBuilder*>(model->getHandle());
        CompilationBuilder* c = nullptr;
        int result = m->createCompilation(&c, devices);
        EXPECT_EQ(result, 0);
        // We need to ensure that we use our TestDriver and do not
        // fall back to CPU.  (If we allow CPU fallback, then when our
        // TestDriver reports an execution failure, we'll re-execute
        // on CPU, and will not see the failure.)
        c->setPartitioning(DeviceManager::kPartitioningWithoutFallback);
        mCompilation = reinterpret_cast<ANeuralNetworksCompilation*>(c);
    }
};

// This class has roughly the same functionality as TestCompilation class.
// The major difference is that Introspection API is used to select the device.
class TestIntrospectionCompilation : public WrapperCompilation {
   public:
    TestIntrospectionCompilation(const WrapperModel* model, const std::string& deviceName) {
        std::vector<ANeuralNetworksDevice*> mDevices;
        uint32_t numDevices = 0;
        EXPECT_EQ(ANeuralNetworks_getDeviceCount(&numDevices), ANEURALNETWORKS_NO_ERROR);
        EXPECT_GE(numDevices, (uint32_t)1);

        for (uint32_t i = 0; i < numDevices; i++) {
            ANeuralNetworksDevice* device = nullptr;
            EXPECT_EQ(ANeuralNetworks_getDevice(i, &device), ANEURALNETWORKS_NO_ERROR);
            const char* buffer = nullptr;
            int result = ANeuralNetworksDevice_getName(device, &buffer);
            if (result == ANEURALNETWORKS_NO_ERROR && deviceName.compare(buffer) == 0) {
                mDevices.push_back(device);
            }
        }
        // In CPU only mode, DeviceManager::getDrivers() will not be able to
        // provide the actual device list. We will not be able to find the test
        // driver with specified deviceName.
        if (!DeviceManager::get()->getUseCpuOnly()) {
            EXPECT_EQ(mDevices.size(), (uint32_t)1);

            int result = ANeuralNetworksCompilation_createForDevices(
                    model->getHandle(), mDevices.data(), mDevices.size(), &mCompilation);
            EXPECT_EQ(result, ANEURALNETWORKS_NO_ERROR);
        }
    }
};

template <class DriverClass>
class ExecutionTestTemplate
    : public ::testing::TestWithParam<std::tuple<ErrorStatus, Result, bool>> {
   public:
    ExecutionTestTemplate()
        : kName(toString(std::get<0>(GetParam()))),
          kForceErrorStatus(std::get<0>(GetParam())),
          kExpectResult(std::get<1>(GetParam())),
          kUseIntrospectionAPI(std::get<2>(GetParam())),
          mModel(makeModel()) {
        if (kUseIntrospectionAPI) {
            DeviceManager::get()->forTest_registerDevice(kName.c_str(),
                                                         new DriverClass(kName, kForceErrorStatus));
            mCompilation = TestIntrospectionCompilation(&mModel, kName);
        } else {
            mCompilation = TestCompilation<DriverClass>(&mModel, kName, kForceErrorStatus);
        }
    }

   protected:
    // Unit test method
    void TestWait();

    virtual void TearDown() {
        // Reinitialize the device list since Introspection API path altered it.
        if (kUseIntrospectionAPI) {
            DeviceManager::get()->forTest_reInitializeDeviceList();
        }
    }

    const std::string kName;

    // Allow dummying up the error status for execution.  If
    // kForceErrorStatus is NONE, then execution behaves normally (and
    // sends back the actual execution status).  Otherwise, don't
    // bother to execute, and just send back kForceErrorStatus (as the
    // execution status, not the launch status).
    const ErrorStatus kForceErrorStatus;

    // What result do we expect from the execution?  (The Result
    // equivalent of kForceErrorStatus.)
    const Result kExpectResult;

    // Whether mCompilation is created via Introspection API or not.
    const bool kUseIntrospectionAPI;

    WrapperModel mModel;
    WrapperCompilation mCompilation;

    void setInputOutput(WrapperExecution* execution) {
        mInputBuffer = kInputBuffer;
        mOutputBuffer = kOutputBufferInitial;
        ASSERT_EQ(execution->setInput(0, &mInputBuffer, sizeof(mInputBuffer)), Result::NO_ERROR);
        ASSERT_EQ(execution->setOutput(0, &mOutputBuffer, sizeof(mOutputBuffer)), Result::NO_ERROR);
    }

    const float kInputBuffer = 3.14;
    const float kOutputBufferInitial = 0;
    float mInputBuffer;
    float mOutputBuffer;
    const float kOutputBufferExpected = 3;
    const std::vector<uint32_t> kOutputDimensionsExpected = {1};

   private:
    static WrapperModel makeModel() {
        static const WrapperOperandType tensorType(WrapperType::TENSOR_FLOAT32, {1});

        WrapperModel model;
        uint32_t input = model.addOperand(&tensorType);
        uint32_t output = model.addOperand(&tensorType);
        model.addOperation(ANEURALNETWORKS_FLOOR, {input}, {output});
        model.identifyInputsAndOutputs({input}, {output});
        assert(model.finish() == Result::NO_ERROR);

        return model;
    }
};

template <class DriverClass>
void ExecutionTestTemplate<DriverClass>::TestWait() {
    SCOPED_TRACE(kName);
    // Skip Introspection API tests when CPU only flag is forced on.
    if (kUseIntrospectionAPI && DeviceManager::get()->getUseCpuOnly()) {
        GTEST_SKIP();
    }

    ASSERT_EQ(mCompilation.finish(), Result::NO_ERROR);

    const auto getDimensionsWhileRunning = [](WrapperExecution& execution) {
        TestPreparedModelLatest::waitForExecutionToBegin();
        // Cannot query dimensions while execution is running
        std::vector<uint32_t> dimensions;
        EXPECT_EQ(execution.getOutputOperandDimensions(0, &dimensions), Result::BAD_STATE);
    };

    {
        SCOPED_TRACE("startCompute");
        WrapperExecution execution(&mCompilation);
        ASSERT_NO_FATAL_FAILURE(setInputOutput(&execution));
        TestPreparedModelLatest::pauseExecutions(true);
        WrapperEvent event;
        ASSERT_EQ(execution.startCompute(&event), Result::NO_ERROR);
        getDimensionsWhileRunning(execution);
        TestPreparedModelLatest::pauseExecutions(false);
        ASSERT_EQ(event.wait(), kExpectResult);
        if (kExpectResult == Result::NO_ERROR) {
            ASSERT_EQ(mOutputBuffer, kOutputBufferExpected);
        }
        std::vector<uint32_t> dimensions;
        if (kExpectResult == Result::NO_ERROR ||
            kExpectResult == Result::OUTPUT_INSUFFICIENT_SIZE) {
            // Only one output operand, hardcoded as index 0.
            ASSERT_EQ(execution.getOutputOperandDimensions(0, &dimensions), kExpectResult);
            ASSERT_EQ(dimensions, kOutputDimensionsExpected);
        } else {
            ASSERT_EQ(execution.getOutputOperandDimensions(0, &dimensions), Result::BAD_STATE);
        }
    }
    {
        SCOPED_TRACE("compute");
        WrapperExecution execution(&mCompilation);
        ASSERT_NO_FATAL_FAILURE(setInputOutput(&execution));
        TestPreparedModelLatest::pauseExecutions(true);
        std::thread run([this, &execution] { EXPECT_EQ(execution.compute(), kExpectResult); });
        getDimensionsWhileRunning(execution);
        TestPreparedModelLatest::pauseExecutions(false);
        run.join();
        if (kExpectResult == Result::NO_ERROR) {
            ASSERT_EQ(mOutputBuffer, kOutputBufferExpected);
        }
        std::vector<uint32_t> dimensions;
        if (kExpectResult == Result::NO_ERROR ||
            kExpectResult == Result::OUTPUT_INSUFFICIENT_SIZE) {
            // Only one output operand, hardcoded as index 0.
            ASSERT_EQ(execution.getOutputOperandDimensions(0, &dimensions), kExpectResult);
            ASSERT_EQ(dimensions, kOutputDimensionsExpected);
        } else {
            ASSERT_EQ(execution.getOutputOperandDimensions(0, &dimensions), Result::BAD_STATE);
        }
    }
    {
        SCOPED_TRACE("burstCompute");

        // TODO: If a burst API is added to nn::test_wrapper (e.g.,
        // Execution::burstCompute()), then use that, rather than using
        // Execution::setComputeMode() to make Execution::compute() use burst
        // functionality.

        auto oldComputeMode =
                WrapperExecution::setComputeMode(WrapperExecution::ComputeMode::BURST);
        base::ScopeGuard restore(
                [oldComputeMode] { WrapperExecution::setComputeMode(oldComputeMode); });

        WrapperExecution execution(&mCompilation);
        ASSERT_NO_FATAL_FAILURE(setInputOutput(&execution));
        TestPreparedModelLatest::pauseExecutions(true);
        std::thread run([this, &execution] { EXPECT_EQ(execution.compute(), kExpectResult); });
        getDimensionsWhileRunning(execution);
        TestPreparedModelLatest::pauseExecutions(false);
        run.join();
        if (kExpectResult == Result::NO_ERROR) {
            ASSERT_EQ(mOutputBuffer, kOutputBufferExpected);
        }
        std::vector<uint32_t> dimensions;
        if (kExpectResult == Result::NO_ERROR ||
            kExpectResult == Result::OUTPUT_INSUFFICIENT_SIZE) {
            // Only one output operand, hardcoded as index 0.
            ASSERT_EQ(execution.getOutputOperandDimensions(0, &dimensions), kExpectResult);
            ASSERT_EQ(dimensions, kOutputDimensionsExpected);
        } else {
            ASSERT_EQ(execution.getOutputOperandDimensions(0, &dimensions), Result::BAD_STATE);
        }
    }
    if (kExpectResult != Result::OUTPUT_INSUFFICIENT_SIZE) {
        // computeWithDependencies doesn't support OUTPUT_INSUFFICIENT_SIZE
        SCOPED_TRACE("computeWithDependencies");
        WrapperExecution execution(&mCompilation);
        ASSERT_NO_FATAL_FAILURE(setInputOutput(&execution));
        TestPreparedModelLatest::pauseExecutions(true);

        WrapperEvent event;
        // Note, due to the limitation of SampleDriver implementation, the call is synchronous.
        // If the SampleDriver is updated to return real sync fence, this must be updated.
        std::thread run([this, &execution, &event] {
            EXPECT_EQ(execution.startComputeWithDependencies({}, 0, &event), kExpectResult);
        });
        getDimensionsWhileRunning(execution);
        TestPreparedModelLatest::pauseExecutions(false);
        run.join();
        if (kExpectResult == Result::NO_ERROR) {
            ASSERT_EQ(event.wait(), kExpectResult);
            ASSERT_EQ(mOutputBuffer, kOutputBufferExpected);
        } else {
            ASSERT_EQ(event.wait(), Result::UNEXPECTED_NULL);
        }
        std::vector<uint32_t> dimensions;
        if (kExpectResult == Result::NO_ERROR) {
            // Only one output operand, hardcoded as index 0.
            ASSERT_EQ(execution.getOutputOperandDimensions(0, &dimensions), kExpectResult);
            ASSERT_EQ(dimensions, kOutputDimensionsExpected);
        } else {
            ASSERT_EQ(execution.getOutputOperandDimensions(0, &dimensions), Result::BAD_STATE);
        }
    }
}

auto kTestValues = ::testing::Values(
        std::make_tuple(ErrorStatus::NONE, Result::NO_ERROR, /* kUseIntrospectionAPI */ false),
        std::make_tuple(ErrorStatus::DEVICE_UNAVAILABLE, Result::UNAVAILABLE_DEVICE,
                        /* kUseIntrospectionAPI */ false),
        std::make_tuple(ErrorStatus::GENERAL_FAILURE, Result::OP_FAILED,
                        /* kUseIntrospectionAPI */ false),
        std::make_tuple(ErrorStatus::OUTPUT_INSUFFICIENT_SIZE, Result::OUTPUT_INSUFFICIENT_SIZE,
                        /* kUseIntrospectionAPI */ false),
        std::make_tuple(ErrorStatus::INVALID_ARGUMENT, Result::BAD_DATA,
                        /* kUseIntrospectionAPI */ false));

class ExecutionTest13 : public ExecutionTestTemplate<TestDriver13> {};
TEST_P(ExecutionTest13, Wait) {
    TestWait();
}
INSTANTIATE_TEST_CASE_P(Flavor, ExecutionTest13, kTestValues);

class ExecutionTest12 : public ExecutionTestTemplate<TestDriver12> {};
TEST_P(ExecutionTest12, Wait) {
    TestWait();
}
INSTANTIATE_TEST_CASE_P(Flavor, ExecutionTest12, kTestValues);

class ExecutionTest11 : public ExecutionTestTemplate<TestDriver11> {};
TEST_P(ExecutionTest11, Wait) {
    if (kForceErrorStatus == ErrorStatus::OUTPUT_INSUFFICIENT_SIZE) return;
    TestWait();
}
INSTANTIATE_TEST_CASE_P(Flavor, ExecutionTest11, kTestValues);

class ExecutionTest10 : public ExecutionTestTemplate<TestDriver10> {};
TEST_P(ExecutionTest10, Wait) {
    if (kForceErrorStatus == ErrorStatus::OUTPUT_INSUFFICIENT_SIZE) return;
    TestWait();
}
INSTANTIATE_TEST_CASE_P(Flavor, ExecutionTest10, kTestValues);

auto kIntrospectionTestValues = ::testing::Values(
        std::make_tuple(ErrorStatus::NONE, Result::NO_ERROR, /* kUseIntrospectionAPI */ true),
        std::make_tuple(ErrorStatus::DEVICE_UNAVAILABLE, Result::UNAVAILABLE_DEVICE,
                        /* kUseIntrospectionAPI */ true),
        std::make_tuple(ErrorStatus::GENERAL_FAILURE, Result::OP_FAILED,
                        /* kUseIntrospectionAPI */ true),
        std::make_tuple(ErrorStatus::OUTPUT_INSUFFICIENT_SIZE, Result::OUTPUT_INSUFFICIENT_SIZE,
                        /* kUseIntrospectionAPI */ true),
        std::make_tuple(ErrorStatus::INVALID_ARGUMENT, Result::BAD_DATA,
                        /* kUseIntrospectionAPI */ true));

INSTANTIATE_TEST_CASE_P(IntrospectionFlavor, ExecutionTest13, kIntrospectionTestValues);

}  // namespace
}  // namespace android
