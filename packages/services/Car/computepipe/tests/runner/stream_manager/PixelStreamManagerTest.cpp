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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vndk/hardware_buffer.h>
#include <android-base/logging.h>

#include "EventGenerator.h"
#include "InputFrame.h"
#include "MockEngine.h"
#include "OutputConfig.pb.h"
#include "PixelFormatUtils.h"
#include "PixelStreamManager.h"
#include "RunnerComponent.h"
#include "StreamEngineInterface.h"
#include "StreamManager.h"
#include "gmock/gmock-matchers.h"
#include "types/Status.h"

using ::android::automotive::computepipe::runner::RunnerComponentInterface;
using ::android::automotive::computepipe::runner::RunnerEvent;
using ::android::automotive::computepipe::runner::generator::DefaultEvent;
using ::testing::Contains;
using ::testing::Not;
using ::testing::Return;

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace stream_manager {
namespace {

MATCHER_P(ContainsDataFromFrame, data, "") {
    const uint8_t* dataPtr = data->getFramePtr();
    FrameInfo info = data->getFrameInfo();
    AHardwareBuffer_Desc desc;
    AHardwareBuffer_describe(arg, &desc);

    if (desc.width != info.width) {
        *result_listener << "Width does not match with values " << desc.width << " and "
                         << info.width;
        return false;
    }

    if (desc.height != info.height) {
        *result_listener << "Height does not match with values " << desc.height << " and "
                         << info.height;
        return false;
    }

    AHardwareBuffer_Format expectedFormat = PixelFormatToHardwareBufferFormat(info.format);
    if (expectedFormat != desc.format) {
        *result_listener << "Format does not match";
        return false;
    }

    void* mappedBuffer = nullptr;
    int err = AHardwareBuffer_lock(arg, AHARDWAREBUFFER_USAGE_CPU_READ_RARELY, -1, nullptr,
                                   &mappedBuffer);
    if (err != 0 || mappedBuffer == nullptr) {
        *result_listener << "Unable to lock the buffer for reading and comparing";
        return false;
    }

    bool dataMatched = true;
    int bytesPerPixel = numBytesPerPixel(expectedFormat);
    for (int y = 0; y < info.height; y++) {
        uint8_t* mappedRow = (uint8_t*)mappedBuffer + y * desc.stride * bytesPerPixel;
        if (memcmp(mappedRow, dataPtr + y * info.stride,
                   std::min(info.stride, desc.stride * bytesPerPixel))) {
            *result_listener << "Row " << y << " does not match";
            dataMatched = false;
            break;
        }
    }
    AHardwareBuffer_unlock(arg, nullptr);
    return dataMatched;
}

TEST(PixelMemHandleTest, SuccessfullyCreatesMemHandleOnFirstAttempt) {
    int bufferId = 10;
    int streamId = 1;
    uint64_t timestamp = 100;
    PixelMemHandle memHandle(bufferId, streamId, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN);

    EXPECT_EQ(memHandle.getBufferId(), bufferId);
    EXPECT_EQ(memHandle.getStreamId(), streamId);
    EXPECT_EQ(memHandle.getHardwareBuffer(), nullptr);

    std::vector<uint8_t> data(16 * 16 * 3, 0);
    InputFrame frame(16, 16, PixelFormat::RGB, 16 * 3, &data[0]);
    Status status = memHandle.setFrameData(timestamp, frame);
    EXPECT_EQ(status, Status::SUCCESS);
    ASSERT_NE(memHandle.getHardwareBuffer(), nullptr);

    AHardwareBuffer_Desc desc;
    AHardwareBuffer* buffer = memHandle.getHardwareBuffer();
    AHardwareBuffer_describe(buffer, &desc);
    EXPECT_EQ(desc.height, 16);
    EXPECT_EQ(desc.width, 16);
    EXPECT_EQ(desc.usage,
              AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN);
    EXPECT_EQ(desc.format, AHardwareBuffer_Format::AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM);

    EXPECT_THAT(buffer, ContainsDataFromFrame(&frame));
}

TEST(PixelMemHandleTest, FailsToOverwriteFrameDataWithDifferentImageFormat) {
    int bufferId = 10;
    int streamId = 1;
    uint64_t timestamp = 100;
    PixelMemHandle memHandle(bufferId, streamId, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN);

    EXPECT_EQ(memHandle.getBufferId(), bufferId);
    EXPECT_EQ(memHandle.getStreamId(), streamId);
    EXPECT_EQ(memHandle.getHardwareBuffer(), nullptr);

    uint8_t data[16 * 16 * 3] = {0};
    InputFrame frame(16, 16, PixelFormat::RGB, 16 * 3, &data[0]);
    Status status = memHandle.setFrameData(timestamp, frame);
    EXPECT_EQ(status, Status::SUCCESS);
    ASSERT_NE(memHandle.getHardwareBuffer(), nullptr);

    InputFrame frameWithNewFormat(16, 16, PixelFormat::RGBA, 16 * 4, nullptr);
    status = memHandle.setFrameData(timestamp, frameWithNewFormat);
    EXPECT_EQ(status, Status::INVALID_ARGUMENT);

    InputFrame frameWithNewDimensions(8, 8, PixelFormat::RGB, 8 * 3, nullptr);
    status = memHandle.setFrameData(timestamp, frameWithNewDimensions);
    EXPECT_EQ(status, Status::INVALID_ARGUMENT);
}

TEST(PixelMemHandleTest, SuccessfullyOverwritesOldData) {
    int bufferId = 10;
    int streamId = 1;
    uint64_t timestamp = 100;
    PixelMemHandle memHandle(bufferId, streamId, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN);

    EXPECT_EQ(memHandle.getBufferId(), bufferId);
    EXPECT_EQ(memHandle.getStreamId(), streamId);
    EXPECT_EQ(memHandle.getHardwareBuffer(), nullptr);

    std::vector<uint8_t> data(16 * 16 * 3, 0);
    InputFrame frame(16, 16, PixelFormat::RGB, 16 * 3, &data[0]);
    Status status = memHandle.setFrameData(timestamp, frame);
    EXPECT_EQ(status, Status::SUCCESS);
    ASSERT_NE(memHandle.getHardwareBuffer(), nullptr);
    EXPECT_THAT(memHandle.getHardwareBuffer(), ContainsDataFromFrame(&frame));

    std::vector<uint8_t> newData(16 * 16 * 3, 1);
    uint64_t newTimestamp = 200;
    InputFrame newFrame(16, 16, PixelFormat::RGB, 16 * 3, &newData[0]);
    memHandle.setFrameData(newTimestamp, newFrame);
    EXPECT_THAT(memHandle.getHardwareBuffer(), ContainsDataFromFrame(&newFrame));
    EXPECT_THAT(memHandle.getTimeStamp(), newTimestamp);
}

TEST(PixelMemHandleTest, CreatesBuffersOfExpectedFormats) {
    int bufferId = 10;
    int streamId = 1;
    uint64_t timestamp = 100;

    std::vector<uint8_t> rgbData(16 * 16 * 3, 10);
    InputFrame rgbFrame(16, 16, PixelFormat::RGB, 16 * 3, &rgbData[0]);
    PixelMemHandle rgbHandle(bufferId, streamId, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN);
    rgbHandle.setFrameData(timestamp, rgbFrame);
    EXPECT_THAT(rgbHandle.getHardwareBuffer(), ContainsDataFromFrame(&rgbFrame));

    std::vector<uint8_t> rgbaData(16 * 16 * 4, 20);
    InputFrame rgbaFrame(16, 16, PixelFormat::RGBA, 16 * 4, &rgbaData[0]);
    PixelMemHandle rgbaHandle(bufferId, streamId, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN);
    rgbaHandle.setFrameData(timestamp, rgbaFrame);
    EXPECT_THAT(rgbaHandle.getHardwareBuffer(), ContainsDataFromFrame(&rgbaFrame));

    std::vector<uint8_t> yData(16 * 16, 40);
    InputFrame yFrame(16, 16, PixelFormat::GRAY, 16, &yData[0]);
    PixelMemHandle yHandle(bufferId, streamId, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN);
    yHandle.setFrameData(timestamp, yFrame);
    EXPECT_THAT(yHandle.getHardwareBuffer(), ContainsDataFromFrame(&yFrame));
}

std::pair<std::shared_ptr<MockEngine>, std::unique_ptr<StreamManager>> CreateStreamManagerAndEngine(
    int maxInFlightPackets) {
    StreamManagerFactory factory;
    proto::OutputConfig outputConfig;
    outputConfig.set_type(proto::PacketType::PIXEL_DATA);
    outputConfig.set_stream_name("pixel_stream");
    std::shared_ptr<MockEngine> mockEngine = std::make_shared<MockEngine>();
    std::unique_ptr<StreamManager> manager =
        factory.getStreamManager(outputConfig, mockEngine, maxInFlightPackets);

    return std::pair(mockEngine, std::move(manager));
}

TEST(PixelStreamManagerTest, PacketQueueingProducesACallback) {
    // Create stream manager
    int maxInFlightPackets = 1;
    auto [mockEngine, manager] = CreateStreamManagerAndEngine(maxInFlightPackets);

    DefaultEvent e = DefaultEvent::generateEntryEvent(DefaultEvent::Phase::RUN);

    ASSERT_EQ(manager->handleExecutionPhase(e), Status::SUCCESS);
    std::vector<uint8_t> data(16 * 16 * 3, 100);
    InputFrame frame(16, 16, PixelFormat::RGB, 16 * 3, &data[0]);

    std::shared_ptr<MemHandle> memHandle;
    EXPECT_CALL((*mockEngine), dispatchPacket)
        .WillOnce(testing::DoAll(testing::SaveArg<0>(&memHandle), (Return(Status::SUCCESS))));

    EXPECT_EQ(manager->queuePacket(frame, 0), Status::SUCCESS);
    sleep(1);
    ASSERT_NE(memHandle, nullptr);
    EXPECT_THAT(memHandle->getHardwareBuffer(), ContainsDataFromFrame(&frame));
    EXPECT_THAT(memHandle->getTimeStamp(), 0);
    EXPECT_THAT(memHandle->getStreamId(), 0);
}

TEST(PixelStreamManagerTest, MorePacketsThanMaxInFlightAreNotDispatched) {
    int maxInFlightPackets = 3;
    auto [mockEngine, manager] = CreateStreamManagerAndEngine(maxInFlightPackets);

    DefaultEvent e = DefaultEvent::generateEntryEvent(DefaultEvent::Phase::RUN);

    ASSERT_EQ(manager->handleExecutionPhase(e), Status::SUCCESS);
    std::vector<uint8_t> data(16 * 16 * 3, 100);
    InputFrame frame(16, 16, PixelFormat::RGB, 16 * 3, &data[0]);
    std::set<int> activeBufferIds;

    std::shared_ptr<MemHandle> memHandle;
    EXPECT_CALL((*mockEngine), dispatchPacket)
        .Times(3)
        .WillRepeatedly(testing::DoAll(testing::SaveArg<0>(&memHandle), (Return(Status::SUCCESS))));

    EXPECT_EQ(manager->queuePacket(frame, 0), Status::SUCCESS);
    sleep(1);
    ASSERT_NE(memHandle, nullptr);
    EXPECT_THAT(memHandle->getHardwareBuffer(), ContainsDataFromFrame(&frame));
    EXPECT_THAT(memHandle->getTimeStamp(), 0);
    EXPECT_THAT(memHandle->getStreamId(), 0);
    activeBufferIds.insert(memHandle->getBufferId());

    EXPECT_EQ(manager->queuePacket(frame, 10), Status::SUCCESS);
    sleep(1);
    ASSERT_NE(memHandle, nullptr);
    EXPECT_THAT(memHandle->getHardwareBuffer(), ContainsDataFromFrame(&frame));
    EXPECT_THAT(memHandle->getTimeStamp(), 10);
    EXPECT_THAT(memHandle->getStreamId(), 0);
    EXPECT_THAT(activeBufferIds, Not(Contains(memHandle->getBufferId())));
    activeBufferIds.insert(memHandle->getBufferId());

    EXPECT_EQ(manager->queuePacket(frame, 20), Status::SUCCESS);
    sleep(1);
    ASSERT_NE(memHandle, nullptr);
    EXPECT_THAT(memHandle->getHardwareBuffer(), ContainsDataFromFrame(&frame));
    EXPECT_THAT(memHandle->getTimeStamp(), 20);
    EXPECT_THAT(memHandle->getStreamId(), 0);
    EXPECT_THAT(activeBufferIds, Not(Contains(memHandle->getBufferId())));
    activeBufferIds.insert(memHandle->getBufferId());

    // No new packet is produced as we have now reached the limit of number of
    // packets.
    EXPECT_EQ(manager->queuePacket(frame, 30), Status::SUCCESS);
    sleep(1);
    EXPECT_THAT(memHandle->getTimeStamp(), 20);
    EXPECT_THAT(activeBufferIds, Contains(memHandle->getBufferId()));
}

TEST(PixelStreamManagerTest, DoneWithPacketCallReleasesAPacket) {
    int maxInFlightPackets = 1;
    auto [mockEngine, manager] = CreateStreamManagerAndEngine(maxInFlightPackets);
    std::set<int> activeBufferIds;

    DefaultEvent e = DefaultEvent::generateEntryEvent(DefaultEvent::Phase::RUN);

    ASSERT_EQ(manager->handleExecutionPhase(e), Status::SUCCESS);
    std::vector<uint8_t> data(16 * 16 * 3, 100);
    InputFrame frame(16, 16, PixelFormat::RGB, 16 * 3, &data[0]);

    std::shared_ptr<MemHandle> memHandle;
    EXPECT_CALL((*mockEngine), dispatchPacket)
        .Times(2)
        .WillRepeatedly(testing::DoAll(testing::SaveArg<0>(&memHandle), (Return(Status::SUCCESS))));

    EXPECT_EQ(manager->queuePacket(frame, 10), Status::SUCCESS);
    sleep(1);
    ASSERT_NE(memHandle, nullptr);
    activeBufferIds.insert(memHandle->getBufferId());
    EXPECT_THAT(memHandle->getHardwareBuffer(), ContainsDataFromFrame(&frame));
    EXPECT_THAT(memHandle->getTimeStamp(), 10);
    EXPECT_THAT(memHandle->getStreamId(), 0);

    // Check that new packet has not been dispatched as the old packet has not been released yet.
    EXPECT_EQ(manager->queuePacket(frame, 20), Status::SUCCESS);
    sleep(1);
    ASSERT_NE(memHandle, nullptr);
    EXPECT_THAT(memHandle->getTimeStamp(), 10);

    EXPECT_THAT(manager->freePacket(memHandle->getBufferId()), Status::SUCCESS);
    EXPECT_EQ(manager->queuePacket(frame, 30), Status::SUCCESS);
    sleep(1);
    ASSERT_NE(memHandle, nullptr);
    EXPECT_THAT(memHandle->getTimeStamp(), 30);
}

TEST(PixelStreamManagerTest, EngineReceivesEndOfStreamCallbackOnStoppage) {
    int maxInFlightPackets = 1;
    auto [mockEngine, manager] = CreateStreamManagerAndEngine(maxInFlightPackets);

    DefaultEvent e = DefaultEvent::generateEntryEvent(DefaultEvent::Phase::RUN);

    ASSERT_EQ(manager->handleExecutionPhase(e), Status::SUCCESS);
    std::vector<uint8_t> data(16 * 16 * 3, 100);
    InputFrame frame(16, 16, PixelFormat::RGB, 16 * 3, &data[0]);

    std::shared_ptr<MemHandle> memHandle;
    EXPECT_CALL((*mockEngine), dispatchPacket)
        .WillOnce(testing::DoAll(testing::SaveArg<0>(&memHandle), (Return(Status::SUCCESS))));

    EXPECT_EQ(manager->queuePacket(frame, 10), Status::SUCCESS);
    sleep(1);
    ASSERT_NE(memHandle, nullptr);
    EXPECT_THAT(memHandle->getHardwareBuffer(), ContainsDataFromFrame(&frame));
    EXPECT_THAT(memHandle->getTimeStamp(), 10);
    EXPECT_THAT(memHandle->getStreamId(), 0);
    EXPECT_EQ(manager->handleStopImmediatePhase(e), Status::SUCCESS);
    // handleStopImmediatePhase is non-blocking call, so wait for manager to finish freeing the
    // packets.
    sleep(1);
}

TEST(PixelStreamManagerTest, MultipleFreePacketReleasesPacketAfterClone) {
    int maxInFlightPackets = 1;
    auto [mockEngine, manager] = CreateStreamManagerAndEngine(maxInFlightPackets);

    DefaultEvent e = DefaultEvent::generateEntryEvent(DefaultEvent::Phase::RUN);
    ASSERT_EQ(manager->handleExecutionPhase(e), Status::SUCCESS);
    std::vector<uint8_t> data(16 * 16 * 3, 100);
    InputFrame frame(16, 16, PixelFormat::RGB, 16 * 3, &data[0]);

    std::shared_ptr<MemHandle> memHandle;
    EXPECT_CALL((*mockEngine), dispatchPacket)
        .Times(2)
        .WillRepeatedly(testing::DoAll(testing::SaveArg<0>(&memHandle), (Return(Status::SUCCESS))));

    EXPECT_EQ(manager->queuePacket(frame, 10), Status::SUCCESS);
    sleep(1);
    ASSERT_NE(memHandle, nullptr);
    EXPECT_THAT(memHandle->getHardwareBuffer(), ContainsDataFromFrame(&frame));
    EXPECT_THAT(memHandle->getTimeStamp(), 10);
    EXPECT_THAT(memHandle->getStreamId(), 0);

    std::shared_ptr<MemHandle> clonedMemHandle = manager->clonePacket(memHandle);
    ASSERT_NE(clonedMemHandle, nullptr);
    EXPECT_THAT(clonedMemHandle->getTimeStamp(), 10);

    // Free packet once.
    EXPECT_THAT(manager->freePacket(memHandle->getBufferId()), Status::SUCCESS);

    // Check that new packet has not been dispatched as the old packet has not been released yet.
    EXPECT_EQ(manager->queuePacket(frame, 20), Status::SUCCESS);
    sleep(1);
    EXPECT_THAT(memHandle->getTimeStamp(), 10);

    // Free packet second time, this should dispatch new packet.
    EXPECT_THAT(manager->freePacket(memHandle->getBufferId()), Status::SUCCESS);
    EXPECT_EQ(manager->queuePacket(frame, 30), Status::SUCCESS);
    sleep(1);
    ASSERT_NE(memHandle, nullptr);
    EXPECT_THAT(memHandle->getTimeStamp(), 30);
}


}  // namespace
}  // namespace stream_manager
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
