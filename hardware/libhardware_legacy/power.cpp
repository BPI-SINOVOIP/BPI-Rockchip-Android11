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

#define LOG_TAG "power"
#define ATRACE_TAG ATRACE_TAG_POWER

#include <hardware_legacy/power.h>
#include <wakelock/wakelock.h>

#include <android-base/logging.h>
#include <android/system/suspend/1.0/ISystemSuspend.h>
#include <utils/Trace.h>

#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

using android::sp;
using android::system::suspend::V1_0::ISystemSuspend;
using android::system::suspend::V1_0::IWakeLock;
using android::system::suspend::V1_0::WakeLockType;

static std::mutex gLock;
static std::unordered_map<std::string, sp<IWakeLock>> gWakeLockMap;

static const sp<ISystemSuspend>& getSystemSuspendServiceOnce() {
    static sp<ISystemSuspend> suspendService = ISystemSuspend::getService();
    return suspendService;
}

int acquire_wake_lock(int, const char* id) {
    ATRACE_CALL();
    const auto& suspendService = getSystemSuspendServiceOnce();
    if (!suspendService) {
        LOG(ERROR) << "ISystemSuspend::getService() failed.";
        return -1;
    }

    std::lock_guard<std::mutex> l{gLock};
    if (!gWakeLockMap[id]) {
        auto ret = suspendService->acquireWakeLock(WakeLockType::PARTIAL, id);
        // It's possible that during device shutdown SystemSuspend service has already exited. In
        // these situations HIDL calls to it will result in a DEAD_OBJECT transaction error. We
        // check for DEAD_OBJECT so that libpower clients can shutdown cleanly.
        if (ret.isDeadObject()) {
            return -1;
        } else {
            gWakeLockMap[id] = ret;
        }
    }
    return 0;
}

int release_wake_lock(const char* id) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> l{gLock};
    if (gWakeLockMap[id]) {
        // Ignore errors on release() call since hwbinder driver will clean up the underlying object
        // once we clear the corresponding strong pointer.
        auto ret = gWakeLockMap[id]->release();
        if (!ret.isOk()) {
            LOG(ERROR) << "IWakeLock::release() call failed: " << ret.description();
        }
        gWakeLockMap[id].clear();
        return 0;
    }
    return -1;
}

namespace android {
namespace wakelock {

class WakeLock::WakeLockImpl {
  public:
    WakeLockImpl(const std::string& name);
    ~WakeLockImpl();

  private:
    sp<IWakeLock> mWakeLock;
};

WakeLock::WakeLock(const std::string& name) : mImpl(std::make_unique<WakeLockImpl>(name)) {}

WakeLock::~WakeLock() = default;

WakeLock::WakeLockImpl::WakeLockImpl(const std::string& name) : mWakeLock(nullptr) {
    static sp<ISystemSuspend> suspendService = ISystemSuspend::getService();
    mWakeLock = suspendService->acquireWakeLock(WakeLockType::PARTIAL, name);
}

WakeLock::WakeLockImpl::~WakeLockImpl() {
    auto ret = mWakeLock->release();
    if (!ret.isOk()) {
        LOG(ERROR) << "IWakeLock::release() call failed: " << ret.description();
    }
}

}  // namespace wakelock
}  // namespace android
