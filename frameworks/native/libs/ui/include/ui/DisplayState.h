/*
 * Copyright 2019 The Android Open Source Project
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

#pragma once

#include <ui/Rotation.h>
#include <ui/Size.h>

#include <cstdint>
#include <type_traits>

namespace android::ui {

using LayerStack = uint32_t;
constexpr LayerStack NO_LAYER_STACK = static_cast<LayerStack>(-1);

// Transactional state of physical or virtual display. Note that libgui defines
// android::DisplayState as a superset of android::ui::DisplayState.
struct DisplayState {
    LayerStack layerStack = NO_LAYER_STACK;
    Rotation orientation = ROTATION_0;
    Size viewport;
};

static_assert(std::is_trivially_copyable_v<DisplayState>);

} // namespace android::ui
