/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <batteryservice/BatteryService.h>

#include <string>

namespace hardware {
namespace google {
namespace pixel {
namespace health {

/**
 * updateThermalState is called per battery status update.
 * BatteryThermalControl monitors thermal framework and when device is charging
 * or battery is full, it then disable SOC throtting by setting disabled in
 * SOC thermal zone's mode.
 */
class BatteryThermalControl {
  public:
    BatteryThermalControl(const std::string &path);
    void updateThermalState(const struct android::BatteryProperties *props);

  private:
    void setThermalMode(bool isEnable, bool isWeakCharger);

    const std::string mThermalSocMode;
    bool mStatus;
};

}  // namespace health
}  // namespace pixel
}  // namespace google
}  // namespace hardware
