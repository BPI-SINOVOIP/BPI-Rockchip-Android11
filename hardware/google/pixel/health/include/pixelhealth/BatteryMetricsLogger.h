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

#ifndef HARDWARE_GOOGLE_PIXEL_HEALTH_BATTERYMETRICSLOGGER_H
#define HARDWARE_GOOGLE_PIXEL_HEALTH_BATTERYMETRICSLOGGER_H

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <android/frameworks/stats/1.0/IStats.h>
#include <batteryservice/BatteryService.h>
#include <math.h>
#include <time.h>
#include <utils/Timers.h>

#include <string>

namespace hardware {
namespace google {
namespace pixel {
namespace health {

using android::sp;
using android::frameworks::stats::V1_0::BatteryHealthSnapshotArgs;
using android::frameworks::stats::V1_0::IStats;

class BatteryMetricsLogger {
  public:
    BatteryMetricsLogger(const char *const batt_res, const char *const batt_ocv,
                         const char *const batt_avg_res = "", int sample_period = TEN_MINUTES_SEC,
                         int upload_period = ONE_DAY_SEC);
    void logBatteryProperties(struct android::BatteryProperties *props);

  private:
    enum sampleType {
        TIME,        // time in seconds
        CURR,        // current in mA
        VOLT,        // voltage in mV
        TEMP,        // temp in deci-degC
        SOC,         // SoC in % battery level
        RES,         // resistance in milli-ohms
        OCV,         // open-circuit voltage in mV
        NUM_FIELDS,  // do not reference
    };

    const int kStatsSnapshotType[NUM_FIELDS] = {
            -1,
            (int)BatteryHealthSnapshotArgs::BatterySnapshotType::MIN_CURRENT,
            (int)BatteryHealthSnapshotArgs::BatterySnapshotType::MIN_VOLTAGE,
            (int)BatteryHealthSnapshotArgs::BatterySnapshotType::MIN_TEMP,
            (int)BatteryHealthSnapshotArgs::BatterySnapshotType::MIN_BATT_LEVEL,
            (int)BatteryHealthSnapshotArgs::BatterySnapshotType::MIN_RESISTANCE,
            -1,
    };

    const char *const kBatteryResistance;
    const char *const kBatteryOCV;
    const char *const kBatteryAvgResistance;
    const int kSamplePeriod;
    const int kUploadPeriod;
    const int kMaxSamples;
    static constexpr int TEN_MINUTES_SEC = 10 * 60;
    static constexpr int ONE_DAY_SEC = 24 * 60 * 60;

    // min and max are referenced by type in both the X and Y axes
    // i.e. min[TYPE] is the event where the minimum of that type occurred, and
    // min[TYPE][TYPE] is the reading of that type at that minimum event
    int32_t min_[NUM_FIELDS][NUM_FIELDS];
    int32_t max_[NUM_FIELDS][NUM_FIELDS];
    int32_t num_res_samples_;  // number of res samples since last upload
    int32_t num_samples_;      // number of min/max samples since last upload
    int64_t last_sample_;      // time in seconds since boot of last sample
    int64_t last_upload_;      // time in seconds since boot of last upload

    int64_t getTime();
    bool recordSample(struct android::BatteryProperties *props);
    bool uploadMetrics();
    bool uploadOutlierMetric(sp<IStats> stats_client, sampleType type);
    bool uploadAverageBatteryResistance(sp<IStats> stats_client);
};

}  // namespace health
}  // namespace pixel
}  // namespace google
}  // namespace hardware

#endif
