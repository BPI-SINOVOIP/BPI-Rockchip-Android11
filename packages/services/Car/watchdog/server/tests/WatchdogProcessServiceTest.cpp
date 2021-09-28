/*
 * Copyright 2020 The Android Open Source Project
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

#include "WatchdogProcessService.h"

#include "gmock/gmock.h"

namespace android {
namespace automotive {
namespace watchdog {

using android::sp;
using binder::Status;
using ::testing::_;
using ::testing::Return;

namespace {

class MockBinder : public BBinder {
public:
    MOCK_METHOD(status_t, linkToDeath,
                (const sp<DeathRecipient>& recipient, void* cookie, uint32_t flags), (override));
    MOCK_METHOD(status_t, unlinkToDeath,
                (const wp<DeathRecipient>& recipient, void* cookie, uint32_t flags,
                 wp<DeathRecipient>* outRecipient),
                (override));
};

class MockCarWatchdogClient : public ICarWatchdogClientDefault {
public:
    MockCarWatchdogClient() { mBinder = new MockBinder(); }
    sp<MockBinder> getBinder() const { return mBinder; }

    MOCK_METHOD(IBinder*, onAsBinder, (), (override));

private:
    sp<MockBinder> mBinder;
};

class MockCarWatchdogMonitor : public ICarWatchdogMonitorDefault {
public:
    MockCarWatchdogMonitor() { mBinder = new MockBinder(); }
    sp<MockBinder> getBinder() const { return mBinder; }

    MOCK_METHOD(IBinder*, onAsBinder, (), (override));

private:
    sp<MockBinder> mBinder;
};

}  // namespace

class WatchdogProcessServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        sp<Looper> looper(Looper::prepare(/*opts=*/0));
        mWatchdogProcessService = new WatchdogProcessService(looper);
    }

    void TearDown() override { mWatchdogProcessService = nullptr; }

    sp<WatchdogProcessService> mWatchdogProcessService;
};

sp<MockCarWatchdogClient> createMockCarWatchdogClient(status_t linkToDeathResult) {
    sp<MockCarWatchdogClient> client = new MockCarWatchdogClient;
    sp<MockBinder> binder = client->getBinder();
    EXPECT_CALL(*binder, linkToDeath(_, nullptr, 0)).WillRepeatedly(Return(linkToDeathResult));
    EXPECT_CALL(*binder, unlinkToDeath(_, nullptr, 0, nullptr)).WillRepeatedly(Return(OK));
    EXPECT_CALL(*client, onAsBinder()).WillRepeatedly(Return(binder.get()));
    return client;
}

sp<MockCarWatchdogMonitor> createMockCarWatchdogMonitor(status_t linkToDeathResult) {
    sp<MockCarWatchdogMonitor> monitor = new MockCarWatchdogMonitor;
    sp<MockBinder> binder = monitor->getBinder();
    EXPECT_CALL(*binder, linkToDeath(_, nullptr, 0)).WillRepeatedly(Return(linkToDeathResult));
    EXPECT_CALL(*binder, unlinkToDeath(_, nullptr, 0, nullptr)).WillRepeatedly(Return(OK));
    EXPECT_CALL(*monitor, onAsBinder()).WillRepeatedly(Return(binder.get()));
    return monitor;
}

sp<MockCarWatchdogClient> expectNormalCarWatchdogClient() {
    return createMockCarWatchdogClient(OK);
}

sp<MockCarWatchdogClient> expectCarWatchdogClientBinderDied() {
    return createMockCarWatchdogClient(DEAD_OBJECT);
}

sp<MockCarWatchdogMonitor> expectNormalCarWatchdogMonitor() {
    return createMockCarWatchdogMonitor(OK);
}

sp<MockCarWatchdogMonitor> expectCarWatchdogMonitorBinderDied() {
    return createMockCarWatchdogMonitor(DEAD_OBJECT);
}

TEST_F(WatchdogProcessServiceTest, TestRegisterClient) {
    sp<MockCarWatchdogClient> client = expectNormalCarWatchdogClient();
    Status status =
            mWatchdogProcessService->registerClient(client, TimeoutLength::TIMEOUT_CRITICAL);
    ASSERT_TRUE(status.isOk()) << status;
    status = mWatchdogProcessService->registerClient(client, TimeoutLength::TIMEOUT_CRITICAL);
    ASSERT_TRUE(status.isOk()) << status;
}

TEST_F(WatchdogProcessServiceTest, TestUnregisterClient) {
    sp<MockCarWatchdogClient> client = expectNormalCarWatchdogClient();
    mWatchdogProcessService->registerClient(client, TimeoutLength::TIMEOUT_CRITICAL);
    Status status = mWatchdogProcessService->unregisterClient(client);
    ASSERT_TRUE(status.isOk()) << status;
    ASSERT_FALSE(mWatchdogProcessService->unregisterClient(client).isOk())
            << "Unregistering an unregistered client shoud return an error";
}

TEST_F(WatchdogProcessServiceTest, TestRegisterClient_BinderDied) {
    sp<MockCarWatchdogClient> client = expectCarWatchdogClientBinderDied();
    ASSERT_FALSE(
            mWatchdogProcessService->registerClient(client, TimeoutLength::TIMEOUT_CRITICAL).isOk())
            << "When linkToDeath fails, registerClient should return an error";
}

TEST_F(WatchdogProcessServiceTest, TestRegisterMediator) {
    sp<ICarWatchdogClient> mediator = expectNormalCarWatchdogClient();
    Status status = mWatchdogProcessService->registerMediator(mediator);
    ASSERT_TRUE(status.isOk()) << status;
    status = mWatchdogProcessService->registerMediator(mediator);
    ASSERT_TRUE(status.isOk()) << status;
}

TEST_F(WatchdogProcessServiceTest, TestRegisterMediator_BinderDied) {
    sp<MockCarWatchdogClient> mediator = expectCarWatchdogClientBinderDied();
    ASSERT_FALSE(mWatchdogProcessService->registerMediator(mediator).isOk())
            << "When linkToDeath fails, registerMediator should return an error";
}

TEST_F(WatchdogProcessServiceTest, TestUnregisterMediator) {
    sp<ICarWatchdogClient> mediator = expectNormalCarWatchdogClient();
    mWatchdogProcessService->registerMediator(mediator);
    Status status = mWatchdogProcessService->unregisterMediator(mediator);
    ASSERT_TRUE(status.isOk()) << status;
    ASSERT_FALSE(mWatchdogProcessService->unregisterMediator(mediator).isOk())
            << "Unregistering an unregistered mediator shoud return an error";
}

TEST_F(WatchdogProcessServiceTest, TestRegisterMonitor) {
    sp<ICarWatchdogMonitor> monitorOne = expectNormalCarWatchdogMonitor();
    sp<ICarWatchdogMonitor> monitorTwo = expectNormalCarWatchdogMonitor();
    Status status = mWatchdogProcessService->registerMonitor(monitorOne);
    ASSERT_TRUE(status.isOk()) << status;
    status = mWatchdogProcessService->registerMonitor(monitorOne);
    ASSERT_TRUE(status.isOk()) << status;
    status = mWatchdogProcessService->registerMonitor(monitorTwo);
    ASSERT_TRUE(status.isOk()) << status;
}

TEST_F(WatchdogProcessServiceTest, TestRegisterMonitor_BinderDied) {
    sp<MockCarWatchdogMonitor> monitor = expectCarWatchdogMonitorBinderDied();
    ASSERT_FALSE(mWatchdogProcessService->registerMonitor(monitor).isOk())
            << "When linkToDeath fails, registerMonitor should return an error";
}

TEST_F(WatchdogProcessServiceTest, TestUnregisterMonitor) {
    sp<ICarWatchdogMonitor> monitor = expectNormalCarWatchdogMonitor();
    mWatchdogProcessService->registerMonitor(monitor);
    Status status = mWatchdogProcessService->unregisterMonitor(monitor);
    ASSERT_TRUE(status.isOk()) << status;
    ASSERT_FALSE(mWatchdogProcessService->unregisterMonitor(monitor).isOk())
            << "Unregistering an unregistered monitor should return an error";
}

TEST_F(WatchdogProcessServiceTest, TestTellClientAlive) {
    sp<ICarWatchdogClient> client = expectNormalCarWatchdogClient();
    mWatchdogProcessService->registerClient(client, TimeoutLength::TIMEOUT_CRITICAL);
    ASSERT_FALSE(mWatchdogProcessService->tellClientAlive(client, 1234).isOk())
            << "tellClientAlive not synced with checkIfAlive should return an error";
}

TEST_F(WatchdogProcessServiceTest, TestTellMediatorAlive) {
    sp<ICarWatchdogClient> mediator = expectNormalCarWatchdogClient();
    mWatchdogProcessService->registerMediator(mediator);
    std::vector<int32_t> pids = {111, 222};
    ASSERT_FALSE(mWatchdogProcessService->tellMediatorAlive(mediator, pids, 1234).isOk())
            << "tellMediatorAlive not synced with checkIfAlive should return an error";
}

TEST_F(WatchdogProcessServiceTest, TestTellDumpFinished) {
    sp<ICarWatchdogMonitor> monitor = expectNormalCarWatchdogMonitor();
    ASSERT_FALSE(mWatchdogProcessService->tellDumpFinished(monitor, 1234).isOk())
            << "Unregistered monitor cannot call tellDumpFinished";
    mWatchdogProcessService->registerMonitor(monitor);
    Status status = mWatchdogProcessService->tellDumpFinished(monitor, 1234);
    ASSERT_TRUE(status.isOk()) << status;
}

}  // namespace watchdog
}  // namespace automotive
}  // namespace android
