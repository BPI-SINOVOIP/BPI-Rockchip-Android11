/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef CHRE_CORE_SETTINGS_H_
#define CHRE_CORE_SETTINGS_H_

#include <cinttypes>

#include "chre/util/system/debug_dump.h"

namespace chre {

enum class Setting : uint8_t {
  LOCATION = 0,
  SETTING_MAX,
};

enum class SettingState : uint8_t {
  ENABLED = 0,
  DISABLED,
  SETTING_STATE_MAX,
};

/**
 * Updates the state of a given setting.
 *
 * @param setting The setting to update.
 * @param state The state of the setting.
 */
void postSettingChange(Setting setting, SettingState state);

/**
 * Gets the current state of a given setting. Must be called from the context of
 * the main CHRE thread.
 *
 * @param setting The setting to check the current state of.
 *
 * @return The current state of the setting, SETTING_STATE_MAX if the provided
 * setting is invalid.
 */
SettingState getSettingState(Setting setting);

/**
 * Logs the settings related stats in the debug dump. Must be called from the
 * context of the main CHRE thread.
 *
 * @param debugDump The object that is printed into for debug dump logs.
 */
void logSettingStateToBuffer(DebugDumpWrapper &debugDump);

}  // namespace chre

#endif  // CHRE_CORE_SETTINGS_H_
