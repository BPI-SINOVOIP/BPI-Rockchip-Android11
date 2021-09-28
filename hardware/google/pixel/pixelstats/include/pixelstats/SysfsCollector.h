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

#ifndef HARDWARE_GOOGLE_PIXEL_PIXELSTATS_SYSFSCOLLECTOR_H
#define HARDWARE_GOOGLE_PIXEL_PIXELSTATS_SYSFSCOLLECTOR_H

#include <android/frameworks/stats/1.0/IStats.h>
#include <utils/StrongPointer.h>

#include "BatteryEEPROMReporter.h"

using android::sp;
using android::frameworks::stats::V1_0::IStats;
using android::frameworks::stats::V1_0::SlowIo;

namespace android {
namespace hardware {
namespace google {
namespace pixel {

class SysfsCollector {
  public:
    struct SysfsPaths {
        const char *const SlowioReadCntPath;
        const char *const SlowioWriteCntPath;
        const char *const SlowioUnmapCntPath;
        const char *const SlowioSyncCntPath;
        const char *const CycleCountBinsPath;
        const char *const ImpedancePath;
        const char *const CodecPath;
        const char *const Codec1Path;
        const char *const SpeechDspPath;
        const char *const BatteryCapacityCC;
        const char *const BatteryCapacityVFSOC;
        const char *const UFSLifetimeA;
        const char *const UFSLifetimeB;
        const char *const UFSLifetimeC;
        const char *const F2fsStatsPath;
        const char *const UserdataBlockProp;
        const char *const ZramMmStatPath;
        const char *const ZramBdStatPath;
        const char *const EEPROMPath;
    };

    SysfsCollector(const struct SysfsPaths &paths);
    void collect();

  private:
    bool ReadFileToInt(const std::string &path, int *val);
    bool ReadFileToInt(const char *path, int *val);
    void logAll();

    void logBatteryChargeCycles();
    void logCodecFailed();
    void logCodec1Failed();
    void logSlowIO();
    void logSpeakerImpedance();
    void logSpeechDspStat();
    void logBatteryCapacity();
    void logUFSLifetime();
    void logF2fsStats();
    void logZramStats();
    void logBootStats();
    void logBatteryEEPROM();

    void reportSlowIoFromFile(const char *path, const SlowIo::IoOperation &operation_s);
    void reportZramMmStat();
    void reportZramBdStat();

    unsigned int getValueFromStatus(std::string &f2fsStatus, const char * key);

    const char *const kSlowioReadCntPath;
    const char *const kSlowioWriteCntPath;
    const char *const kSlowioUnmapCntPath;
    const char *const kSlowioSyncCntPath;
    const char *const kCycleCountBinsPath;
    const char *const kImpedancePath;
    const char *const kCodecPath;
    const char *const kCodec1Path;
    const char *const kSpeechDspPath;
    const char *const kBatteryCapacityCC;
    const char *const kBatteryCapacityVFSOC;
    const char *const kUFSLifetimeA;
    const char *const kUFSLifetimeB;
    const char *const kUFSLifetimeC;
    const char *const kF2fsStatsPath;
    const char *const kUserdataBlockProp;
    const char *const kZramMmStatPath;
    const char *const kZramBdStatPath;
    const char *const kEEPROMPath;
    sp<IStats> stats_;

    BatteryEEPROMReporter battery_EEPROM_reporter_;

    // Proto messages are 1-indexed and VendorAtom field numbers start at 2, so
    // store everything in the values array at the index of the field number
    // -2.
    const int kVendorAtomOffset = 2;

    bool log_once_reported = false;
};

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_PIXELSTATS_SYSFSCOLLECTOR_H
