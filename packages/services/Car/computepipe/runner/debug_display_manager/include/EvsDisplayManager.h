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

#ifndef COMPUTEPIPE_RUNNER_EVS_DISPLAY_MANAGER
#define COMPUTEPIPE_RUNNER_EVS_DISPLAY_MANAGER

#include <memory>
#include <mutex>
#include <thread>

#include "DebugDisplayManager.h"
#include "InputFrame.h"
#include "MemHandle.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace debug_display_manager {

class EvsDisplayManager : public DebugDisplayManager {
  public:
    /* Override DebugDisplayManager methods */
    /* Send a frame to debug display.
     * This is a non-blocking call. When the frame is ready to be freed, setFreePacketCallback()
     * should be invoked. */
    Status setArgs(std::string displayManagerArgs) override;
    Status displayFrame(const std::shared_ptr<MemHandle>& dataHandle) override;
    /* Free the packet (represented by buffer id). */
    void setFreePacketCallback(std::function<Status (int bufferId)> freePacketCallback) override;

    /* handle execution phase notification from Runner Engine */
    Status handleExecutionPhase(const RunnerEvent& e) override;
    /* handle a stop with flushing semantics phase notification from the engine */
    Status handleStopWithFlushPhase(const RunnerEvent& e) override;
    /* handle an immediate stop phase notification from the engine */
    Status handleStopImmediatePhase(const RunnerEvent& e) override;
    /* handle an engine notification to return to reset state */
    Status handleResetPhase(const RunnerEvent& e) override;

    ~EvsDisplayManager();

  private:
    void threadFn();

    void stopThread();

    // Variables to remember displayId if set through arguments.
    bool mOverrideDisplayId = false;
    int mDisplayId;

    std::thread mThread;
    bool mStopThread = false;

    // Lock for variables shared with the thread.
    std::mutex mLock;
    std::condition_variable mWait;
    std::shared_ptr<MemHandle> mNextFrame = nullptr;
    std::function<Status (int bufferId)> mFreePacketCallback = nullptr;
};

}  // namespace debug_display_manager
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // COMPUTEPIPE_RUNNER_EVS_DISPLAY_MANAGER
