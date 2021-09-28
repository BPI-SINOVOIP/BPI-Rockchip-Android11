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

#define LOG_TAG "pixelstats: BatteryEEPROM"

#include <log/log.h>
#include <time.h>
#include <utils/Timers.h>
#include <cmath>
#include <inttypes.h>

#include <android-base/file.h>

#include <android/frameworks/stats/1.0/IStats.h>
#include <pixelstats/BatteryEEPROMReporter.h>

#include <hardware/google/pixel/pixelstats/pixelatoms.pb.h>

namespace android {
namespace hardware {
namespace google {
namespace pixel {

using android::base::ReadFileToString;
using android::frameworks::stats::V1_0::IStats;
using android::frameworks::stats::V1_0::VendorAtom;
using android::hardware::google::pixel::PixelAtoms::BatteryEEPROM;

#define LINESIZE 71

BatteryEEPROMReporter::BatteryEEPROMReporter() {}

void BatteryEEPROMReporter::checkAndReport(const std::string &path) {
    std::string file_contents;
    std::string history_each;

    const int kSecondsPerMonth = 60 * 60 * 24 * 30;
    int64_t now = getTimeSecs();

    if ((report_time_ != 0) && (now - report_time_ < kSecondsPerMonth)) {
        ALOGD("Not upload time. now:%ld, pre:%ld", now, report_time_);
        return;
    }

    if (!ReadFileToString(path.c_str(), &file_contents)) {
        ALOGE("Unable to read %s - %s", path.c_str(), strerror(errno));
        return;
    }
    ALOGD("checkAndReport: %s", file_contents.c_str());

    int16_t i, num;
    struct BatteryHistory hist;
    const int kHistTotalLen = strlen(file_contents.c_str());

    for (i = 0; i < (LINESIZE * BATT_HIST_NUM_MAX); i = i + LINESIZE) {
        if (i + LINESIZE > kHistTotalLen)
            break;
        history_each = file_contents.substr(i, LINESIZE);
        num = sscanf(history_each.c_str(),
                   "%4" SCNx16 "%4" SCNx16 "%4" SCNx16 "%4" SCNx16
                   "%2" SCNx8 "%2" SCNx8 " %2" SCNx8 "%2" SCNx8
                   "%2" SCNx8 "%2" SCNx8 " %2" SCNx8 "%2" SCNx8
                   "%2" SCNx8 "%2" SCNx8 " %4" SCNx16 "%4" SCNx16
                   "%4" SCNx16 "%4" SCNx16 "%4" SCNx16,
                   &hist.cycle_cnt, &hist.full_cap, &hist.esr,
                   &hist.rslow, &hist.batt_temp, &hist.soh,
                   &hist.cc_soc, &hist.cutoff_soc, &hist.msoc,
                   &hist.sys_soc, &hist.reserve, &hist.batt_soc,
                   &hist.min_temp, &hist.max_temp,  &hist.max_vbatt,
                   &hist.min_vbatt, &hist.max_ibatt, &hist.min_ibatt,
                   &hist.checksum);

        if (num != kNumBatteryHistoryFields) {
            ALOGE("Couldn't process %s", history_each.c_str());
            continue;
        }

        if (checkLogEvent(hist)) {
            reportEvent(hist);
            report_time_ = getTimeSecs();
        }
    }
}

int64_t BatteryEEPROMReporter::getTimeSecs(void) {
    return nanoseconds_to_seconds(systemTime(SYSTEM_TIME_BOOTTIME));
}

/**
 * @return true if a log should be reported, else false.
 * Here we use checksum to confirm the data is usable or not.
 * The checksum mismatch when storage data overflow or corrupt.
 * We don't need data in such cases.
 */
bool BatteryEEPROMReporter::checkLogEvent(struct BatteryHistory hist) {
    int checksum = 0;

    checksum = hist.cycle_cnt + hist.full_cap + hist.esr + hist.rslow
                + hist.soh + hist.batt_temp + hist.cutoff_soc + hist.cc_soc
                + hist.sys_soc + hist.msoc + hist.batt_soc + hist.reserve
                + hist.max_temp + hist.min_temp + hist.max_vbatt
                + hist.min_vbatt + hist.max_ibatt + hist.min_ibatt;
    /* Compare with checksum data */
    if (checksum == hist.checksum) {
        return true;
    } else {
        return false;
    }
}

void BatteryEEPROMReporter::reportEvent(struct BatteryHistory hist) {
       sp<IStats> stats_client = IStats::tryGetService();
       // upload atom
       std::vector<int> eeprom_history_fields = {BatteryEEPROM::kCycleCntFieldNumber,
                                                 BatteryEEPROM::kFullCapFieldNumber,
                                                 BatteryEEPROM::kEsrFieldNumber,
                                                 BatteryEEPROM::kRslowFieldNumber,
                                                 BatteryEEPROM::kSohFieldNumber,
                                                 BatteryEEPROM::kBattTempFieldNumber,
                                                 BatteryEEPROM::kCutoffSocFieldNumber,
                                                 BatteryEEPROM::kCcSocFieldNumber,
                                                 BatteryEEPROM::kSysSocFieldNumber,
                                                 BatteryEEPROM::kMsocFieldNumber,
                                                 BatteryEEPROM::kBattSocFieldNumber,
                                                 BatteryEEPROM::kReserveFieldNumber,
                                                 BatteryEEPROM::kMaxTempFieldNumber,
                                                 BatteryEEPROM::kMinTempFieldNumber,
                                                 BatteryEEPROM::kMaxVbattFieldNumber,
                                                 BatteryEEPROM::kMinVbattFieldNumber,
                                                 BatteryEEPROM::kMaxIbattFieldNumber,
                                                 BatteryEEPROM::kMinIbattFieldNumber,
                                                 BatteryEEPROM::kChecksumFieldNumber};
       std::vector<VendorAtom::Value> values(eeprom_history_fields.size());
       VendorAtom::Value val;

       ALOGD("reportEvent: cycle_cnt:%d, full_cap:%d, esr:%d, rslow:%d, soh:%d, "
             "batt_temp:%d, cutoff_soc:%d, cc_soc:%d, sys_soc:%d, msoc:%d, "
             "batt_soc:%d, reserve:%d, max_temp:%d, min_temp:%d, max_vbatt:%d, "
             "min_vbatt:%d, max_ibatt:%d, min_ibatt:%d, checksum:%d",
             hist.cycle_cnt, hist.full_cap, hist.esr, hist.rslow, hist.soh,
             hist.batt_temp, hist.cutoff_soc, hist.cc_soc, hist.sys_soc,
             hist.msoc, hist.batt_soc, hist.reserve, hist.max_temp,
             hist.min_temp, hist.max_vbatt, hist.min_vbatt, hist.max_ibatt,
             hist.min_ibatt, hist.checksum);

       val.intValue(hist.cycle_cnt);
       values[BatteryEEPROM::kCycleCntFieldNumber - kVendorAtomOffset] = val;
       val.intValue(hist.full_cap);
       values[BatteryEEPROM::kFullCapFieldNumber - kVendorAtomOffset] = val;
       val.intValue(hist.esr);
       values[BatteryEEPROM::kEsrFieldNumber - kVendorAtomOffset] = val;
       val.intValue(hist.rslow);
       values[BatteryEEPROM::kRslowFieldNumber - kVendorAtomOffset] = val;
       val.intValue(hist.soh);
       values[BatteryEEPROM::kSohFieldNumber - kVendorAtomOffset] = val;
       val.intValue(hist.batt_temp);
       values[BatteryEEPROM::kBattTempFieldNumber - kVendorAtomOffset] = val;
       val.intValue(hist.cutoff_soc);
       values[BatteryEEPROM::kCutoffSocFieldNumber - kVendorAtomOffset] = val;
       val.intValue(hist.cc_soc);
       values[BatteryEEPROM::kCcSocFieldNumber - kVendorAtomOffset] = val;
       val.intValue(hist.sys_soc);
       values[BatteryEEPROM::kSysSocFieldNumber - kVendorAtomOffset] = val;
       val.intValue(hist.msoc);
       values[BatteryEEPROM::kMsocFieldNumber - kVendorAtomOffset] = val;
       val.intValue(hist.batt_soc);
       values[BatteryEEPROM::kBattSocFieldNumber - kVendorAtomOffset] = val;
       val.intValue(hist.reserve);
       values[BatteryEEPROM::kReserveFieldNumber - kVendorAtomOffset] = val;
       val.intValue(hist.max_temp);
       values[BatteryEEPROM::kMaxTempFieldNumber - kVendorAtomOffset] = val;
       val.intValue(hist.min_temp);
       values[BatteryEEPROM::kMinTempFieldNumber - kVendorAtomOffset] = val;
       val.intValue(hist.max_vbatt);
       values[BatteryEEPROM::kMaxVbattFieldNumber - kVendorAtomOffset] = val;
       val.intValue(hist.min_vbatt);
       values[BatteryEEPROM::kMinVbattFieldNumber - kVendorAtomOffset] = val;
       val.intValue(hist.max_ibatt);
       values[BatteryEEPROM::kMaxIbattFieldNumber - kVendorAtomOffset] = val;
       val.intValue(hist.min_ibatt);
       values[BatteryEEPROM::kMinIbattFieldNumber - kVendorAtomOffset] = val;
       val.intValue(hist.checksum);
       values[BatteryEEPROM::kChecksumFieldNumber - kVendorAtomOffset] = val;

       VendorAtom event = {.reverseDomainName = PixelAtoms::ReverseDomainNames().pixel(),
                        .atomId = PixelAtoms::Ids::BATTERY_EEPROM,
                        .values = values};
       Return<void> ret = stats_client->reportVendorAtom(event);
       if (!ret.isOk())
           ALOGE("Unable to report BatteryEEPROM to Stats service");
}


}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
