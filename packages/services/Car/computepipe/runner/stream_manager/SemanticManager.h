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

#ifndef COMPUTEPIPE_RUNNER_STREAM_MANAGER_SEMANTIC_MANAGER_H
#define COMPUTEPIPE_RUNNER_STREAM_MANAGER_SEMANTIC_MANAGER_H

#include <mutex>

#include "InputFrame.h"
#include "OutputConfig.pb.h"
#include "RunnerComponent.h"
#include "StreamManager.h"
#include "StreamManagerInit.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace stream_manager {

class SemanticHandle : public MemHandle {
  public:
    static constexpr uint32_t kMaxSemanticDataSize = 1024;
    /**
     * Override mem handle methods
     */
    int getStreamId() const override;
    int getBufferId() const override;
    proto::PacketType getType() const override;
    uint64_t getTimeStamp() const override;
    uint32_t getSize() const override;
    const char* getData() const override;
    AHardwareBuffer* getHardwareBuffer() const override;
    /* set info for the memory. Make a copy */
    Status setMemInfo(int streamId, const char* data, uint32_t size, uint64_t timestamp,
                      const proto::PacketType& type);
    /* Destroy local copy */
    ~SemanticHandle();

  private:
    char* mData = nullptr;
    uint32_t mSize;
    uint64_t mTimestamp;
    proto::PacketType mType;
    int mStreamId;
};

class SemanticManager : public StreamManager, StreamManagerInit {
  public:
    void setEngineInterface(std::shared_ptr<StreamEngineInterface> engine) override;
    /* Set Max in flight packets based on client specification */
    Status setMaxInFlightPackets(uint32_t maxPackets) override;
    /* Free previously dispatched packet. Once client has confirmed usage */
    Status freePacket(int bufferId) override;
    /* Queue packet produced by graph stream */
    Status queuePacket(const char* data, const uint32_t size, uint64_t timestamp) override;
    /* Queues an image packet produced by graph stream */
    Status queuePacket(const InputFrame& inputData, uint64_t timestamp) override;
    /* Make a copy of the packet. */
    std::shared_ptr<MemHandle> clonePacket(std::shared_ptr<MemHandle> handle) override;
    /* Override handling of Runner Engine Events */
    void notifyEndOfStream();

    Status handleExecutionPhase(const RunnerEvent& e) override;
    Status handleStopWithFlushPhase(const RunnerEvent& e) override;
    Status handleStopImmediatePhase(const RunnerEvent& e) override;

    explicit SemanticManager(std::string name, int streamId, const proto::PacketType& type);
    ~SemanticManager() = default;

  private:
    std::mutex mStateLock;
    int mStreamId;
    std::shared_ptr<StreamEngineInterface> mEngine;
};
}  // namespace stream_manager
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
#endif
