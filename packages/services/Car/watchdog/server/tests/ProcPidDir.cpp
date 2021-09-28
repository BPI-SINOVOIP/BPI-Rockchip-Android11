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

#include "ProcPidDir.h"

#include <android-base/file.h>
#include <android-base/result.h>
#include <errno.h>

#include "ProcPidStat.h"

namespace android {
namespace automotive {
namespace watchdog {
namespace testing {

using android::base::Error;
using android::base::Result;
using android::base::WriteStringToFile;

namespace {

Result<void> makeDir(std::string path) {
    if (mkdir(path.c_str(), 0700) && errno != EEXIST) {
        return Error() << "Could not mkdir " << path << ": " << strerror(errno);
    }
    return {};
}

}  // namespace

Result<void> populateProcPidDir(
        const std::string& procDirPath,
        const std::unordered_map<uint32_t, std::vector<uint32_t>>& pidToTids,
        const std::unordered_map<uint32_t, std::string>& processStat,
        const std::unordered_map<uint32_t, std::string>& processStatus,
        const std::unordered_map<uint32_t, std::string>& threadStat) {
    for (const auto& it : pidToTids) {
        // 1. Create /proc/PID dir.
        const auto& pidDirRes = makeDir(StringPrintf("%s/%" PRIu32, procDirPath.c_str(), it.first));
        if (!pidDirRes) {
            return Error() << "Failed to create top-level per-process directory: "
                           << pidDirRes.error();
        }

        // 2. Create /proc/PID/stat file.
        uint32_t pid = it.first;
        if (processStat.find(pid) != processStat.end()) {
            std::string path = StringPrintf((procDirPath + kStatFileFormat).c_str(), pid);
            if (!WriteStringToFile(processStat.at(pid), path)) {
                return Error() << "Failed to write pid stat file " << path;
            }
        }

        // 3. Create /proc/PID/status file.
        if (processStatus.find(pid) != processStatus.end()) {
            std::string path = StringPrintf((procDirPath + kStatusFileFormat).c_str(), pid);
            if (!WriteStringToFile(processStatus.at(pid), path)) {
                return Error() << "Failed to write pid status file " << path;
            }
        }

        // 4. Create /proc/PID/task dir.
        const auto& taskDirRes = makeDir(StringPrintf((procDirPath + kTaskDirFormat).c_str(), pid));
        if (!taskDirRes) {
            return Error() << "Failed to create task directory: " << taskDirRes.error();
        }

        // 5. Create /proc/PID/task/TID dirs and /proc/PID/task/TID/stat files.
        for (const auto& tid : it.second) {
            const auto& tidDirRes = makeDir(
                    StringPrintf((procDirPath + kTaskDirFormat + "/%" PRIu32).c_str(), pid, tid));
            if (!tidDirRes) {
                return Error() << "Failed to create per-thread directory: " << tidDirRes.error();
            }
            if (threadStat.find(tid) != threadStat.end()) {
                std::string path =
                        StringPrintf((procDirPath + kTaskDirFormat + kStatFileFormat).c_str(), pid,
                                     tid);
                if (!WriteStringToFile(threadStat.at(tid), path)) {
                    return Error() << "Failed to write thread stat file " << path;
                }
            }
        }
    }

    return {};
}

}  // namespace testing
}  // namespace watchdog
}  // namespace automotive
}  // namespace android
