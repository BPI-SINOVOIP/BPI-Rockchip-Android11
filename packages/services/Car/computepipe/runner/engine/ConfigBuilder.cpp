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

#include "ConfigBuilder.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace engine {

void ConfigBuilder::setDebugDisplayStream(int id) {
    mDisplayStream = id;
    mOutputConfig.emplace(id, 1);
}

bool ConfigBuilder::clientConfigEnablesDisplayStream() {
    return mConfigHasDisplayStream;
}

ConfigBuilder& ConfigBuilder::updateInputConfigOption(int id) {
    mInputConfigId = id;
    return *this;
}

ConfigBuilder& ConfigBuilder::updateOutputStreamOption(int id, int maxInFlightPackets) {
    if (id == mDisplayStream) {
        mConfigHasDisplayStream = true;
    }
    mOutputConfig.emplace(id, maxInFlightPackets);
    return *this;
}

ConfigBuilder& ConfigBuilder::updateTerminationOption(int id) {
    mTerminationId = id;
    return *this;
}

ConfigBuilder& ConfigBuilder::updateOffloadOption(int id) {
    mOffloadId = id;
    return *this;
}

ConfigBuilder& ConfigBuilder::updateOptionalConfig(std::string options) {
    mOptionalConfig = options;
    return *this;
}

ConfigBuilder& ConfigBuilder::updateProfilingType(proto::ProfilingType profilingType) {
    mProfilingType = profilingType;
    return *this;
}

ClientConfig ConfigBuilder::emitClientOptions() {
    return ClientConfig(mInputConfigId, mOffloadId, mTerminationId, mOutputConfig, mProfilingType,
            mOptionalConfig);
}

ConfigBuilder& ConfigBuilder::reset() {
    mInputConfigId = ClientConfig::kInvalidId;
    mTerminationId = ClientConfig::kInvalidId;
    mOffloadId = ClientConfig::kInvalidId;
    mOutputConfig.clear();
    mProfilingType = proto::ProfilingType::DISABLED;
    if (mDisplayStream != ClientConfig::kInvalidId) {
        mOutputConfig.emplace(mDisplayStream, 1);
    }
    mConfigHasDisplayStream = false;
    return *this;
}

}  // namespace engine
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
