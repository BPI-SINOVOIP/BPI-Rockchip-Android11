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

#ifndef WATCHDOG_SERVER_SRC_PROCPIDSTAT_H_
#define WATCHDOG_SERVER_SRC_PROCPIDSTAT_H_

#include <android-base/result.h>
#include <android-base/stringprintf.h>
#include <gtest/gtest_prod.h>
#include <inttypes.h>
#include <stdint.h>
#include <utils/Mutex.h>
#include <utils/RefBase.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace android {
namespace automotive {
namespace watchdog {

using android::base::StringPrintf;

#define PID_FOR_INIT 1

constexpr const char* kProcDirPath = "/proc";
constexpr const char* kStatFileFormat = "/%" PRIu32 "/stat";
constexpr const char* kTaskDirFormat = "/%" PRIu32 "/task";
constexpr const char* kStatusFileFormat = "/%" PRIu32 "/status";

struct PidStat {
    uint32_t pid = 0;
    std::string comm = "";
    std::string state = "";
    uint32_t ppid = 0;
    uint64_t majorFaults = 0;
    uint32_t numThreads = 0;
    uint64_t startTime = 0;  // Useful when identifying PID/TID reuse
};

struct ProcessStats {
    int64_t tgid = -1;                              // -1 indicates a failure to read this value
    int64_t uid = -1;                               // -1 indicates a failure to read this value
    PidStat process = {};                           // Aggregated stats across all the threads
    std::unordered_map<uint32_t, PidStat> threads;  // Per-thread stat including the main thread
};

// Collector/parser for `/proc/[pid]/stat`, `/proc/[pid]/task/[tid]/stat` and /proc/[pid]/status`
// files.
class ProcPidStat : public RefBase {
public:
    explicit ProcPidStat(const std::string& path = kProcDirPath) :
          mLastProcessStats({}), mPath(path) {
        std::string pidStatPath = StringPrintf((mPath + kStatFileFormat).c_str(), PID_FOR_INIT);
        std::string tidStatPath = StringPrintf((mPath + kTaskDirFormat + kStatFileFormat).c_str(),
                                               PID_FOR_INIT, PID_FOR_INIT);
        std::string pidStatusPath = StringPrintf((mPath + kStatusFileFormat).c_str(), PID_FOR_INIT);

        mEnabled = !access(pidStatPath.c_str(), R_OK) && !access(tidStatPath.c_str(), R_OK) &&
                !access(pidStatusPath.c_str(), R_OK);
    }

    virtual ~ProcPidStat() {}

    // Collects pid info delta since the last collection.
    virtual android::base::Result<std::vector<ProcessStats>> collect();

    // Called by IoPerfCollection and tests.
    virtual bool enabled() { return mEnabled; }

    virtual std::string dirPath() { return mPath; }

private:
    // Reads the contents of the below files:
    // 1. Pid stat file at |mPath| + |kStatFileFormat|
    // 2. Tid stat file at |mPath| + |kTaskDirFormat| + |kStatFileFormat|
    android::base::Result<std::unordered_map<uint32_t, ProcessStats>> getProcessStatsLocked() const;

    // Reads the tgid and real UID for the given PID from |mPath| + |kStatusFileFormat|.
    android::base::Result<void> getPidStatusLocked(ProcessStats* processStats) const;

    // Makes sure only one collection is running at any given time.
    Mutex mMutex;

    // Last dump of per-process stats. Useful for calculating the delta and identifying PID/TID
    // reuse.
    std::unordered_map<uint32_t, ProcessStats> mLastProcessStats GUARDED_BY(mMutex);

    // True if the below files are accessible:
    // 1. Pid stat file at |mPath| + |kTaskStatFileFormat|
    // 2. Tid stat file at |mPath| + |kTaskDirFormat| + |kStatFileFormat|
    // 3. Pid status file at |mPath| + |kStatusFileFormat|
    // Otherwise, set to false.
    bool mEnabled;

    // Proc directory path. Default value is |kProcDirPath|.
    // Updated by tests to point to a different location when needed.
    std::string mPath;

    FRIEND_TEST(IoPerfCollectionTest, TestValidProcPidContents);
    FRIEND_TEST(ProcPidStatTest, TestValidStatFiles);
    FRIEND_TEST(ProcPidStatTest, TestHandlesProcessTerminationBetweenScanningAndParsing);
    FRIEND_TEST(ProcPidStatTest, TestHandlesPidTidReuse);
};

}  // namespace watchdog
}  // namespace automotive
}  // namespace android

#endif  //  WATCHDOG_SERVER_SRC_PROCPIDSTAT_H_
