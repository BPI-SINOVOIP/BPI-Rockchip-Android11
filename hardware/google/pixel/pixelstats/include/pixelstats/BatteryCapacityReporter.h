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

#ifndef HARDWARE_GOOGLE_PIXEL_PIXELSTATS_BATTERYCAPACITYREPORTER_H
#define HARDWARE_GOOGLE_PIXEL_PIXELSTATS_BATTERYCAPACITYREPORTER_H

namespace android {
namespace hardware {
namespace google {
namespace pixel {

#define MAX_LOG_EVENTS_PER_HOUR 4

/**
 * A class to upload battery capacity metrics
 */
class BatteryCapacityReporter {
  public:
    BatteryCapacityReporter();
    void checkAndReport(const std::string &path);

  private:
    int64_t getTimeSecs();

    bool parse(const std::string &path);
    bool checkLogEvent(void);
    bool shouldReportEvent(void);
    void reportEvent(void);

    /**
     * SOC status translation from sysfs node
     */
    enum SOCStatus {
        SOC_STATUS_UNKNOWN = 0,
        SOC_STATUS_CONNECTED = 1,
        SOC_STATUS_DISCONNECTED = 2,
        SOC_STATUS_FULL = 3,
    };

    enum LogReason {
        LOG_REASON_UNKNOWN = 0,
        LOG_REASON_CONNECTED = 1,
        LOG_REASON_DISCONNECTED = 2,
        LOG_REASON_FULL_CHARGE = 3,
        LOG_REASON_PERCENT_SKIP = 4,
        LOG_REASON_DIVERGING_FG = 5,
    };

    SOCStatus status_ = SOC_STATUS_UNKNOWN;
    SOCStatus status_previous_ = SOC_STATUS_UNKNOWN;
    float gdf_ = 0.0;
    float ssoc_ = 0.0f;
    float gdf_curve_ = 0.0f;
    float ssoc_curve_ = 0.0f;
    float ssoc_previous_ = -1.0f;
    float ssoc_gdf_diff_previous_ = 0.0f;
    LogReason log_reason_ = LOG_REASON_UNKNOWN;

    int num_events_in_last_hour_ = 0;
    int64_t log_event_time_secs_[MAX_LOG_EVENTS_PER_HOUR] = {0};

    // Proto messages are 1-indexed and VendorAtom field numbers start at 2, so
    // store everything in the values array at the index of the field number
    // -2.
    const int kVendorAtomOffset = 2;
};

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_PIXELSTATS_BATTERYCAPACITYREPORTER_H
