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

#ifndef COMPUTEPIPE_RUNNER_GRAPH_INCLUDE_PREBUILTGRAPH_H_
#define COMPUTEPIPE_RUNNER_GRAPH_INCLUDE_PREBUILTGRAPH_H_

#include <string>

#include "InputFrame.h"
#include "Options.pb.h"
#include "PrebuiltEngineInterface.h"
#include "RunnerComponent.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace graph {

enum PrebuiltGraphState {
    RUNNING = 0,
    UNINITIALIZED,
    FLUSHING,
    STOPPED,
};

enum PrebuiltGraphType {
    LOCAL = 0,
    REMOTE = 1,
};

class PrebuiltGraph : public runner::RunnerComponentInterface {
  public:
    // Gets the graph type.
    virtual PrebuiltGraphType GetGraphType() const = 0;

    // Gets the PrebuiltGraphState
    virtual PrebuiltGraphState GetGraphState() const = 0;

    // Gets the graph status and reports any error code or if the status is OK.
    virtual Status GetStatus() const = 0;

    // Gets the error message from the graph.
    virtual std::string GetErrorMessage() const = 0;

    // Gets the supported graph config options.
    virtual const proto::Options& GetSupportedGraphConfigs() const = 0;

    // Sets input stream data. The string is expected to be a serialized proto
    // the definition of which is known to the graph.
    virtual Status SetInputStreamData(int streamIndex, int64_t timestamp,
                                      const std::string& streamData) = 0;

    // Sets pixel data to the specified input stream index.
    virtual Status SetInputStreamPixelData(int streamIndex, int64_t timestamp,
                                           const runner::InputFrame& inputFrame) = 0;

    // Start graph profiling.
    virtual Status StartGraphProfiling() = 0;

    // Stop graph profiling.
    virtual Status StopGraphProfiling() = 0;

    // Collects debugging and profiling information for the graph. The graph
    // needs to be started with debugging enabled in order to get valid info.
    virtual std::string GetDebugInfo() = 0;
};

PrebuiltGraph* GetLocalGraphFromLibrary(
        const std::string& prebuiltLib, std::weak_ptr<PrebuiltEngineInterface> engineInterface);

std::unique_ptr<PrebuiltGraph> GetRemoteGraphFromAddress(
        const std::string& address, std::weak_ptr<PrebuiltEngineInterface> engineInterface);

}  // namespace graph
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // COMPUTEPIPE_RUNNER_GRAPH_INCLUDE_PREBUILTGRAPH_H_
