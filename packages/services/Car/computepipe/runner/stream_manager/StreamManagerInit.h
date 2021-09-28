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

#ifndef COMPUTEPIPE_RUNNER_STREAM_MANAGER_INIT_H
#define COMPUTEPIPE_RUNNER_STREAM_MANAGER_INIT_H

#include <functional>
#include <memory>

#include "MemHandle.h"
#include "StreamEngineInterface.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace stream_manager {

class StreamManagerInit {
  public:
    virtual void setEngineInterface(std::shared_ptr<StreamEngineInterface> engine) = 0;
    /* Set Max in flight packets based on client specification */
    virtual Status setMaxInFlightPackets(uint32_t maxPackets) = 0;
    virtual ~StreamManagerInit() = default;
};

}  // namespace stream_manager
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
#endif
