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

#include "ProcPidStat.h"

#include <android-base/file.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>
#include <dirent.h>
#include <log/log.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace android {
namespace automotive {
namespace watchdog {

using android::base::EndsWith;
using android::base::Error;
using android::base::ParseInt;
using android::base::ParseUint;
using android::base::ReadFileToString;
using android::base::Result;
using android::base::Split;

namespace {

enum ReadError {
    ERR_INVALID_FILE = 0,
    ERR_FILE_OPEN_READ = 1,
    NUM_ERRORS = 2,
};

// /proc/PID/stat or /proc/PID/task/TID/stat format:
// <pid> <comm> <state> <ppid> <pgrp ID> <session ID> <tty_nr> <tpgid> <flags> <minor faults>
// <children minor faults> <major faults> <children major faults> <user mode time>
// <system mode time> <children user mode time> <children kernel mode time> <priority> <nice value>
// <num threads> <start time since boot> <virtual memory size> <resident set size> <rss soft limit>
// <start code addr> <end code addr> <start stack addr> <ESP value> <EIP> <bitmap of pending sigs>
// <bitmap of blocked sigs> <bitmap of ignored sigs> <waiting channel> <num pages swapped>
// <cumulative pages swapped> <exit signal> <processor #> <real-time prio> <agg block I/O delays>
// <guest time> <children guest time> <start data addr> <end data addr> <start break addr>
// <cmd line args start addr> <amd line args end addr> <env start addr> <env end addr> <exit code>
// Example line: 1 (init) S 0 0 0 0 0 0 0 0 220 0 0 0 0 0 0 0 2 0 0 ...etc...
bool parsePidStatLine(const std::string& line, PidStat* pidStat) {
    std::vector<std::string> fields = Split(line, " ");

    // Note: Regex parsing for the below logic increased the time taken to run the
    // ProcPidStatTest#TestProcPidStatContentsFromDevice from 151.7ms to 1.3 seconds.

    // Comm string is enclosed with ( ) brackets and may contain space(s). Thus calculate the
    // commEndOffset based on the field that contains the closing bracket.
    size_t commEndOffset = 0;
    for (size_t i = 1; i < fields.size(); ++i) {
        pidStat->comm += fields[i];
        if (EndsWith(fields[i], ")")) {
            commEndOffset = i - 1;
            break;
        }
        pidStat->comm += " ";
    }

    if (pidStat->comm.front() != '(' || pidStat->comm.back() != ')') {
        ALOGW("Comm string `%s` not enclosed in brackets", pidStat->comm.c_str());
        return false;
    }
    pidStat->comm.erase(pidStat->comm.begin());
    pidStat->comm.erase(pidStat->comm.end() - 1);

    // The required data is in the first 22 + |commEndOffset| fields so make sure there are at least
    // these many fields in the file.
    if (fields.size() < 22 + commEndOffset || !ParseUint(fields[0], &pidStat->pid) ||
        !ParseUint(fields[3 + commEndOffset], &pidStat->ppid) ||
        !ParseUint(fields[11 + commEndOffset], &pidStat->majorFaults) ||
        !ParseUint(fields[19 + commEndOffset], &pidStat->numThreads) ||
        !ParseUint(fields[21 + commEndOffset], &pidStat->startTime)) {
        ALOGW("Invalid proc pid stat contents: \"%s\"", line.c_str());
        return false;
    }
    pidStat->state = fields[2 + commEndOffset];
    return true;
}

Result<void> readPidStatFile(const std::string& path, PidStat* pidStat) {
    std::string buffer;
    if (!ReadFileToString(path, &buffer)) {
        return Error(ERR_FILE_OPEN_READ) << "ReadFileToString failed for " << path;
    }
    std::vector<std::string> lines = Split(std::move(buffer), "\n");
    if (lines.size() != 1 && (lines.size() != 2 || !lines[1].empty())) {
        return Error(ERR_INVALID_FILE) << path << " contains " << lines.size() << " lines != 1";
    }
    if (!parsePidStatLine(std::move(lines[0]), pidStat)) {
        return Error(ERR_INVALID_FILE) << "Failed to parse the contents of " << path;
    }
    return {};
}

}  // namespace

Result<std::vector<ProcessStats>> ProcPidStat::collect() {
    if (!mEnabled) {
        return Error() << "Can not access PID stat files under " << kProcDirPath;
    }

    Mutex::Autolock lock(mMutex);
    const auto& processStats = getProcessStatsLocked();
    if (!processStats) {
        return Error() << processStats.error();
    }

    std::vector<ProcessStats> delta;
    for (const auto& it : *processStats) {
        const ProcessStats& curStats = it.second;
        const auto& cachedIt = mLastProcessStats.find(it.first);
        if (cachedIt == mLastProcessStats.end() ||
            cachedIt->second.process.startTime != curStats.process.startTime) {
            // New/reused PID so don't calculate the delta.
            delta.emplace_back(curStats);
            continue;
        }

        ProcessStats deltaStats = curStats;
        const ProcessStats& cachedStats = cachedIt->second;
        deltaStats.process.majorFaults -= cachedStats.process.majorFaults;
        for (auto& deltaThread : deltaStats.threads) {
            const auto& cachedThread = cachedStats.threads.find(deltaThread.first);
            if (cachedThread == cachedStats.threads.end() ||
                cachedThread->second.startTime != deltaThread.second.startTime) {
                // New TID or TID reused by the same PID so don't calculate the delta.
                continue;
            }
            deltaThread.second.majorFaults -= cachedThread->second.majorFaults;
        }
        delta.emplace_back(deltaStats);
    }
    mLastProcessStats = *processStats;
    return delta;
}

Result<std::unordered_map<uint32_t, ProcessStats>> ProcPidStat::getProcessStatsLocked() const {
    std::unordered_map<uint32_t, ProcessStats> processStats;
    auto procDirp = std::unique_ptr<DIR, int (*)(DIR*)>(opendir(mPath.c_str()), closedir);
    if (!procDirp) {
        return Error() << "Failed to open " << mPath << " directory";
    }
    dirent* pidDir = nullptr;
    while ((pidDir = readdir(procDirp.get())) != nullptr) {
        // 1. Read top-level pid stats.
        uint32_t pid = 0;
        if (pidDir->d_type != DT_DIR || !ParseUint(pidDir->d_name, &pid)) {
            continue;
        }
        ProcessStats curStats;
        std::string path = StringPrintf((mPath + kStatFileFormat).c_str(), pid);
        const auto& ret = readPidStatFile(path, &curStats.process);
        if (!ret) {
            // PID may disappear between scanning the directory and parsing the stat file.
            // Thus treat ERR_FILE_OPEN_READ errors as soft errors.
            if (ret.error().code() != ERR_FILE_OPEN_READ) {
                return Error() << "Failed to read top-level per-process stat file: "
                               << ret.error().message().c_str();
            }
            ALOGW("Failed to read top-level per-process stat file %s: %s", path.c_str(),
                  ret.error().message().c_str());
            continue;
        }

        // 2. When not found in the cache, fetch tgid/UID as soon as possible because processes
        // may terminate during scanning.
        const auto& it = mLastProcessStats.find(curStats.process.pid);
        if (it == mLastProcessStats.end() ||
            it->second.process.startTime != curStats.process.startTime || it->second.tgid == -1 ||
            it->second.uid == -1) {
            const auto& ret = getPidStatusLocked(&curStats);
            if (!ret) {
                if (ret.error().code() != ERR_FILE_OPEN_READ) {
                    return Error() << "Failed to read pid status for pid " << curStats.process.pid
                                   << ": " << ret.error().message().c_str();
                }
                ALOGW("Failed to read pid status for pid %" PRIu32 ": %s", curStats.process.pid,
                      ret.error().message().c_str());
                // Default tgid and uid values are -1 (aka unknown).
            }
        } else {
            // Fetch from cache.
            curStats.tgid = it->second.tgid;
            curStats.uid = it->second.uid;
        }

        if (curStats.tgid != -1 && curStats.tgid != curStats.process.pid) {
            ALOGW("Skipping non-process (i.e., Tgid != PID) entry for PID %" PRIu32,
                  curStats.process.pid);
            continue;
        }

        // 3. Fetch per-thread stats.
        std::string taskDir = StringPrintf((mPath + kTaskDirFormat).c_str(), pid);
        auto taskDirp = std::unique_ptr<DIR, int (*)(DIR*)>(opendir(taskDir.c_str()), closedir);
        if (!taskDirp) {
            // Treat this as a soft error so at least the process stats will be collected.
            ALOGW("Failed to open %s directory", taskDir.c_str());
        }
        dirent* tidDir = nullptr;
        bool didReadMainThread = false;
        while (taskDirp != nullptr && (tidDir = readdir(taskDirp.get())) != nullptr) {
            uint32_t tid = 0;
            if (tidDir->d_type != DT_DIR || !ParseUint(tidDir->d_name, &tid)) {
                continue;
            }
            if (processStats.find(tid) != processStats.end()) {
                return Error() << "Process stats already exists for TID " << tid
                               << ". Stats will be double counted";
            }

            PidStat curThreadStat = {};
            path = StringPrintf((taskDir + kStatFileFormat).c_str(), tid);
            const auto& ret = readPidStatFile(path, &curThreadStat);
            if (!ret) {
                if (ret.error().code() != ERR_FILE_OPEN_READ) {
                    return Error() << "Failed to read per-thread stat file: "
                                   << ret.error().message().c_str();
                }
                // Maybe the thread terminated before reading the file so skip this thread and
                // continue with scanning the next thread's stat.
                ALOGW("Failed to read per-thread stat file %s: %s", path.c_str(),
                      ret.error().message().c_str());
                continue;
            }
            if (curThreadStat.pid == curStats.process.pid) {
                didReadMainThread = true;
            }
            curStats.threads[curThreadStat.pid] = curThreadStat;
        }
        if (!didReadMainThread) {
            // In the event of failure to read main-thread info (mostly because the process
            // terminated during scanning/parsing), fill out the stat that are common between main
            // thread and the process.
            curStats.threads[curStats.process.pid] = PidStat{
                    .pid = curStats.process.pid,
                    .comm = curStats.process.comm,
                    .state = curStats.process.state,
                    .ppid = curStats.process.ppid,
                    .numThreads = curStats.process.numThreads,
                    .startTime = curStats.process.startTime,
            };
        }
        processStats[curStats.process.pid] = curStats;
    }
    return processStats;
}

Result<void> ProcPidStat::getPidStatusLocked(ProcessStats* processStats) const {
    std::string buffer;
    std::string path = StringPrintf((mPath + kStatusFileFormat).c_str(), processStats->process.pid);
    if (!ReadFileToString(path, &buffer)) {
        return Error(ERR_FILE_OPEN_READ) << "ReadFileToString failed for " << path;
    }
    std::vector<std::string> lines = Split(std::move(buffer), "\n");
    bool didReadUid = false;
    bool didReadTgid = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].empty()) {
            continue;
        }
        if (!lines[i].compare(0, 4, "Uid:")) {
            if (didReadUid) {
                return Error(ERR_INVALID_FILE)
                        << "Duplicate UID line: \"" << lines[i] << "\" in file " << path;
            }
            std::vector<std::string> fields = Split(lines[i], "\t");
            if (fields.size() < 2 || !ParseInt(fields[1], &processStats->uid)) {
                return Error(ERR_INVALID_FILE)
                        << "Invalid UID line: \"" << lines[i] << "\" in file " << path;
            }
            didReadUid = true;
        } else if (!lines[i].compare(0, 5, "Tgid:")) {
            if (didReadTgid) {
                return Error(ERR_INVALID_FILE)
                        << "Duplicate Tgid line: \"" << lines[i] << "\" in file" << path;
            }
            std::vector<std::string> fields = Split(lines[i], "\t");
            if (fields.size() != 2 || !ParseInt(fields[1], &processStats->tgid)) {
                return Error(ERR_INVALID_FILE)
                        << "Invalid tgid line: \"" << lines[i] << "\" in file" << path;
            }
            didReadTgid = true;
        }
    }
    if (!didReadUid || !didReadTgid) {
        return Error(ERR_INVALID_FILE) << "Incomplete file " << mPath + kStatusFileFormat;
    }
    return {};
}

}  // namespace watchdog
}  // namespace automotive
}  // namespace android
