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

#ifndef COMPUTEPIPE_RUNNER_INPUT_MANAGER_H
#define COMPUTEPIPE_RUNNER_INPUT_MANAGER_H

#include <memory>

#include "InputConfig.pb.h"
#include "InputEngineInterface.h"
#include "InputFrame.h"
#include "RunnerComponent.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace input_manager {

/**
 * InputManager: Is the runner component repsonsible for managing the input
 * source for the graph. The Component exposes communications from the engine to
 * the input source.
 */
class InputManager : public RunnerComponentInterface {};

/**
 * Factory that instantiates the input manager, for a given input option.
 */
class InputManagerFactory {
  public:
    std::unique_ptr<InputManager> createInputManager(const proto::InputConfig& config,
                                                     std::shared_ptr<InputEngineInterface> engine);
    InputManagerFactory() = default;
    InputManagerFactory(const InputManagerFactory&) = delete;
    InputManagerFactory& operator=(const InputManagerFactory&) = delete;
    InputManagerFactory(const InputManagerFactory&&) = delete;
    InputManagerFactory& operator=(const InputManagerFactory&&) = delete;
};

}  // namespace input_manager
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
#endif
