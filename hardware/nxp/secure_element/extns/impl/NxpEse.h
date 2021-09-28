/******************************************************************************
 *
 *  Copyright (C) 2018 NXP Semiconductors
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#ifndef VENDOR_NXP_NXPNFC_V1_0_NXPNFC_H
#define VENDOR_NXP_NXPNFC_V1_0_NXPNFC_H

#include <hardware/hardware.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <vendor/nxp/nxpese/1.0/INxpEse.h>
#include "hal_nxpese.h"
#include "utils/Log.h"

namespace vendor {
namespace nxp {
namespace nxpese {
namespace V1_0 {
namespace implementation {

using ::android::hidl::base::V1_0::DebugInfo;
using ::android::hidl::base::V1_0::IBase;
using ::vendor::nxp::nxpese::V1_0::INxpEse;
using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;

struct NxpEse : public INxpEse {
  Return<void> ioctl(uint64_t ioctlType, const hidl_vec<uint8_t>& inOutData,
                     ioctl_cb _hidl_cb) override;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace nxpese
}  // namespace nxp
}  // namespace vendor

#endif  // VENDOR_NXP_NXPNFC_V1_0_NXPNFC_H
