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

#include "EvsInputManager.h"
#include "InputManager.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace input_manager {
namespace {

enum InputManagerType {
    EVS = 0,
    IMAGES,
    VIDEO,
};

// Helper function to determine the type of input manager to be created from the
// input config.
// TODO(b/147803315): Implement the actual algorithm to determine the input manager to be
// used. Right now, only EVS manager is enabled, so that is used.
InputManagerType getInputManagerType(const proto::InputConfig& /* inputConfig */) {
    return InputManagerType::EVS;
}

}  // namespace
std::unique_ptr<InputManager> InputManagerFactory::createInputManager(
    const proto::InputConfig& config, std::shared_ptr<InputEngineInterface> inputEngineInterface) {
    InputManagerType inputManagerType = getInputManagerType(config);
    switch (inputManagerType) {
        case InputManagerType::EVS:
            return EvsInputManager::createEvsInputManager(config, inputEngineInterface);
        default:
            return nullptr;
    }
}

}  // namespace input_manager
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
