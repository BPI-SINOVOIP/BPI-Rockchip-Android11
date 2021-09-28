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

#ifndef COMPUTEPIPE_RUNNER_CLIENT_INTERFACE_DEBUGGERIMPL_H_
#define COMPUTEPIPE_RUNNER_CLIENT_INTERFACE_DEBUGGERIMPL_H_
#include <aidl/android/automotive/computepipe/runner/BnPipeDebugger.h>

#include <condition_variable>
#include <memory>
#include <mutex>

#include "ClientEngineInterface.h"
#include "RunnerComponent.h"
#include "types/GraphState.h"
#include "types/Status.h"
#include "Options.pb.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace client_interface {
namespace aidl_client {

// RunnerInterface registers an IPipeRunner interface with computepipe router.
// RunnerInterface handles binder IPC calls and invokes appropriate callbacks.
class DebuggerImpl : public aidl::android::automotive::computepipe::runner::BnPipeDebugger,
                     public RunnerComponentInterface  {
  public:
    explicit DebuggerImpl(const proto::Options graphOptions,
                          const std::shared_ptr<ClientEngineInterface>& engine)
            : mEngine(engine), mGraphOptions(graphOptions) {}

    // Methods from android::automotive::computepipe::runner::BnPipeDebugger
    ndk::ScopedAStatus setPipeProfileOptions(
        aidl::android::automotive::computepipe::runner::PipeProfilingType in_type) override;
    ndk::ScopedAStatus startPipeProfiling() override;
    ndk::ScopedAStatus stopPipeProfiling() override;
    ndk::ScopedAStatus getPipeProfilingInfo(
        aidl::android::automotive::computepipe::runner::ProfilingData* _aidl_return) override;
    ndk::ScopedAStatus releaseDebugger() override;

    // Methods from RunnerComponentInterface
    Status handleConfigPhase(const ClientConfig& e) override;
    Status handleExecutionPhase(const RunnerEvent& e) override;
    Status handleStopWithFlushPhase(const RunnerEvent& e) override;
    Status handleStopImmediatePhase(const RunnerEvent& e) override;
    Status handleResetPhase(const RunnerEvent& e) override;

    Status deliverGraphDebugInfo(const std::string& debugData);

  private:
    std::weak_ptr<ClientEngineInterface> mEngine;

    GraphState mGraphState = GraphState::RESET;
    aidl::android::automotive::computepipe::runner::PipeProfilingType mProfilingType;
    proto::Options mGraphOptions;
    aidl::android::automotive::computepipe::runner::ProfilingData mProfilingData;

    // Lock for mProfilingData.
    std::mutex mLock;
    std::condition_variable mWait;
    const std::string mProfilingDataDirName = "/data/computepipe/profiling";
};

}  // namespace aidl_client
}  // namespace client_interface
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // COMPUTEPIPE_RUNNER_CLIENT_INTERFACE_DEBUGGERIMPL_H_
