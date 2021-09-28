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

#include "ProcStat.h"

#include <android-base/file.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>
#include <log/log.h>

#include <string>
#include <vector>

namespace android {
namespace automotive {
namespace watchdog {

using android::base::Error;
using android::base::ReadFileToString;
using android::base::Result;
using android::base::StartsWith;
using base::ParseUint;
using base::Split;

namespace {

bool parseCpuStats(const std::string& data, CpuStats* cpuStats) {
    std::vector<std::string> fields = Split(data, " ");
    if (fields.size() == 12 && fields[1].empty()) {
        // The first cpu line will have an extra space after the first word. This will generate an
        // empty element when the line is split on " ". Erase the extra element.
        fields.erase(fields.begin() + 1);
    }
    if (fields.size() != 11 || fields[0] != "cpu" || !ParseUint(fields[1], &cpuStats->userTime) ||
        !ParseUint(fields[2], &cpuStats->niceTime) || !ParseUint(fields[3], &cpuStats->sysTime) ||
        !ParseUint(fields[4], &cpuStats->idleTime) ||
        !ParseUint(fields[5], &cpuStats->ioWaitTime) || !ParseUint(fields[6], &cpuStats->irqTime) ||
        !ParseUint(fields[7], &cpuStats->softIrqTime) ||
        !ParseUint(fields[8], &cpuStats->stealTime) ||
        !ParseUint(fields[9], &cpuStats->guestTime) ||
        !ParseUint(fields[10], &cpuStats->guestNiceTime)) {
        ALOGW("Invalid cpu line: \"%s\"", data.c_str());
        return false;
    }
    return true;
}

bool parseProcsCount(const std::string& data, uint32_t* out) {
    std::vector<std::string> fields = Split(data, " ");
    if (fields.size() != 2 || !StartsWith(fields[0], "procs_") || !ParseUint(fields[1], out)) {
        ALOGW("Invalid procs_ line: \"%s\"", data.c_str());
        return false;
    }
    return true;
}

}  // namespace

Result<ProcStatInfo> ProcStat::collect() {
    if (!kEnabled) {
        return Error() << "Can not access " << kPath;
    }

    Mutex::Autolock lock(mMutex);
    const auto& info = getProcStatLocked();
    if (!info) {
        return Error() << "Failed to get proc stat contents: " << info.error();
    }

    ProcStatInfo delta;

    delta.cpuStats.userTime = info->cpuStats.userTime - mLastCpuStats.userTime;
    delta.cpuStats.niceTime = info->cpuStats.niceTime - mLastCpuStats.niceTime;
    delta.cpuStats.sysTime = info->cpuStats.sysTime - mLastCpuStats.sysTime;
    delta.cpuStats.idleTime = info->cpuStats.idleTime - mLastCpuStats.idleTime;
    delta.cpuStats.ioWaitTime = info->cpuStats.ioWaitTime - mLastCpuStats.ioWaitTime;
    delta.cpuStats.irqTime = info->cpuStats.irqTime - mLastCpuStats.irqTime;
    delta.cpuStats.softIrqTime = info->cpuStats.softIrqTime - mLastCpuStats.softIrqTime;
    delta.cpuStats.stealTime = info->cpuStats.stealTime - mLastCpuStats.stealTime;
    delta.cpuStats.guestTime = info->cpuStats.guestTime - mLastCpuStats.guestTime;
    delta.cpuStats.guestNiceTime = info->cpuStats.guestNiceTime - mLastCpuStats.guestNiceTime;

    // Process counts are real-time values. Thus they should be reported as-is and not their deltas.
    delta.runnableProcessesCnt = info->runnableProcessesCnt;
    delta.ioBlockedProcessesCnt = info->ioBlockedProcessesCnt;

    mLastCpuStats = info->cpuStats;

    return delta;
}

Result<ProcStatInfo> ProcStat::getProcStatLocked() const {
    std::string buffer;
    if (!ReadFileToString(kPath, &buffer)) {
        return Error() << "ReadFileToString failed for " << kPath;
    }

    std::vector<std::string> lines = Split(std::move(buffer), "\n");
    ProcStatInfo info;
    bool didReadProcsRunning = false;
    bool didReadProcsBlocked = false;
    for (size_t i = 0; i < lines.size(); i++) {
        if (lines[i].empty()) {
            continue;
        }
        if (!lines[i].compare(0, 4, "cpu ")) {
            if (info.totalCpuTime() != 0) {
                return Error() << "Duplicate `cpu .*` line in " << kPath;
            }
            if (!parseCpuStats(std::move(lines[i]), &info.cpuStats)) {
                return Error() << "Failed to parse `cpu .*` line in " << kPath;
            }
        } else if (!lines[i].compare(0, 6, "procs_")) {
            if (!lines[i].compare(0, 13, "procs_running")) {
                if (didReadProcsRunning) {
                    return Error() << "Duplicate `procs_running .*` line in " << kPath;
                }
                if (!parseProcsCount(std::move(lines[i]), &info.runnableProcessesCnt)) {
                    return Error() << "Failed to parse `procs_running .*` line in " << kPath;
                }
                didReadProcsRunning = true;
                continue;
            } else if (!lines[i].compare(0, 13, "procs_blocked")) {
                if (didReadProcsBlocked) {
                    return Error() << "Duplicate `procs_blocked .*` line in " << kPath;
                }
                if (!parseProcsCount(std::move(lines[i]), &info.ioBlockedProcessesCnt)) {
                    return Error() << "Failed to parse `procs_blocked .*` line in " << kPath;
                }
                didReadProcsBlocked = true;
                continue;
            }
            return Error() << "Unknown procs_ line `" << lines[i] << "` in " << kPath;
        }
    }
    if (info.totalCpuTime() == 0 || !didReadProcsRunning || !didReadProcsBlocked) {
        return Error() << kPath << " is incomplete";
    }
    return info;
}

}  // namespace watchdog
}  // namespace automotive
}  // namespace android
