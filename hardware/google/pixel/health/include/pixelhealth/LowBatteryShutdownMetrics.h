/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef HARDWARE_GOOGLE_PIXEL_HEALTH_LOWBATTERYSHUTDOWNMETRICS_H
#define HARDWARE_GOOGLE_PIXEL_HEALTH_LOWBATTERYSHUTDOWNMETRICS_H

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#include <batteryservice/BatteryService.h>
#include <math.h>
#include <time.h>
#include <utils/Timers.h>
#include <string>

namespace hardware {
namespace google {
namespace pixel {
namespace health {

class LowBatteryShutdownMetrics {
  public:
    LowBatteryShutdownMetrics(
            const char *const voltage_avg,
            const char *const persist_prop = "persist.vendor.shutdown.voltage_avg");
    void logShutdownVoltage(struct android::BatteryProperties *props);

  private:
    const char *const kVoltageAvg;
    const char *const kPersistProp;

    // Helps enforce that we only record kVoltageAvg once per boot cycle
    bool prop_written_;
    // Help us avoid polling kPersistProp if it's empty
    bool prop_empty_;

    bool saveVoltageAvg();
    void readStatus();
    bool uploadVoltageAvg();
};

}  // namespace health
}  // namespace pixel
}  // namespace google
}  // namespace hardware

#endif
