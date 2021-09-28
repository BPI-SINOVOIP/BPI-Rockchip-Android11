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

#ifndef COMPUTEPIPE_RUNNER_GRAPH_INCLUDE_LOCALPREBUILTGRAPH_H
#define COMPUTEPIPE_RUNNER_GRAPH_INCLUDE_LOCALPREBUILTGRAPH_H

#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <thread>

#include "ClientConfig.pb.h"
#include "GrpcPrebuiltGraphService.grpc.pb.h"
#include "GrpcPrebuiltGraphService.pb.h"
#include "InputFrame.h"
#include "Options.pb.h"
#include "OutputConfig.pb.h"
#include "PrebuiltEngineInterface.h"
#include "PrebuiltGraph.h"
#include "RunnerComponent.h"
#include "StreamSetObserver.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace graph {

class GrpcGraph : public PrebuiltGraph, public StreamGraphInterface {

  public:
    GrpcGraph() {}

    virtual ~GrpcGraph();

    Status initialize(const std::string& address,
                      std::weak_ptr<PrebuiltEngineInterface> engineInterface);

    // No copy or move constructors or operators are available.
    GrpcGraph(const GrpcGraph&) = delete;
    GrpcGraph& operator=(const GrpcGraph&) = delete;

    // Override RunnerComponent interface functions for applying configs,
    // starting the graph and stopping the graph.
    Status handleConfigPhase(const runner::ClientConfig& e) override;
    Status handleExecutionPhase(const runner::RunnerEvent& e) override;
    Status handleStopWithFlushPhase(const runner::RunnerEvent& e) override;
    Status handleStopImmediatePhase(const runner::RunnerEvent& e) override;
    Status handleResetPhase(const runner::RunnerEvent& e) override;

    PrebuiltGraphType GetGraphType() const override {
        return PrebuiltGraphType::REMOTE;
    }

    PrebuiltGraphState GetGraphState() const override;
    Status GetStatus() const override;
    std::string GetErrorMessage() const override;

    // Gets the supported graph config options.
    const proto::Options& GetSupportedGraphConfigs() const override {
        return mGraphConfig;
    }

    // Sets input stream data. The string is expected to be a serialized proto
    // the definition of which is known to the graph.
    Status SetInputStreamData(int streamIndex, int64_t timestamp,
                              const std::string& streamData) override;

    // Sets pixel data to the specified input stream index.
    Status SetInputStreamPixelData(int streamIndex, int64_t timestamp,
                                   const runner::InputFrame& inputFrame) override;

    // Starts graph profiling at some point after starting the graph with profiling enabled.
    Status StartGraphProfiling() override;

    // Stops graph profiling.
    Status StopGraphProfiling() override;

    // Collects debugging and profiling information for the graph. The graph
    // needs to be started with debugging enabled in order to get valid info.
    std::string GetDebugInfo() override;

    // Stream Graph interface
    proto::GrpcGraphService::Stub* getServiceStub() override {
        return mGraphStub.get();
    }

    void dispatchPixelData(int streamId, int64_t timestamp_us,
                           const runner::InputFrame& frame) override;

    void dispatchSerializedData(int streamId, int64_t timestamp_us,
                                std::string&& serialized_data) override;

    void dispatchGraphTerminationMessage(Status, std::string&&) override;
  private:
    mutable std::mutex mLock;

    PrebuiltGraphState mGraphState = PrebuiltGraphState::UNINITIALIZED;

    Status mStatus = Status::SUCCESS;

    std::string mErrorMessage = "";

    // Cached callback interface that is passed in from the runner.
    std::weak_ptr<PrebuiltEngineInterface> mEngineInterface;

    // Cached graph config.
    proto::Options mGraphConfig;

    std::unique_ptr<proto::GrpcGraphService::Stub> mGraphStub;

    std::unique_ptr<StreamSetObserver> mStreamSetObserver;
};

}  // namespace graph
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // #define COMPUTEPIPE_RUNNER_GRAPH_INCLUDE_LOCALPREBUILTGRAPH_H
