/*
 * Copyright (C) 2018 The Android Open Source Project
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

#define LOG_TAG "TimeCheck"

#include <utils/Log.h>
#include <mediautils/TimeCheck.h>
#include <mediautils/EventLog.h>
#include "debuggerd/handler.h"

namespace android {

// Audio HAL server pids vector used to generate audio HAL processes tombstone
// when audioserver watchdog triggers.
// We use a lockless storage to avoid potential deadlocks in the context of watchdog
// trigger.
// Protection again simultaneous writes is not needed given one update takes place
// during AudioFlinger construction and other comes necessarily later once the IAudioFlinger
// interface is available.
// The use of an atomic index just guaranties that current vector is fully initialized
// when read.
/* static */
void TimeCheck::accessAudioHalPids(std::vector<pid_t>* pids, bool update) {
    static constexpr int kNumAudioHalPidsVectors = 3;
    static std::vector<pid_t> audioHalPids[kNumAudioHalPidsVectors];
    static std::atomic<int> curAudioHalPids = 0;

    if (update) {
        audioHalPids[(curAudioHalPids++ + 1) % kNumAudioHalPidsVectors] = *pids;
    } else {
        *pids = audioHalPids[curAudioHalPids % kNumAudioHalPidsVectors];
    }
}

/* static */
void TimeCheck::setAudioHalPids(const std::vector<pid_t>& pids) {
    accessAudioHalPids(&(const_cast<std::vector<pid_t>&>(pids)), true);
}

/* static */
std::vector<pid_t> TimeCheck::getAudioHalPids() {
    std::vector<pid_t> pids;
    accessAudioHalPids(&pids, false);
    return pids;
}

/* static */
sp<TimeCheck::TimeCheckThread> TimeCheck::getTimeCheckThread()
{
    static sp<TimeCheck::TimeCheckThread> sTimeCheckThread = new TimeCheck::TimeCheckThread();
    return sTimeCheckThread;
}

TimeCheck::TimeCheck(const char *tag, uint32_t timeoutMs)
    : mEndTimeNs(getTimeCheckThread()->startMonitoring(tag, timeoutMs))
{
}

TimeCheck::~TimeCheck() {
    getTimeCheckThread()->stopMonitoring(mEndTimeNs);
}

TimeCheck::TimeCheckThread::~TimeCheckThread()
{
    AutoMutex _l(mMutex);
    requestExit();
    mMonitorRequests.clear();
    mCond.signal();
}

nsecs_t TimeCheck::TimeCheckThread::startMonitoring(const char *tag, uint32_t timeoutMs) {
    Mutex::Autolock _l(mMutex);
    nsecs_t endTimeNs = systemTime() + milliseconds(timeoutMs);
    for (; mMonitorRequests.indexOfKey(endTimeNs) >= 0; ++endTimeNs);
    mMonitorRequests.add(endTimeNs, tag);
    mCond.signal();
    return endTimeNs;
}

void TimeCheck::TimeCheckThread::stopMonitoring(nsecs_t endTimeNs) {
    Mutex::Autolock _l(mMutex);
    mMonitorRequests.removeItem(endTimeNs);
    mCond.signal();
}

bool TimeCheck::TimeCheckThread::threadLoop()
{
    status_t status = TIMED_OUT;
    {
        AutoMutex _l(mMutex);

        if (exitPending()) {
            return false;
        }

        nsecs_t endTimeNs = INT64_MAX;
        const char *tag = "<unspecified>";
        // KeyedVector mMonitorRequests is ordered so take first entry as next timeout
        if (mMonitorRequests.size() != 0) {
            endTimeNs = mMonitorRequests.keyAt(0);
            tag = mMonitorRequests.valueAt(0);
        }

        const nsecs_t waitTimeNs = endTimeNs - systemTime();
        if (waitTimeNs > 0) {
            status = mCond.waitRelative(mMutex, waitTimeNs);
        }
        if (status != NO_ERROR) {
            // Generate audio HAL processes tombstones and allow time to complete
            // before forcing restart
            std::vector<pid_t> pids = getAudioHalPids();
            if (pids.size() != 0) {
                for (const auto& pid : pids) {
                    ALOGI("requesting tombstone for pid: %d", pid);
                    sigqueue(pid, DEBUGGER_SIGNAL, {.sival_int = 0});
                }
                sleep(1);
            } else {
                ALOGI("No HAL process pid available, skipping tombstones");
            }
            LOG_EVENT_STRING(LOGTAG_AUDIO_BINDER_TIMEOUT, tag);
            LOG_ALWAYS_FATAL("TimeCheck timeout for %s", tag);
        }
    }
    return true;
}

}; // namespace android
