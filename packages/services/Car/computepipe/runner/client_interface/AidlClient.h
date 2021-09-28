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

#ifndef COMPUTEPIPE_RUNNER_CLIENT_INTERFACE_AIDLCLIENT_H_
#define COMPUTEPIPE_RUNNER_CLIENT_INTERFACE_AIDLCLIENT_H_

#include <memory>

#include "ClientInterface.h"
#include "AidlClientImpl.h"
#include "DebuggerImpl.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace client_interface {
namespace aidl_client {

class AidlClient : public ClientInterface {
  public:
    explicit AidlClient(const proto::Options graphOptions,
                        const std::shared_ptr<ClientEngineInterface>& engine)
            : mGraphOptions(graphOptions), mRunnerEngine(engine) {}
    /**
     * Override ClientInterface Functions
     */
    Status dispatchPacketToClient(int32_t streamId,
                                  const std::shared_ptr<MemHandle> packet) override;
    Status activate() override;
    Status deliverGraphDebugInfo(const std::string& debugData) override;
    /**
     * Override RunnerComponentInterface function
     */
    Status handleResetPhase(const RunnerEvent& e) override;
    Status handleConfigPhase(const ClientConfig& e) override;
    Status handleExecutionPhase(const RunnerEvent& e) override;
    Status handleStopWithFlushPhase(const RunnerEvent& e) override;
    Status handleStopImmediatePhase(const RunnerEvent& e) override;

    void routerDied();

    virtual ~AidlClient() = default;

  private:
    // Attempt to register pipe runner with router. Returns true on success.
    // This is a blocking API, calling thread will be blocked until router connection is
    // established or max attempts are made without success.
    void tryRegisterPipeRunner();
    const proto::Options mGraphOptions;
    std::shared_ptr<AidlClientImpl> mPipeRunner = nullptr;
    std::shared_ptr<DebuggerImpl> mPipeDebugger = nullptr;
    std::shared_ptr<ClientEngineInterface> mRunnerEngine;
};

}  // namespace aidl_client
}  // namespace client_interface
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
#endif  // COMPUTEPIPE_RUNNER_CLIENT_INTERFACE_AIDLCLIENT_H_
