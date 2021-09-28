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

#include "LocalPrebuiltGraph.h"

#include <android-base/logging.h>
#include <dlfcn.h>

#include <functional>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#include "ClientConfig.pb.h"
#include "InputFrame.h"
#include "PrebuiltGraph.h"
#include "RunnerComponent.h"
#include "prebuilt_interface.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace graph {

#define LOAD_FUNCTION(name)                                                        \
    {                                                                              \
        std::string func_name = std::string("PrebuiltComputepipeRunner_") + #name; \
        mPrebuiltGraphInstance->mFn##name =                                        \
                dlsym(mPrebuiltGraphInstance->mHandle, func_name.c_str());         \
        if (mPrebuiltGraphInstance->mFn##name == nullptr) {                        \
            initialized = false;                                                   \
            LOG(ERROR) << std::string(dlerror()) << std::endl;                     \
        }                                                                          \
    }

std::mutex LocalPrebuiltGraph::mCreationMutex;
LocalPrebuiltGraph* LocalPrebuiltGraph::mPrebuiltGraphInstance = nullptr;

// Function to confirm that there would be no further changes to the graph configuration. This
// needs to be called before starting the graph.
Status LocalPrebuiltGraph::handleConfigPhase(const runner::ClientConfig& e) {
    if (mGraphState.load() == PrebuiltGraphState::UNINITIALIZED) {
        return Status::ILLEGAL_STATE;
    }

    // handleConfigPhase is a blocking call, so abort call is pointless for this RunnerEvent.
    if (e.isAborted()) {
        return Status::INVALID_ARGUMENT;
    } else if (e.isTransitionComplete()) {
        return Status::SUCCESS;
    }

    std::string config = e.getSerializedClientConfig();
    auto mappedFn = (PrebuiltComputepipeRunner_ErrorCode(*)(const unsigned char*,
                                                            size_t))mFnUpdateGraphConfig;
    PrebuiltComputepipeRunner_ErrorCode errorCode =
            mappedFn(reinterpret_cast<const unsigned char*>(config.c_str()), config.length());
    if (errorCode != PrebuiltComputepipeRunner_ErrorCode::SUCCESS) {
        return static_cast<Status>(static_cast<int>(errorCode));
    }

    // Set the pixel stream callback function. The same function will be called for all requested
    // pixel output streams.
    if (mEngineInterface.lock() != nullptr) {
        auto pixelCallbackFn = (PrebuiltComputepipeRunner_ErrorCode(*)(
                void (*)(void* cookie, int, int64_t, const uint8_t* pixels, int width, int height,
                         int step, int format)))mFnSetOutputPixelStreamCallback;
        PrebuiltComputepipeRunner_ErrorCode errorCode =
                pixelCallbackFn(LocalPrebuiltGraph::OutputPixelStreamCallbackFunction);
        if (errorCode != PrebuiltComputepipeRunner_ErrorCode::SUCCESS) {
            return static_cast<Status>(static_cast<int>(errorCode));
        }

        // Set the serialized stream callback function. The same callback function will be invoked
        // for all requested serialized output streams.
        auto streamCallbackFn = (PrebuiltComputepipeRunner_ErrorCode(*)(
                void (*)(void* cookie, int, int64_t, const unsigned char*,
                         size_t)))mFnSetOutputStreamCallback;
        errorCode = streamCallbackFn(LocalPrebuiltGraph::OutputStreamCallbackFunction);
        if (errorCode != PrebuiltComputepipeRunner_ErrorCode::SUCCESS) {
            return static_cast<Status>(static_cast<int>(errorCode));
        }

        // Set the callback function for when the graph terminates.
        auto terminationCallback = (PrebuiltComputepipeRunner_ErrorCode(*)(
                void (*)(void* cookie, const unsigned char*,
                         size_t)))mFnSetGraphTerminationCallback;
        errorCode = terminationCallback(LocalPrebuiltGraph::GraphTerminationCallbackFunction);
        if (errorCode != PrebuiltComputepipeRunner_ErrorCode::SUCCESS) {
            return static_cast<Status>(static_cast<int>(errorCode));
        }
    }

    return Status::SUCCESS;
}

// Starts the graph.
Status LocalPrebuiltGraph::handleExecutionPhase(const runner::RunnerEvent& e) {
    if (mGraphState.load() != PrebuiltGraphState::STOPPED) {
        return Status::ILLEGAL_STATE;
    }

    if (e.isAborted()) {
        // Starting the graph is a blocking call and cannot be aborted in between.
        return Status::INVALID_ARGUMENT;
    } else if (e.isTransitionComplete()) {
        return Status::SUCCESS;
    }

    auto mappedFn = (PrebuiltComputepipeRunner_ErrorCode(*)(void*))mFnStartGraphExecution;
    PrebuiltComputepipeRunner_ErrorCode errorCode = mappedFn(reinterpret_cast<void*>(this));
    if (errorCode == PrebuiltComputepipeRunner_ErrorCode::SUCCESS) {
        mGraphState.store(PrebuiltGraphState::RUNNING);
    }
    return static_cast<Status>(static_cast<int>(errorCode));
}

// Stops the graph while letting the graph flush output packets in flight.
Status LocalPrebuiltGraph::handleStopWithFlushPhase(const runner::RunnerEvent& e) {
    if (mGraphState.load() != PrebuiltGraphState::RUNNING) {
        return Status::ILLEGAL_STATE;
    }

    if (e.isAborted()) {
        return Status::INVALID_ARGUMENT;
    } else if (e.isTransitionComplete()) {
        return Status::SUCCESS;
    }

    return StopGraphExecution(/* flushOutputFrames = */ true);
}

// Stops the graph and cancels all the output packets.
Status LocalPrebuiltGraph::handleStopImmediatePhase(const runner::RunnerEvent& e) {
    if (mGraphState.load() != PrebuiltGraphState::RUNNING) {
        return Status::ILLEGAL_STATE;
    }

    if (e.isAborted()) {
        return Status::INVALID_ARGUMENT;
    } else if (e.isTransitionComplete()) {
        return Status::SUCCESS;
    }

    return StopGraphExecution(/* flushOutputFrames = */ false);
}

Status LocalPrebuiltGraph::handleResetPhase(const runner::RunnerEvent& e) {
    if (mGraphState.load() != PrebuiltGraphState::STOPPED) {
        return Status::ILLEGAL_STATE;
    }

    if (e.isAborted()) {
        return Status::INVALID_ARGUMENT;
    } else if (e.isTransitionComplete()) {
        return Status::SUCCESS;
    }

    auto mappedFn = (PrebuiltComputepipeRunner_ErrorCode(*)())mFnResetGraph;
    mappedFn();
    return Status::SUCCESS;
}

LocalPrebuiltGraph* LocalPrebuiltGraph::GetPrebuiltGraphFromLibrary(
        const std::string& prebuilt_library,
        std::weak_ptr<PrebuiltEngineInterface> engineInterface) {
    std::unique_lock<std::mutex> lock(LocalPrebuiltGraph::mCreationMutex);
    if (mPrebuiltGraphInstance == nullptr) {
        mPrebuiltGraphInstance = new LocalPrebuiltGraph();
    }
    if (mPrebuiltGraphInstance->mGraphState.load() != PrebuiltGraphState::UNINITIALIZED) {
        return mPrebuiltGraphInstance;
    }
    mPrebuiltGraphInstance->mHandle = dlopen(prebuilt_library.c_str(), RTLD_NOW);

    if (mPrebuiltGraphInstance->mHandle) {
        bool initialized = true;

        // Load config and version number first.
        const unsigned char* (*getVersionFn)() =
                (const unsigned char* (*)())dlsym(mPrebuiltGraphInstance->mHandle,
                                                  "PrebuiltComputepipeRunner_GetVersion");
        if (getVersionFn != nullptr) {
            mPrebuiltGraphInstance->mGraphVersion =
                    std::string(reinterpret_cast<const char*>(getVersionFn()));
        } else {
            LOG(ERROR) << std::string(dlerror());
            initialized = false;
        }

        void (*getSupportedGraphConfigsFn)(const void**, size_t*) =
                (void (*)(const void**,
                          size_t*))dlsym(mPrebuiltGraphInstance->mHandle,
                                         "PrebuiltComputepipeRunner_GetSupportedGraphConfigs");
        if (getSupportedGraphConfigsFn != nullptr) {
            size_t graphConfigSize;
            const void* graphConfig;

            getSupportedGraphConfigsFn(&graphConfig, &graphConfigSize);

            if (graphConfigSize > 0) {
                initialized &= mPrebuiltGraphInstance->mGraphConfig.ParseFromString(
                        std::string(reinterpret_cast<const char*>(graphConfig), graphConfigSize));
            }
        } else {
            LOG(ERROR) << std::string(dlerror());
            initialized = false;
        }

        // Null callback interface is not acceptable.
        if (initialized && engineInterface.lock() == nullptr) {
            initialized = false;
        }

        LOAD_FUNCTION(GetErrorCode);
        LOAD_FUNCTION(GetErrorMessage);
        LOAD_FUNCTION(ResetGraph);
        LOAD_FUNCTION(UpdateGraphConfig);
        LOAD_FUNCTION(SetInputStreamData);
        LOAD_FUNCTION(SetInputStreamPixelData);
        LOAD_FUNCTION(SetOutputStreamCallback);
        LOAD_FUNCTION(SetOutputPixelStreamCallback);
        LOAD_FUNCTION(SetGraphTerminationCallback);
        LOAD_FUNCTION(StartGraphExecution);
        LOAD_FUNCTION(StopGraphExecution);
        LOAD_FUNCTION(StartGraphProfiling);
        LOAD_FUNCTION(StopGraphProfiling);
        LOAD_FUNCTION(GetDebugInfo);

        // This is the only way to create this object and there is already a
        // lock around object creation, so no need to hold the graphState lock
        // here.
        if (initialized) {
            mPrebuiltGraphInstance->mGraphState.store(PrebuiltGraphState::STOPPED);
            mPrebuiltGraphInstance->mEngineInterface = engineInterface;
        }
    }

    return mPrebuiltGraphInstance;
}

LocalPrebuiltGraph::~LocalPrebuiltGraph() {
    if (mHandle) {
        dlclose(mHandle);
    }
}

Status LocalPrebuiltGraph::GetStatus() const {
    if (mGraphState.load() == PrebuiltGraphState::UNINITIALIZED) {
        return Status::ILLEGAL_STATE;
    }

    auto mappedFn = (PrebuiltComputepipeRunner_ErrorCode(*)())mFnGetErrorCode;
    PrebuiltComputepipeRunner_ErrorCode errorCode = mappedFn();
    return static_cast<Status>(static_cast<int>(errorCode));
}

std::string LocalPrebuiltGraph::GetErrorMessage() const {
    if (mGraphState.load() == PrebuiltGraphState::UNINITIALIZED) {
        return "Graph has not been initialized";
    }
    auto mappedFn = (PrebuiltComputepipeRunner_ErrorCode(*)(unsigned char*, size_t,
                                                            size_t*))mFnGetErrorMessage;
    size_t errorMessageSize = 0;

    PrebuiltComputepipeRunner_ErrorCode errorCode = mappedFn(nullptr, 0, &errorMessageSize);
    std::vector<unsigned char> errorMessage(errorMessageSize);

    errorCode = mappedFn(&errorMessage[0], errorMessage.size(), &errorMessageSize);
    if (errorCode != PrebuiltComputepipeRunner_ErrorCode::SUCCESS) {
        return "Unable to get error message from the graph.";
    }

    return std::string(reinterpret_cast<char*>(&errorMessage[0]),
                       reinterpret_cast<char*>(&errorMessage[0]) + errorMessage.size());
}

Status LocalPrebuiltGraph::SetInputStreamData(int streamIndex, int64_t timestamp,
                                              const std::string& streamData) {
    if (mGraphState.load() == PrebuiltGraphState::UNINITIALIZED) {
        return Status::ILLEGAL_STATE;
    }
    auto mappedFn = (PrebuiltComputepipeRunner_ErrorCode(*)(int, int64_t, const unsigned char*,
                                                            size_t))mFnSetInputStreamData;
    PrebuiltComputepipeRunner_ErrorCode errorCode =
            mappedFn(streamIndex, timestamp,
                     reinterpret_cast<const unsigned char*>(streamData.c_str()),
                     streamData.length());
    return static_cast<Status>(static_cast<int>(errorCode));
}

Status LocalPrebuiltGraph::SetInputStreamPixelData(int streamIndex, int64_t timestamp,
                                                   const runner::InputFrame& inputFrame) {
    if (mGraphState.load() == PrebuiltGraphState::UNINITIALIZED) {
        return Status::ILLEGAL_STATE;
    }

    auto mappedFn =
            (PrebuiltComputepipeRunner_ErrorCode(*)(int, int64_t, const uint8_t*, int, int, int,
                                                    PrebuiltComputepipeRunner_PixelDataFormat))
                    mFnSetInputStreamPixelData;
    PrebuiltComputepipeRunner_ErrorCode errorCode =
            mappedFn(streamIndex, timestamp, inputFrame.getFramePtr(),
                     inputFrame.getFrameInfo().width, inputFrame.getFrameInfo().height,
                     inputFrame.getFrameInfo().stride,
                     static_cast<PrebuiltComputepipeRunner_PixelDataFormat>(
                             static_cast<int>(inputFrame.getFrameInfo().format)));
    return static_cast<Status>(static_cast<int>(errorCode));
}

Status LocalPrebuiltGraph::StopGraphExecution(bool flushOutputFrames) {
    auto mappedFn = (PrebuiltComputepipeRunner_ErrorCode(*)(bool))mFnStopGraphExecution;
    PrebuiltComputepipeRunner_ErrorCode errorCode = mappedFn(flushOutputFrames);
    if (errorCode == PrebuiltComputepipeRunner_ErrorCode::SUCCESS) {
        mGraphState.store(flushOutputFrames ? PrebuiltGraphState::FLUSHING
                                            : PrebuiltGraphState::STOPPED);
    }
    return static_cast<Status>(static_cast<int>(errorCode));
}

Status LocalPrebuiltGraph::StartGraphProfiling() {
    auto mappedFn = (PrebuiltComputepipeRunner_ErrorCode(*)())mFnStartGraphProfiling;
    PrebuiltComputepipeRunner_ErrorCode errorCode = mappedFn();
    return static_cast<Status>(static_cast<int>(errorCode));
}

Status LocalPrebuiltGraph::StopGraphProfiling() {
    auto mappedFn = (PrebuiltComputepipeRunner_ErrorCode(*)())mFnStopGraphProfiling;
    PrebuiltComputepipeRunner_ErrorCode errorCode = mappedFn();
    return static_cast<Status>(static_cast<int>(errorCode));
}

std::string LocalPrebuiltGraph::GetDebugInfo() {
    if (mGraphState.load() == PrebuiltGraphState::UNINITIALIZED) {
        return "";
    }
    auto mappedFn = (PrebuiltComputepipeRunner_ErrorCode(*)(unsigned char*, size_t,
                                                            size_t*))mFnGetDebugInfo;

    size_t debugInfoSize = 0;
    PrebuiltComputepipeRunner_ErrorCode errorCode = mappedFn(nullptr, 0, &debugInfoSize);
    std::vector<unsigned char> debugInfo(debugInfoSize);

    errorCode = mappedFn(&debugInfo[0], debugInfo.size(), &debugInfoSize);
    if (errorCode != PrebuiltComputepipeRunner_ErrorCode::SUCCESS) {
        return "";
    }

    return std::string(reinterpret_cast<char*>(&debugInfo[0]),
                       reinterpret_cast<char*>(&debugInfo[0]) + debugInfo.size());
}

void LocalPrebuiltGraph::OutputStreamCallbackFunction(void* cookie, int streamIndex,
                                                      int64_t timestamp, const unsigned char* data,
                                                      size_t data_size) {
    LocalPrebuiltGraph* graph = reinterpret_cast<LocalPrebuiltGraph*>(cookie);
    CHECK(graph);
    std::shared_ptr<PrebuiltEngineInterface> engineInterface = graph->mEngineInterface.lock();
    if (engineInterface != nullptr) {
        engineInterface->DispatchSerializedData(streamIndex, timestamp,
                                                std::string(data, data + data_size));
    }
}

void LocalPrebuiltGraph::OutputPixelStreamCallbackFunction(void* cookie, int streamIndex,
                                                           int64_t timestamp, const uint8_t* pixels,
                                                           int width, int height, int step,
                                                           int format) {
    LocalPrebuiltGraph* graph = reinterpret_cast<LocalPrebuiltGraph*>(cookie);
    CHECK(graph);
    std::shared_ptr<PrebuiltEngineInterface> engineInterface = graph->mEngineInterface.lock();

    if (engineInterface) {
        runner::InputFrame frame(height, width, static_cast<PixelFormat>(format), step, pixels);
        engineInterface->DispatchPixelData(streamIndex, timestamp, frame);
    }
}

void LocalPrebuiltGraph::GraphTerminationCallbackFunction(void* cookie,
                                                          const unsigned char* termination_message,
                                                          size_t termination_message_size) {
    LocalPrebuiltGraph* graph = reinterpret_cast<LocalPrebuiltGraph*>(cookie);
    CHECK(graph);
    std::shared_ptr<PrebuiltEngineInterface> engineInterface = graph->mEngineInterface.lock();

    if (engineInterface) {
        std::string errorMessage = "";
        if (termination_message != nullptr && termination_message_size > 0) {
            std::string(termination_message, termination_message + termination_message_size);
        }
        graph->mGraphState.store(PrebuiltGraphState::STOPPED);
        engineInterface->DispatchGraphTerminationMessage(graph->GetStatus(),
                                                         std::move(errorMessage));
    }
}

PrebuiltGraph* GetLocalGraphFromLibrary(const std::string& prebuilt_library,
                                        std::weak_ptr<PrebuiltEngineInterface> engineInterface) {
    return LocalPrebuiltGraph::GetPrebuiltGraphFromLibrary(prebuilt_library, engineInterface);
}

}  // namespace graph
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
