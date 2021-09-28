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

#include "HalCamera.h"
#include "StatsCollector.h"
#include "VirtualCamera.h"

#include <processgroup/sched_policy.h>
#include <pthread.h>

#include <android-base/file.h>
#include <android-base/strings.h>
#include <android-base/stringprintf.h>
#include <utils/SystemClock.h>

namespace {

    const char* kSingleIndent = "\t";
    const char* kDoubleIndent = "\t\t";
    const char* kDumpAllDevices = "all";

}

namespace android {
namespace automotive {
namespace evs {
namespace V1_1 {
namespace implementation {

using android::base::Error;
using android::base::EqualsIgnoreCase;
using android::base::Result;
using android::base::StringAppendF;
using android::base::StringPrintf;
using android::base::WriteStringToFd;
using android::hardware::automotive::evs::V1_1::BufferDesc;

namespace {

const auto kPeriodicCollectionInterval = 10s;
const auto kPeriodicCollectionCacheSize = 180;
const auto kMinCollectionInterval = 1s;
const auto kCustomCollectionMaxDuration = 30min;
const auto kMaxDumpHistory = 10;

}

void StatsCollector::handleMessage(const Message& message) {
    const auto received = static_cast<CollectionEvent>(message.what);
    Result<void> ret;
    switch (received) {
        case CollectionEvent::PERIODIC:
            ret = handleCollectionEvent(received, &mPeriodicCollectionInfo);
            break;

        case CollectionEvent::CUSTOM_START:
            ret = handleCollectionEvent(received, &mCustomCollectionInfo);
            break;

        case CollectionEvent::CUSTOM_END: {
            AutoMutex lock(mMutex);
            if (mCurrentCollectionEvent != CollectionEvent::CUSTOM_START) {
                LOG(WARNING) << "Ignoring a message to end custom collection "
                             << "as current collection is " << toString(mCurrentCollectionEvent);
                return;
            }

            // Starts a periodic collection
            mLooper->removeMessages(this);
            mCurrentCollectionEvent = CollectionEvent::PERIODIC;
            mPeriodicCollectionInfo.lastCollectionTime = mLooper->now();
            mLooper->sendMessage(this, CollectionEvent::PERIODIC);
            return;
        }

        default:
            LOG(WARNING) << "Unknown event is received: " << received;
            break;
    }

    if (!ret.ok()) {
        Mutex::Autolock lock(mMutex);
        LOG(ERROR) << "Terminating data collection: "
                   << ret.error();

        mCurrentCollectionEvent = CollectionEvent::TERMINATED;
        mLooper->removeMessages(this);
        mLooper->wake();
    }
}


Result<void> StatsCollector::handleCollectionEvent(CollectionEvent event,
                                                   CollectionInfo* info) {
    AutoMutex lock(mMutex);
    if (mCurrentCollectionEvent != event) {
        if (mCurrentCollectionEvent != CollectionEvent::TERMINATED) {
            LOG(WARNING) << "Skipping " << toString(event) << " collection event "
                         << "on collection event " << toString(mCurrentCollectionEvent);

            return {};
        } else {
            return Error() << "A collection has been terminated "
                           << "while a current event was pending in the message queue.";
        }
    }

    if (info->maxCacheSize < 1) {
        return Error() << "Maximum cache size must be greater than 0";
    }

    using std::chrono::duration_cast;
    using std::chrono::seconds;
    if (info->interval < kMinCollectionInterval) {
        LOG(WARNING) << "Collection interval of "
                     << duration_cast<seconds>(info->interval).count()
                     << " seconds for " << toString(event)
                     << " collection cannot be shorter than "
                     << duration_cast<seconds>(kMinCollectionInterval).count()
                     << " seconds.";
        info->interval = kMinCollectionInterval;
    }

    auto ret = collectLocked(info);
    if (!ret) {
        return Error() << toString(event) << " collection failed: "
                       << ret.error();
    }

    // Arms a message for next periodic collection
    info->lastCollectionTime += info->interval.count();
    mLooper->sendMessageAtTime(info->lastCollectionTime, this, event);
    return {};
}


Result<void> StatsCollector::collectLocked(CollectionInfo* info) REQUIRES(mMutex) {
    for (auto&& [id, ptr] : mClientsToMonitor) {
        auto pClient = ptr.promote();
        if (!pClient) {
            LOG(DEBUG) << id << " seems not alive.";
            continue;
        }

        // Pulls a snapshot and puts a timestamp
        auto snapshot = pClient->getStats();
        snapshot.timestamp = mLooper->now();

        // Removes the oldest record if cache is full
        if (info->records[id].history.size() > info->maxCacheSize) {
            info->records[id].history.pop_front();
        }

        // Stores the latest record and the deltas
        auto delta = snapshot - info->records[id].latest;
        info->records[id].history.emplace_back(delta);
        info->records[id].latest = snapshot;
    }

    return {};
}


Result<void> StatsCollector::startCollection() {
    {
        AutoMutex lock(mMutex);
        if (mCurrentCollectionEvent != CollectionEvent::INIT ||
            mCollectionThread.joinable()) {
            return Error(INVALID_OPERATION)
                   << "Camera usages collection is already running.";
        }

        // Create the collection info w/ the default values
        mPeriodicCollectionInfo = {
            .interval = kPeriodicCollectionInterval,
            .maxCacheSize = kPeriodicCollectionCacheSize,
            .lastCollectionTime = 0,
        };

    }

    // Starts a background worker thread
    mCollectionThread = std::thread([&]() {
        {
            AutoMutex lock(mMutex);
            if (mCurrentCollectionEvent != CollectionEvent::INIT) {
                LOG(ERROR) << "Skipping the statistics collection because "
                           << "the current collection event is "
                           << toString(mCurrentCollectionEvent);
                return;
            }

            // Staring with a periodic collection
            mCurrentCollectionEvent = CollectionEvent::PERIODIC;
        }

        if (set_sched_policy(0, SP_BACKGROUND) != 0) {
            PLOG(WARNING) << "Failed to set background scheduling prioirty";
        }

        // Sets a looper for the communication
        mLooper->setLooper(Looper::prepare(/*opts=*/0));

        // Starts collecting the usage statistics periodically
        mLooper->sendMessage(this, CollectionEvent::PERIODIC);

        // Polls the messages until the collection is stopped.
        bool isActive = true;
        while (isActive) {
            mLooper->pollAll(/*timeoutMillis=*/-1);
            {
                AutoMutex lock(mMutex);
                isActive = mCurrentCollectionEvent != CollectionEvent::TERMINATED;
            }
        }
    });

    auto ret = pthread_setname_np(mCollectionThread.native_handle(), "EvsUsageCollect");
    if (ret != 0) {
        PLOG(WARNING) << "Failed to name a collection thread";
    }

    return {};
}


Result<void> StatsCollector::stopCollection() {
    {
        AutoMutex lock(mMutex);
        if (mCurrentCollectionEvent == CollectionEvent::TERMINATED) {
            LOG(WARNING) << "Camera usage data collection was stopped already.";
            return {};
        }

        LOG(INFO) << "Stopping a camera usage data collection";
        mCurrentCollectionEvent = CollectionEvent::TERMINATED;
    }

    // Join a background thread
    if (mCollectionThread.joinable()) {
        mLooper->removeMessages(this);
        mLooper->wake();
        mCollectionThread.join();
    }

    return {};
}


Result<void> StatsCollector::startCustomCollection(
        std::chrono::nanoseconds interval,
        std::chrono::nanoseconds maxDuration) {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    if (interval < kMinCollectionInterval || maxDuration < kMinCollectionInterval) {
        return Error(INVALID_OPERATION)
                << "Collection interval and maximum maxDuration must be >= "
                << duration_cast<milliseconds>(kMinCollectionInterval).count()
                << " milliseconds.";
    }

    if (maxDuration > kCustomCollectionMaxDuration) {
        return Error(INVALID_OPERATION)
                << "Collection maximum maxDuration must be less than "
                << duration_cast<milliseconds>(kCustomCollectionMaxDuration).count()
                << " milliseconds.";
    }

    {
        AutoMutex lock(mMutex);
        if (mCurrentCollectionEvent != CollectionEvent::PERIODIC) {
            return Error(INVALID_OPERATION)
                    << "Cannot start a custom collection when "
                    << "the current collection event " << toString(mCurrentCollectionEvent)
                    << " != " << toString(CollectionEvent::PERIODIC) << " collection event";
        }

        // Notifies the user if a preview custom collection result is
        // not used yet.
        if (mCustomCollectionInfo.records.size() > 0) {
            LOG(WARNING) << "Previous custom collection result, which was done at "
                         << mCustomCollectionInfo.lastCollectionTime
                         << " has not pulled yet will be overwritten.";
        }

        // Programs custom collection configurations
        mCustomCollectionInfo = {
                .interval = interval,
                .maxCacheSize = std::numeric_limits<std::size_t>::max(),
                .lastCollectionTime = mLooper->now(),
                .records = {},
        };

        mLooper->removeMessages(this);
        nsecs_t uptime = mLooper->now() + maxDuration.count();
        mLooper->sendMessageAtTime(uptime, this, CollectionEvent::CUSTOM_END);
        mCurrentCollectionEvent = CollectionEvent::CUSTOM_START;
        mLooper->sendMessage(this, CollectionEvent::CUSTOM_START);
    }

    return {};
}


Result<std::string> StatsCollector::stopCustomCollection(std::string targetId) {
    Mutex::Autolock lock(mMutex);
    if (mCurrentCollectionEvent == CollectionEvent::CUSTOM_START) {
        // Stops a running custom collection
        mLooper->removeMessages(this);
        mLooper->sendMessage(this, CollectionEvent::CUSTOM_END);
    }

    auto ret = collectLocked(&mCustomCollectionInfo);
    if (!ret) {
        return Error() << toString(mCurrentCollectionEvent) << " collection failed: "
                       << ret.error();
    }

    // Prints out the all collected statistics
    std::string buffer;
    using std::chrono::duration_cast;
    using std::chrono::seconds;
    const intmax_t interval =
        duration_cast<seconds>(mCustomCollectionInfo.interval).count();
    if (EqualsIgnoreCase(targetId, kDumpAllDevices)) {
        for (auto& [id, records] : mCustomCollectionInfo.records) {

            StringAppendF(&buffer, "%s\n"
                                   "%sNumber of collections: %zu\n"
                                   "%sCollection interval: %" PRIdMAX " secs\n",
                                   id.c_str(),
                                   kSingleIndent, records.history.size(),
                                   kSingleIndent, interval);
            auto it = records.history.rbegin();
            while (it != records.history.rend()) {
                buffer += it++->toString(kDoubleIndent);
            }
        }

        // Clears the collection
        mCustomCollectionInfo = {};
    } else {
        auto it = mCustomCollectionInfo.records.find(targetId);
        if (it != mCustomCollectionInfo.records.end()) {
            StringAppendF(&buffer, "%s\n"
                                   "%sNumber of collections: %zu\n"
                                   "%sCollection interval: %" PRIdMAX " secs\n",
                                   targetId.c_str(),
                                   kSingleIndent, it->second.history.size(),
                                   kSingleIndent, interval);
            auto recordIter = it->second.history.rbegin();
            while (recordIter != it->second.history.rend()) {
                buffer += recordIter++->toString(kDoubleIndent);
            }

            // Clears the collection
            mCustomCollectionInfo = {};
        } else {
            // Keeps the collection as the users may want to execute a command
            // again with a right device id
            StringAppendF(&buffer, "%s has not been monitored.", targetId.c_str());
        }
    }

    return buffer;
}


Result<void> StatsCollector::registerClientToMonitor(android::sp<HalCamera>& camera) {
    if (!camera) {
        return Error(BAD_VALUE) << "Given camera client is invalid";
    }

    AutoMutex lock(mMutex);
    const auto id = camera->getId();
    if (mClientsToMonitor.find(id) != mClientsToMonitor.end()) {
        LOG(WARNING) << id << " is already registered.";
    } else {
        mClientsToMonitor.insert_or_assign(id, camera);
    }

    return {};
}


Result<void> StatsCollector::unregisterClientToMonitor(const std::string& id) {
    AutoMutex lock(mMutex);
    auto entry = mClientsToMonitor.find(id);
    if (entry != mClientsToMonitor.end()) {
        mClientsToMonitor.erase(entry);
    } else {
        LOG(WARNING) << id << " has not been registerd.";
    }

    return {};
}


std::string StatsCollector::toString(const CollectionEvent& event) const {
    switch(event) {
        case CollectionEvent::INIT:
            return "CollectionEvent::INIT";
        case CollectionEvent::PERIODIC:
            return "CollectionEvent::PERIODIC";
        case CollectionEvent::CUSTOM_START:
            return "CollectionEvent::CUSTOM_START";
        case CollectionEvent::CUSTOM_END:
            return "CollectionEvent::CUSTOM_END";
        case CollectionEvent::TERMINATED:
            return "CollectionEvent::TERMINATED";

        default:
            return "Unknown";
    }
}


Result<void> StatsCollector::toString(std::unordered_map<std::string, std::string>* usages,
                                      const char* indent) EXCLUDES(mMutex) {
    std::string double_indent(indent);
    double_indent += indent;

    {
        AutoMutex lock(mMutex);
        using std::chrono::duration_cast;
        using std::chrono::seconds;
        const intmax_t interval =
            duration_cast<seconds>(mPeriodicCollectionInfo.interval).count();

        for (auto&& [id, records] : mPeriodicCollectionInfo.records) {
            std::string buffer;
            StringAppendF(&buffer, "%s\n"
                                   "%sNumber of collections: %zu\n"
                                   "%sCollection interval: %" PRIdMAX "secs\n",
                                   id.c_str(),
                                   indent, records.history.size(),
                                   indent, interval);

            // Adding up to kMaxDumpHistory records
            auto it = records.history.rbegin();
            auto count = 0;
            while (it != records.history.rend() && count < kMaxDumpHistory) {
                buffer += it->toString(double_indent.c_str());
                ++it;
                ++count;
            }

            usages->insert_or_assign(id, std::move(buffer));
        }
    }

    return {};
}


} // namespace implementation
} // namespace V1_1
} // namespace evs
} // namespace automotive
} // namespace android

