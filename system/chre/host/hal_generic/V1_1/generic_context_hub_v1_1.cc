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

#define LOG_TAG "ContextHubHal"
#define LOG_NDEBUG 0

#include "generic_context_hub_v1_1.h"

#include <chrono>
#include <cinttypes>
#include <vector>

#include <log/log.h>
#include <unistd.h>

namespace android {
namespace hardware {
namespace contexthub {
namespace V1_1 {
namespace implementation {

using ::android::chre::HostProtocolHost;
using ::android::hardware::Return;
using ::flatbuffers::FlatBufferBuilder;

// Aliased for consistency with the way these symbols are referenced in
// CHRE-side code
namespace fbs = ::chre::fbs;

namespace {

bool getFbsSetting(Setting setting, fbs::Setting *fbsSetting) {
  bool foundSetting = true;

  switch (setting) {
    case Setting::LOCATION:
      *fbsSetting = fbs::Setting::LOCATION;
      break;
    default:
      foundSetting = false;
      ALOGE("Setting update with invalid enum value %hhu", setting);
      break;
  }

  return foundSetting;
}

bool getFbsSettingValue(SettingValue newValue, fbs::SettingState *fbsState) {
  bool foundSettingValue = true;

  switch (newValue) {
    case SettingValue::ENABLED:
      *fbsState = fbs::SettingState::ENABLED;
      break;
    case SettingValue::DISABLED:
      *fbsState = fbs::SettingState::DISABLED;
      break;
    default:
      foundSettingValue = false;
      ALOGE("Setting value update with invalid enum value %hhu", newValue);
      break;
  }

  return foundSettingValue;
}

}  // namespace

Return<void> GenericContextHubV1_1::onSettingChanged(Setting setting,
                                                     SettingValue newValue) {
  fbs::Setting fbsSetting;
  fbs::SettingState fbsState;
  if (getFbsSetting(setting, &fbsSetting) &&
      getFbsSettingValue(newValue, &fbsState)) {
    FlatBufferBuilder builder(64);
    HostProtocolHost::encodeSettingChangeNotification(builder, fbsSetting,
                                                      fbsState);
    mClient.sendMessage(builder.GetBufferPointer(), builder.GetSize());
  }

  return Void();
}

}  // namespace implementation
}  // namespace V1_1
}  // namespace contexthub
}  // namespace hardware
}  // namespace android
