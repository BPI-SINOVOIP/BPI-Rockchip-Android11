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

#ifndef ANDROID_SYSTEM_SUSPEND_WAKE_LOCK_ENTRY_LIST_H
#define ANDROID_SYSTEM_SUSPEND_WAKE_LOCK_ENTRY_LIST_H

#include <android-base/unique_fd.h>
#include <android/system/suspend/WakeLockInfo.h>
#include <utils/Mutex.h>

#include <list>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace android {
namespace system {
namespace suspend {
namespace V1_0 {

using android::base::unique_fd;
using TimestampType = int64_t;

TimestampType getTimeNow();

/*
 * WakeLockEntryList to collect wake lock stats.
 * This class is thread safe.
 */
class WakeLockEntryList {
   public:
    WakeLockEntryList(size_t capacity, unique_fd kernelWakelockStatsFd);
    void updateOnAcquire(const std::string& name, int pid, TimestampType timeNow);
    void updateOnRelease(const std::string& name, int pid, TimestampType timeNow);
    // updateNow() should be called before getWakeLockStats() to ensure stats are
    // updated wrt the current time.
    void updateNow();
    void getWakeLockStats(std::vector<WakeLockInfo>* aidl_return) const;
    friend std::ostream& operator<<(std::ostream& out, const WakeLockEntryList& list);

   private:
    void evictIfFull() REQUIRES(mStatsLock);
    void insertEntry(WakeLockInfo entry) REQUIRES(mStatsLock);
    void deleteEntry(std::list<WakeLockInfo>::iterator entry) REQUIRES(mStatsLock);
    WakeLockInfo createNativeEntry(const std::string& name, int pid, TimestampType timeNow) const;
    WakeLockInfo createKernelEntry(const std::string& name) const;
    void getKernelWakelockStats(std::vector<WakeLockInfo>* aidl_return) const;

    // Hash for WakeLockEntry key (pair<std::string, int>)
    struct LockHash {
        std::size_t operator()(const std::pair<std::string, int>& key) const {
            return std::hash<std::string>()(key.first) ^ std::hash<int>()(key.second);
        }
    };

    size_t mCapacity;
    unique_fd mKernelWakelockStatsFd;

    mutable std::mutex mStatsLock;

    // std::list and std::unordered map are used to support both inserting a stat
    // and eviction of the LRU stat in O(1) time. The LRU stat is maintained at
    // the back of the list.
    std::list<WakeLockInfo> mStats GUARDED_BY(mStatsLock);
    std::unordered_map<std::pair<std::string, int>, std::list<WakeLockInfo>::iterator, LockHash>
        mLookupTable GUARDED_BY(mStatsLock);
};

}  // namespace V1_0
}  // namespace suspend
}  // namespace system
}  // namespace android

#endif  // ANDROID_SYSTEM_SUSPEND_WAKE_LOCK_ENTRY_LIST_H
