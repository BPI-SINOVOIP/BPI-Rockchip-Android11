/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef ANDROID_FRAMEWORKS_ML_NN_RUNTIME_EVENT_H
#define ANDROID_FRAMEWORKS_ML_NN_RUNTIME_EVENT_H

#include <android/sync.h>
#include <utility>

#include "Callbacks.h"
#include "HalInterfaces.h"

namespace android::nn {

class IEvent {
   public:
    virtual ~IEvent() = default;
    virtual void wait() const = 0;
    virtual hal::ErrorStatus getStatus() const = 0;
    virtual int getSyncFenceFd(bool shouldDup) const = 0;
};

// The CallbackEvent wraps ExecutionCallback
class CallbackEvent : public IEvent {
   public:
    CallbackEvent(sp<ExecutionCallback> callback) : kExecutionCallback(std::move(callback)) {
        CHECK(kExecutionCallback != nullptr);
    }

    void wait() const override { kExecutionCallback->wait(); }
    hal::ErrorStatus getStatus() const override { return kExecutionCallback->getStatus(); }
    // Always return -1 as this is not backed by a sync fence.
    int getSyncFenceFd(bool /*should_dup*/) const override { return -1; }

   private:
    const sp<ExecutionCallback> kExecutionCallback;
};

// The SyncFenceEvent wraps sync fence and IFencedExecutionCallback
class SyncFenceEvent : public IEvent {
   public:
    SyncFenceEvent(int sync_fence_fd, const sp<hal::IFencedExecutionCallback>& callback)
        : kFencedExecutionCallback(callback) {
        if (sync_fence_fd > 0) {
            // Dup the provided file descriptor
            mSyncFenceFd = dup(sync_fence_fd);
            CHECK(mSyncFenceFd > 0);
        }
    }

    // Close the fd the event owns.
    ~SyncFenceEvent() { close(mSyncFenceFd); }

    // Use syncWait to wait for the sync fence until the status change.
    void wait() const override { syncWait(mSyncFenceFd, -1); }

    // Get the status of the event.
    // In case of syncWait error, query the dispatch callback for detailed
    // error status.
    hal::ErrorStatus getStatus() const override {
        auto error = hal::ErrorStatus::NONE;
        if (mSyncFenceFd > 0 && syncWait(mSyncFenceFd, -1) != FenceState::SIGNALED) {
            error = hal::ErrorStatus::GENERAL_FAILURE;
            // If there is a callback available, use the callback to get the error code.
            if (kFencedExecutionCallback != nullptr) {
                const hal::Return<void> ret = kFencedExecutionCallback->getExecutionInfo(
                        [&error](hal::ErrorStatus status, hal::Timing, hal::Timing) {
                            error = status;
                        });
                if (!ret.isOk()) {
                    error = hal::ErrorStatus::GENERAL_FAILURE;
                }
            }
        }
        return error;
    }

    // Return the sync fence fd.
    // If shouldDup is true, the caller must close the fd returned:
    //  - When being used internally within NNAPI runtime, set shouldDup to false.
    //  - When being used to return a fd to application code, set shouldDup to
    //  true.
    int getSyncFenceFd(bool shouldDup) const override {
        if (shouldDup) {
            return dup(mSyncFenceFd);
        }
        return mSyncFenceFd;
    }

   private:
    // TODO(b/148423931): used android::base::unique_fd instead.
    int mSyncFenceFd = -1;
    const sp<hal::IFencedExecutionCallback> kFencedExecutionCallback;
};

}  // namespace android::nn

#endif  // ANDROID_FRAMEWORKS_ML_NN_RUNTIME_EVENT_H
