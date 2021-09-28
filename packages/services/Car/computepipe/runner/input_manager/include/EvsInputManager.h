// Copyright 2020 The Android Open Source Project
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
#ifndef COMPUTEPIPE_RUNNER_INPUT_MANAGER_INCLUDE_EVSINPUTMANAGER_H_
#define COMPUTEPIPE_RUNNER_INPUT_MANAGER_INCLUDE_EVSINPUTMANAGER_H_

#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "AnalyzeUseCase.h"
#include "BaseAnalyzeCallback.h"
#include "InputConfig.pb.h"
#include "InputEngineInterface.h"
#include "InputManager.h"
#include "RunnerComponent.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace input_manager {

// Class that is used as a callback for EVS camera streams.
class AnalyzeCallback : public ::android::automotive::evs::support::BaseAnalyzeCallback {
  public:
    AnalyzeCallback(int inputStreamId) : mInputStreamId(inputStreamId) {
    }

    void analyze(const ::android::automotive::evs::support::Frame&) override;

    void setEngineInterface(std::shared_ptr<InputEngineInterface> inputEngineInterface);

    virtual ~AnalyzeCallback(){};

  private:
    std::shared_ptr<InputEngineInterface> mInputEngineInterface;
    std::shared_mutex mEngineInterfaceLock;
    const int mInputStreamId;
};

class EvsInputManager : public InputManager {
  public:
    explicit EvsInputManager(const proto::InputConfig& inputConfig,
                             std::shared_ptr<InputEngineInterface> inputEngineInterface);

    static std::unique_ptr<EvsInputManager> createEvsInputManager(
        const proto::InputConfig& inputConfig,
        std::shared_ptr<InputEngineInterface> inputEngineInterface);

    Status initializeCameras();

    Status handleExecutionPhase(const RunnerEvent& e) override;

    Status handleStopImmediatePhase(const RunnerEvent& e) override;

    Status handleStopWithFlushPhase(const RunnerEvent& e) override;

    Status handleResetPhase(const RunnerEvent& e) override;

  private:
    std::unordered_map<int, ::android::automotive::evs::support::AnalyzeUseCase> mEvsUseCases;
    std::vector<std::unique_ptr<AnalyzeCallback>> mAnalyzeCallbacks;
    std::shared_ptr<InputEngineInterface> mInputEngineInterface;
    const proto::InputConfig mInputConfig;
};

}  // namespace input_manager
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // COMPUTEPIPE_RUNNER_INPUT_MANAGER_INCLUDE_EVSINPUTMANAGER_H_
