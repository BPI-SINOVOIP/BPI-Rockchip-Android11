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
#include "EvsInputManager.h"

#include <chrono>
#include <map>
#include <shared_mutex>
#include <string>
#include <thread>

#include "AnalyzeUseCase.h"
#include "BaseAnalyzeCallback.h"
#include "InputConfig.pb.h"
#include "InputEngineInterface.h"
#include "Options.pb.h"

using ::android::automotive::evs::support::AnalyzeUseCase;
using ::android::automotive::evs::support::BaseAnalyzeCallback;

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace input_manager {

void AnalyzeCallback::analyze(const ::android::automotive::evs::support::Frame& frame) {
    std::shared_lock lock(mEngineInterfaceLock);
    if (mInputEngineInterface != nullptr) {
        auto time_point = std::chrono::system_clock::now();
        int64_t timestamp = std::chrono::time_point_cast<std::chrono::microseconds>(time_point)
                                .time_since_epoch()
                                .count();
        // Stride for hardware buffers is specified in pixels whereas for
        // InputFrame, it is specified in bytes. We therefore need to multiply
        // the stride by 4 for an RGBA frame.
        InputFrame inputFrame(frame.height, frame.width, PixelFormat::RGBA, frame.stride * 4,
                              frame.data);
        mInputEngineInterface->dispatchInputFrame(mInputStreamId, timestamp, inputFrame);
    }
}

void AnalyzeCallback::setEngineInterface(std::shared_ptr<InputEngineInterface> inputEngineInterface) {
    std::lock_guard lock(mEngineInterfaceLock);
    mInputEngineInterface = inputEngineInterface;
}

EvsInputManager::EvsInputManager(const proto::InputConfig& inputConfig,
                                 std::shared_ptr<InputEngineInterface> inputEngineInterface)
    : mInputEngineInterface(inputEngineInterface), mInputConfig(inputConfig) {
}

std::unique_ptr<EvsInputManager> EvsInputManager::createEvsInputManager(
    const proto::InputConfig& inputConfig,
    std::shared_ptr<InputEngineInterface> inputEngineInterface) {
    auto evsManager = std::make_unique<EvsInputManager>(inputConfig, inputEngineInterface);
    if (evsManager->initializeCameras() == Status::SUCCESS) {
        return evsManager;
    }

    return nullptr;
}

Status EvsInputManager::initializeCameras() {
    for (int i = 0; i < mInputConfig.input_stream_size(); i++) {
        // Verify that the stream type specified is a camera stream which is necessary for evs
        // manager.
        if (mInputConfig.input_stream(i).type() != proto::InputStreamConfig_InputType_CAMERA) {
            ALOGE("Evs stream manager expects the input stream type to be camera.");
            return Status::INVALID_ARGUMENT;
        }
        const std::string& cameraId = mInputConfig.input_stream(i).cam_config().cam_id();
        std::unique_ptr<AnalyzeCallback> analyzeCallback =
            std::make_unique<AnalyzeCallback>(mInputConfig.input_stream(i).stream_id());
        AnalyzeUseCase analyzeUseCase =
            AnalyzeUseCase::createDefaultUseCase(cameraId, analyzeCallback.get());
        mAnalyzeCallbacks.push_back(std::move(analyzeCallback));

        int streamId = mInputConfig.input_stream(i).stream_id();
        auto [it, result] = mEvsUseCases.try_emplace(std::move(streamId),
                                                     std::move(analyzeUseCase));
        if (!result) {
            // Multiple camera streams found to have the same camera id.
            ALOGE("Multiple camera streams have the same stream id.");
            return Status::INVALID_ARGUMENT;
        }
    }

    return Status::SUCCESS;
}

Status EvsInputManager::handleExecutionPhase(const RunnerEvent& e) {
    // Starting execution cannot be stopped in between. handleStopImmediate needs to be called.
    if (e.isAborted()) {
        return Status::INVALID_ARGUMENT;
    } else if (e.isTransitionComplete()) {
        return Status::SUCCESS;
    }

    if (mEvsUseCases.empty()) {
        ALOGE("No evs use cases configured. Verify that handleConfigPhase has been called");
        return Status::ILLEGAL_STATE;
    }

    // Start all the video streams.
    bool successfullyStartedAllCameras = true;
    for (auto& [streamId, evsUseCase] : mEvsUseCases) {
        if (!evsUseCase.startVideoStream()) {
            successfullyStartedAllCameras = false;
            ALOGE("Unable to successfully start all cameras");
            break;
        }
    }

    // If not all video streams have started successfully, stop the streams.
    if (!successfullyStartedAllCameras) {
        for (auto& [streamId, evsUseCase] : mEvsUseCases) {
            evsUseCase.stopVideoStream();
        }
        return Status::INTERNAL_ERROR;
    }

    // Set the input to engine interface for callbacks only when all the streams have successfully
    // started. This prevents any callback from going out unless all of the streams have started.
    for (auto& analyzeCallback : mAnalyzeCallbacks) {
        analyzeCallback->setEngineInterface(mInputEngineInterface);
    }

    return Status::SUCCESS;
}

Status EvsInputManager::handleStopImmediatePhase(const RunnerEvent& e) {
    if (e.isAborted()) {
        ALOGE(
            "Unable to abort immediate stopping of EVS cameras. Please start the video streams "
            "again if "
            "needed.");
    } else if (e.isTransitionComplete()) {
        return Status::SUCCESS;
    }

    // Reset all input engine interfaces so that callbacks stop going out even if there are evs
    // frames in flux.
    for (auto& analyzeCallback : mAnalyzeCallbacks) {
        analyzeCallback->setEngineInterface(nullptr);
    }

    for (auto& [streamId, evsUseCase] : mEvsUseCases) {
        evsUseCase.stopVideoStream();
    }

    return Status::SUCCESS;
}

Status EvsInputManager::handleStopWithFlushPhase(const RunnerEvent& e) {
    if (e.isAborted()) {
        ALOGE(
            "Unable to abort stopping and flushing of EVS cameras. Please start the video streams "
            "again if "
            "needed.");
    } else if (e.isTransitionComplete()) {
        return Status::SUCCESS;
    }

    for (auto& [streamId, evsUseCase] : mEvsUseCases) {
        evsUseCase.stopVideoStream();
    }
    return Status::SUCCESS;
}

Status EvsInputManager::handleResetPhase(const RunnerEvent& e) {
    if (e.isAborted()) {
        ALOGE("Unable to abort reset.");
        return Status::INVALID_ARGUMENT;
    }
    mEvsUseCases.clear();
    mAnalyzeCallbacks.clear();
    return Status::SUCCESS;
}

}  // namespace input_manager
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
