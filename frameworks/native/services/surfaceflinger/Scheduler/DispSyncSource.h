/*
 * Copyright 2018 The Android Open Source Project
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
#pragma once

#include <mutex>
#include <string>

#include "DispSync.h"
#include "EventThread.h"
#include "TracedOrdinal.h"

namespace android {

class DispSyncSource final : public VSyncSource, private DispSync::Callback {
public:
    DispSyncSource(DispSync* dispSync, nsecs_t phaseOffset, bool traceVsync, const char* name);

    ~DispSyncSource() override = default;

    // The following methods are implementation of VSyncSource.
    const char* getName() const override { return mName; }
    void setVSyncEnabled(bool enable) override;
    void setCallback(VSyncSource::Callback* callback) override;
    void setPhaseOffset(nsecs_t phaseOffset) override;

    void dump(std::string&) const override;

private:
    // The following method is the implementation of the DispSync::Callback.
    void onDispSyncEvent(nsecs_t when, nsecs_t expectedVSyncTimestamp) override;

    const char* const mName;
    TracedOrdinal<int> mValue;

    const bool mTraceVsync;
    const std::string mVsyncOnLabel;
    nsecs_t mLastCallbackTime GUARDED_BY(mVsyncMutex) = 0;

    DispSync* mDispSync;

    std::mutex mCallbackMutex;
    VSyncSource::Callback* mCallback GUARDED_BY(mCallbackMutex) = nullptr;

    mutable std::mutex mVsyncMutex;
    TracedOrdinal<nsecs_t> mPhaseOffset GUARDED_BY(mVsyncMutex);
    bool mEnabled GUARDED_BY(mVsyncMutex) = false;
};

} // namespace android
