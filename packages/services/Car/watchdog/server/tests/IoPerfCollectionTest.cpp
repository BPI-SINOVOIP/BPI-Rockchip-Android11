/*
 * Copyright 2020 The Android Open Source Project
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

#include "IoPerfCollection.h"

#include <WatchdogProperties.sysprop.h>
#include <android-base/file.h>
#include <cutils/android_filesystem_config.h>

#include <algorithm>
#include <future>
#include <queue>
#include <string>
#include <vector>

#include "LooperStub.h"
#include "ProcPidDir.h"
#include "ProcPidStat.h"
#include "ProcStat.h"
#include "UidIoStats.h"
#include "gmock/gmock.h"

namespace android {
namespace automotive {
namespace watchdog {

using android::base::Error;
using android::base::Result;
using android::base::WriteStringToFile;
using testing::LooperStub;
using testing::populateProcPidDir;

namespace {

const std::chrono::seconds kTestBootInterval = 1s;
const std::chrono::seconds kTestPeriodicInterval = 2s;
const std::chrono::seconds kTestCustomInterval = 3s;
const std::chrono::seconds kTestCustomCollectionDuration = 11s;

class UidIoStatsStub : public UidIoStats {
public:
    explicit UidIoStatsStub(bool enabled = false) : mEnabled(enabled) {}
    Result<std::unordered_map<uint32_t, UidIoUsage>> collect() override {
        if (mCache.empty()) {
            return Error() << "Cache is empty";
        }
        const auto entry = mCache.front();
        mCache.pop();
        return entry;
    }
    bool enabled() override { return mEnabled; }
    std::string filePath() override { return kUidIoStatsPath; }
    void push(const std::unordered_map<uint32_t, UidIoUsage>& entry) { mCache.push(entry); }

private:
    bool mEnabled;
    std::queue<std::unordered_map<uint32_t, UidIoUsage>> mCache;
};

class ProcStatStub : public ProcStat {
public:
    explicit ProcStatStub(bool enabled = false) : mEnabled(enabled) {}
    Result<ProcStatInfo> collect() override {
        if (mCache.empty()) {
            return Error() << "Cache is empty";
        }
        const auto entry = mCache.front();
        mCache.pop();
        return entry;
    }
    bool enabled() override { return mEnabled; }
    std::string filePath() override { return kProcStatPath; }
    void push(const ProcStatInfo& entry) { mCache.push(entry); }

private:
    bool mEnabled;
    std::queue<ProcStatInfo> mCache;
};

class ProcPidStatStub : public ProcPidStat {
public:
    explicit ProcPidStatStub(bool enabled = false) : mEnabled(enabled) {}
    Result<std::vector<ProcessStats>> collect() override {
        if (mCache.empty()) {
            return Error() << "Cache is empty";
        }
        const auto entry = mCache.front();
        mCache.pop();
        return entry;
    }
    bool enabled() override { return mEnabled; }
    std::string dirPath() override { return kProcDirPath; }
    void push(const std::vector<ProcessStats>& entry) { mCache.push(entry); }

private:
    bool mEnabled;
    std::queue<std::vector<ProcessStats>> mCache;
};

bool isEqual(const UidIoPerfData& lhs, const UidIoPerfData& rhs) {
    if (lhs.topNReads.size() != rhs.topNReads.size() ||
        lhs.topNWrites.size() != rhs.topNWrites.size()) {
        return false;
    }
    for (int i = 0; i < METRIC_TYPES; ++i) {
        for (int j = 0; j < UID_STATES; ++j) {
            if (lhs.total[i][j] != rhs.total[i][j]) {
                return false;
            }
        }
    }
    auto comp = [&](const UidIoPerfData::Stats& l, const UidIoPerfData::Stats& r) -> bool {
        bool isEqual = l.userId == r.userId && l.packageName == r.packageName;
        for (int i = 0; i < UID_STATES; ++i) {
            isEqual &= l.bytes[i] == r.bytes[i] && l.fsync[i] == r.fsync[i];
        }
        return isEqual;
    };
    return lhs.topNReads.size() == rhs.topNReads.size() &&
            std::equal(lhs.topNReads.begin(), lhs.topNReads.end(), rhs.topNReads.begin(), comp) &&
            lhs.topNWrites.size() == rhs.topNWrites.size() &&
            std::equal(lhs.topNWrites.begin(), lhs.topNWrites.end(), rhs.topNWrites.begin(), comp);
}

bool isEqual(const SystemIoPerfData& lhs, const SystemIoPerfData& rhs) {
    return lhs.cpuIoWaitTime == rhs.cpuIoWaitTime && lhs.totalCpuTime == rhs.totalCpuTime &&
            lhs.ioBlockedProcessesCnt == rhs.ioBlockedProcessesCnt &&
            lhs.totalProcessesCnt == rhs.totalProcessesCnt;
}

bool isEqual(const ProcessIoPerfData& lhs, const ProcessIoPerfData& rhs) {
    if (lhs.topNIoBlockedUids.size() != rhs.topNIoBlockedUids.size() ||
        lhs.topNMajorFaultUids.size() != rhs.topNMajorFaultUids.size() ||
        lhs.totalMajorFaults != rhs.totalMajorFaults ||
        lhs.majorFaultsPercentChange != rhs.majorFaultsPercentChange) {
        return false;
    }
    auto comp = [&](const ProcessIoPerfData::UidStats& l,
                    const ProcessIoPerfData::UidStats& r) -> bool {
        auto comp = [&](const ProcessIoPerfData::UidStats::ProcessStats& l,
                        const ProcessIoPerfData::UidStats::ProcessStats& r) -> bool {
            return l.comm == r.comm && l.count == r.count;
        };
        return l.userId == r.userId && l.packageName == r.packageName && l.count == r.count &&
                l.topNProcesses.size() == r.topNProcesses.size() &&
                std::equal(l.topNProcesses.begin(), l.topNProcesses.end(), r.topNProcesses.begin(),
                           comp);
    };
    return lhs.topNIoBlockedUids.size() == lhs.topNIoBlockedUids.size() &&
            std::equal(lhs.topNIoBlockedUids.begin(), lhs.topNIoBlockedUids.end(),
                       rhs.topNIoBlockedUids.begin(), comp) &&
            lhs.topNIoBlockedUidsTotalTaskCnt.size() == rhs.topNIoBlockedUidsTotalTaskCnt.size() &&
            std::equal(lhs.topNIoBlockedUidsTotalTaskCnt.begin(),
                       lhs.topNIoBlockedUidsTotalTaskCnt.end(),
                       rhs.topNIoBlockedUidsTotalTaskCnt.begin()) &&
            lhs.topNMajorFaultUids.size() == rhs.topNMajorFaultUids.size() &&
            std::equal(lhs.topNMajorFaultUids.begin(), lhs.topNMajorFaultUids.end(),
                       rhs.topNMajorFaultUids.begin(), comp);
}

bool isEqual(const IoPerfRecord& lhs, const IoPerfRecord& rhs) {
    return isEqual(lhs.uidIoPerfData, rhs.uidIoPerfData) &&
            isEqual(lhs.systemIoPerfData, rhs.systemIoPerfData) &&
            isEqual(lhs.processIoPerfData, rhs.processIoPerfData);
}

}  // namespace

TEST(IoPerfCollectionTest, TestCollectionStartAndTerminate) {
    sp<IoPerfCollection> collector = new IoPerfCollection();
    const auto& ret = collector->start();
    ASSERT_TRUE(ret) << ret.error().message();
    ASSERT_TRUE(collector->mCollectionThread.joinable()) << "Collection thread not created";
    ASSERT_FALSE(collector->start())
            << "No error returned when collector was started more than once";
    ASSERT_TRUE(sysprop::topNStatsPerCategory().has_value());
    ASSERT_EQ(collector->mTopNStatsPerCategory, sysprop::topNStatsPerCategory().value());

    ASSERT_TRUE(sysprop::topNStatsPerSubcategory().has_value());
    ASSERT_EQ(collector->mTopNStatsPerSubcategory, sysprop::topNStatsPerSubcategory().value());

    ASSERT_TRUE(sysprop::boottimeCollectionInterval().has_value());
    ASSERT_EQ(std::chrono::duration_cast<std::chrono::seconds>(
                      collector->mBoottimeCollection.interval)
                      .count(),
              sysprop::boottimeCollectionInterval().value());

    ASSERT_TRUE(sysprop::topNStatsPerCategory().has_value());
    ASSERT_EQ(std::chrono::duration_cast<std::chrono::seconds>(
                      collector->mPeriodicCollection.interval)
                      .count(),
              sysprop::periodicCollectionInterval().value());

    ASSERT_TRUE(sysprop::periodicCollectionBufferSize().has_value());
    ASSERT_EQ(collector->mPeriodicCollection.maxCacheSize,
              sysprop::periodicCollectionBufferSize().value());

    collector->terminate();
    ASSERT_FALSE(collector->mCollectionThread.joinable()) << "Collection thread did not terminate";
}

TEST(IoPerfCollectionTest, TestValidCollectionSequence) {
    sp<UidIoStatsStub> uidIoStatsStub = new UidIoStatsStub(true);
    sp<ProcStatStub> procStatStub = new ProcStatStub(true);
    sp<ProcPidStatStub> procPidStatStub = new ProcPidStatStub(true);
    sp<LooperStub> looperStub = new LooperStub();

    sp<IoPerfCollection> collector = new IoPerfCollection();
    collector->mUidIoStats = uidIoStatsStub;
    collector->mProcStat = procStatStub;
    collector->mProcPidStat = procPidStatStub;
    collector->mHandlerLooper = looperStub;

    auto ret = collector->start();
    ASSERT_TRUE(ret) << ret.error().message();

    collector->mBoottimeCollection.interval = kTestBootInterval;
    collector->mPeriodicCollection.interval = kTestPeriodicInterval;
    collector->mPeriodicCollection.maxCacheSize = 1;

    // #1 Boot-time collection
    uidIoStatsStub->push({{1009, {.uid = 1009, .ios = {0, 20000, 0, 30000, 0, 300}}}});
    procStatStub->push(ProcStatInfo{
            /*stats=*/{6200, 5700, 1700, 3100, /*ioWaitTime=*/1100, 5200, 3900, 0, 0, 0},
            /*runnableCnt=*/17,
            /*ioBlockedCnt=*/5,
    });
    procPidStatStub->push({{.tgid = 100,
                            .uid = 1009,
                            .process = {.pid = 100,
                                        .comm = "disk I/O",
                                        .state = "D",
                                        .ppid = 1,
                                        .majorFaults = 5000,
                                        .numThreads = 1,
                                        .startTime = 234},
                            .threads = {{100,
                                         {.pid = 100,
                                          .comm = "disk I/O",
                                          .state = "D",
                                          .ppid = 1,
                                          .majorFaults = 5000,
                                          .numThreads = 1,
                                          .startTime = 234}}}}});
    IoPerfRecord bootExpectedFirst = {
            .uidIoPerfData = {.topNReads = {{.userId = 0,
                                             .packageName = "mount",
                                             .bytes = {0, 20000},
                                             .fsync{0, 300}}},
                              .topNWrites = {{.userId = 0,
                                              .packageName = "mount",
                                              .bytes = {0, 30000},
                                              .fsync{0, 300}}},
                              .total = {{0, 20000}, {0, 30000}, {0, 300}}},
            .systemIoPerfData = {.cpuIoWaitTime = 1100,
                                 .totalCpuTime = 26900,
                                 .ioBlockedProcessesCnt = 5,
                                 .totalProcessesCnt = 22},
            .processIoPerfData = {.topNIoBlockedUids = {{0, "mount", 1, {{"disk I/O", 1}}}},
                                  .topNIoBlockedUidsTotalTaskCnt = {1},
                                  .topNMajorFaultUids = {{0, "mount", 5000, {{"disk I/O", 5000}}}},
                                  .totalMajorFaults = 5000,
                                  .majorFaultsPercentChange = 0.0},
    };
    ret = looperStub->pollCache();
    ASSERT_TRUE(ret) << ret.error().message();
    ASSERT_EQ(looperStub->numSecondsElapsed(), 0)
            << "Boot-time collection didn't start immediately";

    // #2 Boot-time collection
    uidIoStatsStub->push({
            {1009, {.uid = 1009, .ios = {0, 2000, 0, 3000, 0, 100}}},
    });
    procStatStub->push(ProcStatInfo{
            /*stats=*/{1200, 1700, 2700, 7800, /*ioWaitTime=*/5500, 500, 300, 0, 0, 100},
            /*runnableCnt=*/8,
            /*ioBlockedCnt=*/6,
    });
    procPidStatStub->push({{.tgid = 100,
                            .uid = 1009,
                            .process = {.pid = 100,
                                        .comm = "disk I/O",
                                        .state = "D",
                                        .ppid = 1,
                                        .majorFaults = 11000,
                                        .numThreads = 1,
                                        .startTime = 234},
                            .threads = {{100,
                                         {.pid = 100,
                                          .comm = "disk I/O",
                                          .state = "D",
                                          .ppid = 1,
                                          .majorFaults = 10000,
                                          .numThreads = 1,
                                          .startTime = 234}},
                                        {200,
                                         {.pid = 200,
                                          .comm = "disk I/O",
                                          .state = "D",
                                          .ppid = 1,
                                          .majorFaults = 1000,
                                          .numThreads = 1,
                                          .startTime = 1234}}}}});
    IoPerfRecord bootExpectedSecond = {
            .uidIoPerfData = {.topNReads = {{.userId = 0,
                                             .packageName = "mount",
                                             .bytes = {0, 2000},
                                             .fsync{0, 100}}},
                              .topNWrites = {{.userId = 0,
                                              .packageName = "mount",
                                              .bytes = {0, 3000},
                                              .fsync{0, 100}}},
                              .total = {{0, 2000}, {0, 3000}, {0, 100}}},
            .systemIoPerfData = {.cpuIoWaitTime = 5500,
                                 .totalCpuTime = 19800,
                                 .ioBlockedProcessesCnt = 6,
                                 .totalProcessesCnt = 14},
            .processIoPerfData =
                    {.topNIoBlockedUids = {{0, "mount", 2, {{"disk I/O", 2}}}},
                     .topNIoBlockedUidsTotalTaskCnt = {2},
                     .topNMajorFaultUids = {{0, "mount", 11000, {{"disk I/O", 11000}}}},
                     .totalMajorFaults = 11000,
                     .majorFaultsPercentChange = ((11000.0 - 5000.0) / 5000.0) * 100},
    };
    ret = looperStub->pollCache();
    ASSERT_TRUE(ret) << ret.error().message();
    ASSERT_EQ(looperStub->numSecondsElapsed(), kTestBootInterval.count())
            << "Subsequent boot-time collection didn't happen at " << kTestBootInterval.count()
            << " seconds interval";

    // #3 Last boot-time collection
    ret = collector->onBootFinished();
    ASSERT_TRUE(ret) << ret.error().message();
    uidIoStatsStub->push({
            {1009, {.uid = 1009, .ios = {0, 7000, 0, 8000, 0, 50}}},
    });
    procStatStub->push(ProcStatInfo{
            /*stats=*/{1400, 1900, 2900, 8000, /*ioWaitTime=*/5700, 700, 500, 0, 0, 300},
            /*runnableCnt=*/10,
            /*ioBlockedCnt=*/8,
    });
    procPidStatStub->push({{.tgid = 100,
                            .uid = 1009,
                            .process = {.pid = 100,
                                        .comm = "disk I/O",
                                        .state = "D",
                                        .ppid = 1,
                                        .majorFaults = 5000,
                                        .numThreads = 1,
                                        .startTime = 234},
                            .threads = {{100,
                                         {.pid = 100,
                                          .comm = "disk I/O",
                                          .state = "D",
                                          .ppid = 1,
                                          .majorFaults = 3000,
                                          .numThreads = 1,
                                          .startTime = 234}},
                                        {200,
                                         {.pid = 200,
                                          .comm = "disk I/O",
                                          .state = "D",
                                          .ppid = 1,
                                          .majorFaults = 2000,
                                          .numThreads = 1,
                                          .startTime = 1234}}}}});
    IoPerfRecord bootExpectedThird = {
            .uidIoPerfData = {.topNReads = {{.userId = 0,
                                             .packageName = "mount",
                                             .bytes = {0, 7000},
                                             .fsync{0, 50}}},
                              .topNWrites = {{.userId = 0,
                                              .packageName = "mount",
                                              .bytes = {0, 8000},
                                              .fsync{0, 50}}},
                              .total = {{0, 7000}, {0, 8000}, {0, 50}}},
            .systemIoPerfData = {.cpuIoWaitTime = 5700,
                                 .totalCpuTime = 21400,
                                 .ioBlockedProcessesCnt = 8,
                                 .totalProcessesCnt = 18},
            .processIoPerfData = {.topNIoBlockedUids = {{0, "mount", 2, {{"disk I/O", 2}}}},
                                  .topNIoBlockedUidsTotalTaskCnt = {2},
                                  .topNMajorFaultUids = {{0, "mount", 5000, {{"disk I/O", 5000}}}},
                                  .totalMajorFaults = 5000,
                                  .majorFaultsPercentChange = ((5000.0 - 11000.0) / 11000.0) * 100},
    };
    ret = looperStub->pollCache();
    ASSERT_TRUE(ret) << ret.error().message();
    ASSERT_EQ(looperStub->numSecondsElapsed(), 0)
            << "Last boot-time collection didn't happen immediately after receiving boot complete "
            << "notification";

    ASSERT_EQ(collector->mBoottimeCollection.records.size(), 3);
    ASSERT_TRUE(isEqual(collector->mBoottimeCollection.records[0], bootExpectedFirst))
            << "Boot-time collection record 1 doesn't match.\nExpected:\n"
            << toString(bootExpectedFirst) << "\nActual:\n"
            << toString(collector->mBoottimeCollection.records[0]);
    ASSERT_TRUE(isEqual(collector->mBoottimeCollection.records[1], bootExpectedSecond))
            << "Boot-time collection record 2 doesn't match.\nExpected:\n"
            << toString(bootExpectedSecond) << "\nActual:\n"
            << toString(collector->mBoottimeCollection.records[1]);
    ASSERT_TRUE(isEqual(collector->mBoottimeCollection.records[2], bootExpectedThird))
            << "Boot-time collection record 3 doesn't match.\nExpected:\n"
            << toString(bootExpectedSecond) << "\nActual:\n"
            << toString(collector->mBoottimeCollection.records[2]);

    // #4 Periodic collection
    uidIoStatsStub->push({
            {1009, {.uid = 1009, .ios = {0, 4000, 0, 6000, 0, 100}}},
    });
    procStatStub->push(ProcStatInfo{
            /*stats=*/{200, 700, 400, 800, /*ioWaitTime=*/500, 666, 780, 0, 0, 230},
            /*runnableCnt=*/12,
            /*ioBlockedCnt=*/3,
    });
    procPidStatStub->push({{.tgid = 100,
                            .uid = 1009,
                            .process = {.pid = 100,
                                        .comm = "disk I/O",
                                        .state = "D",
                                        .ppid = 1,
                                        .majorFaults = 4100,
                                        .numThreads = 1,
                                        .startTime = 234},
                            .threads = {{100,
                                         {.pid = 100,
                                          .comm = "disk I/O",
                                          .state = "D",
                                          .ppid = 1,
                                          .majorFaults = 100,
                                          .numThreads = 1,
                                          .startTime = 234}},
                                        {1200,
                                         {.pid = 1200,
                                          .comm = "disk I/O",
                                          .state = "S",
                                          .ppid = 1,
                                          .majorFaults = 4000,
                                          .numThreads = 1,
                                          .startTime = 567890}}}}});
    IoPerfRecord periodicExpectedFirst = {
            .uidIoPerfData = {.topNReads = {{.userId = 0,
                                             .packageName = "mount",
                                             .bytes = {0, 4000},
                                             .fsync{0, 100}}},
                              .topNWrites = {{.userId = 0,
                                              .packageName = "mount",
                                              .bytes = {0, 6000},
                                              .fsync{0, 100}}},
                              .total = {{0, 4000}, {0, 6000}, {0, 100}}},
            .systemIoPerfData = {.cpuIoWaitTime = 500,
                                 .totalCpuTime = 4276,
                                 .ioBlockedProcessesCnt = 3,
                                 .totalProcessesCnt = 15},
            .processIoPerfData = {.topNIoBlockedUids = {{0, "mount", 1, {{"disk I/O", 1}}}},
                                  .topNIoBlockedUidsTotalTaskCnt = {2},
                                  .topNMajorFaultUids = {{0, "mount", 4100, {{"disk I/O", 4100}}}},
                                  .totalMajorFaults = 4100,
                                  .majorFaultsPercentChange = ((4100.0 - 5000.0) / 5000.0) * 100},
    };
    ret = looperStub->pollCache();
    ASSERT_TRUE(ret) << ret.error().message();
    ASSERT_EQ(looperStub->numSecondsElapsed(), kTestPeriodicInterval.count())
            << "First periodic collection didn't happen at " << kTestPeriodicInterval.count()
            << " seconds interval";

    // #5 Periodic collection
    uidIoStatsStub->push({
            {1009, {.uid = 1009, .ios = {0, 3000, 0, 5000, 0, 800}}},
    });
    procStatStub->push(ProcStatInfo{
            /*stats=*/{2300, 7300, 4300, 8300, /*ioWaitTime=*/5300, 6366, 7380, 0, 0, 2330},
            /*runnableCnt=*/2,
            /*ioBlockedCnt=*/4,
    });
    procPidStatStub->push({{.tgid = 100,
                            .uid = 1009,
                            .process = {.pid = 100,
                                        .comm = "disk I/O",
                                        .state = "D",
                                        .ppid = 1,
                                        .majorFaults = 44300,
                                        .numThreads = 1,
                                        .startTime = 234},
                            .threads = {{100,
                                         {.pid = 100,
                                          .comm = "disk I/O",
                                          .state = "D",
                                          .ppid = 1,
                                          .majorFaults = 1300,
                                          .numThreads = 1,
                                          .startTime = 234}},
                                        {1200,
                                         {.pid = 1200,
                                          .comm = "disk I/O",
                                          .state = "D",
                                          .ppid = 1,
                                          .majorFaults = 43000,
                                          .numThreads = 1,
                                          .startTime = 567890}}}}});
    IoPerfRecord periodicExpectedSecond = {
            .uidIoPerfData = {.topNReads = {{.userId = 0,
                                             .packageName = "mount",
                                             .bytes = {0, 3000},
                                             .fsync{0, 800}}},
                              .topNWrites = {{.userId = 0,
                                              .packageName = "mount",
                                              .bytes = {0, 5000},
                                              .fsync{0, 800}}},
                              .total = {{0, 3000}, {0, 5000}, {0, 800}}},
            .systemIoPerfData = {.cpuIoWaitTime = 5300,
                                 .totalCpuTime = 43576,
                                 .ioBlockedProcessesCnt = 4,
                                 .totalProcessesCnt = 6},
            .processIoPerfData =
                    {.topNIoBlockedUids = {{0, "mount", 2, {{"disk I/O", 2}}}},
                     .topNIoBlockedUidsTotalTaskCnt = {2},
                     .topNMajorFaultUids = {{0, "mount", 44300, {{"disk I/O", 44300}}}},
                     .totalMajorFaults = 44300,
                     .majorFaultsPercentChange = ((44300.0 - 4100.0) / 4100.0) * 100},
    };
    ret = looperStub->pollCache();
    ASSERT_TRUE(ret) << ret.error().message();
    ASSERT_EQ(looperStub->numSecondsElapsed(), kTestPeriodicInterval.count())
            << "Subsequent periodic collection didn't happen at " << kTestPeriodicInterval.count()
            << " seconds interval";

    ASSERT_EQ(collector->mPeriodicCollection.records.size(), 2);
    ASSERT_TRUE(isEqual(collector->mPeriodicCollection.records[0], periodicExpectedFirst))
            << "Periodic collection snapshot 1, record 1 doesn't match.\nExpected:\n"
            << toString(periodicExpectedFirst) << "\nActual:\n"
            << toString(collector->mPeriodicCollection.records[0]);
    ASSERT_TRUE(isEqual(collector->mPeriodicCollection.records[1], periodicExpectedSecond))
            << "Periodic collection snapshot 1, record 2 doesn't match.\nExpected:\n"
            << toString(periodicExpectedSecond) << "\nActual:\n"
            << toString(collector->mPeriodicCollection.records[1]);

    // #6 Custom collection
    Vector<String16> args;
    args.push_back(String16(kStartCustomCollectionFlag));
    args.push_back(String16(kIntervalFlag));
    args.push_back(String16(std::to_string(kTestCustomInterval.count()).c_str()));
    args.push_back(String16(kMaxDurationFlag));
    args.push_back(String16(std::to_string(kTestCustomCollectionDuration.count()).c_str()));

    ret = collector->onCustomCollection(-1, args);
    ASSERT_TRUE(ret.ok()) << ret.error().message();
    uidIoStatsStub->push({
            {1009, {.uid = 1009, .ios = {0, 13000, 0, 15000, 0, 100}}},
    });
    procStatStub->push(ProcStatInfo{
            /*stats=*/{2800, 7800, 4800, 8800, /*ioWaitTime=*/5800, 6866, 7880, 0, 0, 2830},
            /*runnableCnt=*/200,
            /*ioBlockedCnt=*/13,
    });
    procPidStatStub->push({{.tgid = 100,
                            .uid = 1009,
                            .process = {.pid = 100,
                                        .comm = "disk I/O",
                                        .state = "D",
                                        .ppid = 1,
                                        .majorFaults = 49800,
                                        .numThreads = 1,
                                        .startTime = 234},
                            .threads = {{100,
                                         {.pid = 100,
                                          .comm = "disk I/O",
                                          .state = "D",
                                          .ppid = 1,
                                          .majorFaults = 1800,
                                          .numThreads = 1,
                                          .startTime = 234}},
                                        {1200,
                                         {.pid = 1200,
                                          .comm = "disk I/O",
                                          .state = "D",
                                          .ppid = 1,
                                          .majorFaults = 48000,
                                          .numThreads = 1,
                                          .startTime = 567890}}}}});
    IoPerfRecord customExpectedFirst = {
            .uidIoPerfData = {.topNReads = {{.userId = 0,
                                             .packageName = "mount",
                                             .bytes = {0, 13000},
                                             .fsync{0, 100}}},
                              .topNWrites = {{.userId = 0,
                                              .packageName = "mount",
                                              .bytes = {0, 15000},
                                              .fsync{0, 100}}},
                              .total = {{0, 13000}, {0, 15000}, {0, 100}}},
            .systemIoPerfData = {.cpuIoWaitTime = 5800,
                                 .totalCpuTime = 47576,
                                 .ioBlockedProcessesCnt = 13,
                                 .totalProcessesCnt = 213},
            .processIoPerfData =
                    {.topNIoBlockedUids = {{0, "mount", 2, {{"disk I/O", 2}}}},
                     .topNIoBlockedUidsTotalTaskCnt = {2},
                     .topNMajorFaultUids = {{0, "mount", 49800, {{"disk I/O", 49800}}}},
                     .totalMajorFaults = 49800,
                     .majorFaultsPercentChange = ((49800.0 - 44300.0) / 44300.0) * 100},
    };
    ret = looperStub->pollCache();
    ASSERT_TRUE(ret) << ret.error().message();
    ASSERT_EQ(looperStub->numSecondsElapsed(), 0) << "Custom collection didn't start immediately";

    // #7 Custom collection
    uidIoStatsStub->push({
            {1009, {.uid = 1009, .ios = {0, 14000, 0, 16000, 0, 100}}},
    });
    procStatStub->push(ProcStatInfo{
            /*stats=*/{2900, 7900, 4900, 8900, /*ioWaitTime=*/5900, 6966, 7980, 0, 0, 2930},
            /*runnableCnt=*/100,
            /*ioBlockedCnt=*/57,
    });
    procPidStatStub->push({{.tgid = 100,
                            .uid = 1009,
                            .process = {.pid = 100,
                                        .comm = "disk I/O",
                                        .state = "D",
                                        .ppid = 1,
                                        .majorFaults = 50900,
                                        .numThreads = 1,
                                        .startTime = 234},
                            .threads = {{100,
                                         {.pid = 100,
                                          .comm = "disk I/O",
                                          .state = "D",
                                          .ppid = 1,
                                          .majorFaults = 1900,
                                          .numThreads = 1,
                                          .startTime = 234}},
                                        {1200,
                                         {.pid = 1200,
                                          .comm = "disk I/O",
                                          .state = "D",
                                          .ppid = 1,
                                          .majorFaults = 49000,
                                          .numThreads = 1,
                                          .startTime = 567890}}}}});
    IoPerfRecord customExpectedSecond = {
            .uidIoPerfData = {.topNReads = {{.userId = 0,
                                             .packageName = "mount",
                                             .bytes = {0, 14000},
                                             .fsync{0, 100}}},
                              .topNWrites = {{.userId = 0,
                                              .packageName = "mount",
                                              .bytes = {0, 16000},
                                              .fsync{0, 100}}},
                              .total = {{0, 14000}, {0, 16000}, {0, 100}}},
            .systemIoPerfData = {.cpuIoWaitTime = 5900,
                                 .totalCpuTime = 48376,
                                 .ioBlockedProcessesCnt = 57,
                                 .totalProcessesCnt = 157},
            .processIoPerfData =
                    {.topNIoBlockedUids = {{0, "mount", 2, {{"disk I/O", 2}}}},
                     .topNIoBlockedUidsTotalTaskCnt = {2},
                     .topNMajorFaultUids = {{0, "mount", 50900, {{"disk I/O", 50900}}}},
                     .totalMajorFaults = 50900,
                     .majorFaultsPercentChange = ((50900.0 - 49800.0) / 49800.0) * 100},
    };
    ret = looperStub->pollCache();
    ASSERT_TRUE(ret) << ret.error().message();
    ASSERT_EQ(looperStub->numSecondsElapsed(), kTestCustomInterval.count())
            << "Subsequent custom collection didn't happen at " << kTestCustomInterval.count()
            << " seconds interval";

    ASSERT_EQ(collector->mCustomCollection.records.size(), 2);
    ASSERT_TRUE(isEqual(collector->mCustomCollection.records[0], customExpectedFirst))
            << "Custom collection record 1 doesn't match.\nExpected:\n"
            << toString(customExpectedFirst) << "\nActual:\n"
            << toString(collector->mCustomCollection.records[0]);
    ASSERT_TRUE(isEqual(collector->mCustomCollection.records[1], customExpectedSecond))
            << "Custom collection record 2 doesn't match.\nExpected:\n"
            << toString(customExpectedSecond) << "\nActual:\n"
            << toString(collector->mCustomCollection.records[1]);

    // #8 Switch to periodic collection
    args.clear();
    args.push_back(String16(kEndCustomCollectionFlag));
    TemporaryFile customDump;
    ret = collector->onCustomCollection(customDump.fd, args);
    ASSERT_TRUE(ret.ok()) << ret.error().message();
    ret = looperStub->pollCache();
    ASSERT_TRUE(ret) << ret.error().message();

    // Custom collection cache should be emptied on ending the collection.
    ASSERT_EQ(collector->mCustomCollection.records.size(), 0);

    // #7 periodic collection
    uidIoStatsStub->push({
            {1009, {.uid = 1009, .ios = {0, 123, 0, 456, 0, 25}}},
    });
    procStatStub->push(ProcStatInfo{
            /*stats=*/{3400, 2300, 5600, 7800, /*ioWaitTime=*/1100, 166, 180, 0, 0, 130},
            /*runnableCnt=*/3,
            /*ioBlockedCnt=*/1,
    });
    procPidStatStub->push({{.tgid = 100,
                            .uid = 1009,
                            .process = {.pid = 100,
                                        .comm = "disk I/O",
                                        .state = "D",
                                        .ppid = 1,
                                        .majorFaults = 5701,
                                        .numThreads = 1,
                                        .startTime = 234},
                            .threads = {{100,
                                         {.pid = 100,
                                          .comm = "disk I/O",
                                          .state = "D",
                                          .ppid = 1,
                                          .majorFaults = 23,
                                          .numThreads = 1,
                                          .startTime = 234}},
                                        {1200,
                                         {.pid = 1200,
                                          .comm = "disk I/O",
                                          .state = "D",
                                          .ppid = 1,
                                          .majorFaults = 5678,
                                          .numThreads = 1,
                                          .startTime = 567890}}}}});
    IoPerfRecord periodicExpectedThird = {
            .uidIoPerfData = {.topNReads = {{.userId = 0,
                                             .packageName = "mount",
                                             .bytes = {0, 123},
                                             .fsync{0, 25}}},
                              .topNWrites = {{.userId = 0,
                                              .packageName = "mount",
                                              .bytes = {0, 456},
                                              .fsync{0, 25}}},
                              .total = {{0, 123}, {0, 456}, {0, 25}}},
            .systemIoPerfData = {.cpuIoWaitTime = 1100,
                                 .totalCpuTime = 20676,
                                 .ioBlockedProcessesCnt = 1,
                                 .totalProcessesCnt = 4},
            .processIoPerfData = {.topNIoBlockedUids = {{0, "mount", 2, {{"disk I/O", 2}}}},
                                  .topNIoBlockedUidsTotalTaskCnt = {2},
                                  .topNMajorFaultUids = {{0, "mount", 5701, {{"disk I/O", 5701}}}},
                                  .totalMajorFaults = 5701,
                                  .majorFaultsPercentChange = ((5701.0 - 50900.0) / 50900.0) * 100},
    };
    ret = looperStub->pollCache();
    ASSERT_TRUE(ret) << ret.error().message();
    ASSERT_EQ(looperStub->numSecondsElapsed(), 0)
            << "Periodic collection didn't start immediately after ending custom collection";

    // Maximum periodic collection buffer size is 2.
    ASSERT_EQ(collector->mPeriodicCollection.records.size(), 2);
    ASSERT_TRUE(isEqual(collector->mPeriodicCollection.records[0], periodicExpectedSecond))
            << "Periodic collection snapshot 2, record 1 doesn't match.\nExpected:\n"
            << toString(periodicExpectedSecond) << "\nActual:\n"
            << toString(collector->mPeriodicCollection.records[0]);
    ASSERT_TRUE(isEqual(collector->mPeriodicCollection.records[1], periodicExpectedThird))
            << "Periodic collection snapshot 2, record 2 doesn't match.\nExpected:\n"
            << toString(periodicExpectedThird) << "\nActual:\n"
            << toString(collector->mPeriodicCollection.records[1]);

    ASSERT_EQ(collector->mBoottimeCollection.records.size(), 3)
            << "Boot-time records not persisted until collector termination";

    TemporaryFile bugreportDump;
    ret = collector->onDump(bugreportDump.fd);
    ASSERT_TRUE(ret.ok()) << ret.error().message();

    collector->terminate();
}

TEST(IoPerfCollectionTest, TestCollectionTerminatesOnZeroEnabledCollectors) {
    sp<IoPerfCollection> collector = new IoPerfCollection();
    collector->mUidIoStats = new UidIoStatsStub();
    collector->mProcStat = new ProcStatStub();
    collector->mProcPidStat = new ProcPidStatStub();

    const auto& ret = collector->start();
    ASSERT_TRUE(ret) << ret.error().message();

    ASSERT_EQ(std::async([&]() {
                  if (collector->mCollectionThread.joinable()) {
                      collector->mCollectionThread.join();
                  }
              }).wait_for(1s),
              std::future_status::ready)
            << "Collection thread didn't terminate within 1 second.";
    ASSERT_EQ(collector->mCurrCollectionEvent, CollectionEvent::TERMINATED);

    // When the collection doesn't auto-terminate on error, the test will hang if the collector is
    // not terminated explicitly. Thus call terminate to avoid this.
    collector->terminate();
}

TEST(IoPerfCollectionTest, TestCollectionTerminatesOnError) {
    sp<IoPerfCollection> collector = new IoPerfCollection();
    collector->mUidIoStats = new UidIoStatsStub(true);
    collector->mProcStat = new ProcStatStub(true);
    collector->mProcPidStat = new ProcPidStatStub(true);

    // Stub caches are empty so polling them should trigger error.
    const auto& ret = collector->start();
    ASSERT_TRUE(ret) << ret.error().message();

    ASSERT_EQ(std::async([&]() {
                  if (collector->mCollectionThread.joinable()) {
                      collector->mCollectionThread.join();
                  }
              }).wait_for(1s),
              std::future_status::ready)
            << "Collection thread didn't terminate within 1 second.";
    ASSERT_EQ(collector->mCurrCollectionEvent, CollectionEvent::TERMINATED);

    // When the collection doesn't auto-terminate on error, the test will hang if the collector is
    // not terminated explicitly. Thus call terminate to avoid this.
    collector->terminate();
}

TEST(IoPerfCollectionTest, TestCustomCollectionFiltersPackageNames) {
    sp<UidIoStatsStub> uidIoStatsStub = new UidIoStatsStub(true);
    sp<ProcStatStub> procStatStub = new ProcStatStub(true);
    sp<ProcPidStatStub> procPidStatStub = new ProcPidStatStub(true);
    sp<LooperStub> looperStub = new LooperStub();

    sp<IoPerfCollection> collector = new IoPerfCollection();
    collector->mUidIoStats = uidIoStatsStub;
    collector->mProcStat = procStatStub;
    collector->mProcPidStat = procPidStatStub;
    collector->mHandlerLooper = looperStub;
    // Filter by package name should ignore this limit.
    collector->mTopNStatsPerCategory = 1;

    auto ret = collector->start();
    ASSERT_TRUE(ret) << ret.error().message();

    // Dummy boot-time collection
    uidIoStatsStub->push({});
    procStatStub->push(ProcStatInfo{});
    procPidStatStub->push({});
    ret = looperStub->pollCache();
    ASSERT_TRUE(ret) << ret.error().message();

    // Dummy Periodic collection
    ret = collector->onBootFinished();
    ASSERT_TRUE(ret) << ret.error().message();
    uidIoStatsStub->push({});
    procStatStub->push(ProcStatInfo{});
    procPidStatStub->push({});
    ret = looperStub->pollCache();
    ASSERT_TRUE(ret) << ret.error().message();

    // Start custom Collection
    Vector<String16> args;
    args.push_back(String16(kStartCustomCollectionFlag));
    args.push_back(String16(kIntervalFlag));
    args.push_back(String16(std::to_string(kTestCustomInterval.count()).c_str()));
    args.push_back(String16(kMaxDurationFlag));
    args.push_back(String16(std::to_string(kTestCustomCollectionDuration.count()).c_str()));
    args.push_back(String16(kFilterPackagesFlag));
    args.push_back(String16("android.car.cts,system_server"));

    ret = collector->onCustomCollection(-1, args);
    ASSERT_TRUE(ret.ok()) << ret.error().message();

    // Custom collection
    collector->mUidToPackageNameMapping[1009] = "android.car.cts";
    collector->mUidToPackageNameMapping[2001] = "system_server";
    collector->mUidToPackageNameMapping[3456] = "random_process";
    uidIoStatsStub->push({
            {1009, {.uid = 1009, .ios = {0, 14000, 0, 16000, 0, 100}}},
            {2001, {.uid = 2001, .ios = {0, 3400, 0, 6700, 0, 200}}},
            {3456, {.uid = 3456, .ios = {0, 4200, 0, 5600, 0, 300}}},
    });
    procStatStub->push(ProcStatInfo{
            /*stats=*/{2900, 7900, 4900, 8900, /*ioWaitTime=*/5900, 6966, 7980, 0, 0, 2930},
            /*runnableCnt=*/100,
            /*ioBlockedCnt=*/57,
    });
    procPidStatStub->push({{.tgid = 100,
                            .uid = 1009,
                            .process = {.pid = 100,
                                        .comm = "cts_test",
                                        .state = "D",
                                        .ppid = 1,
                                        .majorFaults = 50900,
                                        .numThreads = 2,
                                        .startTime = 234},
                            .threads = {{100,
                                         {.pid = 100,
                                          .comm = "cts_test",
                                          .state = "D",
                                          .ppid = 1,
                                          .majorFaults = 50900,
                                          .numThreads = 1,
                                          .startTime = 234}},
                                        {200,
                                         {.pid = 200,
                                          .comm = "cts_test_2",
                                          .state = "D",
                                          .ppid = 1,
                                          .majorFaults = 0,
                                          .numThreads = 1,
                                          .startTime = 290}}}},
                           {.tgid = 1000,
                            .uid = 2001,
                            .process = {.pid = 1000,
                                        .comm = "system_server",
                                        .state = "D",
                                        .ppid = 1,
                                        .majorFaults = 1234,
                                        .numThreads = 1,
                                        .startTime = 345},
                            .threads = {{1000,
                                         {.pid = 1000,
                                          .comm = "system_server",
                                          .state = "D",
                                          .ppid = 1,
                                          .majorFaults = 1234,
                                          .numThreads = 1,
                                          .startTime = 345}}}},
                           {.tgid = 4000,
                            .uid = 3456,
                            .process = {.pid = 4000,
                                        .comm = "random_process",
                                        .state = "D",
                                        .ppid = 1,
                                        .majorFaults = 3456,
                                        .numThreads = 1,
                                        .startTime = 890},
                            .threads = {{4000,
                                         {.pid = 4000,
                                          .comm = "random_process",
                                          .state = "D",
                                          .ppid = 1,
                                          .majorFaults = 50900,
                                          .numThreads = 1,
                                          .startTime = 890}}}}});
    IoPerfRecord expected = {
            .uidIoPerfData = {.topNReads = {{.userId = 0,
                                             .packageName = "android.car.cts",
                                             .bytes = {0, 14000},
                                             .fsync{0, 100}},
                                            {.userId = 0,
                                             .packageName = "system_server",
                                             .bytes = {0, 3400},
                                             .fsync{0, 200}}},
                              .topNWrites = {{.userId = 0,
                                              .packageName = "android.car.cts",
                                              .bytes = {0, 16000},
                                              .fsync{0, 100}},
                                             {.userId = 0,
                                              .packageName = "system_server",
                                              .bytes = {0, 6700},
                                              .fsync{0, 200}}},
                              .total = {{0, 21600}, {0, 28300}, {0, 600}}},
            .systemIoPerfData = {.cpuIoWaitTime = 5900,
                                 .totalCpuTime = 48376,
                                 .ioBlockedProcessesCnt = 57,
                                 .totalProcessesCnt = 157},
            .processIoPerfData =
                    {.topNIoBlockedUids = {{0, "android.car.cts", 2, {{"cts_test", 2}}},
                                           {0, "system_server", 1, {{"system_server", 1}}}},
                     .topNIoBlockedUidsTotalTaskCnt = {2, 1},
                     .topNMajorFaultUids = {{0, "android.car.cts", 50900, {{"cts_test", 50900}}},
                                            {0, "system_server", 1234, {{"system_server", 1234}}}},
                     .totalMajorFaults = 55590,
                     .majorFaultsPercentChange = 0},
    };
    ret = looperStub->pollCache();
    ASSERT_TRUE(ret) << ret.error().message();
    ASSERT_EQ(looperStub->numSecondsElapsed(), 0) << "Custom collection didn't start immediately";

    ASSERT_EQ(collector->mCurrCollectionEvent, CollectionEvent::CUSTOM);
    ASSERT_EQ(collector->mCustomCollection.records.size(), 1);
    ASSERT_TRUE(isEqual(collector->mCustomCollection.records[0], expected))
            << "Custom collection record doesn't match.\nExpected:\n"
            << toString(expected) << "\nActual:\n"
            << toString(collector->mCustomCollection.records[0]);
    collector->terminate();
}

TEST(IoPerfCollectionTest, TestCustomCollectionTerminatesAfterMaxDuration) {
    sp<UidIoStatsStub> uidIoStatsStub = new UidIoStatsStub(true);
    sp<ProcStatStub> procStatStub = new ProcStatStub(true);
    sp<ProcPidStatStub> procPidStatStub = new ProcPidStatStub(true);
    sp<LooperStub> looperStub = new LooperStub();

    sp<IoPerfCollection> collector = new IoPerfCollection();
    collector->mUidIoStats = uidIoStatsStub;
    collector->mProcStat = procStatStub;
    collector->mProcPidStat = procPidStatStub;
    collector->mHandlerLooper = looperStub;

    auto ret = collector->start();
    ASSERT_TRUE(ret) << ret.error().message();

    // Dummy boot-time collection
    uidIoStatsStub->push({});
    procStatStub->push(ProcStatInfo{});
    procPidStatStub->push({});
    ret = looperStub->pollCache();
    ASSERT_TRUE(ret) << ret.error().message();

    // Dummy Periodic collection
    ret = collector->onBootFinished();
    ASSERT_TRUE(ret) << ret.error().message();
    uidIoStatsStub->push({});
    procStatStub->push(ProcStatInfo{});
    procPidStatStub->push({});
    ret = looperStub->pollCache();
    ASSERT_TRUE(ret) << ret.error().message();

    // Start custom Collection
    Vector<String16> args;
    args.push_back(String16(kStartCustomCollectionFlag));
    args.push_back(String16(kIntervalFlag));
    args.push_back(String16(std::to_string(kTestCustomInterval.count()).c_str()));
    args.push_back(String16(kMaxDurationFlag));
    args.push_back(String16(std::to_string(kTestCustomCollectionDuration.count()).c_str()));

    ret = collector->onCustomCollection(-1, args);
    ASSERT_TRUE(ret.ok()) << ret.error().message();
    // Maximum custom collection iterations during |kTestCustomCollectionDuration|.
    int maxIterations =
            static_cast<int>(kTestCustomCollectionDuration.count() / kTestCustomInterval.count());
    for (int i = 0; i < maxIterations; ++i) {
        ASSERT_TRUE(ret) << ret.error().message();
        uidIoStatsStub->push({});
        procStatStub->push(ProcStatInfo{});
        procPidStatStub->push({});
        ret = looperStub->pollCache();
        ASSERT_TRUE(ret) << ret.error().message();
        int secondsElapsed = (i == 0 ? 0 : kTestCustomInterval.count());
        ASSERT_EQ(looperStub->numSecondsElapsed(), secondsElapsed)
                << "Custom collection didn't happen at " << secondsElapsed
                << " seconds interval in iteration " << i;
    }

    ASSERT_EQ(collector->mCurrCollectionEvent, CollectionEvent::CUSTOM);
    ASSERT_GT(collector->mCustomCollection.records.size(), 0);
    // Next looper message was injected during startCustomCollection to end the custom collection
    // after |kTestCustomCollectionDuration|. Thus on processing this message the custom collection
    // should terminate.
    ret = looperStub->pollCache();
    ASSERT_TRUE(ret) << ret.error().message();
    ASSERT_EQ(looperStub->numSecondsElapsed(),
              kTestCustomCollectionDuration.count() % kTestCustomInterval.count())
            << "Custom collection did't end after " << kTestCustomCollectionDuration.count()
            << " seconds";
    ASSERT_EQ(collector->mCurrCollectionEvent, CollectionEvent::PERIODIC);
    ASSERT_EQ(collector->mCustomCollection.records.size(), 0)
            << "Custom collection records not discarded at the end of the collection";
    collector->terminate();
}

TEST(IoPerfCollectionTest, TestValidUidIoStatFile) {
    // Format: uid fgRdChar fgWrChar fgRdBytes fgWrBytes bgRdChar bgWrChar bgRdBytes bgWrBytes
    // fgFsync bgFsync
    constexpr char firstSnapshot[] =
        "1001234 5000 1000 3000 500 0 0 0 0 20 0\n"
        "1005678 500 100 30 50 300 400 100 200 45 60\n"
        "1009 0 0 0 0 40000 50000 20000 30000 0 300\n"
        "1001000 4000 3000 2000 1000 400 300 200 100 50 10\n";

    struct UidIoPerfData expectedUidIoPerfData = {};
    expectedUidIoPerfData.total[READ_BYTES][FOREGROUND] = 5030;
    expectedUidIoPerfData.total[READ_BYTES][BACKGROUND] = 20300;
    expectedUidIoPerfData.total[WRITE_BYTES][FOREGROUND] = 1550;
    expectedUidIoPerfData.total[WRITE_BYTES][BACKGROUND] = 30300;
    expectedUidIoPerfData.total[FSYNC_COUNT][FOREGROUND] = 115;
    expectedUidIoPerfData.total[FSYNC_COUNT][BACKGROUND] = 370;
    expectedUidIoPerfData.topNReads.push_back({
            // uid: 1009
            .userId = 0,
            .packageName = "mount",
            .bytes = {0, 20000},
            .fsync = {0, 300},
    });
    expectedUidIoPerfData.topNReads.push_back({
            // uid: 1001234
            .userId = 10,
            .packageName = "1001234",
            .bytes = {3000, 0},
            .fsync = {20, 0},
    });
    expectedUidIoPerfData.topNWrites.push_back({
            // uid: 1009
            .userId = 0,
            .packageName = "mount",
            .bytes = {0, 30000},
            .fsync = {0, 300},
    });
    expectedUidIoPerfData.topNWrites.push_back({
            // uid: 1001000
            .userId = 10,
            .packageName = "shared:android.uid.system",
            .bytes = {1000, 100},
            .fsync = {50, 10},
    });

    TemporaryFile tf;
    ASSERT_NE(tf.fd, -1);
    ASSERT_TRUE(WriteStringToFile(firstSnapshot, tf.path));

    IoPerfCollection collector;
    collector.mUidIoStats = new UidIoStats(tf.path);
    collector.mTopNStatsPerCategory = 2;
    ASSERT_TRUE(collector.mUidIoStats->enabled()) << "Temporary file is inaccessible";

    struct UidIoPerfData actualUidIoPerfData = {};
    auto ret = collector.collectUidIoPerfDataLocked(CollectionInfo{}, &actualUidIoPerfData);
    ASSERT_RESULT_OK(ret);
    EXPECT_TRUE(isEqual(expectedUidIoPerfData, actualUidIoPerfData))
        << "First snapshot doesn't match.\nExpected:\n"
        << toString(expectedUidIoPerfData) << "\nActual:\n"
        << toString(actualUidIoPerfData);

    constexpr char secondSnapshot[] =
        "1001234 10000 2000 7000 950 0 0 0 0 45 0\n"
        "1005678 600 100 40 50 1000 1000 1000 600 50 70\n"
        "1003456 300 500 200 300 0 0 0 0 50 0\n"
        "1001000 400 300 200 100 40 30 20 10 5 1\n";

    expectedUidIoPerfData = {};
    expectedUidIoPerfData.total[READ_BYTES][FOREGROUND] = 4210;
    expectedUidIoPerfData.total[READ_BYTES][BACKGROUND] = 900;
    expectedUidIoPerfData.total[WRITE_BYTES][FOREGROUND] = 750;
    expectedUidIoPerfData.total[WRITE_BYTES][BACKGROUND] = 400;
    expectedUidIoPerfData.total[FSYNC_COUNT][FOREGROUND] = 80;
    expectedUidIoPerfData.total[FSYNC_COUNT][BACKGROUND] = 10;
    expectedUidIoPerfData.topNReads.push_back({
            // uid: 1001234
            .userId = 10,
            .packageName = "1001234",
            .bytes = {4000, 0},
            .fsync = {25, 0},
    });
    expectedUidIoPerfData.topNReads.push_back({
            // uid: 1005678
            .userId = 10,
            .packageName = "1005678",
            .bytes = {10, 900},
            .fsync = {5, 10},
    });
    expectedUidIoPerfData.topNWrites.push_back({
            // uid: 1001234
            .userId = 10,
            .packageName = "1001234",
            .bytes = {450, 0},
            .fsync = {25, 0},
    });
    expectedUidIoPerfData.topNWrites.push_back({
            // uid: 1005678
            .userId = 10,
            .packageName = "1005678",
            .bytes = {0, 400},
            .fsync = {5, 10},
    });
    ASSERT_TRUE(WriteStringToFile(secondSnapshot, tf.path));
    actualUidIoPerfData = {};
    ret = collector.collectUidIoPerfDataLocked(CollectionInfo{}, &actualUidIoPerfData);
    ASSERT_RESULT_OK(ret);
    EXPECT_TRUE(isEqual(expectedUidIoPerfData, actualUidIoPerfData))
        << "Second snapshot doesn't match.\nExpected:\n"
        << toString(expectedUidIoPerfData) << "\nActual:\n"
        << toString(actualUidIoPerfData);
}

TEST(IoPerfCollectionTest, TestUidIOStatsLessThanTopNStatsLimit) {
    // Format: uid fgRdChar fgWrChar fgRdBytes fgWrBytes bgRdChar bgWrChar bgRdBytes bgWrBytes
    // fgFsync bgFsync
    constexpr char contents[] = "1001234 5000 1000 3000 500 0 0 0 0 20 0\n";

    struct UidIoPerfData expectedUidIoPerfData = {};
    expectedUidIoPerfData.total[READ_BYTES][FOREGROUND] = 3000;
    expectedUidIoPerfData.total[READ_BYTES][BACKGROUND] = 0;
    expectedUidIoPerfData.total[WRITE_BYTES][FOREGROUND] = 500;
    expectedUidIoPerfData.total[WRITE_BYTES][BACKGROUND] = 0;
    expectedUidIoPerfData.total[FSYNC_COUNT][FOREGROUND] = 20;
    expectedUidIoPerfData.total[FSYNC_COUNT][BACKGROUND] = 0;
    expectedUidIoPerfData.topNReads.push_back({
            // uid: 1001234
            .userId = 10,
            .packageName = "1001234",
            .bytes = {3000, 0},
            .fsync = {20, 0},
    });
    expectedUidIoPerfData.topNWrites.push_back({
            // uid: 1001234
            .userId = 10,
            .packageName = "1001234",
            .bytes = {500, 0},
            .fsync = {20, 0},
    });

    TemporaryFile tf;
    ASSERT_NE(tf.fd, -1);
    ASSERT_TRUE(WriteStringToFile(contents, tf.path));

    IoPerfCollection collector;
    collector.mUidIoStats = new UidIoStats(tf.path);
    collector.mTopNStatsPerCategory = 10;
    ASSERT_TRUE(collector.mUidIoStats->enabled()) << "Temporary file is inaccessible";

    struct UidIoPerfData actualUidIoPerfData = {};
    const auto& ret = collector.collectUidIoPerfDataLocked(CollectionInfo{}, &actualUidIoPerfData);
    ASSERT_RESULT_OK(ret);
    EXPECT_TRUE(isEqual(expectedUidIoPerfData, actualUidIoPerfData))
        << "Collected data doesn't match.\nExpected:\n"
        << toString(expectedUidIoPerfData) << "\nActual:\n"
        << toString(actualUidIoPerfData);
}

TEST(IoPerfCollectionTest, TestValidProcStatFile) {
    constexpr char firstSnapshot[] =
            "cpu  6200 5700 1700 3100 1100 5200 3900 0 0 0\n"
            "cpu0 2400 2900 600 690 340 4300 2100 0 0 0\n"
            "cpu1 1900 2380 510 760 51 370 1500 0 0 0\n"
            "cpu2 900 400 400 1000 600 400 160 0 0 0\n"
            "cpu3 1000 20 190 650 109 130 140 0 0 0\n"
            "intr 694351583 0 0 0 297062868 0 5922464 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 "
            "0 0\n"
            // Skipped most of the intr line as it is not important for testing the ProcStat parsing
            // logic.
            "ctxt 579020168\n"
            "btime 1579718450\n"
            "processes 113804\n"
            "procs_running 17\n"
            "procs_blocked 5\n"
            "softirq 33275060 934664 11958403 5111 516325 200333 0 341482 10651335 0 8667407\n";
    struct SystemIoPerfData expectedSystemIoPerfData = {
            .cpuIoWaitTime = 1100,
            .totalCpuTime = 26900,
            .ioBlockedProcessesCnt = 5,
            .totalProcessesCnt = 22,
    };

    TemporaryFile tf;
    ASSERT_NE(tf.fd, -1);
    ASSERT_TRUE(WriteStringToFile(firstSnapshot, tf.path));

    IoPerfCollection collector;
    collector.mProcStat = new ProcStat(tf.path);
    ASSERT_TRUE(collector.mProcStat->enabled()) << "Temporary file is inaccessible";

    struct SystemIoPerfData actualSystemIoPerfData = {};
    auto ret = collector.collectSystemIoPerfDataLocked(&actualSystemIoPerfData);
    ASSERT_RESULT_OK(ret);
    EXPECT_TRUE(isEqual(expectedSystemIoPerfData, actualSystemIoPerfData))
            << "First snapshot doesn't match.\nExpected:\n"
            << toString(expectedSystemIoPerfData) << "\nActual:\n"
            << toString(actualSystemIoPerfData);

    constexpr char secondSnapshot[] =
            "cpu  16200 8700 2000 4100 2200 6200 5900 0 0 0\n"
            "cpu0 4400 3400 700 890 800 4500 3100 0 0 0\n"
            "cpu1 5900 3380 610 960 100 670 2000 0 0 0\n"
            "cpu2 2900 1000 450 1400 800 600 460 0 0 0\n"
            "cpu3 3000 920 240 850 500 430 340 0 0 0\n"
            "intr 694351583 0 0 0 297062868 0 5922464 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 "
            "0 0\n"
            "ctxt 579020168\n"
            "btime 1579718450\n"
            "processes 113804\n"
            "procs_running 10\n"
            "procs_blocked 2\n"
            "softirq 33275060 934664 11958403 5111 516325 200333 0 341482 10651335 0 8667407\n";
    expectedSystemIoPerfData = {
            .cpuIoWaitTime = 1100,
            .totalCpuTime = 18400,
            .ioBlockedProcessesCnt = 2,
            .totalProcessesCnt = 12,
    };

    ASSERT_TRUE(WriteStringToFile(secondSnapshot, tf.path));
    actualSystemIoPerfData = {};
    ret = collector.collectSystemIoPerfDataLocked(&actualSystemIoPerfData);
    ASSERT_RESULT_OK(ret);
    EXPECT_TRUE(isEqual(expectedSystemIoPerfData, actualSystemIoPerfData))
            << "Second snapshot doesn't match.\nExpected:\n"
            << toString(expectedSystemIoPerfData) << "\nActual:\n"
            << toString(actualSystemIoPerfData);
}

TEST(IoPerfCollectionTest, TestValidProcPidContents) {
    std::unordered_map<uint32_t, std::vector<uint32_t>> pidToTids = {
            {1, {1, 453}},
            {2546, {2546, 3456, 4789}},
            {7890, {7890, 8978, 12890}},
            {18902, {18902, 21345, 32452}},
            {28900, {28900}},
    };
    std::unordered_map<uint32_t, std::string> perProcessStat = {
            {1, "1 (init) S 0 0 0 0 0 0 0 0 220 0 0 0 0 0 0 0 2 0 0\n"},
            {2546, "2546 (system_server) R 1 0 0 0 0 0 0 0 6000 0 0 0 0 0 0 0 3 0 1000\n"},
            {7890, "7890 (logd) D 1 0 0 0 0 0 0 0 15000 0 0 0 0 0 0 0 3 0 2345\n"},
            {18902, "18902 (disk I/O) D 1 0 0 0 0 0 0 0 45678 0 0 0 0 0 0 0 3 0 897654\n"},
            {28900, "28900 (tombstoned) D 1 0 0 0 0 0 0 0 89765 0 0 0 0 0 0 0 3 0 2345671\n"},
    };
    std::unordered_map<uint32_t, std::string> perProcessStatus = {
            {1, "Pid:\t1\nTgid:\t1\nUid:\t0\t0\t0\t0\n"},
            {2546, "Pid:\t2546\nTgid:\t2546\nUid:\t1001000\t1001000\t1001000\t1001000\n"},
            {7890, "Pid:\t7890\nTgid:\t7890\nUid:\t1001000\t1001000\t1001000\t1001000\n"},
            {18902, "Pid:\t18902\nTgid:\t18902\nUid:\t1009\t1009\t1009\t1009\n"},
            {28900, "Pid:\t28900\nTgid:\t28900\nUid:\t1001234\t1001234\t1001234\t1001234\n"},
    };
    std::unordered_map<uint32_t, std::string> perThreadStat = {
            {1, "1 (init) S 0 0 0 0 0 0 0 0 200 0 0 0 0 0 0 0 2 0 0\n"},
            {453, "453 (init) S 0 0 0 0 0 0 0 0 20 0 0 0 0 0 0 0 2 0 275\n"},
            {2546, "2546 (system_server) R 1 0 0 0 0 0 0 0 1000 0 0 0 0 0 0 0 3 0 1000\n"},
            {3456, "3456 (system_server) S 1 0 0 0 0 0 0 0 3000 0 0 0 0 0 0 0 3 0 2300\n"},
            {4789, "4789 (system_server) D 1 0 0 0 0 0 0 0 2000 0 0 0 0 0 0 0 3 0 4500\n"},
            {7890, "7890 (logd) D 1 0 0 0 0 0 0 0 10000 0 0 0 0 0 0 0 3 0 2345\n"},
            {8978, "8978 (logd) D 1 0 0 0 0 0 0 0 1000 0 0 0 0 0 0 0 3 0 2500\n"},
            {12890, "12890 (logd) D 1 0 0 0 0 0 0 0 500 0 0 0 0 0 0 0 3 0 2900\n"},
            {18902, "18902 (disk I/O) D 1 0 0 0 0 0 0 0 30000 0 0 0 0 0 0 0 3 0 897654\n"},
            {21345, "21345 (disk I/O) D 1 0 0 0 0 0 0 0 15000 0 0 0 0 0 0 0 3 0 904000\n"},
            {32452, "32452 (disk I/O) D 1 0 0 0 0 0 0 0 678 0 0 0 0 0 0 0 3 0 1007000\n"},
            {28900, "28900 (tombstoned) D 1 0 0 0 0 0 0 0 89765 0 0 0 0 0 0 0 3 0 2345671\n"},
    };
    struct ProcessIoPerfData expectedProcessIoPerfData = {};
    expectedProcessIoPerfData.topNIoBlockedUids.push_back({
            // uid: 1001000
            .userId = 10,
            .packageName = "shared:android.uid.system",
            .count = 4,
            .topNProcesses = {{"logd", 3}, {"system_server", 1}},
    });
    expectedProcessIoPerfData.topNIoBlockedUidsTotalTaskCnt.push_back(6);
    expectedProcessIoPerfData.topNIoBlockedUids.push_back({
            // uid: 1009
            .userId = 0,
            .packageName = "mount",
            .count = 3,
            .topNProcesses = {{"disk I/O", 3}},
    });
    expectedProcessIoPerfData.topNIoBlockedUidsTotalTaskCnt.push_back(3);
    expectedProcessIoPerfData.topNMajorFaultUids.push_back({
            // uid: 1001234
            .userId = 10,
            .packageName = "1001234",
            .count = 89765,
            .topNProcesses = {{"tombstoned", 89765}},
    });
    expectedProcessIoPerfData.topNMajorFaultUids.push_back({
            // uid: 1009
            .userId = 0,
            .packageName = "mount",
            .count = 45678,
            .topNProcesses = {{"disk I/O", 45678}},
    });
    expectedProcessIoPerfData.totalMajorFaults = 156663;
    expectedProcessIoPerfData.majorFaultsPercentChange = 0;

    TemporaryDir firstSnapshot;
    auto ret = populateProcPidDir(firstSnapshot.path, pidToTids, perProcessStat, perProcessStatus,
                                  perThreadStat);
    ASSERT_TRUE(ret) << "Failed to populate proc pid dir: " << ret.error();

    IoPerfCollection collector;
    collector.mProcPidStat = new ProcPidStat(firstSnapshot.path);
    collector.mTopNStatsPerCategory = 2;
    collector.mTopNStatsPerSubcategory = 2;
    ASSERT_TRUE(collector.mProcPidStat->enabled())
            << "Files under the temporary proc directory are inaccessible";

    struct ProcessIoPerfData actualProcessIoPerfData = {};
    ret = collector.collectProcessIoPerfDataLocked(CollectionInfo{}, &actualProcessIoPerfData);
    ASSERT_TRUE(ret) << "Failed to collect first snapshot: " << ret.error();
    EXPECT_TRUE(isEqual(expectedProcessIoPerfData, actualProcessIoPerfData))
            << "First snapshot doesn't match.\nExpected:\n"
            << toString(expectedProcessIoPerfData) << "\nActual:\n"
            << toString(actualProcessIoPerfData);

    pidToTids = {
            {1, {1, 453}},
            {2546, {2546, 3456, 4789}},
    };
    perProcessStat = {
            {1, "1 (init) S 0 0 0 0 0 0 0 0 880 0 0 0 0 0 0 0 2 0 0\n"},
            {2546, "2546 (system_server) R 1 0 0 0 0 0 0 0 18000 0 0 0 0 0 0 0 3 0 1000\n"},
    };
    perProcessStatus = {
            {1, "Pid:\t1\nTgid:\t1\nUid:\t0\t0\t0\t0\n"},
            {2546, "Pid:\t2546\nTgid:\t2546\nUid:\t1001000\t1001000\t1001000\t1001000\n"},
    };
    perThreadStat = {
            {1, "1 (init) S 0 0 0 0 0 0 0 0 800 0 0 0 0 0 0 0 2 0 0\n"},
            {453, "453 (init) S 0 0 0 0 0 0 0 0 80 0 0 0 0 0 0 0 2 0 275\n"},
            {2546, "2546 (system_server) R 1 0 0 0 0 0 0 0 3000 0 0 0 0 0 0 0 3 0 1000\n"},
            {3456, "3456 (system_server) S 1 0 0 0 0 0 0 0 9000 0 0 0 0 0 0 0 3 0 2300\n"},
            {4789, "4789 (system_server) D 1 0 0 0 0 0 0 0 6000 0 0 0 0 0 0 0 3 0 4500\n"},
    };
    expectedProcessIoPerfData = {};
    expectedProcessIoPerfData.topNIoBlockedUids.push_back({
            // uid: 1001000
            .userId = 10,
            .packageName = "shared:android.uid.system",
            .count = 1,
            .topNProcesses = {{"system_server", 1}},
    });
    expectedProcessIoPerfData.topNIoBlockedUidsTotalTaskCnt.push_back(3);
    expectedProcessIoPerfData.topNMajorFaultUids.push_back({
            // uid: 1001000
            .userId = 10,
            .packageName = "shared:android.uid.system",
            .count = 12000,
            .topNProcesses = {{"system_server", 12000}},
    });
    expectedProcessIoPerfData.topNMajorFaultUids.push_back({
            // uid: 0
            .userId = 0,
            .packageName = "root",
            .count = 660,
            .topNProcesses = {{"init", 660}},
    });
    expectedProcessIoPerfData.totalMajorFaults = 12660;
    expectedProcessIoPerfData.majorFaultsPercentChange = ((12660.0 - 156663.0) / 156663.0) * 100;

    TemporaryDir secondSnapshot;
    ret = populateProcPidDir(secondSnapshot.path, pidToTids, perProcessStat, perProcessStatus,
                             perThreadStat);
    ASSERT_TRUE(ret) << "Failed to populate proc pid dir: " << ret.error();

    collector.mProcPidStat->mPath = secondSnapshot.path;

    actualProcessIoPerfData = {};
    ret = collector.collectProcessIoPerfDataLocked(CollectionInfo{}, &actualProcessIoPerfData);
    ASSERT_TRUE(ret) << "Failed to collect second snapshot: " << ret.error();
    EXPECT_TRUE(isEqual(expectedProcessIoPerfData, actualProcessIoPerfData))
            << "Second snapshot doesn't match.\nExpected:\n"
            << toString(expectedProcessIoPerfData) << "\nActual:\n"
            << toString(actualProcessIoPerfData);
}

TEST(IoPerfCollectionTest, TestProcPidContentsLessThanTopNStatsLimit) {
    std::unordered_map<uint32_t, std::vector<uint32_t>> pidToTids = {
            {1, {1, 453}},
    };
    std::unordered_map<uint32_t, std::string> perProcessStat = {
            {1, "1 (init) S 0 0 0 0 0 0 0 0 880 0 0 0 0 0 0 0 2 0 0\n"},
    };
    std::unordered_map<uint32_t, std::string> perProcessStatus = {
            {1, "Pid:\t1\nTgid:\t1\nUid:\t0\t0\t0\t0\n"},
    };
    std::unordered_map<uint32_t, std::string> perThreadStat = {
            {1, "1 (init) S 0 0 0 0 0 0 0 0 800 0 0 0 0 0 0 0 2 0 0\n"},
            {453, "453 (init) S 0 0 0 0 0 0 0 0 80 0 0 0 0 0 0 0 2 0 275\n"},
    };
    struct ProcessIoPerfData expectedProcessIoPerfData = {};
    expectedProcessIoPerfData.topNMajorFaultUids.push_back({
            // uid: 0
            .userId = 0,
            .packageName = "root",
            .count = 880,
            .topNProcesses = {{"init", 880}},
    });
    expectedProcessIoPerfData.totalMajorFaults = 880;
    expectedProcessIoPerfData.majorFaultsPercentChange = 0.0;

    TemporaryDir prodDir;
    auto ret = populateProcPidDir(prodDir.path, pidToTids, perProcessStat, perProcessStatus,
                                  perThreadStat);
    ASSERT_TRUE(ret) << "Failed to populate proc pid dir: " << ret.error();

    IoPerfCollection collector;
    collector.mTopNStatsPerCategory = 5;
    collector.mTopNStatsPerSubcategory = 3;
    collector.mProcPidStat = new ProcPidStat(prodDir.path);
    struct ProcessIoPerfData actualProcessIoPerfData = {};
    ret = collector.collectProcessIoPerfDataLocked(CollectionInfo{}, &actualProcessIoPerfData);
    ASSERT_TRUE(ret) << "Failed to collect proc pid contents: " << ret.error();
    EXPECT_TRUE(isEqual(expectedProcessIoPerfData, actualProcessIoPerfData))
            << "proc pid contents don't match.\nExpected:\n"
            << toString(expectedProcessIoPerfData) << "\nActual:\n"
            << toString(actualProcessIoPerfData);
}

TEST(IoPerfCollectionTest, TestHandlesInvalidDumpArguments) {
    sp<IoPerfCollection> collector = new IoPerfCollection();
    collector->start();
    Vector<String16> args;
    args.push_back(String16(kStartCustomCollectionFlag));
    args.push_back(String16("Invalid flag"));
    args.push_back(String16("Invalid value"));
    ASSERT_FALSE(collector->onCustomCollection(-1, args).ok());

    args.clear();
    args.push_back(String16(kStartCustomCollectionFlag));
    args.push_back(String16(kIntervalFlag));
    args.push_back(String16("Invalid interval"));
    ASSERT_FALSE(collector->onCustomCollection(-1, args).ok());

    args.clear();
    args.push_back(String16(kStartCustomCollectionFlag));
    args.push_back(String16(kMaxDurationFlag));
    args.push_back(String16("Invalid duration"));
    ASSERT_FALSE(collector->onCustomCollection(-1, args).ok());

    args.clear();
    args.push_back(String16(kEndCustomCollectionFlag));
    args.push_back(String16(kMaxDurationFlag));
    args.push_back(String16(std::to_string(kTestCustomCollectionDuration.count()).c_str()));
    ASSERT_FALSE(collector->onCustomCollection(-1, args).ok());

    args.clear();
    args.push_back(String16("Invalid flag"));
    ASSERT_FALSE(collector->onCustomCollection(-1, args).ok());
    collector->terminate();
}

}  // namespace watchdog
}  // namespace automotive
}  // namespace android
