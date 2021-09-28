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

#ifndef COMPUTEPIPE_RUNNER_DEBUG_DISPLAY_MANAGER
#define COMPUTEPIPE_RUNNER_DEBUG_DISPLAY_MANAGER

#include "MemHandle.h"
#include "RunnerComponent.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace debug_display_manager {

class DebugDisplayManager : public RunnerComponentInterface {
  public:
    static constexpr char kDisplayId[] = "display_id:";

    /* Any args that a given display manager needs in order to configure itself. */
    virtual Status setArgs(std::string displayManagerArgs);

    /* Send a frame to debug display.
     * This is a non-blocking call. When the frame is ready to be freed, setFreePacketCallback()
     * should be invoked. */
    virtual Status displayFrame(const std::shared_ptr<MemHandle>& dataHandle) = 0;
    /* Free the packet (represented by buffer id). */
    virtual void setFreePacketCallback(std::function<Status (int bufferId)> freePacketCallback) = 0;
};

}  // namespace debug_display_manager
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // COMPUTEPIPE_RUNNER_DEBUG_DISPLAY_MANAGER
