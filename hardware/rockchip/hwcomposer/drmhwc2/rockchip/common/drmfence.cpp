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

#include "utils/drmfence.h"

namespace android {

const sp<ReleaseFence> ReleaseFence::NO_FENCE = sp<ReleaseFence>(new ReleaseFence);
const sp<AcquireFence> AcquireFence::NO_FENCE = sp<AcquireFence>(new AcquireFence);

// SyncTimeline:
SyncTimeline::SyncTimeline() noexcept {
    int fd = sw_sync_timeline_create();
    if (fd == -1)
        return;
    bFdInitialized_ = true;
    iFd_ = fd;
}
void SyncTimeline::destroy() {
    if (bFdInitialized_) {
        close(iFd_);
        iFd_ = -1;
        bFdInitialized_ = false;
    }
}
SyncTimeline::~SyncTimeline() {
    destroy();
}
bool SyncTimeline::isValid() const {
    if (bFdInitialized_) {
        int status = fcntl(iFd_, F_GETFD, 0);
        if (status >= 0)
            return true;
        else
            return false;
    }
    else {
        return false;
    }
}
int SyncTimeline::getFd() const {
    return iFd_;
}
int SyncTimeline::IncTimeline() {
    return ++iTimelineCnt_;
}


// Wrapper class for sync fence.
// ReleaseFence
ReleaseFence::~ReleaseFence(){
    destroy();
}
ReleaseFence::ReleaseFence(const int fd, std::string name) noexcept {
    int status = fcntl(fd, F_GETFD, 0);
    if (status < 0)
        return;
    setFd(fd, -1, name);
    bFdInitialized_ = true;
}
ReleaseFence::ReleaseFence(const SyncTimeline &timeline,
          int value,
          const char *name) noexcept {
    std::string autoName = "allocReleaseFence";
    autoName += s_fenceCount;
    s_fenceCount++;
    int fd = sw_sync_fence_create(timeline.getFd(), name ? name : autoName.c_str(), value);
    if (fd == -1)
        return;
    setFd(fd, timeline.getFd(), name);

}

int ReleaseFence::merge(int fd, const char* name){
    if(isValid()){
      std::string autoName = "mergeReleaseFence";
      autoName += s_fenceCount;
      s_fenceCount++;
      return sync_merge(name ? name : autoName.c_str(), getFd(), fd);
    }
    return -1;
}
void ReleaseFence::clearFd() {
    iFd_ = -1;
    bFdInitialized_ = false;
}
void ReleaseFence::destroy() {
    if (isValid()) {
        // ALOGD("rk-debug destroy ReleaseFence fd=%d",iFd_);
        close(iFd_);
        clearFd();
    }
}
bool ReleaseFence::isValid() const {
    if (bFdInitialized_) {
        int status = fcntl(iFd_, F_GETFD, 0);
        if (status >= 0)
            return true;
        else
            return false;
    }
    else {
        return false;
    }
}
int ReleaseFence::getFd() const {
    return iFd_;
}
int ReleaseFence::getSyncTimelineFd() const {
    return iSyncTimelineFd_;
}
std::string ReleaseFence::getName() const{
    return sName_;
}
int ReleaseFence::wait(int timeout) {
    return sync_wait(iFd_, timeout);
}
int ReleaseFence::signal() {
    if(iSyncTimelineFd_ < 0)
      return -1;
    return sw_sync_timeline_inc(iSyncTimelineFd_, 1);
}
std::vector<SyncPointInfo> ReleaseFence::getInfo() const {
    std::vector<SyncPointInfo> fenceInfo;
    struct sync_file_info *info = sync_file_info(getFd());
    if (!info) {
        return fenceInfo;
    }
    const auto fences = sync_get_fence_info(info);
    for (uint32_t i = 0; i < info->num_fences; i++) {
        fenceInfo.push_back(SyncPointInfo{
            fences[i].driver_name,
            fences[i].obj_name,
            fences[i].timestamp_ns,
            fences[i].status});
    }
    sync_file_info_free(info);
    return fenceInfo;
}
int ReleaseFence::getSize() const {
    return getInfo().size();
}
int ReleaseFence::getSignaledCount() const {
    return countWithStatus(1);
}
int ReleaseFence::getActiveCount() const {
    return countWithStatus(0);
}
int ReleaseFence::getErrorCount() const {
    return countWithStatus(-1);
}

std::string ReleaseFence::dump() const {
    std::string output;
    for (auto &info : getInfo()) {
      output += info.driverName + ":" + info.objectName + ":" + std::to_string(info.timeStampNs) + ":state=" + std::to_string(info.status) + "\n";
    }
    return output;
}

void ReleaseFence::setFd(int fd, int sync_timeline_fd, std::string name) {
    // ALOGD("rk-debug ReleaseFence fd=%d timeline=%d name=%s",fd,sync_timeline_fd,name.c_str());
    iFd_ = fd;
    iSyncTimelineFd_ = sync_timeline_fd;
    sName_ = name;
    bFdInitialized_ = true;
}
int ReleaseFence::countWithStatus(int status) const {
    int count = 0;
    for (auto &info : getInfo()) {
        if (info.status == status) {
            count++;
        }
    }
    return count;
}

// AcquireFence
AcquireFence::AcquireFence(const int fd) noexcept {
    int status = fcntl(fd, F_GETFD, 0);
    if (status < 0)
        return;
    setFd(fd);
    bFdInitialized_ = true;
}

int AcquireFence::merge(int fd, const char* name){
    if(isValid()){
      std::string autoName = "mergeAcquireFence";
      autoName += s_fenceCount;
      s_fenceCount++;
      return sync_merge(name ? name : autoName.c_str(), getFd(), fd);
    }
    return -1;
}

AcquireFence::~AcquireFence() {
    destroy();
}

void AcquireFence::destroy() {
    if (isValid()) {
        // ALOGD("rk-debug destroy AcquireFence fd=%d",iFd_);
        close(iFd_);
        clearFd();
    }
}
bool AcquireFence::isValid() const {
    if (bFdInitialized_) {
        int status = fcntl(iFd_, F_GETFD, 0);
        if (status >= 0)
            return true;
        else
            return false;
    }
    else {
        return false;
    }
}

int AcquireFence::getFd() const {
    return iFd_;
}
int AcquireFence::wait(int timeout) {
    if(!isValid()){
      return 0;
    }
    return sync_wait(iFd_, timeout);
}
std::vector<SyncPointInfo> AcquireFence::getInfo() const {
    std::vector<SyncPointInfo> fenceInfo;
    struct sync_file_info *info = sync_file_info(getFd());
    if (!info) {
        return fenceInfo;
    }
    const auto fences = sync_get_fence_info(info);
    for (uint32_t i = 0; i < info->num_fences; i++) {
        fenceInfo.push_back(SyncPointInfo{
            fences[i].driver_name,
            fences[i].obj_name,
            fences[i].timestamp_ns,
            fences[i].status});
    }
    sync_file_info_free(info);
    return fenceInfo;
}
int AcquireFence::getSize() const {
    return getInfo().size();
}
int AcquireFence::getSignaledCount() const {
    return countWithStatus(1);
}
int AcquireFence::getActiveCount() const {
    return countWithStatus(0);
}
int AcquireFence::getErrorCount() const {
    return countWithStatus(-1);
}
void AcquireFence::setFd(int fd) {
    // ALOGD("rk-debug AcquireFence fd=%d",fd);
    iFd_ = fd;
    bFdInitialized_ = true;
}
void AcquireFence::clearFd() {
    iFd_ = -1;
    bFdInitialized_ = false;
}
int AcquireFence::countWithStatus(int status) const {
    int count = 0;
    for (auto &info : getInfo()) {
        if (info.status == status) {
            count++;
        }
    }
    return count;
}
}