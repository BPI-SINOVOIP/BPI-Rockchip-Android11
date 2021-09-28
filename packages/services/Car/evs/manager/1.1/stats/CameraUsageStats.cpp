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

#include "CameraUsageStats.h"

#include <statslog.h>

#include <android-base/logging.h>

namespace {

    // Length of frame roundtrip history
    const int kMaxHistoryLength = 100;

}

namespace android {
namespace automotive {
namespace evs {
namespace V1_1 {
namespace implementation {

using ::android::base::Result;
using ::android::base::StringAppendF;
using ::android::hardware::hidl_vec;
using ::android::hardware::automotive::evs::V1_1::BufferDesc;

void CameraUsageStats::updateFrameStatsOnArrival(
        const hidl_vec<BufferDesc>& bufs) {
    const auto now = android::uptimeMillis();
    for (const auto& b : bufs) {
        auto it = mBufferHistory.find(b.bufferId);
        if (it == mBufferHistory.end()) {
            mBufferHistory.emplace(b.bufferId, now);
        } else {
            it->second.timestamp = now;
        }
    }
}


void CameraUsageStats::updateFrameStatsOnReturn(
        const hidl_vec<BufferDesc>& bufs) {
    const auto now = android::uptimeMillis();
    for (auto& b : bufs) {
        auto it = mBufferHistory.find(b.bufferId);
        if (it == mBufferHistory.end()) {
            LOG(WARNING) << "Buffer " << b.bufferId << " from "
                         << b.deviceId << " is unknown.";
        } else {
            const auto roundtrip = now - it->second.timestamp;
            it->second.history.emplace(roundtrip);
            it->second.sum += roundtrip;
            if (it->second.history.size() > kMaxHistoryLength) {
                it->second.sum -= it->second.history.front();
                it->second.history.pop();
            }

            if (roundtrip > it->second.peak) {
                it->second.peak = roundtrip;
            }

            if (mStats.framesFirstRoundtripLatency == 0) {
                mStats.framesFirstRoundtripLatency = roundtrip;
            }
        }
    }
}


void CameraUsageStats::framesReceived(int n) {
    AutoMutex lock(mMutex);
    mStats.framesReceived += n;
}


void CameraUsageStats::framesReceived(
        const hidl_vec<BufferDesc>& bufs) {
    AutoMutex lock(mMutex);
    mStats.framesReceived += bufs.size();

    updateFrameStatsOnArrival(bufs);
}


void CameraUsageStats::framesReturned(int n) {
    AutoMutex lock(mMutex);
    mStats.framesReturned += n;
}


void CameraUsageStats::framesReturned(
        const hidl_vec<BufferDesc>& bufs) {
    AutoMutex lock(mMutex);
    mStats.framesReturned += bufs.size();

    updateFrameStatsOnReturn(bufs);
}


void CameraUsageStats::framesIgnored(int n) {
    AutoMutex lock(mMutex);
    mStats.framesIgnored += n;
}


void CameraUsageStats::framesSkippedToSync(int n) {
    AutoMutex lock(mMutex);
    mStats.framesSkippedToSync += n;
}


void CameraUsageStats::eventsReceived() {
    AutoMutex lock(mMutex);
    ++mStats.erroneousEventsCount;
}


void CameraUsageStats::updateNumClients(size_t n) {
    AutoMutex lock(mMutex);
    if (n > mStats.peakClientsCount) {
        mStats.peakClientsCount = n;
    }
}


int64_t CameraUsageStats::getTimeCreated() const {
    AutoMutex lock(mMutex);
    return mTimeCreatedMs;
}


int64_t CameraUsageStats::getFramesReceived() const {
    AutoMutex lock(mMutex);
    return mStats.framesReceived;
}


int64_t CameraUsageStats::getFramesReturned() const {
    AutoMutex lock(mMutex);
    return mStats.framesReturned;
}


CameraUsageStatsRecord CameraUsageStats::snapshot() {
    AutoMutex lock(mMutex);

    int32_t sum = 0;
    int32_t peak = 0;
    int32_t len = 0;
    for (auto& [id, rec] : mBufferHistory) {
        sum += rec.sum;
        len += rec.history.size();
        if (peak < rec.peak) {
            peak = rec.peak;
        }
    }

    mStats.framesPeakRoundtripLatency = peak;
    mStats.framesAvgRoundtripLatency = (double)sum / len;
    return mStats;
}


Result<void> CameraUsageStats::writeStats() const {
    AutoMutex lock(mMutex);

    // Reports the usage statistics before the destruction
    // EvsUsageStatsReported atom is defined in
    // frameworks/base/cmds/statsd/src/atoms.proto
    const auto duration = android::uptimeMillis() - mTimeCreatedMs;
    android::util::stats_write(android::util::EVS_USAGE_STATS_REPORTED,
                               mId,
                               mStats.peakClientsCount,
                               mStats.erroneousEventsCount,
                               mStats.framesFirstRoundtripLatency,
                               mStats.framesAvgRoundtripLatency,
                               mStats.framesPeakRoundtripLatency,
                               mStats.framesReceived,
                               mStats.framesIgnored,
                               mStats.framesSkippedToSync,
                               duration);
    return {};
}


std::string CameraUsageStats::toString(const CameraUsageStatsRecord& record, const char* indent) {
    return record.toString(indent);
}

} // namespace implementation
} // namespace V1_1
} // namespace evs
} // namespace automotive
} // namespace android

