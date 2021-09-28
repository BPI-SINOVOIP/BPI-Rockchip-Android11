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
#include "GrpcGraph.h"
#include "GrpcGraphServerImpl.h"

using ::android::automotive::computepipe::runner::ClientConfig;

namespace android {
namespace automotive {
namespace computepipe {
namespace graph {

namespace {

enum GRPC_GRAPH_FUZZ_FUNCS {
    GRAPH_RUNNER_BASE_ENUM,
    DISPATCH_PIXEL_DATA,            /* verify dispatchPixelData */
    DISPATCH_SERIALIZED_DATA,       /* dispatchSerializedData */
    DISPATCH_GRAPH_TERMINATION_MSG, /* dispatchGraphTerminationMessage */
    RUNNER_COMP_BASE_ENUM
};

bool DoInitialization() {
    // Initialization goes here
    std::shared_ptr<GrpcGraphServerImpl> server;
    server = std::make_shared<GrpcGraphServerImpl>(runner::test::kAddress);
    std::thread t = std::thread([server]() { server->startServer(); });
    t.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return true;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static bool initialized = DoInitialization();
    std::shared_ptr<PrebuiltEngineInterfaceImpl> engine;
    std::unique_ptr<GrpcGraph> graph = std::make_unique<GrpcGraph>();
    engine = std::make_shared<PrebuiltEngineInterfaceImpl>();
    Status status = graph->initialize(runner::test::kAddress, engine);
    if (status != Status::SUCCESS) {
        LOG(ERROR) << "Initialization of GrpcGraph failed, aborting...";
        exit(1);
    }

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
            case DISPATCH_PIXEL_DATA: {
                runner::InputFrame inputFrame(0, 0, PixelFormat::RGB, 0, nullptr);
                graph->dispatchPixelData(/*streamIndex =*/2, /*timestamp =*/0,
                                         /*inputFrame =*/inputFrame);
                break;
            }
            case DISPATCH_SERIALIZED_DATA: {
                graph->dispatchSerializedData(/*streamIndex =*/1, /* timestamp =*/0, /* data =*/"");
                break;
            }
            case DISPATCH_GRAPH_TERMINATION_MSG: {
                uint8_t status = fdp.ConsumeIntegralInRange<uint8_t>(0, Status::STATUS_MAX - 1);
                graph->dispatchGraphTerminationMessage(static_cast<Status>(status), "");
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
