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
#ifndef COMPUTEPIPE_RUNNER_GRAPH_INCLUDE_PREBUILTENGINEINTERFACE_H_
#define COMPUTEPIPE_RUNNER_GRAPH_INCLUDE_PREBUILTENGINEINTERFACE_H_

#include <functional>

#include "InputFrame.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace graph {

class PrebuiltEngineInterface {
  public:
    virtual ~PrebuiltEngineInterface() = default;

    virtual void DispatchPixelData(int streamId, int64_t timestamp,
                                   const runner::InputFrame& frame) = 0;

    virtual void DispatchSerializedData(int streamId, int64_t timestamp, std::string&&) = 0;

    virtual void DispatchGraphTerminationMessage(Status, std::string&&) = 0;
};

}  // namespace graph
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // COMPUTEPIPE_RUNNER_GRAPH_INCLUDE_PREBUILTENGINEINTERFACE_H_
