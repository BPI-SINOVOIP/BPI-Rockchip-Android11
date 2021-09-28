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

#ifndef WATCHDOG_SERVER_SRC_UIDIOSTATS_H_
#define WATCHDOG_SERVER_SRC_UIDIOSTATS_H_

#include <android-base/result.h>
#include <stdint.h>
#include <utils/Mutex.h>
#include <utils/RefBase.h>

#include <string>
#include <unordered_map>

namespace android {
namespace automotive {
namespace watchdog {

constexpr const char* kUidIoStatsPath = "/proc/uid_io/stats";

enum UidState {
    FOREGROUND = 0,
    BACKGROUND,
    UID_STATES,
};

enum MetricType {
    READ_BYTES = 0,
    WRITE_BYTES,
    FSYNC_COUNT,
    METRIC_TYPES,
};

struct IoStat {
    uint64_t rchar = 0;       // characters read
    uint64_t wchar = 0;       // characters written
    uint64_t readBytes = 0;   // bytes read (from storage layer)
    uint64_t writeBytes = 0;  // bytes written (to storage layer)
    uint64_t fsync = 0;       // number of fsync syscalls
};

struct UidIoStat {
    uint32_t uid = 0;  // linux user id
    IoStat io[UID_STATES] = {{}};
};

class IoUsage {
  public:
    IoUsage() : metrics{{0}} {};
    IoUsage(uint64_t fgRdBytes, uint64_t bgRdBytes, uint64_t fgWrBytes, uint64_t bgWrBytes,
            uint64_t fgFsync, uint64_t bgFsync) {
        metrics[READ_BYTES][FOREGROUND] = fgRdBytes;
        metrics[READ_BYTES][BACKGROUND] = bgRdBytes;
        metrics[WRITE_BYTES][FOREGROUND] = fgWrBytes;
        metrics[WRITE_BYTES][BACKGROUND] = bgWrBytes;
        metrics[FSYNC_COUNT][FOREGROUND] = fgFsync;
        metrics[FSYNC_COUNT][BACKGROUND] = bgFsync;
    }
    bool operator==(const IoUsage& usage) const {
        return memcmp(&metrics, &usage.metrics, sizeof(metrics)) == 0;
    }
    uint64_t sumReadBytes() const {
        return metrics[READ_BYTES][FOREGROUND] + metrics[READ_BYTES][BACKGROUND];
    }
    uint64_t sumWriteBytes() const {
        return metrics[WRITE_BYTES][FOREGROUND] + metrics[WRITE_BYTES][BACKGROUND];
    }
    bool isZero() const;
    std::string toString() const;
    uint64_t metrics[METRIC_TYPES][UID_STATES];
};

struct UidIoUsage {
    uint32_t uid = 0;
    IoUsage ios = {};
};

class UidIoStats : public RefBase {
public:
    explicit UidIoStats(const std::string& path = kUidIoStatsPath) :
          kEnabled(!access(path.c_str(), R_OK)), kPath(path) {}

    virtual ~UidIoStats() {}

    // Collects the I/O usage since the last collection.
    virtual android::base::Result<std::unordered_map<uint32_t, UidIoUsage>> collect();

    // Returns true when the uid_io stats file is accessible. Otherwise, returns false.
    // Called by IoPerfCollection and tests.
    virtual bool enabled() { return kEnabled; }

    virtual std::string filePath() { return kPath; }

private:
    // Reads the contents of |kPath|.
    android::base::Result<std::unordered_map<uint32_t, UidIoStat>> getUidIoStatsLocked() const;

    // Makes sure only one collection is running at any given time.
    Mutex mMutex;

    // Last dump from the file at |kPath|.
    std::unordered_map<uint32_t, UidIoStat> mLastUidIoStats GUARDED_BY(mMutex);

    // True if kPath is accessible.
    const bool kEnabled;

    // Path to uid_io stats file. Default path is |kUidIoStatsPath|.
    const std::string kPath;
};

}  // namespace watchdog
}  // namespace automotive
}  // namespace android

#endif  //  WATCHDOG_SERVER_SRC_UIDIOSTATS_H_
