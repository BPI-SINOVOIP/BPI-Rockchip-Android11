/*
 * Copyright (C) 2013-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#ifndef _CAMERA3_HAL_SYNCFENCE_H_
#define _CAMERA3_HAL_SYNCFENCE_H_

#include <utils/Errors.h>
#include <sync/sync.h>
#include <sw_sync.h>
#include <fcntl.h>
#include <vector>
#include <string>
#include <cassert>
#include <iostream>
#include <unistd.h>
#include <thread>
#include <poll.h>
#include <mutex>
#include <algorithm>
#include <tuple>
#include <random>
#include <unordered_map>
#include "LogHelper.h"

#if defined(ANDROID_VERSION_ABOVE_10_X)
/* These deprecated declarations were in the legacy android/sync.h. They've been removed to
 * encourage code to move to the modern equivalents. But they are still implemented in libsync.so
 * to avoid breaking existing binaries; as long as that's true we should keep testing them here.
 * That means making local copies of the declarations.
 */
extern "C" {

struct sync_fence_info_data {
    uint32_t len;
    char name[32];
    int32_t status;
    uint8_t pt_info[0];
};

struct sync_pt_info {
    uint32_t len;
    char obj_name[32];
    char driver_name[32];
    int32_t status;
    uint64_t timestamp_ns;
    uint8_t driver_data[0];
};

struct sync_fence_info_data* sync_fence_info(int fd);
struct sync_pt_info* sync_pt_info(struct sync_fence_info_data* info, struct sync_pt_info* itr);
void sync_fence_info_free(struct sync_fence_info_data* info);

}  // extern "C"
#endif

namespace android {
namespace camera2 {

using namespace std;

// C++ wrapper class for sync timeline.
class SyncTimeline {
    int m_fd = -1;
    bool m_fdInitialized = false;
public:
    SyncTimeline(const SyncTimeline &) = delete;
    SyncTimeline& operator=(SyncTimeline&) = delete;
    SyncTimeline() noexcept {
        int fd = sw_sync_timeline_create();
        if (fd == -1)
            return;
        m_fdInitialized = true;
        m_fd = fd;
    }
    void destroy() {
        if (m_fdInitialized) {
            close(m_fd);
            m_fd = -1;
            m_fdInitialized = false;
        }
    }
    ~SyncTimeline() {
        destroy();
    }
    bool isValid() const {
        if (m_fdInitialized) {
            int status = fcntl(m_fd, F_GETFD, 0);
            if (status >= 0)
                return true;
            else
                return false;
        }
        else {
            return false;
        }
    }
    int getFd() const {
        return m_fd;
    }
    int inc(int val = 1) {
        int ret = sw_sync_timeline_inc(m_fd, val);
        if(ret != 0)
            ALOGE("@%s : sw_sync_timeline_inc failed fd:%d, ret:%d", __FUNCTION__, m_fd, ret);
        return ret;
    }
};

struct SyncPointInfo {
    std::string driverName;
    std::string objectName;
    uint64_t timeStampNs;
    int status; // 1 sig, 0 active, neg is err
};

// Wrapper class for sync fence.
class SyncFence {
public:
    bool isValid() const {
        if (m_fdInitialized) {
            int status = fcntl(m_fd, F_GETFD, 0);
            if (status >= 0) {
                return true;
            }
            else {
                LOGE("@%s : fd %d maybe closed, flag:%d", __FUNCTION__, m_fd, status);
                return false;
            }
        }
        else {
            return false;
        }
    }
    SyncFence(int value,
              const char *name = nullptr) noexcept {
        mName = name ? name : "allocFence";
        int fd = sw_sync_fence_create(mTimeline.getFd(), mName.c_str(), value);
        if (fd == -1) {
            LOGE("@%s : sw_sync_fence_create failed", __FUNCTION__);
            return;
        }
        setFd(fd);
    }
    void destroy() {
        if (isValid()) {
            close(m_fd);
            clearFd();
        }
    }
    ~SyncFence() {
        destroy();
    }
    int getFd() const {
        return m_fd;
    }
    int dup() const {
        return ::dup(m_fd);
    }
    int inc(int val = 1) {
        if (!isValid())
            return -1;
        return mTimeline.inc(val);
    }
    int wait(int timeout = -1) {
        return sync_wait(m_fd, timeout);
    }
    vector<SyncPointInfo> getInfo() const {
        struct sync_pt_info *pointInfo = nullptr;
        vector<SyncPointInfo> fenceInfo;
        if (!isValid())
            return fenceInfo;

        sync_fence_info_data *info = sync_fence_info(getFd());
        if (!info) {
            return fenceInfo;
        }
        while ((pointInfo = sync_pt_info(info, pointInfo))) {
            fenceInfo.push_back(SyncPointInfo{
                                pointInfo->driver_name,
                                pointInfo->obj_name,
                                pointInfo->timestamp_ns,
                                pointInfo->status});
        }
        sync_fence_info_free(info);
        return fenceInfo;
    }
    const char * name() {
        return mName.c_str();
    }
    int getSize() const {
        return getInfo().size();
    }
    int getSignaledCount() const {
        return countWithStatus(1);
    }
    int getActiveCount() const {
        return countWithStatus(0);
    }
    int getErrorCount() const {
        return countWithStatus(-1);
    }

private:
    void setFd(int fd) {
        m_fd = fd;
        m_fdInitialized = true;
    }
    void clearFd() {
        m_fd = -1;
        m_fdInitialized = false;
    }
    int countWithStatus(int status) const {
        int count = 0;
        for (auto &info : getInfo()) {
            if (info.status == status) {
                count++;
            }
        }
        return count;
    }
private:
    int m_fd = -1;
    bool m_fdInitialized = false;
    std::string mName;
    SyncTimeline mTimeline;
};

} /* namespace camera2 */
} /* namespace android */
#endif // _CAMERA3_HAL_SYNCFENCE_H_
