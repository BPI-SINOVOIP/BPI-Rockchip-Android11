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

#ifndef COMPUTEPIPE_RUNNER_CLIENT_INTERFACE_AIDLCLIENTIMPL_H_
#define COMPUTEPIPE_RUNNER_CLIENT_INTERFACE_AIDLCLIENTIMPL_H_
#include <aidl/android/automotive/computepipe/runner/BnPipeRunner.h>
#include <aidl/android/automotive/computepipe/runner/BnPipeDebugger.h>

#include <map>
#include <memory>
#include <string>

#include "ClientEngineInterface.h"
#include "MemHandle.h"
#include "Options.pb.h"
#include "types/GraphState.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace client_interface {
namespace aidl_client {

// RunnerInterface registers an IPipeRunner interface with computepipe router.
// RunnerInterface handles binder IPC calls and invokes appropriate callbacks.
class AidlClientImpl : public aidl::android::automotive::computepipe::runner::BnPipeRunner {
  public:
    explicit AidlClientImpl(const proto::Options graphOptions,
                            const std::shared_ptr<ClientEngineInterface>& engine)
        : mGraphOptions(graphOptions), mEngine(engine) {
    }

    ~AidlClientImpl() {
    }

    Status dispatchPacketToClient(int32_t streamId, const std::shared_ptr<MemHandle>& packetHandle);
    void setPipeDebugger(
        const std::shared_ptr<aidl::android::automotive::computepipe::runner::IPipeDebugger>&
        pipeDebugger);

    Status stateUpdateNotification(const GraphState newState);

    // Methods from android::automotive::computepipe::runner::BnPipeRunner
    ndk::ScopedAStatus init(
        const std::shared_ptr<aidl::android::automotive::computepipe::runner::IPipeStateCallback>&
            stateCb) override;
    ndk::ScopedAStatus getPipeDescriptor(
        aidl::android::automotive::computepipe::runner::PipeDescriptor* _aidl_return) override;
    ndk::ScopedAStatus setPipeInputSource(int32_t configId) override;
    ndk::ScopedAStatus setPipeOffloadOptions(int32_t configId) override;
    ndk::ScopedAStatus setPipeTermination(int32_t configId) override;
    ndk::ScopedAStatus setPipeOutputConfig(
        int32_t streamId, int32_t maxInFlightCount,
        const std::shared_ptr<aidl::android::automotive::computepipe::runner::IPipeStream>& handler)
        override;
    ndk::ScopedAStatus applyPipeConfigs() override;
    ndk::ScopedAStatus resetPipeConfigs() override;
    ndk::ScopedAStatus startPipe() override;
    ndk::ScopedAStatus stopPipe() override;
    ndk::ScopedAStatus doneWithPacket(int32_t bufferId, int32_t streamId) override;

    ndk::ScopedAStatus getPipeDebugger(
        std::shared_ptr<aidl::android::automotive::computepipe::runner::IPipeDebugger>*
        _aidl_return) override;

    ndk::ScopedAStatus releaseRunner() override;

    void clientDied();

  private:
    // Dispatch semantic data to client. Has copy semantics and does not expect
    // client to invoke doneWithPacket.
    Status DispatchSemanticData(int32_t streamId, const std::shared_ptr<MemHandle>& packetHandle);

    // Dispatch pixel data to client. Expects the client to invoke done with
    // packet.
    Status DispatchPixelData(int32_t streamId, const std::shared_ptr<MemHandle>& packetHandle);

    bool isClientInitDone();

    const proto::Options mGraphOptions;
    std::shared_ptr<ClientEngineInterface> mEngine;

    // If value of mClientStateChangeCallback is null pointer, client has not
    // invoked init.
    std::shared_ptr<aidl::android::automotive::computepipe::runner::IPipeStateCallback>
        mClientStateChangeCallback = nullptr;

    std::map<int, std::shared_ptr<aidl::android::automotive::computepipe::runner::IPipeStream>>
        mPacketHandlers;

    std::shared_ptr<aidl::android::automotive::computepipe::runner::IPipeDebugger> mPipeDebugger =
        nullptr;
};

}  // namespace aidl_client
}  // namespace client_interface
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // COMPUTEPIPE_RUNNER_CLIENT_INTERFACE_AIDLCLIENTIMPL_H_
