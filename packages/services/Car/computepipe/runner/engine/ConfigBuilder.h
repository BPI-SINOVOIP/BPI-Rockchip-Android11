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

#ifndef COMPUTEPIPE_RUNNER_ENGINE_CONFIGBUILDER_H_
#define COMPUTEPIPE_RUNNER_ENGINE_CONFIGBUILDER_H_

#include "ProfilingType.pb.h"

#include "RunnerComponent.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace engine {

class ConfigBuilder {
  public:
    /**
     * Set debug display stream in the final client config
     */
    void setDebugDisplayStream(int id);
    /**
     * Does client explicitly enable display stream
     */
    bool clientConfigEnablesDisplayStream();
    /**
     * Update current input option
     */
    ConfigBuilder& updateInputConfigOption(int id);
    /**
     * Update current output options
     */
    ConfigBuilder& updateOutputStreamOption(int id, int maxInFlightPackets);
    /**
     * Update current termination options
     */
    ConfigBuilder& updateTerminationOption(int id);
    /**
     * Update current offload options
     */
    ConfigBuilder& updateOffloadOption(int id);
    /**
     * Update optional Config
     */
    ConfigBuilder& updateOptionalConfig(std::string options);
    /**
     * Update profiling Config
     */
    ConfigBuilder& updateProfilingType(proto::ProfilingType profilingType);
    /**
     * Emit Options
     */
    ClientConfig emitClientOptions();
    /**
     * Clear current options.
     */
    ConfigBuilder& reset();

  private:
    int mDisplayStream = ClientConfig::kInvalidId;
    int mInputConfigId = ClientConfig::kInvalidId;
    int mOffloadId = ClientConfig::kInvalidId;
    int mTerminationId = ClientConfig::kInvalidId;
    proto::ProfilingType mProfilingType = proto::ProfilingType::DISABLED;
    bool mConfigHasDisplayStream = false;
    std::map<int, int> mOutputConfig;
    std::string mOptionalConfig;
};

}  // namespace engine
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // COMPUTEPIPE_RUNNER_ENGINE_CONFIGBUILDER_H_
