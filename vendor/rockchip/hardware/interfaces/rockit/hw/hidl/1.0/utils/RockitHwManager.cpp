/*
 * Copyright 2019 2019 Rockchip Electronics Co. LTD
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

//#define LOG_NDEBUG 0
#define LOG_TAG "RockitHwManager"

#include <dlfcn.h>
#include <android-base/logging.h>
#include "RockitHwManager.h"
#include <hidl/HidlBinderSupport.h>
#include "RTUtils.h"
#include "hwbinder/Parcel.h"
#include <rockchip/hardware/rockit/hw/1.0/types.h>
#include "hw/mpi/RockitHwMpi.h"

namespace rockchip {
namespace hardware {
namespace rockit {
namespace hw {
namespace V1_0 {
namespace utils {

using namespace ::android;
using namespace ::android::hardware;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWType;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWBuffer;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWBufferList;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWCtrCmd;


static Status int2Status(int ret) {
    if (ret == 0)
        return Status::OK;
    return Status::BAD_VALUE;
}

RockitHwManager::RockitHwManager() {
    mImpl = NULL;
}

RockitHwManager::~RockitHwManager() {
    if (mImpl != NULL) {
        mImpl->flush();
        mImpl->reset();
        delete mImpl;
        mImpl = NULL;
    }
}

Return<Status> RockitHwManager::init(RockitHWType type, const RockitHWParamPairs& param) {
    RockitHwInterface* impl = NULL;
    switch (type) {
        case RockitHWType::HW_DECODER_MPI:
            impl = new RockitHwMpi();
            impl->init(param);

            mImpl = impl;
            break;
        default:
            ALOGD("%s type = %d not support, add codes here", __FUNCTION__, type);
            break;
    }
    return Status::OK;
}

Return<Status> RockitHwManager::enqueue(const RockitHWBuffer& buffer) {
    (void)buffer;
    int result = mImpl->enqueue(buffer);
    return int2Status(result);
}

Return<void> RockitHwManager::dequeue(dequeue_cb _hidl_cb) {
    RockitHWBuffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    int result = mImpl->dequeue(buffer);
    Status status = Status::OK;
    if (result < 0) {
        status = Status::BAD_VALUE;
    }

    _hidl_cb(status, buffer);
    return Void();
}

Return<Status> RockitHwManager::commitBuffer(const RockitHWBuffer& buffer) {
    if (mImpl != NULL) {
        int ret = mImpl->commitBuffer(buffer);
        return int2Status(ret);
    }
    return Status::BAD_VALUE;
}

Return<Status> RockitHwManager::giveBackBuffer(const RockitHWBuffer& buffer) {
    if (mImpl != NULL) {
        int ret = mImpl->giveBackBuffer(buffer);
        return int2Status(ret);
    }
    return Status::BAD_VALUE;
}

Return<Status> RockitHwManager::process(const RockitHWBufferList& list) {
    (void)list;
    return Status::OK;
}

Return<Status> RockitHwManager::reset() {
    if (mImpl != NULL) {
        int ret = mImpl->reset();
        return int2Status(ret);
    }
    return Status::BAD_VALUE;
}

Return<Status> RockitHwManager::flush() {
    if (mImpl != NULL) {
        int ret = mImpl->flush();
        return int2Status(ret);
    }
    return Status::BAD_VALUE;
}

Return<Status> RockitHwManager::control(uint32_t cmd, const RockitHWParamPairs& param) {
    if (mImpl != NULL) {
        int ret = mImpl->control(cmd, param);
        return int2Status(ret);
    }
    return Status::BAD_VALUE;
}

Return<void> RockitHwManager::query(uint32_t cmd, query_cb _hidl_cb) {
    RockitHWParamPairs reply;
    memset(&reply, 0, sizeof(RockitHWParamPairs));
    int ret = -1;
    Status status = Status::BAD_VALUE;
    if (mImpl != NULL) {
        ret = mImpl->query(cmd, reply);
        if (ret != -1) {
            status = Status::OK;
        }
    }

    _hidl_cb(status, reply);
    return Void();
}


}  // namespace utils
}  // namespace V1_0
}  // namespace hw
}  // namespace rockit
}  // namespace hardware
}  // namespace rockchip

