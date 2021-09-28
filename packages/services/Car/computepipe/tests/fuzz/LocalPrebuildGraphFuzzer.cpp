/*
 * Copyright 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android-base/logging.h>
#include <fuzzer/FuzzedDataProvider.h>
#include <gmock/gmock.h>
#include "ClientConfig.pb.h"
#include "Common.h"
#include "LocalPrebuiltGraph.h"
#include "PrebuiltEngineInterfaceImpl.h"

using ::android::automotive::computepipe::runner::ClientConfig;

namespace android {
namespace automotive {
namespace computepipe {
namespace graph {

namespace {

enum LOCAL_PREBUILD_GRAPH_FUZZ_FUNCS { GRAPH_RUNNER_BASE_ENUM, RUNNER_COMP_BASE_ENUM };

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Initialization goes here
    bool graphHasTerminated = false;
    int numOutputStreamCallbacksReceived[4] = {0, 0, 0, 0};

    PrebuiltEngineInterfaceImpl callback;
    callback.SetGraphTerminationCallback(
            [&graphHasTerminated](Status, std::string) { graphHasTerminated = true; });

    // Add multiple pixel stream callback functions to see if all of them register.
    callback.SetPixelCallback([&numOutputStreamCallbacksReceived](int streamIndex, int64_t,
                                                                  const runner::InputFrame&) {
        ASSERT_TRUE(streamIndex == 0 || streamIndex == 1);
        numOutputStreamCallbacksReceived[streamIndex]++;
    });

    // Add multiple stream callback functions to see if all of them register.
    callback.SetSerializedStreamCallback(
            [&numOutputStreamCallbacksReceived](int streamIndex, int64_t, std::string&&) {
                ASSERT_TRUE(streamIndex == 2 || streamIndex == 3);
                numOutputStreamCallbacksReceived[streamIndex]++;
            });

    std::shared_ptr<PrebuiltEngineInterface> engineInterface =
            std::static_pointer_cast<PrebuiltEngineInterface, PrebuiltEngineInterfaceImpl>(
                    std::make_shared<PrebuiltEngineInterfaceImpl>(callback));
    PrebuiltGraph* graph = GetLocalGraphFromLibrary("libstubgraphimpl.so", engineInterface);

    // Fuzz goes here
    FuzzedDataProvider fdp(data, size);
    while (fdp.remaining_bytes() > runner::test::kMaxFuzzerConsumedBytes) {
        switch (fdp.ConsumeIntegralInRange<uint32_t>(0, API_SUM - 1)) {
            case GET_GRAPH_TYPE: {
                graph->GetGraphType();
                break;
            }
            case GET_GRAPH_STATE: {
                graph->GetGraphState();
                break;
            }
            case GET_STATUS: {
                graph->GetStatus();
                break;
            }
            case GET_ERROR_MESSAGE: {
                graph->GetErrorMessage();
                break;
            }
            case GET_SUPPORTED_GRAPH_CONFIGS: {
                graph->GetSupportedGraphConfigs();
                break;
            }
            case SET_INPUT_STREAM_DATA: {
                graph->SetInputStreamData(/*streamIndex =*/2, /* timestamp =*/0, /* data =*/"");
                break;
            }
            case SET_INPUT_STREAM_PIXEL_DATA: {
                runner::InputFrame inputFrame(0, 0, PixelFormat::RGB, 0, nullptr);
                graph->SetInputStreamPixelData(/*streamIndex =*/1, /*timestamp =*/0,
                                               /*inputFrame =*/inputFrame);
                break;
            }
            case START_GRAPH_PROFILING: {
                graph->StartGraphProfiling();
                break;
            }
            case STOP_GRAPH_PROFILING: {
                graph->StopGraphProfiling();
                break;
            }
            case HANDLE_CONFIG_PHASE: {
                std::map<int, int> maxOutputPacketsPerStream;
                ClientConfig e(0, 0, 0, maxOutputPacketsPerStream, proto::ProfilingType::DISABLED);
                e.setPhaseState(runner::PhaseState::ENTRY);
                graph->handleConfigPhase(e);
                break;
            }
            case HANDLE_EXECUTION_PHASE: {
                std::map<int, int> maxOutputPacketsPerStream;
                ClientConfig e(0, 0, 0, maxOutputPacketsPerStream, proto::ProfilingType::DISABLED);
                e.setPhaseState(runner::PhaseState::ENTRY);
                graph->handleExecutionPhase(e);
                break;
            }
            case HANDLE_STOP_IMMEDIATE_PHASE: {
                std::map<int, int> maxOutputPacketsPerStream;
                ClientConfig e(0, 0, 0, maxOutputPacketsPerStream, proto::ProfilingType::DISABLED);
                e.setPhaseState(runner::PhaseState::ENTRY);
                graph->handleStopImmediatePhase(e);
                break;
            }
            case HANDLE_STOP_WITH_FLUSH_PHASE: {
                std::map<int, int> maxOutputPacketsPerStream;
                ClientConfig e(0, 0, 0, maxOutputPacketsPerStream, proto::ProfilingType::DISABLED);
                e.setPhaseState(runner::PhaseState::ENTRY);
                graph->handleStopWithFlushPhase(e);
                break;
            }
            case HANDLE_RESET_PHASE: {
                std::map<int, int> maxOutputPacketsPerStream;
                ClientConfig e(0, 0, 0, maxOutputPacketsPerStream, proto::ProfilingType::DISABLED);
                e.setPhaseState(runner::PhaseState::ENTRY);
                graph->handleResetPhase(e);
                break;
            }
            default:
                LOG(ERROR) << "Unexpected option aborting...";
                break;
        }
    }
    return 0;
}

}  // namespace
}  // namespace graph
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
