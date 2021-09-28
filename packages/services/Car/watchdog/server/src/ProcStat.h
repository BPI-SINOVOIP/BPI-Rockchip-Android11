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

#ifndef WATCHDOG_SERVER_SRC_PROCSTAT_H_
#define WATCHDOG_SERVER_SRC_PROCSTAT_H_

#include <android-base/result.h>
#include <stdint.h>
#include <utils/Mutex.h>
#include <utils/RefBase.h>

namespace android {
namespace automotive {
namespace watchdog {

constexpr const char* kProcStatPath = "/proc/stat";

struct CpuStats {
    uint64_t userTime = 0;       // Time spent in user mode.
    uint64_t niceTime = 0;       // Time spent in user mode with low priority (nice).
    uint64_t sysTime = 0;        // Time spent in system mode.
    uint64_t idleTime = 0;       // Time spent in the idle task.
    uint64_t ioWaitTime = 0;     // Time spent on context switching/waiting due to I/O operations.
    uint64_t irqTime = 0;        // Time servicing interrupts.
    uint64_t softIrqTime = 0;    // Time servicing soft interrupts.
    uint64_t stealTime = 0;      // Stolen time (Time spent in other OS in a virtualized env).
    uint64_t guestTime = 0;      // Time spent running a virtual CPU for guest OS.
    uint64_t guestNiceTime = 0;  // Time spent running a niced virtual CPU for guest OS.
};

class ProcStatInfo {
public:
    ProcStatInfo() : cpuStats({}), runnableProcessesCnt(0), ioBlockedProcessesCnt(0) {}
    ProcStatInfo(CpuStats stats, uint32_t runnableCnt, uint32_t ioBlockedCnt) :
          cpuStats(stats), runnableProcessesCnt(runnableCnt), ioBlockedProcessesCnt(ioBlockedCnt) {}
    CpuStats cpuStats;
    uint32_t runnableProcessesCnt;
    uint32_t ioBlockedProcessesCnt;

    uint64_t totalCpuTime() const {
        return cpuStats.userTime + cpuStats.niceTime + cpuStats.sysTime + cpuStats.idleTime +
                cpuStats.ioWaitTime + cpuStats.irqTime + cpuStats.softIrqTime + cpuStats.stealTime +
                cpuStats.guestTime + cpuStats.guestNiceTime;
    }
    uint32_t totalProcessesCnt() const { return runnableProcessesCnt + ioBlockedProcessesCnt; }
    bool operator==(const ProcStatInfo& info) const {
        return memcmp(&cpuStats, &info.cpuStats, sizeof(cpuStats)) == 0 &&
                runnableProcessesCnt == info.runnableProcessesCnt &&
                ioBlockedProcessesCnt == info.ioBlockedProcessesCnt;
    }
};

// Collector/parser for `/proc/stat` file.
class ProcStat : public RefBase {
public:
    explicit ProcStat(const std::string& path = kProcStatPath) :
          mLastCpuStats({}), kEnabled(!access(path.c_str(), R_OK)), kPath(path) {}

    virtual ~ProcStat() {}

    // Collects proc stat delta since the last collection.
    virtual android::base::Result<ProcStatInfo> collect();

    // Returns true when the proc stat file is accessible. Otherwise, returns false.
    // Called by IoPerfCollection and tests.
    virtual bool enabled() { return kEnabled; }

    virtual std::string filePath() { return kProcStatPath; }

private:
    // Reads the contents of |kPath|.
    android::base::Result<ProcStatInfo> getProcStatLocked() const;

    // Makes sure only one collection is running at any given time.
    Mutex mMutex;

    // Last dump of cpu stats from the file at |kPath|.
    CpuStats mLastCpuStats GUARDED_BY(mMutex);

    // True if |kPath| is accessible.
    const bool kEnabled;

    // Path to proc stat file. Default path is |kProcStatPath|.
    const std::string kPath;
};

}  // namespace watchdog
}  // namespace automotive
}  // namespace android

#endif  //  WATCHDOG_SERVER_SRC_PROCSTAT_H_
