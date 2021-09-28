// Copyright (C) 2020 The Android Open Source Project
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

#ifndef COMPUTEPIPE_RUNNER_STREAM_ENGINE_INTERFACE_H
#define COMPUTEPIPE_RUNNER_STREAM_ENGINE_INTERFACE_H

#include "MemHandle.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace stream_manager {

/**
 * Stream manager -> Engine interface.
 */
class StreamEngineInterface {
  public:
    /**
     * Does not block on the remote client to handle the packet.
     */
    virtual Status dispatchPacket(const std::shared_ptr<MemHandle>& outData) = 0;
    /**
     * After receiving StopWithFlush, once all outstanding packets have been
     * freed by the client, notify the engine of end of stream.
     * Should not be called in the thread that initiates the StopWithFlush, but
     * rather in a separate thread
     */
    virtual void notifyEndOfStream() = 0;
    /**
     * Notify engine of error
     */
    virtual void notifyError(std::string msg) = 0;
    virtual ~StreamEngineInterface() = default;
};

}  // namespace stream_manager
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif
