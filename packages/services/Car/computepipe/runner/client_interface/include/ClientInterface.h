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

#ifndef COMPUTEPIPE_RUNNER_CLIENT_INTERFACE_INCLUDE_CLIENTINTERFACE_H_
#define COMPUTEPIPE_RUNNER_CLIENT_INTERFACE_INCLUDE_CLIENTINTERFACE_H_

#include <memory>

#include "ClientEngineInterface.h"
#include "MemHandle.h"
#include "Options.pb.h"
#include "RunnerComponent.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace client_interface {

/**
 * ClientInterface: Is the runner component that represents the client of the
 * runner. This Component exposes communications from engine to the client.
 */

class ClientInterface : public RunnerComponentInterface {
  public:
    /**
     * Used by the runner engine to dispatch Graph output packes to the clients
     */
    virtual Status dispatchPacketToClient(int32_t streamId,
                                          const std::shared_ptr<MemHandle> packet) = 0;
    /**
     * Used by the runner engine to activate the client interface and open it to
     * external clients
     */
    virtual Status activate() = 0;

    /*
     *
     */
    virtual Status deliverGraphDebugInfo(const std::string& debugData) = 0;
    virtual ~ClientInterface() = default;
};

class ClientInterfaceFactory {
  public:
    std::unique_ptr<ClientInterface> createClientInterface(
        std::string ifaceName, const proto::Options graphOptions,
        const std::shared_ptr<ClientEngineInterface>& engine);
    ClientInterfaceFactory(const ClientInterfaceFactory&) = delete;
    ClientInterfaceFactory& operator=(const ClientInterfaceFactory&) = delete;
    ClientInterfaceFactory() = default;
};

}  // namespace client_interface
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // COMPUTEPIPE_RUNNER_CLIENT_INTERFACE_INCLUDE_CLIENTINTERFACE_H_
