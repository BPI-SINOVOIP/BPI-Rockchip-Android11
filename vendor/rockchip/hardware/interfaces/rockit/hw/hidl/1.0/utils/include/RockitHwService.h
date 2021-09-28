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

#ifndef ROCKIT_HIDL_V1_0_UTILS_ROCKITHWSERVICE_H
#define ROCKIT_HIDL_V1_0_UTILS_ROCKITHWSERVICE_H

#include <rockchip/hardware/rockit/hw/1.0/IRockitHwInterface.h>
#include <rockchip/hardware/rockit/hw/1.0/IRockitHwService.h>

#include <hidl/Status.h>
#include <hwbinder/IBinder.h>
#include <hwbinder/Parcel.h>

#include <utils/threads.h>
#include <utils/KeyedVector.h>

namespace rockchip {
namespace hardware {
namespace rockit {
namespace hw {
namespace V1_0 {
namespace utils {

using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::IBinder;
using ::rockchip::hardware::rockit::hw::V1_0::IRockitHwService;
using ::rockchip::hardware::rockit::hw::V1_0::Status;
using ::android::sp;
using ::android::wp;
using namespace android;

struct RockitHwService : public IRockitHwService {
    RockitHwService();

    Return<void>            create(create_cb _hidl_cb);
    Return<Status>          destroy(const sp<IRockitHwInterface>& player);

protected:
    virtual ~RockitHwService();

private:
    void                    addClient(const sp<IRockitHwInterface>& client);
    void                    removeClient(const sp<IRockitHwInterface>& client);

    mutable Mutex                          mLock;
    SortedVector<sp<IRockitHwInterface>>     mClients;
};


}  // namespace utils
}  // namespace V1_0
}  // namespace hw
}  // namespace rockit
}  // namespace hardware
}  // namespace rockchip

#endif  // ROCKIT_HIDL_V1_0_UTILS_ROCKITHWSERVICE_H