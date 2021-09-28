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

#ifndef WATCHDOG_SERVER_SRC_IOPERFCOLLECTION_H_
#define WATCHDOG_SERVER_SRC_IOPERFCOLLECTION_H_

#include <android-base/chrono_utils.h>
#include <android-base/result.h>
#include <android/content/pm/IPackageManagerNative.h>
#include <cutils/multiuser.h>
#include <gtest/gtest_prod.h>
#include <time.h>
#include <utils/Errors.h>
#include <utils/Looper.h>
#include <utils/Mutex.h>
#include <utils/String16.h>
#include <utils/StrongPointer.h>
#include <utils/Vector.h>

#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "LooperWrapper.h"
#include "ProcPidStat.h"
#include "ProcStat.h"
#include "UidIoStats.h"

namespace android {
namespace automotive {
namespace watchdog {

constexpr const char* kStartCustomCollectionFlag = "--start_io";
constexpr const char* kEndCustomCollectionFlag = "--stop_io";
constexpr const char* kIntervalFlag = "--interval";
constexpr const char* kMaxDurationFlag = "--max_duration";
constexpr const char* kFilterPackagesFlag = "--filter_packages";

// Performance data collected from the `/proc/uid_io/stats` file.
struct UidIoPerfData {
    struct Stats {
        userid_t userId = 0;
        std::string packageName;
        uint64_t bytes[UID_STATES];
        uint64_t fsync[UID_STATES];
    };
    std::vector<Stats> topNReads = {};
    std::vector<Stats> topNWrites = {};
    uint64_t total[METRIC_TYPES][UID_STATES] = {{0}};
};

std::string toString(const UidIoPerfData& perfData);

// Performance data collected from the `/proc/stats` file.
struct SystemIoPerfData {
    uint64_t cpuIoWaitTime = 0;
    uint64_t totalCpuTime = 0;
    uint32_t ioBlockedProcessesCnt = 0;
    uint32_t totalProcessesCnt = 0;
};

std::string toString(const SystemIoPerfData& perfData);

// Performance data collected from the `/proc/[pid]/stat` and `/proc/[pid]/task/[tid]/stat` files.
struct ProcessIoPerfData {
    struct UidStats {
        userid_t userId = 0;
        std::string packageName;
        uint64_t count = 0;
        struct ProcessStats {
            std::string comm = "";
            uint64_t count = 0;
        };
        std::vector<ProcessStats> topNProcesses = {};
    };
    std::vector<UidStats> topNIoBlockedUids = {};
    // Total # of tasks owned by each UID in |topNIoBlockedUids|.
    std::vector<uint64_t> topNIoBlockedUidsTotalTaskCnt = {};
    std::vector<UidStats> topNMajorFaultUids = {};
    uint64_t totalMajorFaults = 0;
    // Percentage of increase/decrease in the major page faults since last collection.
    double majorFaultsPercentChange = 0.0;
};

std::string toString(const ProcessIoPerfData& data);

struct IoPerfRecord {
    time_t time;  // Collection time.
    UidIoPerfData uidIoPerfData;
    SystemIoPerfData systemIoPerfData;
    ProcessIoPerfData processIoPerfData;
};

std::string toString(const IoPerfRecord& record);

struct CollectionInfo {
    std::chrono::nanoseconds interval = 0ns;  // Collection interval between subsequent collections.
    size_t maxCacheSize = 0;                  // Maximum cache size for the collection.
    std::unordered_set<std::string> filterPackages;  // Filter the output only to the specified
                                                     // packages.
    nsecs_t lastCollectionUptime = 0;         // Used to calculate the uptime for next collection.
    std::vector<IoPerfRecord> records;        // Cache of collected performance records.
};

std::string toString(const CollectionInfo& collectionInfo);

enum CollectionEvent {
    INIT = 0,
    BOOT_TIME,
    PERIODIC,
    CUSTOM,
    TERMINATED,
    LAST_EVENT,
};

enum SwitchEvent {
    // Ends boot-time collection by collecting the last boot-time record and switching the
    // collection event to periodic collection.
    END_BOOTTIME_COLLECTION = CollectionEvent::LAST_EVENT + 1,
    // Ends custom collection, discards collected data and starts periodic collection.
    END_CUSTOM_COLLECTION
};

static inline std::string toString(CollectionEvent event) {
    switch (event) {
        case CollectionEvent::INIT:
            return "INIT";
        case CollectionEvent::BOOT_TIME:
            return "BOOT_TIME";
        case CollectionEvent::PERIODIC:
            return "PERIODIC";
        case CollectionEvent::CUSTOM:
            return "CUSTOM";
        case CollectionEvent::TERMINATED:
            return "TERMINATED";
        default:
            return "INVALID";
    }
}

// IoPerfCollection implements the I/O performance data collection module of the CarWatchDog
// service. It exposes APIs that the CarWatchDog main thread and binder service can call to start
// a collection, update the collection type, and generate collection dumps.
class IoPerfCollection : public MessageHandler {
public:
    IoPerfCollection() :
          mHandlerLooper(new LooperWrapper()),
          mBoottimeCollection({}),
          mPeriodicCollection({}),
          mCustomCollection({}),
          mCurrCollectionEvent(CollectionEvent::INIT),
          mUidToPackageNameMapping({}),
          mUidIoStats(new UidIoStats()),
          mProcStat(new ProcStat()),
          mProcPidStat(new ProcPidStat()),
          mLastMajorFaults(0) {}

    ~IoPerfCollection() { terminate(); }

    // Starts the boot-time collection in the looper handler on a collection thread and returns
    // immediately. Must be called only once. Otherwise, returns an error.
    android::base::Result<void> start();

    // Terminates the collection thread and returns.
    void terminate();

    // Ends the boot-time collection, caches boot-time perf records, sends message to the looper to
    // begin the periodic collection, and returns immediately.
    virtual android::base::Result<void> onBootFinished();

    // Depending the arguments, it either:
    // 1. Starts custom collection.
    // 2. Ends custom collection and dumps the collected data.
    // Returns any error observed during the dump generation.
    virtual android::base::Result<void> onCustomCollection(int fd, const Vector<String16>& args);

    // Generates a dump from the boot-time and periodic collection events.
    virtual android::base::Result<void> onDump(int fd);

    // Dumps the help text.
    bool dumpHelpText(int fd);

private:
    // Dumps the collectors' status when they are disabled.
    android::base::Result<void> dumpCollectorsStatusLocked(int fd);

    // Starts a custom collection on the looper handler, temporarily stops the periodic collection
    // (won't discard the collected data), and returns immediately. Returns any error observed
    // during this process. The custom collection happens once every |interval| seconds. When the
    // |maxDuration| is reached, the looper receives a message to end the collection, discards the
    // collected data, and starts the periodic collection. This is needed to ensure the custom
    // collection doesn't run forever when a subsequent |endCustomCollection| call is not received.
    // When |kFilterPackagesFlag| value is provided, the results are filtered only to the specified
    // package names.
    android::base::Result<void> startCustomCollection(
            std::chrono::nanoseconds interval, std::chrono::nanoseconds maxDuration,
            const std::unordered_set<std::string>& filterPackages);

    // Ends the current custom collection, generates a dump, sends message to looper to start the
    // periodic collection, and returns immediately. Returns an error when there is no custom
    // collection running or when a dump couldn't be generated from the custom collection.
    android::base::Result<void> endCustomCollection(int fd);

    // Handles the messages received by the lopper.
    void handleMessage(const Message& message) override;

    // Processes the events received by |handleMessage|.
    android::base::Result<void> processCollectionEvent(CollectionEvent event, CollectionInfo* info);

    // Collects/stores the performance data for the current collection event.
    android::base::Result<void> collectLocked(CollectionInfo* collectionInfo);

    // Collects performance data from the `/proc/uid_io/stats` file.
    android::base::Result<void> collectUidIoPerfDataLocked(const CollectionInfo& collectionInfo,
                                                           UidIoPerfData* uidIoPerfData);

    // Collects performance data from the `/proc/stats` file.
    android::base::Result<void> collectSystemIoPerfDataLocked(SystemIoPerfData* systemIoPerfData);

    // Collects performance data from the `/proc/[pid]/stat` and
    // `/proc/[pid]/task/[tid]/stat` files.
    android::base::Result<void> collectProcessIoPerfDataLocked(
            const CollectionInfo& collectionInfo, ProcessIoPerfData* processIoPerfData);

    // Updates the |mUidToPackageNameMapping| for the given |uids|.
    android::base::Result<void> updateUidToPackageNameMapping(
            const std::unordered_set<uint32_t>& uids);

    // Retrieves package manager from the default service manager.
    android::base::Result<void> retrievePackageManager();

    // Top N per-UID stats per category.
    int mTopNStatsPerCategory;

    // Top N per-process stats per subcategory.
    int mTopNStatsPerSubcategory;

    // Thread on which the actual collection happens.
    std::thread mCollectionThread;

    // Makes sure only one collection is running at any given time.
    Mutex mMutex;

    // Handler lopper to execute different collection events on the collection thread.
    android::sp<LooperWrapper> mHandlerLooper GUARDED_BY(mMutex);

    // Info for the |CollectionEvent::BOOT_TIME| collection event. The cache is persisted until
    // system shutdown/reboot.
    CollectionInfo mBoottimeCollection GUARDED_BY(mMutex);

    // Info for the |CollectionEvent::PERIODIC| collection event. The cache size is limited by
    // |ro.carwatchdog.periodic_collection_buffer_size|.
    CollectionInfo mPeriodicCollection GUARDED_BY(mMutex);

    // Info for the |CollectionEvent::CUSTOM| collection event. The info is cleared at the end of
    // every custom collection.
    CollectionInfo mCustomCollection GUARDED_BY(mMutex);

    // Tracks the current collection event. Updated on |start|, |onBootComplete|,
    // |startCustomCollection| and |endCustomCollection|.
    CollectionEvent mCurrCollectionEvent GUARDED_BY(mMutex);

    // Cache of uid to package name mapping.
    std::unordered_map<uint64_t, std::string> mUidToPackageNameMapping GUARDED_BY(mMutex);

    // Collector/parser for `/proc/uid_io/stats`.
    android::sp<UidIoStats> mUidIoStats GUARDED_BY(mMutex);

    // Collector/parser for `/proc/stat`.
    android::sp<ProcStat> mProcStat GUARDED_BY(mMutex);

    // Collector/parser for `/proc/PID/*` stat files.
    android::sp<ProcPidStat> mProcPidStat GUARDED_BY(mMutex);

    // Major faults delta from last collection. Useful when calculating the percentage change in
    // major faults since last collection.
    uint64_t mLastMajorFaults GUARDED_BY(mMutex);

    // To get the package names from app uids.
    android::sp<android::content::pm::IPackageManagerNative> mPackageManager GUARDED_BY(mMutex);

    FRIEND_TEST(IoPerfCollectionTest, TestCollectionStartAndTerminate);
    FRIEND_TEST(IoPerfCollectionTest, TestValidCollectionSequence);
    FRIEND_TEST(IoPerfCollectionTest, TestCollectionTerminatesOnZeroEnabledCollectors);
    FRIEND_TEST(IoPerfCollectionTest, TestCollectionTerminatesOnError);
    FRIEND_TEST(IoPerfCollectionTest, TestCustomCollectionTerminatesAfterMaxDuration);
    FRIEND_TEST(IoPerfCollectionTest, TestValidUidIoStatFile);
    FRIEND_TEST(IoPerfCollectionTest, TestUidIOStatsLessThanTopNStatsLimit);
    FRIEND_TEST(IoPerfCollectionTest, TestProcUidIoStatsContentsFromDevice);
    FRIEND_TEST(IoPerfCollectionTest, TestValidProcStatFile);
    FRIEND_TEST(IoPerfCollectionTest, TestValidProcPidContents);
    FRIEND_TEST(IoPerfCollectionTest, TestProcPidContentsLessThanTopNStatsLimit);
    FRIEND_TEST(IoPerfCollectionTest, TestCustomCollectionFiltersPackageNames);
};

}  // namespace watchdog
}  // namespace automotive
}  // namespace android

#endif  //  WATCHDOG_SERVER_SRC_IOPERFCOLLECTION_H_
