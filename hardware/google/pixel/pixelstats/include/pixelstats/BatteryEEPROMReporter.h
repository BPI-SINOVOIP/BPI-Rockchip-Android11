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

#ifndef HARDWARE_GOOGLE_PIXEL_PIXELSTATS_BATTERYEEPROMREPORTER_H
#define HARDWARE_GOOGLE_PIXEL_PIXELSTATS_BATTERYEEPROMREPORTER_H

namespace android {
namespace hardware {
namespace google {
namespace pixel {

// The storage for save whole history is 928 byte
// each history contains 19 items with total size 28 byte
// hence the history number is 928/28~33
#define BATT_HIST_NUM_MAX	33

/**
 * A class to upload battery EEPROM metrics
 */
class BatteryEEPROMReporter {
  public:
    BatteryEEPROMReporter();
    void checkAndReport(const std::string &path);

  private:
    // Proto messages are 1-indexed and VendorAtom field numbers start at 2, so
    // store everything in the values array at the index of the field number
    // -2.
    const int kVendorAtomOffset = 2;

    struct BatteryHistory {
        /* The cycle count number; record of charge/discharge times */
        uint16_t cycle_cnt;
        /* The current full capacity of the battery under nominal conditions */
        uint16_t full_cap;
        /* The battery equivalent series resistance */
        uint16_t esr;
        /* Battery resistance related to temperature change */
        uint16_t rslow;
        /* Battery health indicator reflecting the battery age state */
        uint8_t soh;
        /* The battery temperature */
        int8_t batt_temp;

        /* Battery state of charge (SOC) shutdown point */
        uint8_t cutoff_soc;
        /* Raw battery state of charge (SOC), based on battery current (CC = Coulomb Counter) */
        uint8_t cc_soc;
        /* Estimated battery state of charge (SOC) from batt_soc with endpoint limiting (0% and 100%) */
        uint8_t sys_soc;
        /* Filtered monotonic SOC, handles situations where the cutoff_soc is increased and
         * then decreased from the battery physical properties
         */
        uint8_t msoc;
        /* Estimated SOC derived from cc_soc that provides voltage loop feedback correction using
         * battery voltage, current, and status values
         */
        uint8_t batt_soc;

        /* Field used for data padding in the EEPROM data */
        uint8_t reserve;

        /* The maximum battery temperature ever seen */
        int8_t max_temp;
        /* The minimum battery temperature ever seen */
        int8_t min_temp;
        /* The maximum battery voltage ever seen */
        uint16_t max_vbatt;
        /* The minimum battery voltage ever seen */
        uint16_t min_vbatt;
        /* The maximum battery current ever seen */
        int16_t max_ibatt;
        /* The minimum battery current ever seen */
        int16_t min_ibatt;
        /* Field used to verify the integrity of the EEPROM data */
        uint16_t checksum;
    };
    /* The number of elements in struct BatteryHistory */
    const int kNumBatteryHistoryFields = 19;

    int64_t report_time_ = 0;
    int64_t getTimeSecs();

    bool checkLogEvent(struct BatteryHistory hist);
    void reportEvent(struct BatteryHistory hist);
};

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_PIXELSTATS_BATTERYEEPROMREPORTER_H
