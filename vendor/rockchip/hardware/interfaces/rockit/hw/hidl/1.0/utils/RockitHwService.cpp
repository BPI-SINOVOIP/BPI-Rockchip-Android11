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

#define LOG_NDEBUG 0
#define LOG_TAG "RockitHwService"

#include <dlfcn.h>
#include <android-base/logging.h>
#include <hidl/HidlBinderSupport.h>
#include "RTUtils.h"
#include "RockitHwManager.h"
#include "RockitHwService.h"
#include "hwbinder/Parcel.h"
#include "include/RockitHwManager.h"

namespace rockchip {
namespace hardware {
namespace rockit {
namespace hw {
namespace V1_0 {
namespace utils {

using namespace ::android;
using namespace ::android::hardware;

RockitHwService::RockitHwService()
{
    ALOGD("RockitHwService");
}

RockitHwService::~RockitHwService() {
    ALOGD("~RockitHwService");
}

Return<void> RockitHwService::create(create_cb _hidl_cb)
{
    ALOGD("%s", __FUNCTION__);
    sp<IRockitHwInterface> p = new RockitHwManager();
    if (p.get() != NULL) {
        _hidl_cb(Status::OK, p);
        addClient(p);
    } else {
        _hidl_cb(Status::CORRUPTED, p);
    }

    return Void();
}

Return<Status> RockitHwService::destroy(const sp<IRockitHwInterface>& hw)
{
    ALOGD("%s", __FUNCTION__);
    if (hw.get() != NULL) {
    //    hw->destroy();
        removeClient(hw);
        return Status::OK;
    } else {
        return Status::BAD_VALUE;
    }
}

void RockitHwService::addClient(const sp<IRockitHwInterface>& hw)
{
    Mutex::Autolock lock(mLock);
    mClients.add(hw);
}

void RockitHwService::removeClient(const sp<IRockitHwInterface>& hw)
{
    Mutex::Autolock lock(mLock);
    mClients.remove(hw);
}




}  // namespace utils
}  // namespace V1_0
}  // namespace hw
}  // namespace rockit
}  // namespace hardware
}  // namespace rockchip

