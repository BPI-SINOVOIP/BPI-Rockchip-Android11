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

#include <optional>
#include <type_traits>

#include <ui/DeviceProductInfo.h>

namespace android {

enum class DisplayConnectionType { Internal, External };

// Immutable information about physical display.
struct DisplayInfo {
    DisplayConnectionType connectionType = DisplayConnectionType::Internal;
    float density = 0.f;
    bool secure = false;
    std::optional<DeviceProductInfo> deviceProductInfo;
};

static_assert(std::is_trivially_copyable_v<DisplayInfo>);

} // namespace android
