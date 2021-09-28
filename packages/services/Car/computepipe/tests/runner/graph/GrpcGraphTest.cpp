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
#include <map>
#include <memory>
#include <string>
#include <thread>

#include <android-base/logging.h>
#include <grpc++/grpc++.h>

#include "ClientConfig.pb.h"
#include "GrpcGraphServerImpl.h"
#include "GrpcPrebuiltGraphService.grpc.pb.h"
#include "GrpcPrebuiltGraphService.pb.h"
#include "Options.pb.h"
#include "PrebuiltEngineInterface.h"
#include "PrebuiltGraph.h"
#include "RunnerComponent.h"
#include "gmock/gmock-matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "types/Status.h"

using ::android::automotive::computepipe::runner::ClientConfig;
using ::android::automotive::computepipe::runner::RunnerComponentInterface;
using ::android::automotive::computepipe::runner::RunnerEvent;
using ::testing::HasSubstr;

namespace android {
namespace automotive {
namespace computepipe {
namespace graph {
namespace {

class GrpcGraphTest : public ::testing::Test {
private:
    std::unique_ptr<GrpcGraphServerImpl> mServer;
    std::shared_ptr<PrebuiltEngineInterfaceImpl> mEngine;
    std::string mAddress = "[::]:10000";

public:
    std::unique_ptr<PrebuiltGraph> mGrpcGraph;

    void SetUp() override {
        mServer = std::make_unique<GrpcGraphServerImpl>(mAddress);
        std::thread t = std::thread([this]() { mServer->startServer(); });
        t.detach();
        std::this_thread::sleep_for(std::chrono::seconds(1));

        mEngine = std::make_shared<PrebuiltEngineInterfaceImpl>();
        mGrpcGraph = GetRemoteGraphFromAddress(mAddress, mEngine);
        ASSERT_TRUE(mGrpcGraph != nullptr);
        EXPECT_EQ(mGrpcGraph->GetSupportedGraphConfigs().graph_name(), kGraphName);
        EXPECT_EQ(mGrpcGraph->GetGraphType(), PrebuiltGraphType::REMOTE);
    }

    void TearDown() override { mServer.reset(); }

    bool waitForTermination() { return mEngine->waitForTermination(); }

    int numPacketsForStream(int streamId) { return mEngine->numPacketsForStream(streamId); }
};

class TestRunnerEvent : public runner::RunnerEvent {
    bool isPhaseEntry() const override { return true; }
    bool isTransitionComplete() const override { return false; }
    bool isAborted() const override { return false; }
    Status dispatchToComponent(const std::shared_ptr<runner::RunnerComponentInterface>&) override {
        return Status::SUCCESS;
    };
};

// Test to see if stop with flush produces exactly as many packets as expected. The number
// of packets produced by stopImmediate is variable as the number of packets already dispatched
// when stop is called is variable.
TEST_F(GrpcGraphTest, EndToEndTestOnStopWithFlush) {
    std::map<int, int> outputConfigs = {{5, 1}, {6, 1}};
    runner::ClientConfig clientConfig(0, 0, 0, outputConfigs, proto::ProfilingType::DISABLED);

    EXPECT_EQ(mGrpcGraph->handleConfigPhase(clientConfig), Status::SUCCESS);
    EXPECT_EQ(mGrpcGraph->GetGraphState(), PrebuiltGraphState::STOPPED);
    EXPECT_EQ(mGrpcGraph->GetStatus(), Status::SUCCESS);

    TestRunnerEvent e;
    EXPECT_EQ(mGrpcGraph->handleExecutionPhase(e), Status::SUCCESS);
    EXPECT_EQ(mGrpcGraph->GetGraphState(), PrebuiltGraphState::RUNNING);
    EXPECT_EQ(mGrpcGraph->GetStatus(), Status::SUCCESS);

    EXPECT_EQ(mGrpcGraph->handleStopWithFlushPhase(e), Status::SUCCESS);
    EXPECT_EQ(mGrpcGraph->GetGraphState(), PrebuiltGraphState::FLUSHING);
    EXPECT_EQ(mGrpcGraph->GetStatus(), Status::SUCCESS);

    EXPECT_TRUE(waitForTermination());
    EXPECT_EQ(mGrpcGraph->GetGraphState(), PrebuiltGraphState::STOPPED);
    EXPECT_EQ(mGrpcGraph->GetStatus(), Status::SUCCESS);
    EXPECT_EQ(numPacketsForStream(5), 5);
    EXPECT_EQ(numPacketsForStream(6), 6);
}

TEST_F(GrpcGraphTest, GraphStopCallbackProducedOnImmediateStop) {
    std::map<int, int> outputConfigs = {{5, 1}, {6, 1}};
    runner::ClientConfig clientConfig(0, 0, 0, outputConfigs, proto::ProfilingType::DISABLED);

    EXPECT_EQ(mGrpcGraph->handleConfigPhase(clientConfig), Status::SUCCESS);
    EXPECT_EQ(mGrpcGraph->GetGraphState(), PrebuiltGraphState::STOPPED);
    EXPECT_EQ(mGrpcGraph->GetStatus(), Status::SUCCESS);

    TestRunnerEvent e;
    EXPECT_EQ(mGrpcGraph->handleExecutionPhase(e), Status::SUCCESS);
    EXPECT_EQ(mGrpcGraph->GetGraphState(), PrebuiltGraphState::RUNNING);
    EXPECT_EQ(mGrpcGraph->GetStatus(), Status::SUCCESS);

    EXPECT_EQ(mGrpcGraph->handleStopImmediatePhase(e), Status::SUCCESS);
    EXPECT_EQ(mGrpcGraph->GetGraphState(), PrebuiltGraphState::STOPPED);
    EXPECT_EQ(mGrpcGraph->GetStatus(), Status::SUCCESS);

    EXPECT_TRUE(waitForTermination());
}

TEST_F(GrpcGraphTest, GraphStopCallbackProducedOnFlushedStopWithNoOutputStreams) {
    std::map<int, int> outputConfigs = {};
    runner::ClientConfig clientConfig(0, 0, 0, outputConfigs, proto::ProfilingType::DISABLED);
    EXPECT_EQ(mGrpcGraph->handleConfigPhase(clientConfig), Status::SUCCESS);
    EXPECT_EQ(mGrpcGraph->GetGraphState(), PrebuiltGraphState::STOPPED);
    EXPECT_EQ(mGrpcGraph->GetStatus(), Status::SUCCESS);

    TestRunnerEvent e;
    EXPECT_EQ(mGrpcGraph->handleExecutionPhase(e), Status::SUCCESS);
    EXPECT_EQ(mGrpcGraph->GetGraphState(), PrebuiltGraphState::RUNNING);
    EXPECT_EQ(mGrpcGraph->GetStatus(), Status::SUCCESS);

    EXPECT_EQ(mGrpcGraph->handleStopWithFlushPhase(e), Status::SUCCESS);
    EXPECT_EQ(mGrpcGraph->GetStatus(), Status::SUCCESS);

    EXPECT_TRUE(waitForTermination());
}

TEST_F(GrpcGraphTest, SetInputStreamsFailAsExpected) {
    runner::InputFrame frame(0, 0, static_cast<PixelFormat>(0), 0, nullptr);
    EXPECT_EQ(mGrpcGraph->SetInputStreamData(0, 0, ""), Status::FATAL_ERROR);
    EXPECT_EQ(mGrpcGraph->SetInputStreamPixelData(0, 0, frame), Status::FATAL_ERROR);
}

}  // namespace
}  // namespace graph
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
