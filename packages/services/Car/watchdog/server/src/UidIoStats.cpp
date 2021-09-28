/**
 * Copyright (c) 2020, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "carwatchdogd"
#define DEBUG false

#include "UidIoStats.h"

#include <android-base/file.h>
#include <android-base/parseint.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <inttypes.h>
#include <log/log.h>

#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

namespace android {
namespace automotive {
namespace watchdog {

using android::base::Error;
using android::base::ReadFileToString;
using android::base::Result;
using android::base::StringPrintf;
using base::ParseUint;
using base::Split;

namespace {

bool parseUidIoStats(const std::string& data, UidIoStat* uidIoStat) {
    std::vector<std::string> fields = Split(data, " ");
    if (fields.size() < 11 || !ParseUint(fields[0], &uidIoStat->uid) ||
        !ParseUint(fields[1], &uidIoStat->io[FOREGROUND].rchar) ||
        !ParseUint(fields[2], &uidIoStat->io[FOREGROUND].wchar) ||
        !ParseUint(fields[3], &uidIoStat->io[FOREGROUND].readBytes) ||
        !ParseUint(fields[4], &uidIoStat->io[FOREGROUND].writeBytes) ||
        !ParseUint(fields[5], &uidIoStat->io[BACKGROUND].rchar) ||
        !ParseUint(fields[6], &uidIoStat->io[BACKGROUND].wchar) ||
        !ParseUint(fields[7], &uidIoStat->io[BACKGROUND].readBytes) ||
        !ParseUint(fields[8], &uidIoStat->io[BACKGROUND].writeBytes) ||
        !ParseUint(fields[9], &uidIoStat->io[FOREGROUND].fsync) ||
        !ParseUint(fields[10], &uidIoStat->io[BACKGROUND].fsync)) {
        ALOGW("Invalid uid I/O stats: \"%s\"", data.c_str());
        return false;
    }
    return true;
}

}  // namespace

bool IoUsage::isZero() const {
    for (int i = 0; i < METRIC_TYPES; i++) {
        for (int j = 0; j < UID_STATES; j++) {
            if (metrics[i][j]) {
                return false;
            }
        }
    }
    return true;
}

std::string IoUsage::toString() const {
    return StringPrintf("FgRdBytes:%" PRIu64 " BgRdBytes:%" PRIu64 " FgWrBytes:%" PRIu64
                        " BgWrBytes:%" PRIu64 " FgFsync:%" PRIu64 " BgFsync:%" PRIu64,
                        metrics[READ_BYTES][FOREGROUND], metrics[READ_BYTES][BACKGROUND],
                        metrics[WRITE_BYTES][FOREGROUND], metrics[WRITE_BYTES][BACKGROUND],
                        metrics[FSYNC_COUNT][FOREGROUND], metrics[FSYNC_COUNT][BACKGROUND]);
}

Result<std::unordered_map<uint32_t, UidIoUsage>> UidIoStats::collect() {
    if (!kEnabled) {
        return Error() << "Can not access " << kPath;
    }

    Mutex::Autolock lock(mMutex);
    const auto& uidIoStats = getUidIoStatsLocked();
    if (!uidIoStats.ok() || uidIoStats->empty()) {
        return Error() << "Failed to get UID IO stats: " << uidIoStats.error();
    }

    std::unordered_map<uint32_t, UidIoUsage> usage;
    for (const auto& it : *uidIoStats) {
        const UidIoStat& uidIoStat = it.second;
        usage[uidIoStat.uid] = {};
        struct UidIoUsage& uidUsage = usage[uidIoStat.uid];
        uidUsage.uid = uidIoStat.uid;

        int64_t fgRdDelta = uidIoStat.io[FOREGROUND].readBytes -
                            mLastUidIoStats[uidIoStat.uid].io[FOREGROUND].readBytes;
        int64_t bgRdDelta = uidIoStat.io[BACKGROUND].readBytes -
                            mLastUidIoStats[uidIoStat.uid].io[BACKGROUND].readBytes;
        int64_t fgWrDelta = uidIoStat.io[FOREGROUND].writeBytes -
                            mLastUidIoStats[uidIoStat.uid].io[FOREGROUND].writeBytes;
        int64_t bgWrDelta = uidIoStat.io[BACKGROUND].writeBytes -
                            mLastUidIoStats[uidIoStat.uid].io[BACKGROUND].writeBytes;
        int64_t fgFsDelta =
            uidIoStat.io[FOREGROUND].fsync - mLastUidIoStats[uidIoStat.uid].io[FOREGROUND].fsync;
        int64_t bgFsDelta =
            uidIoStat.io[BACKGROUND].fsync - mLastUidIoStats[uidIoStat.uid].io[BACKGROUND].fsync;

        uidUsage.ios.metrics[READ_BYTES][FOREGROUND] += (fgRdDelta < 0) ? 0 : fgRdDelta;
        uidUsage.ios.metrics[READ_BYTES][BACKGROUND] += (bgRdDelta < 0) ? 0 : bgRdDelta;
        uidUsage.ios.metrics[WRITE_BYTES][FOREGROUND] += (fgWrDelta < 0) ? 0 : fgWrDelta;
        uidUsage.ios.metrics[WRITE_BYTES][BACKGROUND] += (bgWrDelta < 0) ? 0 : bgWrDelta;
        uidUsage.ios.metrics[FSYNC_COUNT][FOREGROUND] += (fgFsDelta < 0) ? 0 : fgFsDelta;
        uidUsage.ios.metrics[FSYNC_COUNT][BACKGROUND] += (bgFsDelta < 0) ? 0 : bgFsDelta;
    }
    mLastUidIoStats = *uidIoStats;
    return usage;
}

Result<std::unordered_map<uint32_t, UidIoStat>> UidIoStats::getUidIoStatsLocked() const {
    std::string buffer;
    if (!ReadFileToString(kPath, &buffer)) {
        return Error() << "ReadFileToString failed for " << kPath;
    }

    std::vector<std::string> ioStats = Split(std::move(buffer), "\n");
    std::unordered_map<uint32_t, UidIoStat> uidIoStats;
    UidIoStat uidIoStat;
    for (size_t i = 0; i < ioStats.size(); i++) {
        if (ioStats[i].empty() || !ioStats[i].compare(0, 4, "task")) {
            // Skip per-task stats as CONFIG_UID_SYS_STATS_DEBUG is not set in the kernel and
            // the collected data is aggregated only per-UID.
            continue;
        }
        if (!parseUidIoStats(std::move(ioStats[i]), &uidIoStat)) {
            return Error() << "Failed to parse the contents of " << kPath;
        }
        uidIoStats[uidIoStat.uid] = uidIoStat;
    }
    return uidIoStats;
}

}  // namespace watchdog
}  // namespace automotive
}  // namespace android
