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

#include <fuzzer/FuzzedDataProvider.h>
#include <gmock/gmock.h>
#include "EventGenerator.h"
#include "MockEngine.h"
#include "StreamManager.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace stream_manager {

using android::automotive::computepipe::runner::generator::DefaultEvent;
using ::testing::Return;

namespace {

std::unique_ptr<StreamManager> SetupStreamManager(const std::shared_ptr<MockEngine>& engine) {
    StreamManagerFactory factory;
    proto::OutputConfig config;
    config.set_type(proto::PacketType::SEMANTIC_DATA);
    config.set_stream_name("semantic_stream");
    return factory.getStreamManager(config, engine, 0);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Set up process
    char* fakeData = reinterpret_cast<char*>(malloc(sizeof(char) * size));
    memcpy(fakeData, data, size * sizeof(char));

    std::shared_ptr<MemHandle> currentPacket;
    DefaultEvent e = DefaultEvent::generateEntryEvent(DefaultEvent::Phase::RUN);
    std::shared_ptr<testing::NiceMock<MockEngine>> mockEngine =
            std::make_shared<testing::NiceMock<MockEngine>>();
    std::unique_ptr<StreamManager> manager = SetupStreamManager(mockEngine);
    manager->handleExecutionPhase(e);

    // Probably not called
    ON_CALL((*mockEngine), dispatchPacket)
            .WillByDefault(
                    testing::DoAll(testing::SaveArg<0>(&currentPacket), (Return(Status::SUCCESS))));

    manager->queuePacket(fakeData, size, 0);

    if (currentPacket) {
        EXPECT_EQ(std::memcmp(currentPacket->getData(), fakeData, size), 0);
        currentPacket = nullptr;
    }

    free(fakeData);
    return 0;
}

}  // namespace
}  // namespace stream_manager
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
