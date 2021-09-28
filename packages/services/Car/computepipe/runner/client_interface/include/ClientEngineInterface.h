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

#ifndef COMPUTEPIPE_RUNNER_CLIENT_INTERFACE_INCLUDE_CLIENTENGINEINTERFACE_H_
#define COMPUTEPIPE_RUNNER_CLIENT_INTERFACE_INCLUDE_CLIENTENGINEINTERFACE_H_

#include <memory>
#include <string>

#include "ConfigurationCommand.pb.h"
#include "ControlCommand.pb.h"
#include "MemHandle.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace client_interface {

/**
 * Communications from client component to runner engine
 */
class ClientEngineInterface {
  public:
    /**
     * Used by client to provide the engine with incremental client configuration
     * choices
     */
    virtual Status processClientConfigUpdate(const proto::ConfigurationCommand& command) = 0;
    /**
     * Used by client to provide the engine, the latest client command
     */
    virtual Status processClientCommand(const proto::ControlCommand& command) = 0;
    /**
     * Used by client to notify the engine of a consumed packet
     */
    virtual Status freePacket(int bufferId, int streamId) = 0;
    virtual ~ClientEngineInterface() = default;
};

}  // namespace client_interface
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // COMPUTEPIPE_RUNNER_CLIENT_INTERFACE_INCLUDE_CLIENTENGINEINTERFACE_H_
