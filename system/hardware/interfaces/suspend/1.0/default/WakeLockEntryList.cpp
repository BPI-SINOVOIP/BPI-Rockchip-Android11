/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "WakeLockEntryList.h"

#include <android-base/file.h>
#include <android-base/logging.h>

#include <iomanip>

using android::base::ReadFdToString;

namespace android {
namespace system {
namespace suspend {
namespace V1_0 {

static std::ostream& operator<<(std::ostream& out, const WakeLockInfo& entry) {
    const char* sep = " | ";
    const char* notApplicable = "---";
    bool kernelWakelock = entry.isKernelWakelock;

    // clang-format off
    out << sep
        << std::left << std::setw(30) << entry.name << sep
        << std::right << std::setw(6)
        << ((kernelWakelock) ? notApplicable : std::to_string(entry.pid)) << sep
        << std::left << std::setw(6) << ((kernelWakelock) ? "Kernel" : "Native") << sep
        << std::left << std::setw(8) << ((entry.isActive) ? "Active" : "Inactive") << sep
        << std::right << std::setw(12) << entry.activeCount << sep
        << std::right << std::setw(12) << std::to_string(entry.totalTime) + "ms" << sep
        << std::right << std::setw(12) << std::to_string(entry.maxTime) + "ms" << sep
        << std::right << std::setw(12)
        << ((kernelWakelock) ? std::to_string(entry.eventCount) : notApplicable) << sep
        << std::right << std::setw(12)
        << ((kernelWakelock) ? std::to_string(entry.wakeupCount) : notApplicable) << sep
        << std::right << std::setw(12)
        << ((kernelWakelock) ? std::to_string(entry.expireCount) : notApplicable) << sep
        << std::right << std::setw(20)
        << ((kernelWakelock) ? std::to_string(entry.preventSuspendTime) + "ms" : notApplicable)
        << sep
        << std::right << std::setw(16) << std::to_string(entry.lastChange) + "ms" << sep;
    // clang-format on

    return out;
}

std::ostream& operator<<(std::ostream& out, const WakeLockEntryList& list) {
    std::vector<WakeLockInfo> wlStats;
    list.getWakeLockStats(&wlStats);
    int width = 194;
    const char* sep = " | ";
    std::stringstream ss;
    ss << "  " << std::setfill('-') << std::setw(width) << "\n";
    std::string div = ss.str();

    out << div;

    std::stringstream header;
    header << sep << std::right << std::setw(((width - 14) / 2) + 14) << "WAKELOCK STATS"
           << std::right << std::setw((width - 14) / 2) << sep << "\n";
    out << header.str();

    out << div;

    // Col names
    // clang-format off
    out << sep
        << std::left << std::setw(30) << "NAME" << sep
        << std::left << std::setw(6) << "PID" << sep
        << std::left << std::setw(6) << "TYPE" << sep
        << std::left << std::setw(8) << "STATUS" << sep
        << std::left << std::setw(12) << "ACTIVE COUNT" << sep
        << std::left << std::setw(12) << "TOTAL TIME" << sep
        << std::left << std::setw(12) << "MAX TIME" << sep
        << std::left << std::setw(12) << "EVENT COUNT" << sep
        << std::left << std::setw(12) << "WAKEUP COUNT" << sep
        << std::left << std::setw(12) << "EXPIRE COUNT" << sep
        << std::left << std::setw(20) << "PREVENT SUSPEND TIME" << sep
        << std::left << std::setw(16) << "LAST CHANGE" << sep
        << "\n";
    // clang-format on

    out << div;

    // Rows
    for (const WakeLockInfo& entry : wlStats) {
        out << entry << "\n";
    }

    out << div;
    return out;
}

/**
 * Returns the monotonic time in milliseconds.
 */
TimestampType getTimeNow() {
    timespec monotime;
    clock_gettime(CLOCK_MONOTONIC, &monotime);
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::nanoseconds{monotime.tv_nsec})
               .count() +
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::seconds{monotime.tv_sec})
               .count();
}

WakeLockEntryList::WakeLockEntryList(size_t capacity, unique_fd kernelWakelockStatsFd)
    : mCapacity(capacity), mKernelWakelockStatsFd(std::move(kernelWakelockStatsFd)) {}

/**
 * Evicts LRU from back of list if stats is at capacity.
 */
void WakeLockEntryList::evictIfFull() {
    if (mStats.size() == mCapacity) {
        auto evictIt = mStats.end();
        std::advance(evictIt, -1);
        auto evictKey = std::make_pair(evictIt->name, evictIt->pid);
        mLookupTable.erase(evictKey);
        mStats.erase(evictIt);
        LOG(ERROR) << "WakeLock Stats: Stats capacity met, consider adjusting capacity to "
                      "avoid stats eviction.";
    }
}

/**
 * Inserts entry as MRU.
 */
void WakeLockEntryList::insertEntry(WakeLockInfo entry) {
    auto key = std::make_pair(entry.name, entry.pid);
    mStats.emplace_front(std::move(entry));
    mLookupTable[key] = mStats.begin();
}

/**
 * Removes entry from the stats list.
 */
void WakeLockEntryList::deleteEntry(std::list<WakeLockInfo>::iterator entry) {
    auto key = std::make_pair(entry->name, entry->pid);
    mLookupTable.erase(key);
    mStats.erase(entry);
}

/**
 * Creates and returns a native wakelock entry.
 */
WakeLockInfo WakeLockEntryList::createNativeEntry(const std::string& name, int pid,
                                                  TimestampType timeNow) const {
    WakeLockInfo info;

    info.name = name;
    // It only makes sense to create a new entry on initial activation of the lock.
    info.activeCount = 1;
    info.lastChange = timeNow;
    info.maxTime = 0;
    info.totalTime = 0;
    info.isActive = true;
    info.activeTime = 0;
    info.isKernelWakelock = false;

    info.pid = pid;

    info.eventCount = 0;
    info.expireCount = 0;
    info.preventSuspendTime = 0;
    info.wakeupCount = 0;

    return info;
}

/*
 * Checks whether a given directory entry is a stat file we're interested in.
 */
static bool isStatFile(const struct dirent* de) {
    const char* statName = de->d_name;
    if (!strcmp(statName, ".") || !strcmp(statName, "..") || !strcmp(statName, "device") ||
        !strcmp(statName, "power") || !strcmp(statName, "subsystem") ||
        !strcmp(statName, "uevent")) {
        return false;
    }
    return true;
}

/*
 * Creates and returns a kernel wakelock entry with data read from mKernelWakelockStatsFd
 */
WakeLockInfo WakeLockEntryList::createKernelEntry(const std::string& kwlId) const {
    WakeLockInfo info;

    info.activeCount = 0;
    info.lastChange = 0;
    info.maxTime = 0;
    info.totalTime = 0;
    info.isActive = false;
    info.activeTime = 0;
    info.isKernelWakelock = true;

    info.pid = -1;  // N/A

    info.eventCount = 0;
    info.expireCount = 0;
    info.preventSuspendTime = 0;
    info.wakeupCount = 0;

    unique_fd wakelockFd{TEMP_FAILURE_RETRY(
        openat(mKernelWakelockStatsFd, kwlId.c_str(), O_DIRECTORY | O_CLOEXEC | O_RDONLY))};
    if (wakelockFd < 0) {
        PLOG(ERROR) << "Error opening kernel wakelock stats for: " << kwlId;
    }

    std::unique_ptr<DIR, decltype(&closedir)> wakelockDp(fdopendir(dup(wakelockFd.get())),
                                                         &closedir);
    if (wakelockDp) {
        struct dirent* de;
        while ((de = readdir(wakelockDp.get()))) {
            if (!isStatFile(de)) {
                continue;
            }

            std::string statName(de->d_name);
            unique_fd statFd{
                TEMP_FAILURE_RETRY(openat(wakelockFd, statName.c_str(), O_CLOEXEC | O_RDONLY))};
            if (statFd < 0) {
                PLOG(ERROR) << "Error opening " << statName << " for " << kwlId;
            }

            std::string valStr;
            if (!ReadFdToString(statFd.get(), &valStr)) {
                PLOG(ERROR) << "Error reading " << statName << " for " << kwlId;
                continue;
            }

            // Trim newline
            valStr.erase(std::remove(valStr.begin(), valStr.end(), '\n'), valStr.end());

            if (statName == "name") {
                info.name = valStr;
                continue;
            }

            int64_t statVal = std::stoll(valStr);

            if (statName == "active_count") {
                info.activeCount = statVal;
            } else if (statName == "active_time_ms") {
                info.activeTime = statVal;
            } else if (statName == "event_count") {
                info.eventCount = statVal;
            } else if (statName == "expire_count") {
                info.expireCount = statVal;
            } else if (statName == "last_change_ms") {
                info.lastChange = statVal;
            } else if (statName == "max_time_ms") {
                info.maxTime = statVal;
            } else if (statName == "prevent_suspend_time_ms") {
                info.preventSuspendTime = statVal;
            } else if (statName == "total_time_ms") {
                info.totalTime = statVal;
            } else if (statName == "wakeup_count") {
                info.wakeupCount = statVal;
            }
        }
    }

    // Derived stats
    info.isActive = info.activeTime > 0;

    return info;
}

void WakeLockEntryList::getKernelWakelockStats(std::vector<WakeLockInfo>* aidl_return) const {
    std::unique_ptr<DIR, decltype(&closedir)> dp(fdopendir(dup(mKernelWakelockStatsFd.get())),
                                                 &closedir);
    if (dp) {
        // rewinddir, else subsequent calls will not get any kernel wakelocks.
        rewinddir(dp.get());

        struct dirent* de;
        while ((de = readdir(dp.get()))) {
            std::string kwlId(de->d_name);
            if ((kwlId == ".") || (kwlId == "..")) {
                continue;
            }
            WakeLockInfo entry = createKernelEntry(kwlId);
            aidl_return->emplace_back(std::move(entry));
        }
    }
}

void WakeLockEntryList::updateOnAcquire(const std::string& name, int pid, TimestampType timeNow) {
    std::lock_guard<std::mutex> lock(mStatsLock);

    auto key = std::make_pair(name, pid);
    auto it = mLookupTable.find(key);
    if (it == mLookupTable.end()) {
        evictIfFull();
        WakeLockInfo newEntry = createNativeEntry(name, pid, timeNow);
        insertEntry(newEntry);
    } else {
        auto staleEntry = it->second;
        WakeLockInfo updatedEntry = *staleEntry;

        // Update entry
        updatedEntry.isActive = true;
        updatedEntry.activeTime = 0;
        updatedEntry.activeCount++;
        updatedEntry.lastChange = timeNow;

        deleteEntry(staleEntry);
        insertEntry(std::move(updatedEntry));
    }
}

void WakeLockEntryList::updateOnRelease(const std::string& name, int pid, TimestampType timeNow) {
    std::lock_guard<std::mutex> lock(mStatsLock);

    auto key = std::make_pair(name, pid);
    auto it = mLookupTable.find(key);
    if (it == mLookupTable.end()) {
        LOG(INFO) << "WakeLock Stats: A stats entry for, \"" << name
                  << "\" was not found. This is most likely due to it being evicted.";
    } else {
        auto staleEntry = it->second;
        WakeLockInfo updatedEntry = *staleEntry;

        // Update entry
        TimestampType timeDelta = timeNow - updatedEntry.lastChange;
        updatedEntry.isActive = false;
        updatedEntry.activeTime += timeDelta;
        updatedEntry.maxTime = std::max(updatedEntry.maxTime, updatedEntry.activeTime);
        updatedEntry.activeTime = 0;  // No longer active
        updatedEntry.totalTime += timeDelta;
        updatedEntry.lastChange = timeNow;

        deleteEntry(staleEntry);
        insertEntry(std::move(updatedEntry));
    }
}
/**
 * Updates the native wakelock stats based on the current time.
 */
void WakeLockEntryList::updateNow() {
    std::lock_guard<std::mutex> lock(mStatsLock);

    TimestampType timeNow = getTimeNow();

    for (std::list<WakeLockInfo>::iterator it = mStats.begin(); it != mStats.end(); ++it) {
        if (it->isActive) {
            TimestampType timeDelta = timeNow - it->lastChange;
            it->activeTime += timeDelta;
            it->maxTime = std::max(it->maxTime, it->activeTime);
            it->totalTime += timeDelta;
            it->lastChange = timeNow;
        }
    }
}

void WakeLockEntryList::getWakeLockStats(std::vector<WakeLockInfo>* aidl_return) const {
    std::lock_guard<std::mutex> lock(mStatsLock);

    for (const WakeLockInfo& entry : mStats) {
        aidl_return->emplace_back(entry);
    }

    getKernelWakelockStats(aidl_return);
}

}  // namespace V1_0
}  // namespace suspend
}  // namespace system
}  // namespace android
