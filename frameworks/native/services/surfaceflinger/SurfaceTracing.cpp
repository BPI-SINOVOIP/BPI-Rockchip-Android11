/*
 * Copyright 2017 The Android Open Source Project
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

// TODO(b/129481165): remove the #pragma below and fix conversion issues
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#undef LOG_TAG
#define LOG_TAG "SurfaceTracing"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include "SurfaceTracing.h"
#include <SurfaceFlinger.h>

#include <android-base/file.h>
#include <android-base/stringprintf.h>
#include <log/log.h>
#include <utils/SystemClock.h>
#include <utils/Trace.h>

namespace android {

SurfaceTracing::SurfaceTracing(SurfaceFlinger& flinger)
      : mFlinger(flinger), mSfLock(flinger.mTracingLock) {}

void SurfaceTracing::mainLoop() {
    bool enabled = addFirstEntry();
    while (enabled) {
        LayersTraceProto entry = traceWhenNotified();
        enabled = addTraceToBuffer(entry);
    }
}

bool SurfaceTracing::addFirstEntry() {
    LayersTraceProto entry;
    {
        std::scoped_lock lock(mSfLock);
        entry = traceLayersLocked("tracing.enable");
    }
    return addTraceToBuffer(entry);
}

LayersTraceProto SurfaceTracing::traceWhenNotified() {
    std::unique_lock<std::mutex> lock(mSfLock);
    mCanStartTrace.wait(lock);
    android::base::ScopedLockAssertion assumeLock(mSfLock);
    LayersTraceProto entry = traceLayersLocked(mWhere);
    mTracingInProgress = false;
    mMissedTraceEntries = 0;
    lock.unlock();
    return entry;
}

bool SurfaceTracing::addTraceToBuffer(LayersTraceProto& entry) {
    std::scoped_lock lock(mTraceLock);
    mBuffer.emplace(std::move(entry));
    if (mWriteToFile) {
        writeProtoFileLocked();
        mWriteToFile = false;
    }
    return mEnabled;
}

void SurfaceTracing::notify(const char* where) {
    std::scoped_lock lock(mSfLock);
    notifyLocked(where);
}

void SurfaceTracing::notifyLocked(const char* where) {
    mWhere = where;
    if (mTracingInProgress) {
        mMissedTraceEntries++;
    }
    mTracingInProgress = true;
    mCanStartTrace.notify_one();
}

void SurfaceTracing::writeToFileAsync() {
    std::scoped_lock lock(mTraceLock);
    mWriteToFile = true;
    mCanStartTrace.notify_one();
}

void SurfaceTracing::LayersTraceBuffer::reset(size_t newSize) {
    // use the swap trick to make sure memory is released
    std::queue<LayersTraceProto>().swap(mStorage);
    mSizeInBytes = newSize;
    mUsedInBytes = 0U;
}

void SurfaceTracing::LayersTraceBuffer::emplace(LayersTraceProto&& proto) {
    auto protoSize = proto.ByteSize();
    while (mUsedInBytes + protoSize > mSizeInBytes) {
        if (mStorage.empty()) {
            return;
        }
        mUsedInBytes -= mStorage.front().ByteSize();
        mStorage.pop();
    }
    mUsedInBytes += protoSize;
    mStorage.emplace();
    mStorage.back().Swap(&proto);
}

void SurfaceTracing::LayersTraceBuffer::flush(LayersTraceFileProto* fileProto) {
    fileProto->mutable_entry()->Reserve(mStorage.size());

    while (!mStorage.empty()) {
        auto entry = fileProto->add_entry();
        entry->Swap(&mStorage.front());
        mStorage.pop();
    }
}

bool SurfaceTracing::enable() {
    std::scoped_lock lock(mTraceLock);

    if (mEnabled) {
        return false;
    }

    mBuffer.reset(mBufferSize);
    mEnabled = true;
    mThread = std::thread(&SurfaceTracing::mainLoop, this);
    return true;
}

status_t SurfaceTracing::writeToFile() {
    std::thread thread;
    {
        std::scoped_lock lock(mTraceLock);
        thread = std::move(mThread);
    }
    thread.join();
    return mLastErr;
}

bool SurfaceTracing::disable() {
    std::scoped_lock lock(mTraceLock);

    if (!mEnabled) {
        return false;
    }

    mEnabled = false;
    mWriteToFile = true;
    mCanStartTrace.notify_all();
    return true;
}

bool SurfaceTracing::isEnabled() const {
    std::scoped_lock lock(mTraceLock);
    return mEnabled;
}

void SurfaceTracing::setBufferSize(size_t bufferSizeInByte) {
    std::scoped_lock lock(mTraceLock);
    mBufferSize = bufferSizeInByte;
    mBuffer.setSize(bufferSizeInByte);
}

void SurfaceTracing::setTraceFlags(uint32_t flags) {
    std::scoped_lock lock(mSfLock);
    mTraceFlags = flags;
}

LayersTraceProto SurfaceTracing::traceLayersLocked(const char* where) {
    ATRACE_CALL();

    LayersTraceProto entry;
    entry.set_elapsed_realtime_nanos(elapsedRealtimeNano());
    entry.set_where(where);
    LayersProto layers(mFlinger.dumpDrawingStateProto(mTraceFlags));

    if (flagIsSetLocked(SurfaceTracing::TRACE_EXTRA)) {
        mFlinger.dumpOffscreenLayersProto(layers);
    }
    entry.mutable_layers()->Swap(&layers);

    if (mTraceFlags & SurfaceTracing::TRACE_HWC) {
        std::string hwcDump;
        mFlinger.dumpHwc(hwcDump);
        entry.set_hwc_blob(hwcDump);
    }
    if (!flagIsSetLocked(SurfaceTracing::TRACE_COMPOSITION)) {
        entry.set_excludes_composition_state(true);
    }
    entry.set_missed_entries(mMissedTraceEntries);

    return entry;
}

void SurfaceTracing::writeProtoFileLocked() {
    ATRACE_CALL();

    LayersTraceFileProto fileProto;
    std::string output;

    fileProto.set_magic_number(uint64_t(LayersTraceFileProto_MagicNumber_MAGIC_NUMBER_H) << 32 |
                               LayersTraceFileProto_MagicNumber_MAGIC_NUMBER_L);
    mBuffer.flush(&fileProto);
    mBuffer.reset(mBufferSize);

    if (!fileProto.SerializeToString(&output)) {
        ALOGE("Could not save the proto file! Permission denied");
        mLastErr = PERMISSION_DENIED;
    }

    // -rw-r--r--
    const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    if (!android::base::WriteStringToFile(output, kDefaultFileName, mode, getuid(), getgid(),
                                          true)) {
        ALOGE("Could not save the proto file! There are missing fields");
        mLastErr = PERMISSION_DENIED;
    }

    mLastErr = NO_ERROR;
}

void SurfaceTracing::dump(std::string& result) const {
    std::scoped_lock lock(mTraceLock);
    base::StringAppendF(&result, "Tracing state: %s\n", mEnabled ? "enabled" : "disabled");
    base::StringAppendF(&result, "  number of entries: %zu (%.2fMB / %.2fMB)\n",
                        mBuffer.frameCount(), float(mBuffer.used()) / float(1_MB),
                        float(mBuffer.size()) / float(1_MB));
}

} // namespace android

// TODO(b/129481165): remove the #pragma below and fix conversion issues
#pragma clang diagnostic pop // ignored "-Wconversion"
