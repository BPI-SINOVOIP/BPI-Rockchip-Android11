/*
 * Copyright (C) 2016 The Android Open Source Project
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
#ifndef DRM_FENCE_H_
#define DRM_FENCE_H_
#include "rockchip/utils/drmdebug.h"

#include <android/sync.h>
#include <libsync/sw_sync.h>
#include <utils/RefBase.h>
#include <vector>
#include <queue>
#include <unistd.h>
#include <fcntl.h>

namespace android {

// C++ wrapper class for sync timeline.
class SyncTimeline {
public:
    SyncTimeline() noexcept;
    ~SyncTimeline();
    SyncTimeline(const SyncTimeline &) = delete;
    SyncTimeline& operator=(SyncTimeline&) = delete;
    void destroy();
    bool isValid() const;
    int getFd() const;
    int IncTimeline();
private:
    int iFd_ = -1;
    bool bFdInitialized_ = false;
    int iTimelineCnt_ = 0;
};

struct SyncPointInfo {
    std::string driverName;
    std::string objectName;
    uint64_t timeStampNs;
    int status; // 1 sig, 0 active, neg is err
};

static int s_fenceCount = 0;

// Wrapper class for sync fence.
class ReleaseFence : public LightRefBase<ReleaseFence> {
public:
    static const sp<ReleaseFence> NO_FENCE;
    ReleaseFence(){};
    ~ReleaseFence();

    ReleaseFence(const ReleaseFence& rhs) = delete;
    ReleaseFence& operator=(const ReleaseFence& rhs) = delete;
    ReleaseFence(ReleaseFence&& rhs) = delete;
    ReleaseFence& operator=(ReleaseFence&& rhs) = delete;

    ReleaseFence(const int fd, std::string name = "UnKnow") noexcept;
    ReleaseFence(const SyncTimeline &timeline,
              int value,
              const char *name = nullptr) noexcept;

    void clearFd();
    void destroy();
    bool isValid() const;
    int merge(int fd, const char* name = nullptr);
    int wait(int timeout = -1);
    int signal();
    int getFd() const;
    int getSyncTimelineFd() const;
    std::vector<SyncPointInfo> getInfo() const;
    int getSize() const;
    int getSignaledCount() const;
    int getActiveCount() const;
    int getErrorCount() const;
    std::string dump() const;
    std::string getName() const;
private:
    // Only allow instantiation using ref counting.
    friend class LightRefBase<ReleaseFence>;
    void setFd(int fd, int sync_timeline_fd, std::string name = "UnKnow");
    int countWithStatus(int status) const;
    int iFd_ = -1;
    int iSyncTimelineFd_ = -1;
    bool bFdInitialized_ = false;
    std::string sName_;
};

// Wrapper class for sync fence.
class AcquireFence : public LightRefBase<AcquireFence> {
public:
    static const sp<AcquireFence> NO_FENCE;
    AcquireFence(){};
    ~AcquireFence();

    AcquireFence(const AcquireFence& rhs) = delete;
    AcquireFence& operator=(const AcquireFence& rhs) = delete;
    AcquireFence(AcquireFence&& rhs) = delete;
    AcquireFence& operator=(AcquireFence&& rhs) = delete;
    AcquireFence(const int fd) noexcept;

    void destroy();
    bool isValid() const;
    int getFd() const;
    int merge(int fd, const char* name = nullptr);
    int wait(int timeout = -1);
    std::vector<SyncPointInfo> getInfo() const;
    int getSize() const;
    int getSignaledCount() const;
    int getActiveCount() const;
    int getErrorCount() const;
private:
    // Only allow instantiation using ref counting.
    friend class LightRefBase<AcquireFence>;
    void setFd(int fd);
    void clearFd();
    int countWithStatus(int status) const;
    int iFd_ = -1;
    bool bFdInitialized_ = false;
};

// The semantics of the fences returned by the device differ between
// hwc1.set() and hwc2.present(). Read hwcomposer.h and hwcomposer2.h
// for more information.
//
// Release fences in hwc1 are obtained on set() for a frame n and signaled
// when the layer buffer is not needed for read operations anymore
// (typically on frame n+1). In HWC2, release fences are obtained with a
// special call after present() for frame n. These fences signal
// on frame n: More specifically, the fence for a given buffer provided in
// frame n will signal when the prior buffer is no longer required.
//
// A retire fence (HWC1) is signaled when a composition is replaced
// on the panel whereas a present fence (HWC2) is signaled when a
// composition starts to be displayed on a panel.
//
// The HWC2to1Adapter emulates the new fence semantics for a frame
// n by returning the fence from frame n-1. For frame 0, the adapter
// returns NO_FENCE.
class DeferredRetireFence {
    public:
        DeferredRetireFence()
          : mFences({ReleaseFence::NO_FENCE, ReleaseFence::NO_FENCE}) {}

        void add(int32_t fenceFd, std::string name) {
            mFences.emplace(new ReleaseFence(fenceFd, name));
            mFences.pop();
        }

        const sp<ReleaseFence> &get() const {
            return mFences.front();
        }
        const sp<ReleaseFence> &get_back() const {
            return mFences.back();
        }
    private:
        // There are always two fences in this queue.
        std::queue<sp<ReleaseFence>> mFences;
};

class DeferredReleaseFence {
    public:
        DeferredReleaseFence()
          : mFences({ReleaseFence::NO_FENCE, ReleaseFence::NO_FENCE}) {}

        void add(sp<ReleaseFence> rf) {
            mFences.push(rf);
            mFences.pop();
        }

        const sp<ReleaseFence> &get() const {
            return mFences.front();
        }

        const sp<ReleaseFence> &get_back() const {
            return mFences.back();
        }
    private:
        // There are always two fences in this queue.
        std::queue<sp<ReleaseFence>> mFences;
};
} // namespace android
#endif