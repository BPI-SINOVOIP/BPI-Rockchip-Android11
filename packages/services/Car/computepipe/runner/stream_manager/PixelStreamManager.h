// Copyright 2020 The Android Open Source Project
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
#ifndef COMPUTEPIPE_RUNNER_STREAM_MANAGER_PIXEL_STREAM_MANAGER_H
#define COMPUTEPIPE_RUNNER_STREAM_MANAGER_PIXEL_STREAM_MANAGER_H

#include <vndk/hardware_buffer.h>

#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "InputFrame.h"
#include "MemHandle.h"
#include "RunnerComponent.h"
#include "StreamManager.h"
#include "StreamManagerInit.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace stream_manager {

class PixelMemHandle : public MemHandle {
  public:
    explicit PixelMemHandle(int bufferId, int streamId, int additionalUsageFlags = 0);

    virtual ~PixelMemHandle();

    // Delete copy and move constructors as well as copy and move assignment operators.
    PixelMemHandle(const PixelMemHandle&) = delete;
    PixelMemHandle(PixelMemHandle&&) = delete;
    PixelMemHandle operator=(const PixelMemHandle&) = delete;
    PixelMemHandle operator=(PixelMemHandle&&) = delete;

    /**
     * Overrides mem handle methods
     */
    int getStreamId() const override;
    int getBufferId() const override;
    proto::PacketType getType() const override;
    uint64_t getTimeStamp() const override;
    uint32_t getSize() const override;
    const char* getData() const override;
    AHardwareBuffer* getHardwareBuffer() const override;

    /* Sets frame info */
    Status setFrameData(uint64_t timestamp, const InputFrame& inputFrame);

  private:
    const int mBufferId;
    const int mStreamId;
    AHardwareBuffer_Desc mDesc;
    AHardwareBuffer* mBuffer;
    uint64_t mTimestamp;
    int mUsage;
};

class PixelStreamManager : public StreamManager, StreamManagerInit {
  public:
    void setEngineInterface(std::shared_ptr<StreamEngineInterface> engine) override;
    // Set Max in flight packets based on client specification
    Status setMaxInFlightPackets(uint32_t maxPackets) override;
    // Free previously dispatched packet. Once client has confirmed usage
    Status freePacket(int bufferId) override;
    // Queue packet produced by graph stream
    Status queuePacket(const char* data, const uint32_t size, uint64_t timestamp) override;
    // Queues pixel packet produced by graph stream
    Status queuePacket(const InputFrame& frame, uint64_t timestamp) override;
    /* Make a copy of the packet. */
    std::shared_ptr<MemHandle> clonePacket(std::shared_ptr<MemHandle> handle) override;

    Status handleExecutionPhase(const RunnerEvent& e) override;
    Status handleStopWithFlushPhase(const RunnerEvent& e) override;
    Status handleStopImmediatePhase(const RunnerEvent& e) override;

    explicit PixelStreamManager(std::string name, int streamId);
    ~PixelStreamManager() = default;

  private:
    void freeAllPackets();
    std::mutex mLock;
    std::mutex mStateLock;
    int mStreamId;
    uint32_t mMaxInFlightPackets;
    std::shared_ptr<StreamEngineInterface> mEngine;

    struct BufferMetadata {
        int outstandingRefCount;
        std::shared_ptr<PixelMemHandle> handle;
    };

    std::map<int, BufferMetadata> mBuffersInUse;
    std::vector<std::shared_ptr<PixelMemHandle>> mBuffersReady;
};

}  // namespace stream_manager
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // COMPUTEPIPE_RUNNER_STREAM_MANAGER_PIXEL_STREAM_MANAGER_H
