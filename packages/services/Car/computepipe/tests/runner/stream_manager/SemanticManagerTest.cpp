/*
 * Copyright 2019 The Android Open Source Project
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "EventGenerator.h"
#include "MockEngine.h"
#include "OutputConfig.pb.h"
#include "RunnerComponent.h"
#include "StreamEngineInterface.h"
#include "StreamManager.h"
#include "gmock/gmock-matchers.h"
#include "types/Status.h"

using namespace android::automotive::computepipe::runner::stream_manager;
using namespace android::automotive::computepipe;
using android::automotive::computepipe::runner::RunnerComponentInterface;
using android::automotive::computepipe::runner::RunnerEvent;
using android::automotive::computepipe::runner::generator::DefaultEvent;
using testing::Return;

class SemanticManagerTest : public ::testing::Test {
  protected:
    static constexpr uint32_t kMaxSemanticDataSize = 1024;
    /**
     * Setup for the test fixture to initialize the semantic manager
     * After this, the semantic manager should be in RESET state.
     */
    void SetUp() override {
    }
    void TearDown() override {
        if (mCurrentPacket) {
            mCurrentPacket = nullptr;
            ;
        }
    }

    std::unique_ptr<StreamManager> SetupStreamManager(std::shared_ptr<MockEngine>& engine) {
        proto::OutputConfig config;
        config.set_type(proto::PacketType::SEMANTIC_DATA);
        config.set_stream_name("semantic_stream");

        return mFactory.getStreamManager(config, engine, 0);
    }
    StreamManagerFactory mFactory;
    std::shared_ptr<MemHandle> mCurrentPacket;
};

/**
 * Checks Packet Queing without start.
 * Checks Packet Queuing with bad arguments.
 * Checks successful packet queuing.
 */
TEST_F(SemanticManagerTest, PacketQueueTest) {
    DefaultEvent e = DefaultEvent::generateEntryEvent(DefaultEvent::Phase::RUN);
    std::shared_ptr<MockEngine> mockEngine = std::make_shared<MockEngine>();
    std::unique_ptr<StreamManager> manager = SetupStreamManager(mockEngine);
    ASSERT_EQ(manager->handleExecutionPhase(e), Status::SUCCESS);
    std::string fakeData("FakeData");
    uint32_t size = fakeData.size();
    EXPECT_EQ(manager->queuePacket(nullptr, size, 0), Status::INVALID_ARGUMENT);
    EXPECT_EQ(manager->queuePacket(fakeData.c_str(), kMaxSemanticDataSize + 1, 0),
              Status::INVALID_ARGUMENT);
    EXPECT_CALL((*mockEngine), dispatchPacket)
        .WillOnce(testing::DoAll(testing::SaveArg<0>(&mCurrentPacket), (Return(Status::SUCCESS))));

    manager->queuePacket(fakeData.c_str(), size, 0);
    EXPECT_STREQ(mCurrentPacket->getData(), fakeData.c_str());
}
