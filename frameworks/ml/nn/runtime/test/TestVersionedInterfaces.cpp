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

#include <android-base/logging.h>
#include <android/hardware/neuralnetworks/1.0/ADevice.h>
#include <android/hardware/neuralnetworks/1.1/ADevice.h>
#include <android/hardware/neuralnetworks/1.2/ADevice.h>
#include <android/hardware/neuralnetworks/1.3/ADevice.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <hidl/Status.h>
#include <utils/Errors.h>

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "HalInterfaces.h"
#include "MemoryUtils.h"
#include "MetaModel.h"
#include "VersionedInterfaces.h"

namespace android::nn {
namespace {

using namespace hal;
using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::MockFunction;
using MockDeviceFactory = MockFunction<sp<V1_0::IDevice>(bool blocking)>;

constexpr uint32_t kNoCacheFilesNeeded = 0;
constexpr uint32_t kMaxNumberOfCacheFiles =
        static_cast<uint32_t>(Constant::MAX_NUMBER_OF_CACHE_FILES);
constexpr Timing kNoTiming = {.timeOnDevice = std::numeric_limits<uint64_t>::max(),
                              .timeInDriver = std::numeric_limits<uint64_t>::max()};

template <typename... Args>
auto makeCallbackReturn(Args&&... args) {
    return [argPack = std::make_tuple(std::forward<Args>(args)...)](const auto& cb) {
        std::apply(cb, argPack);
        return Void();
    };
};

class MockDevice : public IDevice {
   public:
    static sp<MockDevice> create() {
        const sp<MockDevice> mockDevice = new MockDevice();

        const auto linkToDeathRet_ret = []() -> Return<bool> { return true; };
        const auto getCapabilities_ret =
                makeCallbackReturn(V1_0::ErrorStatus::NONE, V1_0::Capabilities{});
        const auto getCapabilities_1_1_ret =
                makeCallbackReturn(V1_0::ErrorStatus::NONE, V1_1::Capabilities{});
        const auto getVersionString_ret =
                makeCallbackReturn(V1_0::ErrorStatus::NONE, "Google-MockV1");
        const auto getType_ret = makeCallbackReturn(V1_0::ErrorStatus::NONE, DeviceType::OTHER);
        const auto getCapabilities_1_2_ret =
                makeCallbackReturn(V1_0::ErrorStatus::NONE, V1_2::Capabilities{});
        const auto getSupportedExtensions_ret =
                makeCallbackReturn(V1_0::ErrorStatus::NONE, hidl_vec<Extension>{});
        const auto getNumberOfCacheFilesNeeded_ret = makeCallbackReturn(
                V1_0::ErrorStatus::NONE, kMaxNumberOfCacheFiles, kMaxNumberOfCacheFiles);
        const auto getCapabilities_1_3_ret =
                makeCallbackReturn(V1_3::ErrorStatus::NONE, V1_3::Capabilities{});

        ON_CALL(*mockDevice, linkToDeathRet()).WillByDefault(Invoke(linkToDeathRet_ret));
        ON_CALL(*mockDevice, getCapabilities(_)).WillByDefault(Invoke(getCapabilities_ret));
        ON_CALL(*mockDevice, getCapabilities_1_1(_)).WillByDefault(Invoke(getCapabilities_1_1_ret));
        ON_CALL(*mockDevice, getVersionString(_)).WillByDefault(Invoke(getVersionString_ret));
        ON_CALL(*mockDevice, getType(_)).WillByDefault(Invoke(getType_ret));
        ON_CALL(*mockDevice, getCapabilities_1_2(_)).WillByDefault(Invoke(getCapabilities_1_2_ret));
        ON_CALL(*mockDevice, getSupportedExtensions(_))
                .WillByDefault(Invoke(getSupportedExtensions_ret));
        ON_CALL(*mockDevice, getNumberOfCacheFilesNeeded(_))
                .WillByDefault(Invoke(getNumberOfCacheFilesNeeded_ret));
        ON_CALL(*mockDevice, getCapabilities_1_3(_)).WillByDefault(Invoke(getCapabilities_1_3_ret));

        // These EXPECT_CALL(...).Times(testing::AnyNumber()) calls are to
        // suppress warnings on the uninteresting methods calls.
        EXPECT_CALL(*mockDevice, linkToDeathRet()).Times(testing::AnyNumber());
        EXPECT_CALL(*mockDevice, getCapabilities(_)).Times(testing::AnyNumber());
        EXPECT_CALL(*mockDevice, getCapabilities_1_1(_)).Times(testing::AnyNumber());
        EXPECT_CALL(*mockDevice, getVersionString(_)).Times(testing::AnyNumber());
        EXPECT_CALL(*mockDevice, getType(_)).Times(testing::AnyNumber());
        EXPECT_CALL(*mockDevice, getCapabilities_1_2(_)).Times(testing::AnyNumber());
        EXPECT_CALL(*mockDevice, getSupportedExtensions(_)).Times(testing::AnyNumber());
        EXPECT_CALL(*mockDevice, getNumberOfCacheFilesNeeded(_)).Times(testing::AnyNumber());
        EXPECT_CALL(*mockDevice, getCapabilities_1_3(_)).Times(testing::AnyNumber());

        return mockDevice;
    }

    // IBase methods below.
    Return<bool> linkToDeath(const sp<hidl_death_recipient>& recipient,
                             uint64_t /*cookie*/) override {
        mDeathRecipient = recipient;
        return linkToDeathRet();
    }
    MOCK_METHOD(Return<void>, ping, (), (override));

    // V1_0 methods below.
    MOCK_METHOD(Return<void>, getCapabilities, (getCapabilities_cb cb), (override));
    MOCK_METHOD(Return<void>, getSupportedOperations,
                (const V1_0::Model& model, getSupportedOperations_cb cb), (override));
    MOCK_METHOD(Return<V1_0::ErrorStatus>, prepareModel,
                (const V1_0::Model& model, const sp<V1_0::IPreparedModelCallback>& callback),
                (override));
    MOCK_METHOD(Return<DeviceStatus>, getStatus, (), (override));

    // V1_1 methods below.
    MOCK_METHOD(Return<void>, getCapabilities_1_1, (getCapabilities_1_1_cb cb), (override));
    MOCK_METHOD(Return<void>, getSupportedOperations_1_1,
                (const V1_1::Model& model, getSupportedOperations_1_1_cb cb), (override));
    MOCK_METHOD(Return<V1_0::ErrorStatus>, prepareModel_1_1,
                (const V1_1::Model& model, ExecutionPreference preference,
                 const sp<V1_0::IPreparedModelCallback>& callback),
                (override));

    // V1_2 methods below.
    MOCK_METHOD(Return<void>, getVersionString, (getVersionString_cb cb), (override));
    MOCK_METHOD(Return<void>, getType, (getType_cb cb), (override));
    MOCK_METHOD(Return<void>, getCapabilities_1_2, (getCapabilities_1_2_cb cb), (override));
    MOCK_METHOD(Return<void>, getSupportedExtensions, (getSupportedExtensions_cb cb), (override));
    MOCK_METHOD(Return<void>, getSupportedOperations_1_2,
                (const V1_2::Model& model, getSupportedOperations_1_2_cb cb), (override));
    MOCK_METHOD(Return<void>, getNumberOfCacheFilesNeeded, (getNumberOfCacheFilesNeeded_cb cb),
                (override));
    MOCK_METHOD(Return<V1_0::ErrorStatus>, prepareModel_1_2,
                (const V1_2::Model& model, ExecutionPreference preference,
                 const hidl_vec<hidl_handle>& modelCache, const hidl_vec<hidl_handle>& dataCache,
                 const CacheToken& token, const sp<V1_2::IPreparedModelCallback>& callback),
                (override));
    MOCK_METHOD(Return<V1_0::ErrorStatus>, prepareModelFromCache,
                (const hidl_vec<hidl_handle>& modelCache, const hidl_vec<hidl_handle>& dataCache,
                 const CacheToken& token, const sp<V1_2::IPreparedModelCallback>& callback),
                (override));

    // V1_3 methods below.
    MOCK_METHOD(Return<void>, getCapabilities_1_3, (getCapabilities_1_3_cb cb), (override));
    MOCK_METHOD(Return<void>, getSupportedOperations_1_3,
                (const V1_3::Model& model, getSupportedOperations_1_3_cb cb), (override));
    MOCK_METHOD(Return<V1_3::ErrorStatus>, prepareModel_1_3,
                (const V1_3::Model& model, ExecutionPreference preference, Priority priority,
                 const OptionalTimePoint& deadline, const hidl_vec<hidl_handle>& modelCache,
                 const hidl_vec<hidl_handle>& dataCache, const CacheToken& token,
                 const sp<V1_3::IPreparedModelCallback>& callback),
                (override));
    MOCK_METHOD(Return<V1_3::ErrorStatus>, prepareModelFromCache_1_3,
                (const OptionalTimePoint& deadline, const hidl_vec<hidl_handle>& modelCache,
                 const hidl_vec<hidl_handle>& dataCache, const CacheToken& token,
                 const sp<V1_3::IPreparedModelCallback>& callback),
                (override));
    MOCK_METHOD(Return<void>, allocate,
                (const BufferDesc& desc, const hidl_vec<sp<V1_3::IPreparedModel>>& preparedModels,
                 const hidl_vec<BufferRole>& inputRoles, const hidl_vec<BufferRole>& outputRoles,
                 allocate_cb cb),
                (override));

    // Helper methods.
    MOCK_METHOD(Return<bool>, linkToDeathRet, ());
    void simulateCrash() {
        ASSERT_NE(nullptr, mDeathRecipient.get());

        // Currently, the VersionedInterfaces code will not use the `cookie` or
        // `who` arguments, so we pass in 0 and nullptr for these arguments
        // instead. Normally, they are used by the hidl_death_recipient to
        // determine which object is dead. However, the VersionedInterfaces
        // code only pairs a single death recipient with a single HIDL
        // interface object, so these arguments are redundant.
        mDeathRecipient->serviceDied(0, nullptr);
    }

   private:
    // Members.
    sp<hidl_death_recipient> mDeathRecipient;
};

class MockPreparedModel : public IPreparedModel {
   public:
    static sp<MockPreparedModel> create() {
        const sp<MockPreparedModel> mockPreparedModel = new MockPreparedModel();

        const auto linkToDeathRet_ret = []() -> Return<bool> { return true; };
        ON_CALL(*mockPreparedModel, linkToDeathRet()).WillByDefault(Invoke(linkToDeathRet_ret));

        // This EXPECT_CALL(...).Times(testing::AnyNumber()) calls are to
        // suppress warnings on the uninteresting methods calls.
        EXPECT_CALL(*mockPreparedModel, linkToDeathRet()).Times(testing::AnyNumber());

        return mockPreparedModel;
    }

    // IBase methods below.
    Return<bool> linkToDeath(const sp<hidl_death_recipient>& recipient,
                             uint64_t /*cookie*/) override {
        mDeathRecipient = recipient;
        return linkToDeathRet();
    }
    MOCK_METHOD(Return<void>, ping, (), (override));

    // V1_0 methods below.
    MOCK_METHOD(Return<V1_0::ErrorStatus>, execute,
                (const V1_0::Request& request, const sp<V1_0::IExecutionCallback>& callback),
                (override));

    // V1_2 methods below.
    MOCK_METHOD(Return<V1_0::ErrorStatus>, execute_1_2,
                (const V1_0::Request& request, MeasureTiming measure,
                 const sp<V1_2::IExecutionCallback>& callback),
                (override));
    MOCK_METHOD(Return<void>, executeSynchronously,
                (const V1_0::Request& request, MeasureTiming measure, executeSynchronously_cb cb),
                (override));
    MOCK_METHOD(Return<void>, configureExecutionBurst,
                (const sp<V1_2::IBurstCallback>& callback,
                 const hardware::MQDescriptorSync<V1_2::FmqRequestDatum>& requestChannel,
                 const hardware::MQDescriptorSync<V1_2::FmqResultDatum>& resultChannel,
                 configureExecutionBurst_cb cb),
                (override));

    // V1_3 methods below.
    MOCK_METHOD(Return<ErrorStatus>, execute_1_3,
                (const V1_3::Request& request, MeasureTiming measure,
                 const OptionalTimePoint& deadline,
                 const OptionalTimeoutDuration& loopTimeoutDuration,
                 const sp<IExecutionCallback>& callback),
                (override));
    MOCK_METHOD(Return<void>, executeSynchronously_1_3,
                (const V1_3::Request& request, MeasureTiming measure,
                 const OptionalTimePoint& deadline,
                 const OptionalTimeoutDuration& loopTimeoutDuration,
                 executeSynchronously_1_3_cb cb),
                (override));
    MOCK_METHOD(Return<void>, executeFenced,
                (const V1_3::Request& request, const hidl_vec<hidl_handle>& waitFor,
                 MeasureTiming measure, const OptionalTimePoint& deadline,
                 const OptionalTimeoutDuration& loopTimeoutDuration,
                 const OptionalTimeoutDuration& duration, executeFenced_cb cb),
                (override));

    // Helper methods.
    MOCK_METHOD(Return<bool>, linkToDeathRet, ());
    void simulateCrash() {
        ASSERT_NE(nullptr, mDeathRecipient.get());

        // Currently, the VersionedInterfaces code will not use the `cookie` or
        // `who` arguments, so we pass in 0 and nullptr for these arguments
        // instead. Normally, they are used by the hidl_death_recipient to
        // determine which object is dead. However, the VersionedInterfaces
        // code only pairs a single death recipient with a single HIDL
        // interface object, so these arguments are redundant.
        mDeathRecipient->serviceDied(0, nullptr);
    }

   private:
    // Members.
    sp<hidl_death_recipient> mDeathRecipient;
};

class MockBurstContext : public V1_2::IBurstContext {
   public:
    // V1_2 methods below.
    MOCK_METHOD(Return<void>, freeMemory, (int32_t slot), (override));
};

class MockFencedExecutionCallback : public IFencedExecutionCallback {
   public:
    // V1_3 methods below.
    MOCK_METHOD(Return<void>, getExecutionInfo, (getExecutionInfo_cb cb), (override));
};

class MockBuffer : public IBuffer {
   public:
    // V1_3 methods below.
    MOCK_METHOD(Return<ErrorStatus>, copyTo, (const hidl_memory& dst), (override));
    MOCK_METHOD(Return<ErrorStatus>, copyFrom,
                (const hidl_memory& src, const hidl_vec<uint32_t>& dimensions), (override));
};

enum class Version { V1_0, V1_1, V1_2, V1_3, MOCK };

sp<V1_0::IDevice> adaptAs(const sp<MockDevice>& mockDevice, Version version) {
    switch (version) {
        case Version::V1_0:
            return new V1_0::ADevice(mockDevice);
        case Version::V1_1:
            return new V1_1::ADevice(mockDevice);
        case Version::V1_2:
            return new V1_2::ADevice(mockDevice);
        case Version::V1_3:
            return new V1_3::ADevice(mockDevice);
        case Version::MOCK:
            return mockDevice;
    }
    LOG(FATAL) << "unrecognized version: " << static_cast<int>(version);
    return nullptr;
}

auto makePreparedModelReturn(V1_0::ErrorStatus launchStatus, V1_0::ErrorStatus returnStatus,
                             const sp<MockPreparedModel>& preparedModel) {
    return [launchStatus, returnStatus, preparedModel](
                   const V1_0::Model& /*model*/,
                   const sp<V1_0::IPreparedModelCallback>& cb) -> Return<V1_0::ErrorStatus> {
        cb->notify(returnStatus, preparedModel).isOk();
        return launchStatus;
    };
}
auto makePreparedModel_1_1Return(V1_0::ErrorStatus launchStatus, V1_0::ErrorStatus returnStatus,
                                 const sp<MockPreparedModel>& preparedModel) {
    return [launchStatus, returnStatus, preparedModel](
                   const V1_1::Model& /*model*/, ExecutionPreference /*preference*/,
                   const sp<V1_0::IPreparedModelCallback>& cb) -> Return<V1_0::ErrorStatus> {
        cb->notify(returnStatus, preparedModel).isOk();
        return launchStatus;
    };
}
auto makePreparedModel_1_2Return(V1_0::ErrorStatus launchStatus, V1_0::ErrorStatus returnStatus,
                                 const sp<MockPreparedModel>& preparedModel) {
    return [launchStatus, returnStatus, preparedModel](
                   const V1_2::Model& /*model*/, ExecutionPreference /*preference*/,
                   const auto& /*modelCache*/, const auto& /*dataCache*/, const auto& /*token*/,
                   const sp<V1_2::IPreparedModelCallback>& cb) -> Return<V1_0::ErrorStatus> {
        cb->notify_1_2(returnStatus, preparedModel).isOk();
        return launchStatus;
    };
}
auto makePreparedModel_1_3Return(V1_3::ErrorStatus launchStatus, V1_3::ErrorStatus returnStatus,
                                 const sp<MockPreparedModel>& preparedModel) {
    return [launchStatus, returnStatus, preparedModel](
                   const V1_3::Model& /*model*/, ExecutionPreference /*preference*/,
                   Priority /*priority*/, const OptionalTimePoint& /*deadline*/,
                   const hidl_vec<hidl_handle>& /*modelCache*/,
                   const hidl_vec<hidl_handle>& /*dataCache*/, const CacheToken& /*token*/,
                   const sp<V1_3::IPreparedModelCallback>& cb) -> Return<V1_3::ErrorStatus> {
        cb->notify_1_3(returnStatus, preparedModel).isOk();
        return launchStatus;
    };
}

auto makeExecuteReturn(V1_0::ErrorStatus launchStatus, V1_0::ErrorStatus returnStatus) {
    return [launchStatus, returnStatus](
                   const V1_0::Request& /*request*/,
                   const sp<V1_0::IExecutionCallback>& cb) -> Return<V1_0::ErrorStatus> {
        cb->notify(returnStatus);
        return launchStatus;
    };
}
auto makeExecute_1_2Return(V1_0::ErrorStatus launchStatus, V1_0::ErrorStatus returnStatus,
                           const std::vector<OutputShape>& outputShapes, const Timing& timing) {
    return [launchStatus, returnStatus, outputShapes, timing](
                   const V1_0::Request& /*request*/, MeasureTiming /*measureTiming*/,
                   const sp<V1_2::IExecutionCallback>& cb) -> Return<V1_0::ErrorStatus> {
        cb->notify_1_2(returnStatus, outputShapes, timing);
        return launchStatus;
    };
}
auto makeExecute_1_3Return(V1_3::ErrorStatus launchStatus, V1_3::ErrorStatus returnStatus,
                           const std::vector<OutputShape>& outputShapes, const Timing& timing) {
    return [launchStatus, returnStatus, outputShapes, timing](
                   const V1_3::Request& /*request*/, MeasureTiming /*measureTiming*/,
                   const OptionalTimePoint& /*deadline*/,
                   const OptionalTimeoutDuration& /*loopTimeoutDuration*/,
                   const sp<V1_3::IExecutionCallback>& cb) -> Return<V1_3::ErrorStatus> {
        cb->notify_1_3(returnStatus, outputShapes, timing);
        return launchStatus;
    };
}
auto makeExecuteSynchronouslyReturn(V1_0::ErrorStatus status,
                                    const std::vector<OutputShape>& outputShapes,
                                    const Timing& timing) {
    return [status, outputShapes, timing](const V1_0::Request& /*request*/,
                                          MeasureTiming /*measureTiming*/,
                                          const V1_2::IPreparedModel::executeSynchronously_cb& cb) {
        cb(status, outputShapes, timing);
        return Void();
    };
}
auto makeExecuteSynchronously_1_3Return(V1_3::ErrorStatus status,
                                        const std::vector<OutputShape>& outputShapes,
                                        const Timing& timing) {
    return [status, outputShapes, timing](
                   const V1_3::Request& /*request*/, MeasureTiming /*measureTiming*/,
                   const OptionalTimePoint& /*deadline*/,
                   const OptionalTimeoutDuration& /*loopTimeoutDuration*/,
                   const V1_3::IPreparedModel::executeSynchronously_1_3_cb& cb) {
        cb(status, outputShapes, timing);
        return Void();
    };
}
auto makeConfigureExecutionBurst(V1_0::ErrorStatus status,
                                 const sp<MockBurstContext>& burstContext) {
    return [status, burstContext](
                   const sp<V1_2::IBurstCallback>& /*callback*/,
                   const hardware::MQDescriptorSync<V1_2::FmqRequestDatum>& /*requestChannel*/,
                   const hardware::MQDescriptorSync<V1_2::FmqResultDatum>& /*resultChannel*/,
                   V1_2::IPreparedModel::configureExecutionBurst_cb cb) {
        cb(status, burstContext);
        return Void();
    };
}
auto makeExecuteFencedReturn(V1_3::ErrorStatus status, const hidl_handle& syncFence,
                             const sp<IFencedExecutionCallback>& dispatchCallback) {
    return [status, syncFence, dispatchCallback](
                   const V1_3::Request& /*request*/, const hidl_vec<hidl_handle>& /*waitFor*/,
                   MeasureTiming /*measure*/, const OptionalTimePoint& /*deadline*/,
                   const OptionalTimeoutDuration& /*loopTimeoutDuration*/,
                   const OptionalTimeoutDuration& /*duration*/,
                   V1_3::IPreparedModel::executeFenced_cb cb) {
        cb(status, syncFence, dispatchCallback);
        return Void();
    };
}

// TODO: The "setupInitializationExpectation*" calls below re-specify the
// number of expected times each initialization method is called. Because
// this was originally set to `testing::AnyNumber()` when the object was
// created, do these calls act as no-ops, do they override the previous
// expectations, or are both expectations still active?

void setupInitializationExpectationsV1_0(const sp<MockDevice>& mockDevice) {
    EXPECT_CALL(*mockDevice, getCapabilities_1_1(_)).Times(0);
    EXPECT_CALL(*mockDevice, getCapabilities_1_2(_)).Times(0);
    EXPECT_CALL(*mockDevice, getCapabilities_1_3(_)).Times(0);
    EXPECT_CALL(*mockDevice, getVersionString(_)).Times(0);
    EXPECT_CALL(*mockDevice, getType(_)).Times(0);
    EXPECT_CALL(*mockDevice, getSupportedExtensions(_)).Times(0);
    EXPECT_CALL(*mockDevice, getNumberOfCacheFilesNeeded(_)).Times(0);
}

void setupInitializationExpectationsV1_1(const sp<MockDevice>& mockDevice) {
    EXPECT_CALL(*mockDevice, getCapabilities(_)).Times(0);
    EXPECT_CALL(*mockDevice, getCapabilities_1_2(_)).Times(0);
    EXPECT_CALL(*mockDevice, getCapabilities_1_3(_)).Times(0);
    EXPECT_CALL(*mockDevice, getVersionString(_)).Times(0);
    EXPECT_CALL(*mockDevice, getType(_)).Times(0);
    EXPECT_CALL(*mockDevice, getSupportedExtensions(_)).Times(0);
    EXPECT_CALL(*mockDevice, getNumberOfCacheFilesNeeded(_)).Times(0);
}

void setupInitializationExpectationsV1_2(const sp<MockDevice>& mockDevice) {
    EXPECT_CALL(*mockDevice, getCapabilities(_)).Times(0);
    EXPECT_CALL(*mockDevice, getCapabilities_1_1(_)).Times(0);
    EXPECT_CALL(*mockDevice, getCapabilities_1_3(_)).Times(0);
}

void setupInitializationExpectationsV1_3(const sp<MockDevice>& mockDevice) {
    EXPECT_CALL(*mockDevice, getCapabilities(_)).Times(0);
    EXPECT_CALL(*mockDevice, getCapabilities_1_1(_)).Times(0);
    EXPECT_CALL(*mockDevice, getCapabilities_1_2(_)).Times(0);
}

void setupInitializationExpectations(const sp<MockDevice>& mockDevice, Version version) {
    switch (version) {
        case Version::V1_0:
            setupInitializationExpectationsV1_0(mockDevice);
            return;
        case Version::V1_1:
            setupInitializationExpectationsV1_1(mockDevice);
            return;
        case Version::V1_2:
            setupInitializationExpectationsV1_2(mockDevice);
            return;
        case Version::V1_3:
            setupInitializationExpectationsV1_3(mockDevice);
            return;
        case Version::MOCK:
            setupInitializationExpectationsV1_3(mockDevice);
            return;
    }
    LOG(FATAL) << "unrecognized version: " << static_cast<int>(version);
}

void setupSuccessfulInitializationExpectations(const sp<MockDevice>& mockDevice, Version version) {
    EXPECT_CALL(*mockDevice, linkToDeathRet()).Times(testing::AnyNumber());

    const int numCallsForV1_0 = (version == Version::V1_0 ? 1 : 0);
    EXPECT_CALL(*mockDevice, getCapabilities(_)).Times(numCallsForV1_0);

    const int numCallsForV1_1 = (version == Version::V1_1 ? 1 : 0);
    EXPECT_CALL(*mockDevice, getCapabilities_1_1(_)).Times(numCallsForV1_1);

    const int numCallsForV1_2 = (version == Version::V1_2 ? 1 : 0);
    EXPECT_CALL(*mockDevice, getCapabilities_1_2(_)).Times(numCallsForV1_2);

    const int numCallsForAtLeastV1_3 = (version >= Version::V1_3 ? 1 : 0);
    EXPECT_CALL(*mockDevice, getCapabilities_1_3(_)).Times(numCallsForAtLeastV1_3);

    const int numCallsForAtLeastV1_2 = (version >= Version::V1_2 ? 1 : 0);
    EXPECT_CALL(*mockDevice, getVersionString(_)).Times(numCallsForAtLeastV1_2);
    EXPECT_CALL(*mockDevice, getType(_)).Times(numCallsForAtLeastV1_2);
    EXPECT_CALL(*mockDevice, getSupportedExtensions(_)).Times(numCallsForAtLeastV1_2);
    EXPECT_CALL(*mockDevice, getNumberOfCacheFilesNeeded(_)).Times(numCallsForAtLeastV1_2);
}

std::shared_ptr<VersionedIDevice> makeVersionedIDeviceFrom(const sp<MockDevice>& mockDevice,
                                                           MockDeviceFactory* mockDeviceFactory,
                                                           Version version) {
    setupInitializationExpectations(mockDevice, version);
    const auto device = adaptAs(mockDevice, version);
    ON_CALL(*mockDeviceFactory, Call(_)).WillByDefault(testing::Return(device));
    EXPECT_CALL(*mockDeviceFactory, Call(/*blocking=*/true)).Times(testing::AtLeast(1));
    const DeviceFactory makeDevice = mockDeviceFactory->AsStdFunction();
    return VersionedIDevice::create("MockDevice", makeDevice);
}

std::shared_ptr<VersionedIDevice> makeVersionedIDeviceSuccessfulInitializationFrom(
        const sp<MockDevice>& device, MockDeviceFactory* mockDeviceFactory, Version version) {
    setupSuccessfulInitializationExpectations(device, version);
    return makeVersionedIDeviceFrom(device, mockDeviceFactory, version);
}

std::function<hardware::Status()> makeTransportFailure(status_t status) {
    return [status] { return hardware::Status::fromStatusT(status); };
}

const auto makeGeneralTransportFailure = makeTransportFailure(NO_MEMORY);
const auto makeDeadObjectFailure = makeTransportFailure(DEAD_OBJECT);

class VersionedIDeviceTest : public testing::Test {
   protected:
    const sp<MockDevice> kMockDevice = MockDevice::create();
    const std::unique_ptr<MockDeviceFactory> kMockMakeDevice =
            std::make_unique<MockDeviceFactory>();
};

class VersionedIDeviceInitializationTest : public VersionedIDeviceTest {};

template <Version version>
class VersionedIDeviceInitializedTest : public VersionedIDeviceTest {
   protected:
    void SetUp() override {
        VersionedIDeviceTest::SetUp();
        ASSERT_NE(nullptr, kDevice.get());
    }

    const std::shared_ptr<VersionedIDevice> kDevice =
            makeVersionedIDeviceSuccessfulInitializationFrom(kMockDevice, kMockMakeDevice.get(),
                                                             version);
};

class VersionedIDeviceV1_0Test : public VersionedIDeviceInitializedTest<Version::V1_0> {};
class VersionedIDeviceV1_1Test : public VersionedIDeviceInitializedTest<Version::V1_1> {};
class VersionedIDeviceV1_2Test : public VersionedIDeviceInitializedTest<Version::V1_2> {};
class VersionedIDeviceV1_3Test : public VersionedIDeviceInitializedTest<Version::V1_3> {};
class VersionedIDeviceMockTest : public VersionedIDeviceInitializedTest<Version::MOCK> {};

// Simulate initialization/link error

TEST_F(VersionedIDeviceInitializationTest, creationFailure) {
    // setup failure
    EXPECT_CALL(*kMockMakeDevice, Call(_)).Times(1).WillOnce(testing::Return(nullptr));
    const DeviceFactory makeDevice = kMockMakeDevice->AsStdFunction();

    // run test
    const auto device = VersionedIDevice::create("MockDevice", makeDevice);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, linkToDeathTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockDevice, linkToDeathRet())
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));
    EXPECT_CALL(*kMockMakeDevice, Call(_)).Times(1).WillOnce(testing::Return(kMockDevice));
    const DeviceFactory makeDevice = kMockMakeDevice->AsStdFunction();

    // run test
    const auto device = VersionedIDevice::create("MockDevice", makeDevice);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, linkToDeathReturnError) {
    // setup failure
    const auto ret = []() -> Return<bool> { return false; };
    EXPECT_CALL(*kMockMakeDevice, Call(_)).Times(1).WillOnce(testing::Return(kMockDevice));
    EXPECT_CALL(*kMockDevice, linkToDeathRet()).Times(1).WillOnce(InvokeWithoutArgs(ret));
    const DeviceFactory makeDevice = kMockMakeDevice->AsStdFunction();

    // run test
    const auto device = VersionedIDevice::create("MockDevice", makeDevice);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, getCapabilitiesFailure) {
    // setup failure
    const auto ret = makeCallbackReturn(V1_0::ErrorStatus::GENERAL_FAILURE, V1_0::Capabilities{});
    EXPECT_CALL(*kMockDevice, getCapabilities(_)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto device = makeVersionedIDeviceFrom(kMockDevice, kMockMakeDevice.get(), Version::V1_0);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, getCapabilities_1_1Failure) {
    // setup failure
    const auto ret = makeCallbackReturn(V1_0::ErrorStatus::GENERAL_FAILURE, V1_1::Capabilities{});
    EXPECT_CALL(*kMockDevice, getCapabilities_1_1(_)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto device = makeVersionedIDeviceFrom(kMockDevice, kMockMakeDevice.get(), Version::V1_1);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, getCapabilities_1_2Failure) {
    // setup failure
    const auto ret = makeCallbackReturn(V1_0::ErrorStatus::GENERAL_FAILURE, V1_2::Capabilities{});
    EXPECT_CALL(*kMockDevice, getCapabilities_1_2(_)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto device = makeVersionedIDeviceFrom(kMockDevice, kMockMakeDevice.get(), Version::V1_2);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, getCapabilities_1_3Failure) {
    // setup failure
    const auto ret = makeCallbackReturn(V1_3::ErrorStatus::GENERAL_FAILURE, V1_3::Capabilities{});
    EXPECT_CALL(*kMockDevice, getCapabilities_1_3(_)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto device = makeVersionedIDeviceFrom(kMockDevice, kMockMakeDevice.get(), Version::V1_3);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, getVersionStringFailure) {
    // setup failure
    const auto ret = makeCallbackReturn(V1_0::ErrorStatus::GENERAL_FAILURE, "");
    EXPECT_CALL(*kMockDevice, getVersionString(_)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto device = makeVersionedIDeviceFrom(kMockDevice, kMockMakeDevice.get(), Version::V1_2);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, getTypeFailure) {
    // setup failure
    const auto ret = makeCallbackReturn(V1_0::ErrorStatus::GENERAL_FAILURE, DeviceType::OTHER);
    EXPECT_CALL(*kMockDevice, getType(_)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto device = makeVersionedIDeviceFrom(kMockDevice, kMockMakeDevice.get(), Version::V1_2);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, getSupportedExtensionsFailure) {
    // setup failure
    const auto ret = makeCallbackReturn(V1_0::ErrorStatus::GENERAL_FAILURE, hidl_vec<Extension>{});
    EXPECT_CALL(*kMockDevice, getSupportedExtensions(_)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto device = makeVersionedIDeviceFrom(kMockDevice, kMockMakeDevice.get(), Version::V1_2);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, getNumberOfCacheFilesNeededFailure) {
    // setup failure
    const auto ret = makeCallbackReturn(V1_0::ErrorStatus::GENERAL_FAILURE, kMaxNumberOfCacheFiles,
                                        kMaxNumberOfCacheFiles);
    EXPECT_CALL(*kMockDevice, getNumberOfCacheFilesNeeded(_)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto device = makeVersionedIDeviceFrom(kMockDevice, kMockMakeDevice.get(), Version::V1_2);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, dataCacheFilesExceedsSpecifiedMax) {
    // setup failure
    const auto ret = makeCallbackReturn(V1_0::ErrorStatus::NONE, kMaxNumberOfCacheFiles + 1,
                                        kMaxNumberOfCacheFiles);
    EXPECT_CALL(*kMockDevice, getNumberOfCacheFilesNeeded(_)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto device = makeVersionedIDeviceFrom(kMockDevice, kMockMakeDevice.get(), Version::V1_2);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, modelCacheFilesExceedsSpecifiedMax) {
    // setup failure
    const auto ret = makeCallbackReturn(V1_0::ErrorStatus::NONE, kMaxNumberOfCacheFiles,
                                        kMaxNumberOfCacheFiles + 1);
    EXPECT_CALL(*kMockDevice, getNumberOfCacheFilesNeeded(_)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto device = makeVersionedIDeviceFrom(kMockDevice, kMockMakeDevice.get(), Version::V1_2);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, getCapabilitiesTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockDevice, getCapabilities(_))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto device = makeVersionedIDeviceFrom(kMockDevice, kMockMakeDevice.get(), Version::V1_0);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, getCapabilities_1_1TransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockDevice, getCapabilities_1_1(_))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto device = makeVersionedIDeviceFrom(kMockDevice, kMockMakeDevice.get(), Version::V1_1);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, getCapabilities_1_2TransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockDevice, getCapabilities_1_2(_))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto device = makeVersionedIDeviceFrom(kMockDevice, kMockMakeDevice.get(), Version::V1_2);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, getCapabilities_1_3TransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockDevice, getCapabilities_1_3(_))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto device = makeVersionedIDeviceFrom(kMockDevice, kMockMakeDevice.get(), Version::V1_3);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, getVersionStringTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockDevice, getVersionString(_))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto device = makeVersionedIDeviceFrom(kMockDevice, kMockMakeDevice.get(), Version::V1_2);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, getTypeTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockDevice, getType(_))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto device = makeVersionedIDeviceFrom(kMockDevice, kMockMakeDevice.get(), Version::V1_2);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, getSupportedExtensionsTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockDevice, getSupportedExtensions(_))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto device = makeVersionedIDeviceFrom(kMockDevice, kMockMakeDevice.get(), Version::V1_2);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

TEST_F(VersionedIDeviceInitializationTest, getNumberOfCacheFilesNeededTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockDevice, getNumberOfCacheFilesNeeded(_))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto device = makeVersionedIDeviceFrom(kMockDevice, kMockMakeDevice.get(), Version::V1_2);

    // verify failure
    EXPECT_EQ(nullptr, device.get());
}

// Ensure device has cached metadata

TEST_F(VersionedIDeviceV1_0Test, getCapabilities) {
    // run test
    const auto capabilities = kDevice->getCapabilities();
    const auto cached = kDevice->getCapabilities();

    // verify success
    EXPECT_EQ(PerformanceInfo{}, capabilities.relaxedFloat32toFloat16PerformanceScalar);
    EXPECT_EQ(PerformanceInfo{}, capabilities.relaxedFloat32toFloat16PerformanceTensor);
    EXPECT_LT(0u, capabilities.operandPerformance.size());
    EXPECT_EQ(cached, capabilities);
}

TEST_F(VersionedIDeviceV1_1Test, getCapabilities) {
    // run test
    const auto capabilities = kDevice->getCapabilities();
    const auto cached = kDevice->getCapabilities();

    // verify success
    EXPECT_EQ(PerformanceInfo{}, capabilities.relaxedFloat32toFloat16PerformanceScalar);
    EXPECT_EQ(PerformanceInfo{}, capabilities.relaxedFloat32toFloat16PerformanceTensor);
    EXPECT_LT(0u, capabilities.operandPerformance.size());
    EXPECT_EQ(cached, capabilities);
}

TEST_F(VersionedIDeviceV1_2Test, getCapabilities) {
    // run test
    const auto capabilities = kDevice->getCapabilities();
    const auto cached = kDevice->getCapabilities();

    // verify success
    EXPECT_EQ(PerformanceInfo{}, capabilities.relaxedFloat32toFloat16PerformanceScalar);
    EXPECT_EQ(PerformanceInfo{}, capabilities.relaxedFloat32toFloat16PerformanceTensor);
    EXPECT_EQ(0u, capabilities.operandPerformance.size());
    EXPECT_EQ(cached, capabilities);
}

TEST_F(VersionedIDeviceV1_3Test, getCapabilities) {
    // run test
    const auto capabilities = kDevice->getCapabilities();
    const auto cached = kDevice->getCapabilities();

    // verify success
    EXPECT_EQ(PerformanceInfo{}, capabilities.relaxedFloat32toFloat16PerformanceScalar);
    EXPECT_EQ(PerformanceInfo{}, capabilities.relaxedFloat32toFloat16PerformanceTensor);
    EXPECT_EQ(0u, capabilities.operandPerformance.size());
    EXPECT_EQ(cached, capabilities);
}

TEST_F(VersionedIDeviceV1_0Test, getVersionString) {
    // run test
    const auto versionString = kDevice->getVersionString();
    const auto cached = kDevice->getVersionString();

    // verify success
    EXPECT_EQ("UNKNOWN", versionString);
    EXPECT_EQ(cached, versionString);
}

TEST_F(VersionedIDeviceV1_1Test, getVersionString) {
    // run test
    const auto versionString = kDevice->getVersionString();
    const auto cached = kDevice->getVersionString();

    // verify success
    EXPECT_EQ("UNKNOWN", versionString);
    EXPECT_EQ(cached, versionString);
}

TEST_F(VersionedIDeviceV1_2Test, getVersionString) {
    // run test
    const auto versionString = kDevice->getVersionString();
    const auto cached = kDevice->getVersionString();

    // verify success
    EXPECT_EQ("Google-MockV1", versionString);
    EXPECT_EQ(cached, versionString);
}

TEST_F(VersionedIDeviceV1_3Test, getVersionString) {
    // run test
    const auto versionString = kDevice->getVersionString();
    const auto cached = kDevice->getVersionString();

    // verify success
    EXPECT_EQ("Google-MockV1", versionString);
    EXPECT_EQ(cached, versionString);
}

TEST_F(VersionedIDeviceV1_0Test, getType) {
    // run test
    const auto type = kDevice->getType();
    const auto cached = kDevice->getType();

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_DEVICE_UNKNOWN, type);
    EXPECT_EQ(cached, type);
}

TEST_F(VersionedIDeviceV1_1Test, getType) {
    // run test
    const auto type = kDevice->getType();
    const auto cached = kDevice->getType();

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_DEVICE_UNKNOWN, type);
    EXPECT_EQ(cached, type);
}

TEST_F(VersionedIDeviceV1_2Test, getType) {
    // run test
    const auto type = kDevice->getType();
    const auto cached = kDevice->getType();

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_DEVICE_OTHER, type);
    EXPECT_EQ(cached, type);
}

TEST_F(VersionedIDeviceV1_3Test, getType) {
    // run test
    const auto type = kDevice->getType();
    const auto cached = kDevice->getType();

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_DEVICE_OTHER, type);
    EXPECT_EQ(cached, type);
}

TEST_F(VersionedIDeviceV1_0Test, getSupportedExtensions) {
    // run test
    const auto supportedExtensions = kDevice->getSupportedExtensions();
    const auto cached = kDevice->getSupportedExtensions();

    // verify success
    EXPECT_EQ(0u, supportedExtensions.size());
    EXPECT_EQ(cached, supportedExtensions);
}

TEST_F(VersionedIDeviceV1_1Test, getSupportedExtensions) {
    // run test
    const auto supportedExtensions = kDevice->getSupportedExtensions();
    const auto cached = kDevice->getSupportedExtensions();

    // verify success
    EXPECT_EQ(0u, supportedExtensions.size());
    EXPECT_EQ(cached, supportedExtensions);
}

TEST_F(VersionedIDeviceV1_2Test, getSupportedExtensions) {
    // run test
    const auto supportedExtensions = kDevice->getSupportedExtensions();
    const auto cached = kDevice->getSupportedExtensions();

    // verify success
    EXPECT_EQ(0u, supportedExtensions.size());
    EXPECT_EQ(cached, supportedExtensions);
}

TEST_F(VersionedIDeviceV1_3Test, getSupportedExtensions) {
    // run test
    const auto supportedExtensions = kDevice->getSupportedExtensions();
    const auto cached = kDevice->getSupportedExtensions();

    // verify success
    EXPECT_EQ(0u, supportedExtensions.size());
    EXPECT_EQ(cached, supportedExtensions);
}

TEST_F(VersionedIDeviceV1_0Test, getNumberOfCacheFilesNeeded) {
    // run test
    const auto [dataCacheFilesNeeded, modelCacheFilesNeeded] =
            kDevice->getNumberOfCacheFilesNeeded();
    const auto [cachedDataCacheFilesNeeded, cachedModelCacheFilesNeeded] =
            kDevice->getNumberOfCacheFilesNeeded();

    // verify success
    EXPECT_EQ(kNoCacheFilesNeeded, dataCacheFilesNeeded);
    EXPECT_EQ(kNoCacheFilesNeeded, modelCacheFilesNeeded);
    EXPECT_EQ(cachedDataCacheFilesNeeded, dataCacheFilesNeeded);
    EXPECT_EQ(cachedModelCacheFilesNeeded, modelCacheFilesNeeded);
}

TEST_F(VersionedIDeviceV1_1Test, getNumberOfCacheFilesNeeded) {
    // run test
    const auto [dataCacheFilesNeeded, modelCacheFilesNeeded] =
            kDevice->getNumberOfCacheFilesNeeded();
    const auto [cachedDataCacheFilesNeeded, cachedModelCacheFilesNeeded] =
            kDevice->getNumberOfCacheFilesNeeded();

    // verify success
    EXPECT_EQ(kNoCacheFilesNeeded, dataCacheFilesNeeded);
    EXPECT_EQ(kNoCacheFilesNeeded, modelCacheFilesNeeded);
    EXPECT_EQ(cachedDataCacheFilesNeeded, dataCacheFilesNeeded);
    EXPECT_EQ(cachedModelCacheFilesNeeded, modelCacheFilesNeeded);
}

TEST_F(VersionedIDeviceV1_2Test, getNumberOfCacheFilesNeeded) {
    // run test
    const auto [dataCacheFilesNeeded, modelCacheFilesNeeded] =
            kDevice->getNumberOfCacheFilesNeeded();
    const auto [cachedDataCacheFilesNeeded, cachedModelCacheFilesNeeded] =
            kDevice->getNumberOfCacheFilesNeeded();

    // verify success
    EXPECT_EQ(kMaxNumberOfCacheFiles, dataCacheFilesNeeded);
    EXPECT_EQ(kMaxNumberOfCacheFiles, modelCacheFilesNeeded);
    EXPECT_EQ(cachedDataCacheFilesNeeded, dataCacheFilesNeeded);
    EXPECT_EQ(cachedModelCacheFilesNeeded, modelCacheFilesNeeded);
}

TEST_F(VersionedIDeviceV1_3Test, getNumberOfCacheFilesNeeded) {
    // run test
    const auto [dataCacheFilesNeeded, modelCacheFilesNeeded] =
            kDevice->getNumberOfCacheFilesNeeded();
    const auto [cachedDataCacheFilesNeeded, cachedModelCacheFilesNeeded] =
            kDevice->getNumberOfCacheFilesNeeded();

    // verify success
    EXPECT_EQ(kMaxNumberOfCacheFiles, dataCacheFilesNeeded);
    EXPECT_EQ(kMaxNumberOfCacheFiles, modelCacheFilesNeeded);
    EXPECT_EQ(cachedDataCacheFilesNeeded, dataCacheFilesNeeded);
    EXPECT_EQ(cachedModelCacheFilesNeeded, modelCacheFilesNeeded);
}

TEST_F(VersionedIDeviceV1_0Test, getFeatureLevel) {
    // run test
    const auto featureLevel = kDevice->getFeatureLevel();
    const auto cached = kDevice->getFeatureLevel();

    // verify success
    constexpr int64_t expectedFeatureLevel = __ANDROID_API_O_MR1__;
    EXPECT_EQ(expectedFeatureLevel, featureLevel);
    EXPECT_EQ(cached, featureLevel);
}

TEST_F(VersionedIDeviceV1_1Test, getFeatureLevel) {
    // run test
    const auto featureLevel = kDevice->getFeatureLevel();
    const auto cached = kDevice->getFeatureLevel();

    // verify success
    constexpr int64_t expectedFeatureLevel = __ANDROID_API_P__;
    EXPECT_EQ(expectedFeatureLevel, featureLevel);
    EXPECT_EQ(cached, featureLevel);
}

TEST_F(VersionedIDeviceV1_2Test, getFeatureLevel) {
    // run test
    const auto featureLevel = kDevice->getFeatureLevel();
    const auto cached = kDevice->getFeatureLevel();

    // verify success
    constexpr int64_t expectedFeatureLevel = __ANDROID_API_Q__;
    EXPECT_EQ(expectedFeatureLevel, featureLevel);
    EXPECT_EQ(cached, featureLevel);
}

TEST_F(VersionedIDeviceV1_3Test, getFeatureLevel) {
    // run test
    const auto featureLevel = kDevice->getFeatureLevel();
    const auto cached = kDevice->getFeatureLevel();

    // verify success
    constexpr int64_t expectedFeatureLevel = __ANDROID_API_R__;
    EXPECT_EQ(expectedFeatureLevel, featureLevel);
    EXPECT_EQ(cached, featureLevel);
}

// Simulate successful test

TEST_F(VersionedIDeviceV1_0Test, getSupportedOperations) {
    // setup call
    const auto ret = [](const auto& /*model*/, const auto cb) {
        cb(V1_0::ErrorStatus::NONE, {});
        return Void();
    };
    EXPECT_CALL(*kMockDevice, getSupportedOperations(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto metaModel = MetaModel({}, /*strictSlicing=*/true);
    const auto [resultCode, supportedOperations] = kDevice->getSupportedOperations(metaModel);

    // verify success
    EXPECT_EQ(V1_3::ErrorStatus::NONE, resultCode);
    EXPECT_EQ(0u, supportedOperations.size());
}

TEST_F(VersionedIDeviceV1_1Test, getSupportedOperations) {
    // setup call
    const auto ret = [](const auto& /*model*/, const auto cb) {
        cb(V1_0::ErrorStatus::NONE, {});
        return Void();
    };
    EXPECT_CALL(*kMockDevice, getSupportedOperations_1_1(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto metaModel = MetaModel({}, /*strictSlicing=*/true);
    const auto [resultCode, supportedOperations] = kDevice->getSupportedOperations(metaModel);

    // verify success
    EXPECT_EQ(V1_3::ErrorStatus::NONE, resultCode);
    EXPECT_EQ(0u, supportedOperations.size());
}

TEST_F(VersionedIDeviceV1_2Test, getSupportedOperations) {
    // setup call
    const auto ret = [](const auto& /*model*/, const auto cb) {
        cb(V1_0::ErrorStatus::NONE, {});
        return Void();
    };
    EXPECT_CALL(*kMockDevice, getSupportedOperations_1_2(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto metaModel = MetaModel({}, /*strictSlicing=*/true);
    const auto [resultCode, supportedOperations] = kDevice->getSupportedOperations(metaModel);

    // verify success
    EXPECT_EQ(V1_3::ErrorStatus::NONE, resultCode);
    EXPECT_EQ(0u, supportedOperations.size());
}

TEST_F(VersionedIDeviceV1_3Test, getSupportedOperations) {
    // setup call
    const auto ret = [](const auto& /*model*/, const auto cb) {
        cb(V1_3::ErrorStatus::NONE, {});
        return Void();
    };
    EXPECT_CALL(*kMockDevice, getSupportedOperations_1_3(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto metaModel = MetaModel({}, /*strictSlicing=*/true);
    const auto [resultCode, supportedOperations] = kDevice->getSupportedOperations(metaModel);

    // verify success
    EXPECT_EQ(V1_3::ErrorStatus::NONE, resultCode);
    EXPECT_EQ(0u, supportedOperations.size());
}

TEST_F(VersionedIDeviceV1_0Test, prepareModel) {
    // setup call
    const sp<MockPreparedModel> mockPreparedModel = MockPreparedModel::create();
    const auto ret = makePreparedModelReturn(V1_0::ErrorStatus::NONE, V1_0::ErrorStatus::NONE,
                                             mockPreparedModel);
    EXPECT_CALL(*kMockDevice, prepareModel(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
    EXPECT_NE(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_1Test, prepareModel) {
    // setup call
    const sp<MockPreparedModel> mockPreparedModel = MockPreparedModel::create();
    const auto ret = makePreparedModel_1_1Return(V1_0::ErrorStatus::NONE, V1_0::ErrorStatus::NONE,
                                                 mockPreparedModel);
    EXPECT_CALL(*kMockDevice, prepareModel_1_1(_, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
    EXPECT_NE(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_2Test, prepareModel) {
    // setup call
    const sp<MockPreparedModel> mockPreparedModel = MockPreparedModel::create();
    const auto ret = makePreparedModel_1_2Return(V1_0::ErrorStatus::NONE, V1_0::ErrorStatus::NONE,
                                                 mockPreparedModel);
    EXPECT_CALL(*kMockDevice, prepareModel_1_2(_, _, _, _, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
    EXPECT_NE(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_3Test, prepareModel) {
    // setup call
    const sp<MockPreparedModel> mockPreparedModel = MockPreparedModel::create();
    const auto ret = makePreparedModel_1_3Return(V1_3::ErrorStatus::NONE, V1_3::ErrorStatus::NONE,
                                                 mockPreparedModel);
    EXPECT_CALL(*kMockDevice, prepareModel_1_3(_, _, _, _, _, _, _, _))
            .Times(1)
            .WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
    EXPECT_NE(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_0Test, allocate) {
    // run test
    const auto [status, buffer, token] = kDevice->allocate({}, {}, {}, {});

    // verify success
    EXPECT_EQ(V1_3::ErrorStatus::GENERAL_FAILURE, status);
    EXPECT_EQ(nullptr, buffer.get());
    EXPECT_EQ(0u, token);
}

TEST_F(VersionedIDeviceV1_1Test, allocate) {
    // run test
    const auto [status, buffer, token] = kDevice->allocate({}, {}, {}, {});

    // verify success
    EXPECT_EQ(V1_3::ErrorStatus::GENERAL_FAILURE, status);
    EXPECT_EQ(nullptr, buffer.get());
    EXPECT_EQ(0u, token);
}

TEST_F(VersionedIDeviceV1_2Test, allocate) {
    // run test
    const auto [status, buffer, token] = kDevice->allocate({}, {}, {}, {});

    // verify success
    EXPECT_EQ(V1_3::ErrorStatus::GENERAL_FAILURE, status);
    EXPECT_EQ(nullptr, buffer.get());
    EXPECT_EQ(0u, token);
}

TEST_F(VersionedIDeviceV1_3Test, allocate) {
    // setup call
    const sp<MockBuffer> mockBuffer = new MockBuffer();
    constexpr uint32_t mockToken = 1;
    const auto ret = [mockBuffer](const BufferDesc& /*desc*/,
                                  const hidl_vec<sp<V1_3::IPreparedModel>>& /*preparedModels*/,
                                  const hidl_vec<BufferRole>& /*inputRoles*/,
                                  const hidl_vec<BufferRole>& /*outputRoles*/,
                                  V1_3::IDevice::allocate_cb cb) -> Return<void> {
        cb(V1_3::ErrorStatus::NONE, mockBuffer, mockToken);
        return Void();
    };
    EXPECT_CALL(*kMockDevice, allocate(_, _, _, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [status, buffer, token] = kDevice->allocate({}, {}, {}, {});

    // verify success
    EXPECT_EQ(V1_3::ErrorStatus::NONE, status);
    EXPECT_NE(nullptr, buffer.get());
    EXPECT_NE(0u, token);
}

TEST_F(VersionedIDeviceMockTest, wait) {
    // setup call
    const auto ret = []() -> Return<void> { return {}; };
    EXPECT_CALL(*kMockDevice, ping()).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto resultCode = kDevice->wait();

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
}

// Simulate general failure

TEST_F(VersionedIDeviceV1_0Test, getSupportedOperationsFailure) {
    // setup failure
    const auto ret = [](const auto& /*model*/, const auto cb) {
        cb(V1_0::ErrorStatus::GENERAL_FAILURE, {});
        return Void();
    };
    EXPECT_CALL(*kMockDevice, getSupportedOperations(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto metaModel = MetaModel({}, /*strictSlicing=*/true);
    const auto [resultCode, supportedOperations] = kDevice->getSupportedOperations(metaModel);

    // verify failure
    EXPECT_EQ(V1_3::ErrorStatus::GENERAL_FAILURE, resultCode);
    EXPECT_EQ(0u, supportedOperations.size());
}

TEST_F(VersionedIDeviceV1_1Test, getSupportedOperationsFailure) {
    // setup failure
    const auto ret = [](const auto& /*model*/, const auto cb) {
        cb(V1_0::ErrorStatus::GENERAL_FAILURE, {});
        return Void();
    };
    EXPECT_CALL(*kMockDevice, getSupportedOperations_1_1(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto metaModel = MetaModel({}, /*strictSlicing=*/true);
    const auto [resultCode, supportedOperations] = kDevice->getSupportedOperations(metaModel);

    // verify failure
    EXPECT_EQ(V1_3::ErrorStatus::GENERAL_FAILURE, resultCode);
    EXPECT_EQ(0u, supportedOperations.size());
}

TEST_F(VersionedIDeviceV1_2Test, getSupportedOperationsFailure) {
    // setup failure
    const auto ret = [](const auto& /*model*/, const auto cb) {
        cb(V1_0::ErrorStatus::GENERAL_FAILURE, {});
        return Void();
    };
    EXPECT_CALL(*kMockDevice, getSupportedOperations_1_2(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto metaModel = MetaModel({}, /*strictSlicing=*/true);
    const auto [resultCode, supportedOperations] = kDevice->getSupportedOperations(metaModel);

    // verify failure
    EXPECT_EQ(V1_3::ErrorStatus::GENERAL_FAILURE, resultCode);
    EXPECT_EQ(0u, supportedOperations.size());
}

TEST_F(VersionedIDeviceV1_3Test, getSupportedOperationsFailure) {
    // setup failure
    const auto ret = [](const auto& /*model*/, const auto cb) {
        cb(V1_3::ErrorStatus::GENERAL_FAILURE, {});
        return Void();
    };
    EXPECT_CALL(*kMockDevice, getSupportedOperations_1_3(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto metaModel = MetaModel({}, /*strictSlicing=*/true);
    const auto [resultCode, supportedOperations] = kDevice->getSupportedOperations(metaModel);

    // verify failure
    EXPECT_EQ(V1_3::ErrorStatus::GENERAL_FAILURE, resultCode);
    EXPECT_EQ(0u, supportedOperations.size());
}

TEST_F(VersionedIDeviceV1_0Test, prepareModelLaunchFailure) {
    // setup failure
    const sp<MockPreparedModel> mockPreparedModel = MockPreparedModel::create();
    const auto ret = makePreparedModelReturn(V1_0::ErrorStatus::GENERAL_FAILURE,
                                             V1_0::ErrorStatus::NONE, mockPreparedModel);
    EXPECT_CALL(*kMockDevice, prepareModel(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_1Test, prepareModelLaunchFailure) {
    // setup failure
    const sp<MockPreparedModel> mockPreparedModel = MockPreparedModel::create();
    const auto ret = makePreparedModel_1_1Return(V1_0::ErrorStatus::GENERAL_FAILURE,
                                                 V1_0::ErrorStatus::NONE, mockPreparedModel);
    EXPECT_CALL(*kMockDevice, prepareModel_1_1(_, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_2Test, prepareModelLaunchFailure) {
    // setup failure
    const sp<MockPreparedModel> mockPreparedModel = MockPreparedModel::create();
    const auto ret = makePreparedModel_1_2Return(V1_0::ErrorStatus::GENERAL_FAILURE,
                                                 V1_0::ErrorStatus::NONE, mockPreparedModel);
    EXPECT_CALL(*kMockDevice, prepareModel_1_2(_, _, _, _, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_3Test, prepareModelLaunchFailure) {
    // setup failure
    const sp<MockPreparedModel> mockPreparedModel = MockPreparedModel::create();
    const auto ret = makePreparedModel_1_3Return(V1_3::ErrorStatus::GENERAL_FAILURE,
                                                 V1_3::ErrorStatus::NONE, mockPreparedModel);
    EXPECT_CALL(*kMockDevice, prepareModel_1_3(_, _, _, _, _, _, _, _))
            .Times(1)
            .WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_0Test, prepareModelReturnFailure) {
    // setup failure
    const sp<MockPreparedModel> mockPreparedModel = MockPreparedModel::create();
    const auto ret = makePreparedModelReturn(V1_0::ErrorStatus::NONE,
                                             V1_0::ErrorStatus::GENERAL_FAILURE, mockPreparedModel);
    EXPECT_CALL(*kMockDevice, prepareModel(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_1Test, prepareModelReturnFailure) {
    // setup failure
    const sp<MockPreparedModel> mockPreparedModel = MockPreparedModel::create();
    const auto ret = makePreparedModel_1_1Return(
            V1_0::ErrorStatus::NONE, V1_0::ErrorStatus::GENERAL_FAILURE, mockPreparedModel);
    EXPECT_CALL(*kMockDevice, prepareModel_1_1(_, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_2Test, prepareModelReturnFailure) {
    // setup failure
    const sp<MockPreparedModel> mockPreparedModel = MockPreparedModel::create();
    const auto ret = makePreparedModel_1_2Return(
            V1_0::ErrorStatus::NONE, V1_0::ErrorStatus::GENERAL_FAILURE, mockPreparedModel);
    EXPECT_CALL(*kMockDevice, prepareModel_1_2(_, _, _, _, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_3Test, prepareModelReturnFailure) {
    // setup failure
    const sp<MockPreparedModel> mockPreparedModel = MockPreparedModel::create();
    const auto ret = makePreparedModel_1_3Return(
            V1_3::ErrorStatus::NONE, V1_3::ErrorStatus::GENERAL_FAILURE, mockPreparedModel);
    EXPECT_CALL(*kMockDevice, prepareModel_1_3(_, _, _, _, _, _, _, _))
            .Times(1)
            .WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_0Test, prepareModelNullptrError) {
    // setup failure
    const sp<MockPreparedModel> mockPreparedModel = nullptr;
    const auto ret = makePreparedModelReturn(V1_0::ErrorStatus::NONE, V1_0::ErrorStatus::NONE,
                                             mockPreparedModel);
    EXPECT_CALL(*kMockDevice, prepareModel(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_1Test, prepareModelNullptrError) {
    // setup failure
    const sp<MockPreparedModel> mockPreparedModel = nullptr;
    const auto ret = makePreparedModel_1_1Return(V1_0::ErrorStatus::NONE, V1_0::ErrorStatus::NONE,
                                                 mockPreparedModel);
    EXPECT_CALL(*kMockDevice, prepareModel_1_1(_, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_2Test, prepareModelNullptrError) {
    // setup failure
    const sp<MockPreparedModel> mockPreparedModel = nullptr;
    const auto ret = makePreparedModel_1_2Return(V1_0::ErrorStatus::NONE, V1_0::ErrorStatus::NONE,
                                                 mockPreparedModel);
    EXPECT_CALL(*kMockDevice, prepareModel_1_2(_, _, _, _, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_3Test, prepareModelNullptrError) {
    // setup failure
    const sp<MockPreparedModel> mockPreparedModel = nullptr;
    const auto ret = makePreparedModel_1_3Return(V1_3::ErrorStatus::NONE, V1_3::ErrorStatus::NONE,
                                                 mockPreparedModel);
    EXPECT_CALL(*kMockDevice, prepareModel_1_3(_, _, _, _, _, _, _, _))
            .Times(1)
            .WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_3Test, allocateFailure) {
    // setup failure
    const auto ret = [](const BufferDesc& /*desc*/,
                        const hidl_vec<sp<V1_3::IPreparedModel>>& /*preparedModels*/,
                        const hidl_vec<BufferRole>& /*inputRoles*/,
                        const hidl_vec<BufferRole>& /*outputRoles*/,
                        V1_3::IDevice::allocate_cb cb) -> Return<void> {
        cb(V1_3::ErrorStatus::GENERAL_FAILURE, nullptr, 0);
        return Void();
    };
    EXPECT_CALL(*kMockDevice, allocate(_, _, _, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [status, buffer, token] = kDevice->allocate({}, {}, {}, {});

    // verify failure
    EXPECT_EQ(V1_3::ErrorStatus::GENERAL_FAILURE, status);
    EXPECT_EQ(nullptr, buffer.get());
    EXPECT_EQ(0u, token);
}

// Simulate transport failure

TEST_F(VersionedIDeviceV1_0Test, getSupportedOperationsTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockDevice, getSupportedOperations(_, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto metaModel = MetaModel({}, /*strictSlicing=*/true);
    const auto [resultCode, supportedOperations] = kDevice->getSupportedOperations(metaModel);

    // verify failure
    EXPECT_EQ(V1_3::ErrorStatus::GENERAL_FAILURE, resultCode);
    EXPECT_EQ(0u, supportedOperations.size());
}

TEST_F(VersionedIDeviceV1_1Test, getSupportedOperationsTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockDevice, getSupportedOperations_1_1(_, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto metaModel = MetaModel({}, /*strictSlicing=*/true);
    const auto [resultCode, supportedOperations] = kDevice->getSupportedOperations(metaModel);

    // verify failure
    EXPECT_EQ(V1_3::ErrorStatus::GENERAL_FAILURE, resultCode);
    EXPECT_EQ(0u, supportedOperations.size());
}

TEST_F(VersionedIDeviceV1_2Test, getSupportedOperationsTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockDevice, getSupportedOperations_1_2(_, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto metaModel = MetaModel({}, /*strictSlicing=*/true);
    const auto [resultCode, supportedOperations] = kDevice->getSupportedOperations(metaModel);

    // verify failure
    EXPECT_EQ(V1_3::ErrorStatus::GENERAL_FAILURE, resultCode);
    EXPECT_EQ(0u, supportedOperations.size());
}

TEST_F(VersionedIDeviceV1_3Test, getSupportedOperationsTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockDevice, getSupportedOperations_1_3(_, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto metaModel = MetaModel({}, /*strictSlicing=*/true);
    const auto [resultCode, supportedOperations] = kDevice->getSupportedOperations(metaModel);

    // verify failure
    EXPECT_EQ(V1_3::ErrorStatus::GENERAL_FAILURE, resultCode);
    EXPECT_EQ(0u, supportedOperations.size());
}

TEST_F(VersionedIDeviceV1_0Test, prepareModelTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockDevice, prepareModel(_, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_1Test, prepareModelTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockDevice, prepareModel_1_1(_, _, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_2Test, prepareModelTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockDevice, prepareModel_1_2(_, _, _, _, _, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_3Test, prepareModelTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockDevice, prepareModel_1_3(_, _, _, _, _, _, _, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceV1_3Test, allocateTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockDevice, allocate(_, _, _, _, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto [status, buffer, token] = kDevice->allocate({}, {}, {}, {});

    // verify failure
    EXPECT_EQ(V1_3::ErrorStatus::GENERAL_FAILURE, status);
    EXPECT_EQ(nullptr, buffer.get());
    EXPECT_EQ(0u, token);
}

TEST_F(VersionedIDeviceMockTest, waitTransportFailure) {
    // setup call
    EXPECT_CALL(*kMockDevice, ping())
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto resultCode = kDevice->wait();

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
}

// Simulate service crash

// TODO: enable this test once b/154183300 is fixed.
TEST_F(VersionedIDeviceMockTest, DISABLED_prepareModelRecoverCrash) {
    // setup original device calls
    EXPECT_CALL(*kMockDevice, prepareModel_1_3(_, _, _, _, _, _, _, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeDeadObjectFailure));
    EXPECT_CALL(*kMockDevice, ping()).Times(1).WillOnce(InvokeWithoutArgs(makeDeadObjectFailure));

    // setup recovery call
    const sp<MockDevice> mockRecoveredDevice = MockDevice::create();
    EXPECT_CALL(*kMockMakeDevice, Call(/*blocking=*/false))
            .Times(1)
            .WillOnce(testing::Return(mockRecoveredDevice));

    // setup recovered device calls
    const sp<MockPreparedModel> mockPreparedModel = MockPreparedModel::create();
    const auto ret = makePreparedModel_1_3Return(V1_3::ErrorStatus::NONE, V1_3::ErrorStatus::NONE,
                                                 mockPreparedModel);
    EXPECT_CALL(*mockRecoveredDevice, linkToDeathRet()).Times(1);
    EXPECT_CALL(*mockRecoveredDevice, prepareModel_1_3(_, _, _, _, _, _, _, _))
            .Times(1)
            .WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
    EXPECT_NE(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceMockTest, prepareModelFullCrash) {
    // setup failure
    EXPECT_CALL(*kMockDevice, prepareModel_1_3(_, _, _, _, _, _, _, _))
            .Times(1)
            .WillRepeatedly(InvokeWithoutArgs(makeDeadObjectFailure));
    EXPECT_CALL(*kMockDevice, ping())
            .Times(1)
            .WillRepeatedly(InvokeWithoutArgs(makeDeadObjectFailure));
    EXPECT_CALL(*kMockMakeDevice, Call(/*blocking=*/false))
            .Times(1)
            .WillOnce(testing::Return(nullptr));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_DEAD_OBJECT, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceMockTest, prepareModelAsyncCrash) {
    // setup failure
    const auto ret = [this]() -> Return<V1_3::ErrorStatus> {
        kMockDevice->simulateCrash();
        return V1_3::ErrorStatus::NONE;
    };
    EXPECT_CALL(*kMockDevice, prepareModel_1_3(_, _, _, _, _, _, _, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_DEAD_OBJECT, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIDeviceMockTest, waitCrash) {
    // setup failure
    EXPECT_CALL(*kMockDevice, ping())
            .Times(1)
            .WillRepeatedly(InvokeWithoutArgs(makeDeadObjectFailure));
    EXPECT_CALL(*kMockMakeDevice, Call(/*blocking=*/true))
            .Times(1)
            .WillOnce(testing::Return(nullptr));

    // run test
    const auto resultCode = kDevice->wait();

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
}

TEST_F(VersionedIDeviceMockTest, waitRecoverCrash) {
    // setup original device calls
    EXPECT_CALL(*kMockDevice, ping()).Times(1).WillOnce(InvokeWithoutArgs(makeDeadObjectFailure));

    // setup recovery call
    const sp<MockDevice> mockRecoveredDevice = MockDevice::create();
    EXPECT_CALL(*kMockMakeDevice, Call(/*blocking=*/true))
            .Times(1)
            .WillOnce(testing::Return(mockRecoveredDevice));

    // setup recovered device calls
    const auto ret = []() -> Return<bool> { return true; };
    EXPECT_CALL(*mockRecoveredDevice, linkToDeathRet()).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto resultCode = kDevice->wait();

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
}

TEST_F(VersionedIDeviceMockTest, waitFailedRecoverCrash) {
    // setup original device calls
    EXPECT_CALL(*kMockDevice, ping()).Times(1).WillOnce(InvokeWithoutArgs(makeDeadObjectFailure));

    // setup recovery call
    const sp<MockDevice> mockRecoveredDevice = MockDevice::create();
    EXPECT_CALL(*kMockMakeDevice, Call(/*blocking=*/true))
            .Times(1)
            .WillOnce(testing::Return(mockRecoveredDevice));

    // setup recovered device calls
    EXPECT_CALL(*mockRecoveredDevice, linkToDeathRet())
            .Times(1)
            .WillOnce(makeGeneralTransportFailure);

    // run test
    const auto resultCode = kDevice->wait();

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
}

// Harness for VersionedIPreparedModel failures.

class VersionedIPreparedModelInitializationTest : public VersionedIDeviceMockTest {
   protected:
    const sp<MockPreparedModel> kMockPreparedModel = MockPreparedModel::create();
};

std::shared_ptr<VersionedIPreparedModel> makeVersionedIPreparedModelSuccessfulInitializationFrom(
        const sp<MockDevice>& mockDevice, const sp<MockPreparedModel>& mockPreparedModel,
        const VersionedIDevice& device) {
    const auto retV1_0 = makePreparedModelReturn(V1_0::ErrorStatus::NONE, V1_0::ErrorStatus::NONE,
                                                 mockPreparedModel);
    const auto retV1_1 = makePreparedModel_1_1Return(V1_0::ErrorStatus::NONE,
                                                     V1_0::ErrorStatus::NONE, mockPreparedModel);
    const auto retV1_2 = makePreparedModel_1_2Return(V1_0::ErrorStatus::NONE,
                                                     V1_0::ErrorStatus::NONE, mockPreparedModel);
    const auto retV1_3 = makePreparedModel_1_3Return(V1_3::ErrorStatus::NONE,
                                                     V1_3::ErrorStatus::NONE, mockPreparedModel);

    ON_CALL(*mockDevice, prepareModel(_, _)).WillByDefault(Invoke(retV1_0));
    ON_CALL(*mockDevice, prepareModel_1_1(_, _, _)).WillByDefault(Invoke(retV1_1));
    ON_CALL(*mockDevice, prepareModel_1_2(_, _, _, _, _, _)).WillByDefault(Invoke(retV1_2));
    ON_CALL(*mockDevice, prepareModel_1_3(_, _, _, _, _, _, _, _)).WillByDefault(Invoke(retV1_3));

    EXPECT_CALL(*mockDevice, prepareModel(_, _)).Times(testing::AnyNumber());
    EXPECT_CALL(*mockDevice, prepareModel_1_1(_, _, _)).Times(testing::AnyNumber());
    EXPECT_CALL(*mockDevice, prepareModel_1_2(_, _, _, _, _, _)).Times(testing::AnyNumber());
    EXPECT_CALL(*mockDevice, prepareModel_1_3(_, _, _, _, _, _, _, _)).Times(testing::AnyNumber());

    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = device.prepareModel(makeModel, {}, {}, {}, {}, {});

    CHECK_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
    CHECK(preparedModel != nullptr);

    return preparedModel;
}

template <Version version>
class VersionedIPreparedModelTest : public VersionedIDeviceInitializedTest<version> {
    using Base = VersionedIDeviceInitializedTest<version>;

   protected:
    void SetUp() override {
        VersionedIDeviceInitializedTest<version>::SetUp();
        ASSERT_NE(nullptr, kPreparedModel.get());
    }

    const sp<MockPreparedModel> kMockPreparedModel = MockPreparedModel::create();
    const std::shared_ptr<VersionedIPreparedModel> kPreparedModel =
            makeVersionedIPreparedModelSuccessfulInitializationFrom(
                    Base::kMockDevice, kMockPreparedModel, *Base::kDevice);
};

class VersionedIPreparedModelV1_0Test : public VersionedIPreparedModelTest<Version::V1_0> {};
class VersionedIPreparedModelV1_1Test : public VersionedIPreparedModelTest<Version::V1_1> {};
class VersionedIPreparedModelV1_2Test : public VersionedIPreparedModelTest<Version::V1_2> {};
class VersionedIPreparedModelV1_3Test : public VersionedIPreparedModelTest<Version::V1_3> {};
class VersionedIPreparedModelMockTest : public VersionedIPreparedModelTest<Version::MOCK> {};

// Simulate initialization/link error

TEST_F(VersionedIPreparedModelInitializationTest, linkToDeathTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, linkToDeathRet())
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));
    const auto ret = makePreparedModel_1_3Return(V1_3::ErrorStatus::NONE, V1_3::ErrorStatus::NONE,
                                                 kMockPreparedModel);
    EXPECT_CALL(*kMockDevice, prepareModel_1_3(_, _, _, _, _, _, _, _))
            .Times(1)
            .WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIPreparedModelInitializationTest, linkToDeathDeadObject) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, linkToDeathRet())
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeDeadObjectFailure));
    const auto ret = makePreparedModel_1_3Return(V1_3::ErrorStatus::NONE, V1_3::ErrorStatus::NONE,
                                                 kMockPreparedModel);
    EXPECT_CALL(*kMockDevice, prepareModel_1_3(_, _, _, _, _, _, _, _))
            .Times(1)
            .WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_DEAD_OBJECT, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

TEST_F(VersionedIPreparedModelInitializationTest, linkToDeathReturnError) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, linkToDeathRet())
            .Times(1)
            .WillOnce(InvokeWithoutArgs([]() -> Return<bool> { return false; }));
    const auto ret = makePreparedModel_1_3Return(V1_3::ErrorStatus::NONE, V1_3::ErrorStatus::NONE,
                                                 kMockPreparedModel);
    EXPECT_CALL(*kMockDevice, prepareModel_1_3(_, _, _, _, _, _, _, _))
            .Times(1)
            .WillOnce(Invoke(ret));

    // run test
    const ModelFactory makeModel = [] { return V1_3::Model{}; };
    const auto [resultCode, preparedModel] = kDevice->prepareModel(makeModel, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, preparedModel.get());
}

// Simulate successful test

TEST_F(VersionedIPreparedModelV1_0Test, executeAsync) {
    // setup call
    const auto ret = makeExecuteReturn(V1_0::ErrorStatus::NONE, V1_0::ErrorStatus::NONE);
    EXPECT_CALL(*kMockPreparedModel, execute(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_1Test, executeAsync) {
    // setup call
    const auto ret = makeExecuteReturn(V1_0::ErrorStatus::NONE, V1_0::ErrorStatus::NONE);
    EXPECT_CALL(*kMockPreparedModel, execute(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_2Test, executeAsync) {
    // setup call
    const auto ret =
            makeExecute_1_2Return(V1_0::ErrorStatus::NONE, V1_0::ErrorStatus::NONE, {}, kNoTiming);
    EXPECT_CALL(*kMockPreparedModel, execute_1_2(_, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_3Test, executeAsync) {
    // setup call
    const auto ret =
            makeExecute_1_3Return(V1_3::ErrorStatus::NONE, V1_3::ErrorStatus::NONE, {}, kNoTiming);
    EXPECT_CALL(*kMockPreparedModel, execute_1_3(_, _, _, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_0Test, executePreferSync) {
    // setup call
    const auto ret = makeExecuteReturn(V1_0::ErrorStatus::NONE, V1_0::ErrorStatus::NONE);
    EXPECT_CALL(*kMockPreparedModel, execute(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/true);

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_1Test, executePreferSync) {
    // setup call
    const auto ret = makeExecuteReturn(V1_0::ErrorStatus::NONE, V1_0::ErrorStatus::NONE);
    EXPECT_CALL(*kMockPreparedModel, execute(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/true);

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_2Test, executePreferSync) {
    // setup call
    const auto ret = makeExecuteSynchronouslyReturn(V1_0::ErrorStatus::NONE, {}, kNoTiming);
    EXPECT_CALL(*kMockPreparedModel, executeSynchronously(_, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/true);

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_3Test, executePreferSync) {
    // setup call
    const auto ret = makeExecuteSynchronously_1_3Return(V1_3::ErrorStatus::NONE, {}, kNoTiming);
    EXPECT_CALL(*kMockPreparedModel, executeSynchronously_1_3(_, _, _, _, _))
            .Times(1)
            .WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/true);

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_0Test, executeFenced) {
    // setup call
    const auto ret = makeExecuteReturn(V1_0::ErrorStatus::NONE, V1_0::ErrorStatus::NONE);
    EXPECT_CALL(*kMockPreparedModel, execute(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, syncFence, dispatchCallback, timing] =
            kPreparedModel->executeFenced({}, {}, {}, {}, {}, {});

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
    EXPECT_EQ(nullptr, syncFence.getNativeHandle());
    EXPECT_EQ(nullptr, dispatchCallback.get());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_1Test, executeFenced) {
    // setup call
    const auto ret = makeExecuteReturn(V1_0::ErrorStatus::NONE, V1_0::ErrorStatus::NONE);
    EXPECT_CALL(*kMockPreparedModel, execute(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, syncFence, dispatchCallback, timing] =
            kPreparedModel->executeFenced({}, {}, {}, {}, {}, {});

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
    EXPECT_EQ(nullptr, syncFence.getNativeHandle());
    EXPECT_EQ(nullptr, dispatchCallback.get());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_2Test, executeFenced) {
    // setup call
    const auto ret = makeExecuteSynchronouslyReturn(V1_0::ErrorStatus::NONE, {}, kNoTiming);
    EXPECT_CALL(*kMockPreparedModel, executeSynchronously(_, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, syncFence, dispatchCallback, timing] =
            kPreparedModel->executeFenced({}, {}, {}, {}, {}, {});

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
    EXPECT_EQ(nullptr, syncFence.getNativeHandle());
    EXPECT_EQ(nullptr, dispatchCallback.get());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_3Test, executeFenced) {
    // setup call
    auto memory = allocateSharedMemory(4);
    hidl_handle fakeSyncFence(memory.handle());
    const sp<IFencedExecutionCallback> callback = new MockFencedExecutionCallback();
    const auto ret = makeExecuteFencedReturn(V1_3::ErrorStatus::NONE, fakeSyncFence, callback);
    EXPECT_CALL(*kMockPreparedModel, executeFenced(_, _, _, _, _, _, _))
            .Times(1)
            .WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, syncFence, dispatchCallback, timing] =
            kPreparedModel->executeFenced({}, {}, {}, {}, {}, {});

    // verify success
    EXPECT_EQ(ANEURALNETWORKS_NO_ERROR, resultCode);
    EXPECT_NE(nullptr, syncFence.getNativeHandle());
    EXPECT_NE(nullptr, dispatchCallback.get());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_0Test, configureExecutionBurst) {
    // run test
    const auto executionBurstController =
            kPreparedModel->configureExecutionBurst(/*preferPowerOverLatency=*/false);

    // verify success
    EXPECT_EQ(nullptr, executionBurstController);
}

TEST_F(VersionedIPreparedModelV1_1Test, configureExecutionBurst) {
    // run test
    const auto executionBurstController =
            kPreparedModel->configureExecutionBurst(/*preferPowerOverLatency=*/false);

    // verify success
    EXPECT_EQ(nullptr, executionBurstController);
}

TEST_F(VersionedIPreparedModelV1_2Test, configureExecutionBurst) {
    // setup call
    const sp<MockBurstContext> burstContext = new MockBurstContext();
    const auto ret = makeConfigureExecutionBurst(V1_0::ErrorStatus::NONE, burstContext);
    EXPECT_CALL(*kMockPreparedModel, configureExecutionBurst(_, _, _, _))
            .Times(1)
            .WillOnce(Invoke(ret));

    // run test
    const auto executionBurstController =
            kPreparedModel->configureExecutionBurst(/*preferPowerOverLatency=*/false);

    // verify success
    EXPECT_NE(nullptr, executionBurstController);
}

TEST_F(VersionedIPreparedModelV1_3Test, configureExecutionBurst) {
    // setup call
    const sp<MockBurstContext> burstContext = new MockBurstContext();
    const auto ret = makeConfigureExecutionBurst(V1_0::ErrorStatus::NONE, burstContext);
    EXPECT_CALL(*kMockPreparedModel, configureExecutionBurst(_, _, _, _))
            .Times(1)
            .WillOnce(Invoke(ret));

    // run test
    const auto executionBurstController =
            kPreparedModel->configureExecutionBurst(/*preferPowerOverLatency=*/false);

    // verify success
    EXPECT_NE(nullptr, executionBurstController);
}

// Simulate general failure

TEST_F(VersionedIPreparedModelV1_0Test, executeAsyncLaunchFailure) {
    // setup failure
    const auto ret = makeExecuteReturn(V1_0::ErrorStatus::GENERAL_FAILURE, V1_0::ErrorStatus::NONE);
    EXPECT_CALL(*kMockPreparedModel, execute(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_1Test, executeAsyncLaunchFailure) {
    // setup failure
    const auto ret = makeExecuteReturn(V1_0::ErrorStatus::GENERAL_FAILURE, V1_0::ErrorStatus::NONE);
    EXPECT_CALL(*kMockPreparedModel, execute(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_2Test, executeAsyncLaunchFailure) {
    // setup failure
    const auto ret = makeExecute_1_2Return(V1_0::ErrorStatus::GENERAL_FAILURE,
                                           V1_0::ErrorStatus::NONE, {}, kNoTiming);
    EXPECT_CALL(*kMockPreparedModel, execute_1_2(_, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_3Test, executeAsyncLaunchFailure) {
    // setup failure
    const auto ret = makeExecute_1_3Return(V1_3::ErrorStatus::GENERAL_FAILURE,
                                           V1_3::ErrorStatus::NONE, {}, kNoTiming);
    EXPECT_CALL(*kMockPreparedModel, execute_1_3(_, _, _, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_0Test, executeAsyncReturnFailure) {
    // setup failure
    const auto ret = makeExecuteReturn(V1_0::ErrorStatus::NONE, V1_0::ErrorStatus::GENERAL_FAILURE);
    EXPECT_CALL(*kMockPreparedModel, execute(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_1Test, executeAsyncReturnFailure) {
    // setup failure
    const auto ret = makeExecuteReturn(V1_0::ErrorStatus::NONE, V1_0::ErrorStatus::GENERAL_FAILURE);
    EXPECT_CALL(*kMockPreparedModel, execute(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_2Test, executeAsyncReturnFailure) {
    // setup failure
    const auto ret = makeExecute_1_2Return(V1_0::ErrorStatus::NONE,
                                           V1_0::ErrorStatus::GENERAL_FAILURE, {}, kNoTiming);
    EXPECT_CALL(*kMockPreparedModel, execute_1_2(_, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_3Test, executeAsyncReturnFailure) {
    // setup failure
    const auto ret = makeExecute_1_3Return(V1_3::ErrorStatus::NONE,
                                           V1_3::ErrorStatus::GENERAL_FAILURE, {}, kNoTiming);
    EXPECT_CALL(*kMockPreparedModel, execute_1_3(_, _, _, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_0Test, executePreferSyncFailure) {
    // setup failure
    const auto ret = makeExecuteReturn(V1_0::ErrorStatus::GENERAL_FAILURE,
                                       V1_0::ErrorStatus::GENERAL_FAILURE);
    EXPECT_CALL(*kMockPreparedModel, execute(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/true);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_1Test, executePreferSyncFailure) {
    // setup failure
    const auto ret = makeExecuteReturn(V1_0::ErrorStatus::GENERAL_FAILURE,
                                       V1_0::ErrorStatus::GENERAL_FAILURE);
    EXPECT_CALL(*kMockPreparedModel, execute(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/true);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_2Test, executePreferSyncFailure) {
    // setup failure
    const auto ret =
            makeExecuteSynchronouslyReturn(V1_0::ErrorStatus::GENERAL_FAILURE, {}, kNoTiming);
    EXPECT_CALL(*kMockPreparedModel, executeSynchronously(_, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/true);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_3Test, executePreferSyncFailure) {
    // setup failure
    const auto ret =
            makeExecuteSynchronously_1_3Return(V1_3::ErrorStatus::GENERAL_FAILURE, {}, kNoTiming);
    EXPECT_CALL(*kMockPreparedModel, executeSynchronously_1_3(_, _, _, _, _))
            .Times(1)
            .WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/true);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_0Test, executeFencedFailure) {
    // setup failure
    const auto ret = makeExecuteReturn(V1_0::ErrorStatus::GENERAL_FAILURE,
                                       V1_0::ErrorStatus::GENERAL_FAILURE);
    EXPECT_CALL(*kMockPreparedModel, execute(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, syncFence, dispatchCallback, timing] =
            kPreparedModel->executeFenced({}, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, syncFence.getNativeHandle());
    EXPECT_EQ(nullptr, dispatchCallback.get());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_1Test, executeFencedFailure) {
    // setup failure
    const auto ret = makeExecuteReturn(V1_0::ErrorStatus::GENERAL_FAILURE,
                                       V1_0::ErrorStatus::GENERAL_FAILURE);
    EXPECT_CALL(*kMockPreparedModel, execute(_, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, syncFence, dispatchCallback, timing] =
            kPreparedModel->executeFenced({}, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, syncFence.getNativeHandle());
    EXPECT_EQ(nullptr, dispatchCallback.get());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_2Test, executeFencedFailure) {
    // setup failure
    const auto ret =
            makeExecuteSynchronouslyReturn(V1_0::ErrorStatus::GENERAL_FAILURE, {}, kNoTiming);
    EXPECT_CALL(*kMockPreparedModel, executeSynchronously(_, _, _)).Times(1).WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, syncFence, dispatchCallback, timing] =
            kPreparedModel->executeFenced({}, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, syncFence.getNativeHandle());
    EXPECT_EQ(nullptr, dispatchCallback.get());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_3Test, executeFencedFailure) {
    // setup failure
    auto memory = allocateSharedMemory(4);
    hidl_handle fakeSyncFence(memory.handle());
    const sp<IFencedExecutionCallback> callback = new MockFencedExecutionCallback();
    const auto ret =
            makeExecuteFencedReturn(V1_3::ErrorStatus::GENERAL_FAILURE, fakeSyncFence, callback);
    EXPECT_CALL(*kMockPreparedModel, executeFenced(_, _, _, _, _, _, _))
            .Times(1)
            .WillOnce(Invoke(ret));

    // run test
    const auto [resultCode, syncFence, dispatchCallback, timing] =
            kPreparedModel->executeFenced({}, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, syncFence.getNativeHandle());
    EXPECT_EQ(nullptr, dispatchCallback.get());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_2Test, configureExecutionBurstFailure) {
    // setup failure
    const sp<MockBurstContext> burstContext = new MockBurstContext();
    const auto ret = makeConfigureExecutionBurst(V1_0::ErrorStatus::GENERAL_FAILURE, burstContext);
    EXPECT_CALL(*kMockPreparedModel, configureExecutionBurst(_, _, _, _))
            .Times(1)
            .WillOnce(Invoke(ret));

    // run test
    const auto executionBurstController =
            kPreparedModel->configureExecutionBurst(/*preferPowerOverLatency=*/false);

    // verify failure
    EXPECT_EQ(nullptr, executionBurstController);
}

TEST_F(VersionedIPreparedModelV1_3Test, configureExecutionBurstFailure) {
    // setup failure
    const sp<MockBurstContext> burstContext = new MockBurstContext();
    const auto ret = makeConfigureExecutionBurst(V1_0::ErrorStatus::GENERAL_FAILURE, burstContext);
    EXPECT_CALL(*kMockPreparedModel, configureExecutionBurst(_, _, _, _))
            .Times(1)
            .WillOnce(Invoke(ret));

    // run test
    const auto executionBurstController =
            kPreparedModel->configureExecutionBurst(/*preferPowerOverLatency=*/false);

    // verify failure
    EXPECT_EQ(nullptr, executionBurstController);
}

TEST_F(VersionedIPreparedModelV1_2Test, configureExecutionBurstNullptrError) {
    // setup failure
    const auto ret = makeConfigureExecutionBurst(V1_0::ErrorStatus::NONE, nullptr);
    EXPECT_CALL(*kMockPreparedModel, configureExecutionBurst(_, _, _, _))
            .Times(1)
            .WillOnce(Invoke(ret));

    // run test
    const auto executionBurstController =
            kPreparedModel->configureExecutionBurst(/*preferPowerOverLatency=*/false);

    // verify failure
    EXPECT_EQ(nullptr, executionBurstController);
}

TEST_F(VersionedIPreparedModelV1_3Test, configureExecutionBurstNullptrError) {
    // setup failure
    const auto ret = makeConfigureExecutionBurst(V1_0::ErrorStatus::NONE, nullptr);
    EXPECT_CALL(*kMockPreparedModel, configureExecutionBurst(_, _, _, _))
            .Times(1)
            .WillOnce(Invoke(ret));

    // run test
    const auto executionBurstController =
            kPreparedModel->configureExecutionBurst(/*preferPowerOverLatency=*/false);

    // verify failure
    EXPECT_EQ(nullptr, executionBurstController);
}

// Simulate transport failure

TEST_F(VersionedIPreparedModelV1_0Test, executeAsyncTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, execute(_, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_1Test, executeAsyncTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, execute(_, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_2Test, executeAsyncTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, execute_1_2(_, _, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_3Test, executeAsyncTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, execute_1_3(_, _, _, _, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_0Test, executePreferSyncTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, execute(_, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/true);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_1Test, executePreferSyncTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, execute(_, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/true);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_2Test, executePreferSyncTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, executeSynchronously(_, _, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/true);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_3Test, executePreferSyncTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, executeSynchronously_1_3(_, _, _, _, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/true);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_0Test, executeFencedTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, execute(_, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto [resultCode, syncFence, dispatchCallback, timing] =
            kPreparedModel->executeFenced({}, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, syncFence.getNativeHandle());
    EXPECT_EQ(nullptr, dispatchCallback.get());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_1Test, executeFencedTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, execute(_, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto [resultCode, syncFence, dispatchCallback, timing] =
            kPreparedModel->executeFenced({}, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, syncFence.getNativeHandle());
    EXPECT_EQ(nullptr, dispatchCallback.get());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_2Test, executeFencedTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, executeSynchronously(_, _, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto [resultCode, syncFence, dispatchCallback, timing] =
            kPreparedModel->executeFenced({}, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, syncFence.getNativeHandle());
    EXPECT_EQ(nullptr, dispatchCallback.get());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_3Test, executeFencedTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, executeFenced(_, _, _, _, _, _, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto [resultCode, syncFence, dispatchCallback, timing] =
            kPreparedModel->executeFenced({}, {}, {}, {}, {}, {});

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_OP_FAILED, resultCode);
    EXPECT_EQ(nullptr, syncFence.getNativeHandle());
    EXPECT_EQ(nullptr, dispatchCallback.get());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_2Test, configureExecutionBurstTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, configureExecutionBurst(_, _, _, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto executionBurstController =
            kPreparedModel->configureExecutionBurst(/*preferPowerOverLatency=*/false);

    // verify failure
    EXPECT_EQ(nullptr, executionBurstController);
}

TEST_F(VersionedIPreparedModelV1_3Test, configureExecutionBurstTransportFailure) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, configureExecutionBurst(_, _, _, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeGeneralTransportFailure));

    // run test
    const auto executionBurstController =
            kPreparedModel->configureExecutionBurst(/*preferPowerOverLatency=*/false);

    // verify failure
    EXPECT_EQ(nullptr, executionBurstController);
}

// Simulate service crash

TEST_F(VersionedIPreparedModelV1_0Test, executeAsyncLaunchCrash) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, execute(_, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeDeadObjectFailure));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_DEAD_OBJECT, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_1Test, executeAsyncLaunchCrash) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, execute(_, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeDeadObjectFailure));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_DEAD_OBJECT, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_2Test, executeAsyncLaunchCrash) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, execute_1_2(_, _, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeDeadObjectFailure));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_DEAD_OBJECT, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_3Test, executeAsyncLaunchCrash) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, execute_1_3(_, _, _, _, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeDeadObjectFailure));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_DEAD_OBJECT, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_2Test, executePreferSyncCrash) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, executeSynchronously(_, _, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeDeadObjectFailure));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/true);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_DEAD_OBJECT, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelV1_3Test, executePreferSyncCrash) {
    // setup failure
    EXPECT_CALL(*kMockPreparedModel, executeSynchronously_1_3(_, _, _, _, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(makeDeadObjectFailure));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/true);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_DEAD_OBJECT, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

TEST_F(VersionedIPreparedModelMockTest, executeAsyncReturnCrash) {
    // setup failure
    const auto ret = [this]() -> Return<V1_3::ErrorStatus> {
        kMockPreparedModel->simulateCrash();
        return V1_3::ErrorStatus::NONE;
    };
    EXPECT_CALL(*kMockPreparedModel, execute_1_3(_, _, _, _, _))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(ret));

    // run test
    const auto [resultCode, outputShapes, timing] =
            kPreparedModel->execute({}, {}, {}, {}, /*preferSynchronous=*/false);

    // verify failure
    EXPECT_EQ(ANEURALNETWORKS_DEAD_OBJECT, resultCode);
    EXPECT_EQ(0u, outputShapes.size());
    EXPECT_EQ(kNoTiming, timing);
}

}  // namespace
}  // namespace android::nn
