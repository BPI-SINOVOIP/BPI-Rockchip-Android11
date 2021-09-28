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

#include "chre/core/settings.h"

#include <cstddef>

#include "chre/core/event_loop_manager.h"
#include "chre/platform/log.h"
#include "chre/util/nested_data_ptr.h"

namespace chre {

namespace {

//! The current state for each setting.
SettingState gSettingStateList[static_cast<size_t>(Setting::SETTING_MAX)];

/**
 * @param setting The setting to get the index for.
 * @param index A non-null pointer to store the index.
 *
 * @return false if the setting was invalid.
 */
bool getIndexForSetting(Setting setting, size_t *index) {
  if (setting < Setting::SETTING_MAX) {
    *index = static_cast<size_t>(setting);
    return true;
  }

  return false;
}

void setSettingState(Setting setting, SettingState state) {
  size_t index;
  if (!getIndexForSetting(setting, &index)) {
    LOGE("Unknown setting %" PRIu8, static_cast<uint8_t>(setting));
  } else {
    gSettingStateList[index] = state;
  }
}

const char *getSettingStateString(Setting setting) {
  switch (getSettingState(Setting::LOCATION)) {
    case SettingState::ENABLED:
      return "enabled";
      break;
    case SettingState::DISABLED:
      return "disabled";
      break;
    default:
      break;
  }

  return "unknown";
}

}  // anonymous namespace

void postSettingChange(Setting setting, SettingState state) {
  LOGD("Posting setting change: setting type %" PRIu8 " state %" PRIu8,
       static_cast<uint8_t>(setting), static_cast<uint8_t>(state));
  struct SettingChange {
    Setting setting;
    SettingState state;

    SettingChange(Setting setting, SettingState state) {
      this->setting = setting;
      this->state = state;
    }
  };

  NestedDataPtr<SettingChange> nestedPtr(SettingChange(setting, state));

  auto callback = [](uint16_t /* type */, void *data) {
    NestedDataPtr<SettingChange> setting;
    setting.dataPtr = data;
    setSettingState(setting.data.setting, setting.data.state);
#ifdef CHRE_GNSS_SUPPORT_ENABLED
    EventLoopManagerSingleton::get()->getGnssManager().onSettingChanged(
        setting.data.setting, setting.data.state);
#endif  // CHRE_GNSS_SUPPORT_ENABLED
  };

  EventLoopManagerSingleton::get()->deferCallback(
      SystemCallbackType::SettingChangeEvent, nestedPtr.dataPtr, callback);
}

SettingState getSettingState(Setting setting) {
  size_t index;
  if (getIndexForSetting(setting, &index)) {
    return gSettingStateList[index];
  }

  LOGE("Unknown setting %" PRIu8, static_cast<uint8_t>(setting));
  return SettingState::SETTING_STATE_MAX;
}

void logSettingStateToBuffer(DebugDumpWrapper &debugDump) {
  debugDump.print("\nSettings:");
  debugDump.print("\n Location %s", getSettingStateString(Setting::LOCATION));
}

}  // namespace chre
