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

#ifndef COMPUTEPIPE_RUNNER_INPUT_ENGINE_INTERFACE_H
#define COMPUTEPIPE_RUNNER_INPUT_ENGINE_INTERFACE_H

#include "InputFrame.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace input_manager {

class InputEngineInterface {
  public:
    /**
     * Dispatch input frame to engine for consumption by the graph
     */
    virtual Status dispatchInputFrame(int streamId, int64_t timestamp, const InputFrame& frame) = 0;
    /**
     * Report Error Halt to Engine. Engine should report error to other
     * components.
     */
    virtual void notifyInputError() = 0;
    /**
     * Destructor
     */
    virtual ~InputEngineInterface() = default;
};

}  // namespace input_manager
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif
