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

#ifndef COMPUTEPIPE_RUNNER_ENGINE_H
#define COMPUTEPIPE_RUNNER_ENGINE_H

#include <memory>
#include <string>

#include "ClientInterface.h"
#include "PrebuiltGraph.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace engine {

/**
 * Class that offers an interface into an engine. It derives from the client,
 * prebuilt -> engine interfaces to enforce that any instantiation of an engine
 * needs to provide an implementation for those interfaces.
 */
class RunnerEngine : public client_interface::ClientEngineInterface,
                     public graph::PrebuiltEngineInterface {
  public:
    /**
     * Any args that a given engine instance needs in order to configure itself.
     */
    virtual Status setArgs(std::string engine_args) = 0;
    /**
     * Set the client and the prebuilt graph instances
     */
    virtual void setClientInterface(std::unique_ptr<client_interface::ClientInterface>&& client) = 0;

    virtual void setPrebuiltGraph(std::unique_ptr<graph::PrebuiltGraph>&& graph) = 0;
    /**
     * Activates the client interface and advertises to the rest of the world
     * that the runner is online
     */
    virtual Status activate() = 0;
};

class RunnerEngineFactory {
  public:
    static constexpr char kDefault[] = "default_engine";
    std::unique_ptr<RunnerEngine> createRunnerEngine(std::string engine, std::string engine_args);
    RunnerEngineFactory(const RunnerEngineFactory&) = delete;
    RunnerEngineFactory& operator=(const RunnerEngineFactory&) = delete;
    RunnerEngineFactory() = default;
};

}  // namespace engine
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif
