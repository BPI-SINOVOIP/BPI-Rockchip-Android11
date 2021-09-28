/*
 * Copyright 2019 Rockchip Electronics Co. LTD
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

#ifndef ROCKIT_HIDL_V1_0_UTILS_ROCKITHWMANAGER_H
#define ROCKIT_HIDL_V1_0_UTILS_ROCKITHWMANAGER_H

#include <rockchip/hardware/rockit/hw/1.0/IRockitHwInterface.h>
#include <RockitHwInterface.h>

#include <hidl/Status.h>
#include <hwbinder/IBinder.h>
#include <hwbinder/Parcel.h>


using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::IBinder;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWType;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWBuffer;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWBufferList;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWCtrCmd;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWParamPair;
using ::rockchip::hardware::rockit::hw::V1_0::RockitHWParamPairs;

namespace rockchip {
namespace hardware {
namespace rockit {
namespace hw {
namespace V1_0 {
namespace utils {

struct RockitHwManager : public IRockitHwInterface {
public:
    RockitHwManager();

    virtual Return<Status> init(RockitHWType type, const RockitHWParamPairs& param);

    virtual Return<Status> commitBuffer(const RockitHWBuffer& buffer);

    virtual Return<Status> giveBackBuffer(const RockitHWBuffer& buffer);

    virtual Return<Status> process(const RockitHWBufferList& list);

    virtual Return<Status> enqueue(const RockitHWBuffer& buffer);

    virtual Return<void>   dequeue(dequeue_cb _hidl_cb);

    virtual Return<Status> reset();

    virtual Return<Status> flush();

    virtual Return<Status> control(uint32_t cmd, const RockitHWParamPairs& param);

    virtual Return<void>   query(uint32_t cmd, query_cb _hidl_cb);

protected:
    RockitHwInterface* mImpl;

protected:
    virtual ~RockitHwManager();
};


}  // namespace utils
}  // namespace V1_0
}  // namespace hw
}  // namespace rockit
}  // namespace hardware
}  // namespace rockchip

#endif  // ROCKIT_HIDL_V1_0_UTILS_ROCKITHWMANAGER_H

