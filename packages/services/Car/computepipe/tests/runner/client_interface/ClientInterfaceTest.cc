// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <aidl/android/automotive/computepipe/registry/BnClientInfo.h>
#include <aidl/android/automotive/computepipe/registry/IPipeQuery.h>
#include <aidl/android/automotive/computepipe/registry/IPipeRegistration.h>
#include <aidl/android/automotive/computepipe/runner/BnPipeStateCallback.h>
#include <aidl/android/automotive/computepipe/runner/BnPipeStream.h>
#include <aidl/android/automotive/computepipe/runner/PipeState.h>
#include <android/binder_manager.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "ConfigurationCommand.pb.h"
#include "ControlCommand.pb.h"
#include "MemHandle.h"
#include "MockEngine.h"
#include "MockMemHandle.h"
#include "MockRunnerEvent.h"
#include "Options.pb.h"
#include "ProfilingType.pb.h"
#include "runner/client_interface/AidlClient.h"
#include "runner/client_interface/include/ClientEngineInterface.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace client_interface {
namespace aidl_client {
namespace {

using ::aidl::android::automotive::computepipe::registry::BnClientInfo;
using ::aidl::android::automotive::computepipe::registry::IPipeQuery;
using ::aidl::android::automotive::computepipe::runner::BnPipeStateCallback;
using ::aidl::android::automotive::computepipe::runner::BnPipeStream;
using ::aidl::android::automotive::computepipe::runner::IPipeRunner;
using ::aidl::android::automotive::computepipe::runner::IPipeStateCallback;
using ::aidl::android::automotive::computepipe::runner::PacketDescriptor;
using ::aidl::android::automotive::computepipe::runner::PipeState;
using ::android::automotive::computepipe::runner::tests::MockRunnerEvent;
using ::android::automotive::computepipe::tests::MockMemHandle;
using ::ndk::ScopedAStatus;
using ::ndk::SharedRefBase;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;

const char kRegistryInterfaceName[] = "router";
int testIx = 0;

class StateChangeCallback : public BnPipeStateCallback {
  public:
    ScopedAStatus handleState(PipeState state) {
        mState = state;
        return ScopedAStatus::ok();
    }
    PipeState mState = PipeState::RESET;
};

class StreamCallback : public BnPipeStream {
  public:
    ScopedAStatus deliverPacket(const PacketDescriptor& in_packet) override {
        data = std::string(in_packet.data.begin(), in_packet.data.end());
        timestamp = in_packet.sourceTimeStampMillis;
        return ScopedAStatus::ok();
    }
    std::string data;
    uint64_t timestamp;
};

class ClientInfo : public BnClientInfo {
  public:
    ScopedAStatus getClientName(std::string* _aidl_return) {
        if (_aidl_return) {
            *_aidl_return = "ClientInfo";
            return ScopedAStatus::ok();
        }
        return ScopedAStatus::fromExceptionCode(EX_TRANSACTION_FAILED);
    }
};

class ClientInterface : public ::testing::Test {
  protected:
    void SetUp() override {
        const std::string graphName = "graph " + std::to_string(++testIx);
        proto::Options options;
        options.set_graph_name(graphName);
        mAidlClient = std::make_unique<AidlClient>(options, mEngine);

        // Register the instance with router.
        EXPECT_EQ(mAidlClient->activate(), Status::SUCCESS);

        // Init is not a blocking call, so sleep for 3 seconds to allow the runner to register with
        // router.
        sleep(3);

        // Retrieve router query instance from service manager.
        std::string instanceName =
            std::string() + IPipeQuery::descriptor + "/" + kRegistryInterfaceName;
        ndk::SpAIBinder binder(AServiceManager_getService(instanceName.c_str()));
        ASSERT_TRUE(binder.get() != nullptr);
        std::shared_ptr<IPipeQuery> queryService = IPipeQuery::fromBinder(binder);

        // Retrieve pipe runner instance from the router.
        std::shared_ptr<ClientInfo> clientInfo = ndk::SharedRefBase::make<ClientInfo>();
        ASSERT_TRUE(queryService->getPipeRunner(graphName, clientInfo, &mPipeRunner).isOk());
    }

    std::shared_ptr<tests::MockEngine> mEngine = std::make_unique<tests::MockEngine>();
    std::shared_ptr<AidlClient> mAidlClient = nullptr;
    std::shared_ptr<IPipeRunner> mPipeRunner = nullptr;
};

TEST_F(ClientInterface, TestSetConfiguration) {
    proto::ConfigurationCommand command;

    // Configure runner to return success.
    EXPECT_CALL(*mEngine, processClientConfigUpdate(_))
        .Times(AtLeast(4))
        .WillRepeatedly(DoAll(SaveArg<0>(&command), Return(Status::SUCCESS)));

    // Initialize pipe runner.
    std::shared_ptr<StateChangeCallback> stateCallback =
        ndk::SharedRefBase::make<StateChangeCallback>();
    EXPECT_TRUE(mPipeRunner->init(stateCallback).isOk());

    // Test that set input source returns ok status.
    EXPECT_TRUE(mPipeRunner->setPipeInputSource(1).isOk());
    EXPECT_EQ(command.has_set_input_source(), true);
    EXPECT_EQ(command.set_input_source().source_id(), 1);

    // Test that set offload option returns ok status.
    EXPECT_TRUE(mPipeRunner->setPipeOffloadOptions(5).isOk());
    EXPECT_EQ(command.has_set_offload_offload(), true);
    EXPECT_EQ(command.set_offload_offload().offload_option_id(), 5);

    // Test that set termination option returns ok status.
    EXPECT_TRUE(mPipeRunner->setPipeTermination(3).isOk());
    EXPECT_EQ(command.has_set_termination_option(), true);
    EXPECT_EQ(command.set_termination_option().termination_option_id(), 3);

    // Test that set output callback returns ok status.
    std::shared_ptr<StreamCallback> streamCb = ndk::SharedRefBase::make<StreamCallback>();
    EXPECT_TRUE(mPipeRunner->setPipeOutputConfig(0, 10, streamCb).isOk());
    EXPECT_EQ(command.has_set_output_stream(), true);
    EXPECT_EQ(command.set_output_stream().stream_id(), 0);
    EXPECT_EQ(command.set_output_stream().max_inflight_packets_count(), 10);

    // Release runner here. This should remove registry entry from router registry.
    mAidlClient.reset();
}

TEST_F(ClientInterface, TestSetConfigurationError) {
    proto::ConfigurationCommand command;

    // Configure runner to return error.
    EXPECT_CALL(*mEngine, processClientConfigUpdate(_))
        .Times(AtLeast(4))
        .WillRepeatedly(DoAll(SaveArg<0>(&command), Return(Status::INTERNAL_ERROR)));

    ScopedAStatus status;

    // Initialize pipe runner.
    std::shared_ptr<StateChangeCallback> stateCallback =
        ndk::SharedRefBase::make<StateChangeCallback>();
    EXPECT_TRUE(mPipeRunner->init(stateCallback).isOk());

    // Test that set input source returns error status.
    status = mPipeRunner->setPipeInputSource(1);
    EXPECT_EQ(status.getExceptionCode(), EX_TRANSACTION_FAILED);
    EXPECT_EQ(command.has_set_input_source(), true);
    EXPECT_EQ(command.set_input_source().source_id(), 1);

    // Test that set offload option returns error status.
    status = mPipeRunner->setPipeOffloadOptions(5);
    EXPECT_EQ(status.getExceptionCode(), EX_TRANSACTION_FAILED);
    EXPECT_EQ(command.has_set_offload_offload(), true);
    EXPECT_EQ(command.set_offload_offload().offload_option_id(), 5);

    // Test that set termination option returns error status.
    status = mPipeRunner->setPipeTermination(3);
    EXPECT_EQ(status.getExceptionCode(), EX_TRANSACTION_FAILED);
    EXPECT_EQ(command.has_set_termination_option(), true);
    EXPECT_EQ(command.set_termination_option().termination_option_id(), 3);

    // Test that set output callback returns error status.
    std::shared_ptr<StreamCallback> streamCb = ndk::SharedRefBase::make<StreamCallback>();
    status = mPipeRunner->setPipeOutputConfig(0, 10, streamCb);
    EXPECT_EQ(status.getExceptionCode(), EX_TRANSACTION_FAILED);
    EXPECT_EQ(command.has_set_output_stream(), true);
    EXPECT_EQ(command.set_output_stream().stream_id(), 0);
    EXPECT_EQ(command.set_output_stream().max_inflight_packets_count(), 10);

    // Release runner here. This should remove registry entry from router registry.
    mAidlClient.reset();
}

TEST_F(ClientInterface, TestControlCommands) {
    proto::ControlCommand command;
    // Configure runner to return success.
    EXPECT_CALL(*mEngine, processClientCommand(_))
        .Times(AtLeast(4))
        .WillRepeatedly(DoAll(SaveArg<0>(&command), Return(Status::SUCCESS)));

    // Initialize pipe runner.
    std::shared_ptr<StateChangeCallback> stateCallback =
        ndk::SharedRefBase::make<StateChangeCallback>();
    EXPECT_TRUE(mPipeRunner->init(stateCallback).isOk());

    // Test that apply-configs api returns ok status.
    EXPECT_TRUE(mPipeRunner->applyPipeConfigs().isOk());
    EXPECT_EQ(command.has_apply_configs(), true);

    // Test that set stop graph api returns ok status.
    EXPECT_TRUE(mPipeRunner->resetPipeConfigs().isOk());
    EXPECT_EQ(command.has_reset_configs(), true);

    // Test that set start graph api returns ok status.
    EXPECT_TRUE(mPipeRunner->startPipe().isOk());
    EXPECT_EQ(command.has_start_graph(), true);

    // Test that set stop graph api returns ok status.
    EXPECT_TRUE(mPipeRunner->stopPipe().isOk());
    EXPECT_EQ(command.has_stop_graph(), true);

    // Release runner here. This should remove registry entry from router registry.
    mAidlClient.reset();
}

TEST_F(ClientInterface, TestControlCommandsFailure) {
    proto::ControlCommand command;

    // Configure runner to return success.
    EXPECT_CALL(*mEngine, processClientCommand(_))
        .Times(AtLeast(4))
        .WillRepeatedly(DoAll(SaveArg<0>(&command), Return(Status::INTERNAL_ERROR)));
    ScopedAStatus status;

    // Initialize pipe runner.
    std::shared_ptr<StateChangeCallback> stateCallback =
        ndk::SharedRefBase::make<StateChangeCallback>();
    EXPECT_TRUE(mPipeRunner->init(stateCallback).isOk());

    // Test that apply-configs api returns error status.
    status = mPipeRunner->applyPipeConfigs();
    EXPECT_EQ(status.getExceptionCode(), EX_TRANSACTION_FAILED);
    EXPECT_EQ(command.has_apply_configs(), true);

    status = mPipeRunner->resetPipeConfigs();
    EXPECT_EQ(status.getExceptionCode(), EX_TRANSACTION_FAILED);
    EXPECT_EQ(command.has_reset_configs(), true);

    // Test that start graph api returns error status.
    status = mPipeRunner->startPipe();
    EXPECT_EQ(status.getExceptionCode(), EX_TRANSACTION_FAILED);
    EXPECT_EQ(command.has_start_graph(), true);

    // Test that stop graph api returns error status.
    status = mPipeRunner->stopPipe();
    EXPECT_EQ(status.getExceptionCode(), EX_TRANSACTION_FAILED);
    EXPECT_EQ(command.has_stop_graph(), true);

    // Release runner here. This should remove registry entry from router registry.
    mAidlClient.reset();
}

TEST_F(ClientInterface, TestFailureWithoutInit) {
    EXPECT_CALL(*mEngine, processClientConfigUpdate(_)).Times(0);
    EXPECT_CALL(*mEngine, processClientCommand(_)).Times(0);

    // Pipe runner is not initalized here, test that a configuration command returns error status.
    ScopedAStatus status;
    status = mPipeRunner->setPipeInputSource(1);
    EXPECT_EQ(status.getExceptionCode(), EX_ILLEGAL_STATE);

    // Test that a control command returns error status.
    status = mPipeRunner->applyPipeConfigs();
    EXPECT_EQ(status.getExceptionCode(), EX_ILLEGAL_STATE);
}

TEST_F(ClientInterface, TestStateChangeNotification) {
    EXPECT_CALL(*mEngine, processClientConfigUpdate(_)).Times(0);
    EXPECT_CALL(*mEngine, processClientCommand(_)).Times(0);

    // Initialize pipe runner.
    std::shared_ptr<StateChangeCallback> stateCallback =
        ndk::SharedRefBase::make<StateChangeCallback>();
    EXPECT_TRUE(mPipeRunner->init(stateCallback).isOk());

    // Test that config complete status is conveyed to client.
    std::map<int, int> m;
    ClientConfig config(0, 0, 0, m, proto::ProfilingType::DISABLED);
    config.setPhaseState(TRANSITION_COMPLETE);
    EXPECT_EQ(mAidlClient->handleConfigPhase(config), Status::SUCCESS);
    EXPECT_EQ(stateCallback->mState, PipeState::CONFIG_DONE);

    MockRunnerEvent event;
    EXPECT_CALL(event, isTransitionComplete()).Times(AnyNumber()).WillRepeatedly(Return(true));
    EXPECT_CALL(event, isPhaseEntry()).Times(AnyNumber()).WillRepeatedly(Return(false));

    // Test that reset status is conveyed to client.
    EXPECT_EQ(mAidlClient->handleResetPhase(event), Status::SUCCESS);
    EXPECT_EQ(stateCallback->mState, PipeState::RESET);

    // Test that execution start status is conveyed to client.
    EXPECT_EQ(mAidlClient->handleExecutionPhase(event), Status::SUCCESS);
    EXPECT_EQ(stateCallback->mState, PipeState::RUNNING);

    // Test that execution complete status is conveyed to client.
    EXPECT_EQ(mAidlClient->handleStopWithFlushPhase(event), Status::SUCCESS);
    EXPECT_EQ(stateCallback->mState, PipeState::DONE);

    // Test that execution error status is conveyed to client.
    EXPECT_EQ(mAidlClient->handleStopImmediatePhase(event), Status::SUCCESS);
    EXPECT_EQ(stateCallback->mState, PipeState::ERR_HALT);
}

TEST_F(ClientInterface, TestStateChangeToError) {
    EXPECT_CALL(*mEngine, processClientConfigUpdate(_)).Times(0);
    EXPECT_CALL(*mEngine, processClientCommand(_)).Times(0);

    // Initialize pipe runner.
    std::shared_ptr<StateChangeCallback> stateCallback =
        ndk::SharedRefBase::make<StateChangeCallback>();
    EXPECT_TRUE(mPipeRunner->init(stateCallback).isOk());

    // Test that error while applying config is conveyed to client.
    std::map<int, int> m;
    ClientConfig config(0, 0, 0, m, proto::ProfilingType::DISABLED);
    config.setPhaseState(ABORTED);
    EXPECT_EQ(mAidlClient->handleConfigPhase(config), Status::SUCCESS);
    EXPECT_EQ(stateCallback->mState, PipeState::ERR_HALT);

    MockRunnerEvent event;
    EXPECT_CALL(event, isTransitionComplete()).Times(AnyNumber()).WillRepeatedly(Return(false));
    EXPECT_CALL(event, isPhaseEntry()).Times(AnyNumber()).WillRepeatedly(Return(false));
    EXPECT_CALL(event, isAborted()).Times(AnyNumber()).WillRepeatedly(Return(true));

    // Test that error while starting pipe execution is conveyed to client.
    EXPECT_EQ(mAidlClient->handleExecutionPhase(event), Status::SUCCESS);
    EXPECT_EQ(stateCallback->mState, PipeState::ERR_HALT);
}

TEST_F(ClientInterface, TestPacketDelivery) {
    proto::ConfigurationCommand command;

    // Configure runner to return success.
    EXPECT_CALL(*mEngine, processClientConfigUpdate(_))
        .Times(1)
        .WillOnce(DoAll(SaveArg<0>(&command), Return(Status::SUCCESS)));

    // Initialize pipe runner.
    std::shared_ptr<StateChangeCallback> stateCallback =
        ndk::SharedRefBase::make<StateChangeCallback>();
    EXPECT_TRUE(mPipeRunner->init(stateCallback).isOk());

    // Set callback for stream id 0.
    std::shared_ptr<StreamCallback> streamCb = ndk::SharedRefBase::make<StreamCallback>();
    EXPECT_TRUE(mPipeRunner->setPipeOutputConfig(0, 10, streamCb).isOk());
    EXPECT_EQ(command.has_set_output_stream(), true);
    EXPECT_EQ(command.set_output_stream().stream_id(), 0);
    EXPECT_EQ(command.set_output_stream().max_inflight_packets_count(), 10);

    // Send a packet to client and verify the packet.
    std::shared_ptr<MockMemHandle> packet = std::make_unique<MockMemHandle>();
    uint64_t timestamp = 100;
    const std::string testData = "Test String.";
    EXPECT_CALL(*packet, getType())
        .Times(AtLeast(1))
        .WillRepeatedly(Return(proto::PacketType::SEMANTIC_DATA));
    EXPECT_CALL(*packet, getTimeStamp()).Times(AtLeast(1)).WillRepeatedly(Return(timestamp));
    EXPECT_CALL(*packet, getSize()).Times(AtLeast(1)).WillRepeatedly(Return(testData.size()));
    EXPECT_CALL(*packet, getData()).Times(AtLeast(1)).WillRepeatedly(Return(testData.c_str()));
    EXPECT_EQ(
        mAidlClient->dispatchPacketToClient(0, static_cast<std::shared_ptr<MemHandle>>(packet)),
        Status::SUCCESS);
    EXPECT_EQ(streamCb->data, packet->getData());
    EXPECT_EQ(streamCb->timestamp, packet->getTimeStamp());
}

}  // namespace
}  // namespace aidl_client
}  // namespace client_interface
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
