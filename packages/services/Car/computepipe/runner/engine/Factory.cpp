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

#include "DefaultEngine.h"
#include "RunnerEngine.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace engine {

namespace {
std::unique_ptr<DefaultEngine> createDefaultEngine(std::string engine_args) {
    std::unique_ptr<DefaultEngine> engine = std::make_unique<DefaultEngine>();
    if (engine->setArgs(engine_args) != Status::SUCCESS) {
        return nullptr;
    }
    return engine;
}
}  // namespace

std::unique_ptr<RunnerEngine> RunnerEngineFactory::createRunnerEngine(std::string engine,
                                                                      std::string engine_args) {
    if (engine == kDefault) {
        return createDefaultEngine(engine_args);
    }
    return nullptr;
}

}  // namespace engine
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
