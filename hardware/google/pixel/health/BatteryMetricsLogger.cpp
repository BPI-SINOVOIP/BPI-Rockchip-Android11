
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

#include <android/frameworks/stats/1.0/IStats.h>
#include <pixelhealth/BatteryMetricsLogger.h>

namespace hardware {
namespace google {
namespace pixel {
namespace health {

using android::sp;
using android::frameworks::stats::V1_0::BatteryHealthSnapshotArgs;
using android::frameworks::stats::V1_0::IStats;

BatteryMetricsLogger::BatteryMetricsLogger(const char *const batt_res, const char *const batt_ocv,
                                           const char *const batt_avg_res, int sample_period,
                                           int upload_period)
    : kBatteryResistance(batt_res),
      kBatteryOCV(batt_ocv),
      kBatteryAvgResistance(batt_avg_res),
      kSamplePeriod(sample_period),
      kUploadPeriod(upload_period),
      kMaxSamples(upload_period / sample_period) {
    last_sample_ = 0;
    last_upload_ = 0;
    num_samples_ = 0;
    num_res_samples_ = 0;
    memset(min_, 0, sizeof(min_));
    memset(max_, 0, sizeof(max_));
}

int64_t BatteryMetricsLogger::getTime(void) {
    return nanoseconds_to_seconds(systemTime(SYSTEM_TIME_BOOTTIME));
}

bool BatteryMetricsLogger::uploadOutlierMetric(sp<IStats> stats_client, sampleType type) {
    BatteryHealthSnapshotArgs min_stats_ss = {
            .type = static_cast<BatteryHealthSnapshotArgs::BatterySnapshotType>(0),
            .temperatureDeciC = min_[type][TEMP],
            .voltageMicroV = min_[type][VOLT],
            .currentMicroA = min_[type][CURR],
            .openCircuitVoltageMicroV = min_[type][OCV],
            .resistanceMicroOhm = min_[type][RES],
            .levelPercent = min_[type][SOC]};
    BatteryHealthSnapshotArgs max_stats_ss = {
            .type = static_cast<BatteryHealthSnapshotArgs::BatterySnapshotType>(0),
            .temperatureDeciC = max_[type][TEMP],
            .voltageMicroV = max_[type][VOLT],
            .currentMicroA = max_[type][CURR],
            .openCircuitVoltageMicroV = max_[type][OCV],
            .resistanceMicroOhm = max_[type][RES],
            .levelPercent = max_[type][SOC]};
    if (kStatsSnapshotType[type] < 0)
        return false;

    min_stats_ss.type = (BatteryHealthSnapshotArgs::BatterySnapshotType)kStatsSnapshotType[type];
    max_stats_ss.type =
            (BatteryHealthSnapshotArgs::BatterySnapshotType)(kStatsSnapshotType[type] + 1);

    stats_client->reportBatteryHealthSnapshot(min_stats_ss);
    stats_client->reportBatteryHealthSnapshot(max_stats_ss);

    return true;
}

bool BatteryMetricsLogger::uploadAverageBatteryResistance(sp<IStats> stats_client) {
    if (strlen(kBatteryAvgResistance) == 0) {
        LOG(INFO) << "Sysfs path for average battery resistance not specified";
        return true;
    }

    std::string file_content;
    int32_t batt_avg_res;

    if (!android::base::ReadFileToString(kBatteryAvgResistance, &file_content)) {
        LOG(ERROR) << "Can't read " << kBatteryAvgResistance;
        return false;
    }
    std::stringstream ss(file_content);
    if (!(ss >> batt_avg_res)) {
        LOG(ERROR) << "Can't parse average battery resistance " << file_content;
        return false;
    }
    // Upload average metric
    BatteryHealthSnapshotArgs avg_res_ss_stats = {
            .type = BatteryHealthSnapshotArgs::BatterySnapshotType::AVG_RESISTANCE,
            .temperatureDeciC = 0,
            .voltageMicroV = 0,
            .currentMicroA = 0,
            .openCircuitVoltageMicroV = 0,
            .resistanceMicroOhm = batt_avg_res,
            .levelPercent = 0};
    stats_client->reportBatteryHealthSnapshot(avg_res_ss_stats);
    return true;
}

bool BatteryMetricsLogger::uploadMetrics(void) {
    int64_t time = getTime();

    if (last_sample_ == 0)
        return false;

    LOG(INFO) << "Uploading metrics at time " << std::to_string(time) << " w/ "
              << std::to_string(num_samples_) << " samples";

    sp<IStats> stats_client = IStats::tryGetService();
    if (!stats_client) {
        LOG(ERROR) << "Unable to connect to Stats service";
        return false;
    }

    // Only log and upload the min and max for metric types we want to upload
    for (int metric = 0; metric < NUM_FIELDS; metric++) {
        if ((metric == RES && num_res_samples_ == 0) || kStatsSnapshotType[metric] < 0)
            continue;
        std::string log_min = "min-" + std::to_string(metric) + " ";
        std::string log_max = "max-" + std::to_string(metric) + " ";
        for (int j = 0; j < NUM_FIELDS; j++) {
            log_min += std::to_string(min_[metric][j]) + " ";
            log_max += std::to_string(max_[metric][j]) + " ";
        }
        LOG(INFO) << log_min;
        LOG(INFO) << log_max;
        // Upload min/max metrics
        uploadOutlierMetric(stats_client, static_cast<sampleType>(metric));
    }

    uploadAverageBatteryResistance(stats_client);

    // Clear existing data
    memset(min_, 0, sizeof(min_));
    memset(max_, 0, sizeof(max_));
    num_samples_ = 0;
    num_res_samples_ = 0;
    last_upload_ = time;
    LOG(INFO) << "Finished uploading to tron";
    return true;
}

bool BatteryMetricsLogger::recordSample(struct android::BatteryProperties *props) {
    std::string resistance_str, ocv_str;
    int32_t resistance, ocv;
    int32_t time = getTime();

    LOG(INFO) << "Recording a sample at time " << std::to_string(time);

    if (!android::base::ReadFileToString(kBatteryResistance, &resistance_str)) {
        LOG(ERROR) << "Can't read the battery resistance from " << kBatteryResistance;
        resistance = -INT_MAX;
    } else if (!(std::stringstream(resistance_str) >> resistance)) {
        LOG(ERROR) << "Can't parse battery resistance value " << resistance_str;
        resistance = -INT_MAX;
    }

    if (!android::base::ReadFileToString(kBatteryOCV, &ocv_str)) {
        LOG(ERROR) << "Can't read open-circuit voltage (ocv) value from " << kBatteryOCV;
        ocv = -INT_MAX;
    } else if (!(std::stringstream(ocv_str) >> ocv)) {
        LOG(ERROR) << "Can't parse open-circuit voltage (ocv) value " << ocv_str;
        ocv = -INT_MAX;
    }

    int32_t sample[NUM_FIELDS] = {[TIME] = time,
                                  [RES] = resistance,
                                  [CURR] = props->batteryCurrent,
                                  [VOLT] = props->batteryVoltage,
                                  [TEMP] = props->batteryTemperature,
                                  [SOC] = props->batteryLevel,
                                  [OCV] = ocv};
    if (props->batteryStatus != android::BATTERY_STATUS_CHARGING) {
        num_res_samples_++;
    }

    // Only calculate the min and max for metric types we want to upload
    for (int metric = 0; metric < NUM_FIELDS; metric++) {
        // Discard resistance min/max when charging
        if ((metric == RES && props->batteryStatus == android::BATTERY_STATUS_CHARGING) ||
            kStatsSnapshotType[metric] < 0)
            continue;
        if (num_samples_ == 0 || (metric == RES && num_res_samples_ == 0) ||
            sample[metric] < min_[metric][metric]) {
            for (int i = 0; i < NUM_FIELDS; i++) {  // update new min with current sample
                min_[metric][i] = sample[i];
            }
        }
        if (num_samples_ == 0 || (metric == RES && num_res_samples_ == 0) ||
            sample[metric] > max_[metric][metric]) {
            for (int i = 0; i < NUM_FIELDS; i++) {  // update new max with current sample
                max_[metric][i] = sample[i];
            }
        }
    }

    num_samples_++;
    last_sample_ = time;
    return true;
}

void BatteryMetricsLogger::logBatteryProperties(struct android::BatteryProperties *props) {
    int32_t time = getTime();
    if (last_sample_ == 0 || time - last_sample_ >= kSamplePeriod)
        recordSample(props);
    if (last_sample_ - last_upload_ > kUploadPeriod || num_samples_ >= kMaxSamples)
        uploadMetrics();

    return;
}

}  // namespace health
}  // namespace pixel
}  // namespace google
}  // namespace hardware
