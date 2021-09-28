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
#include "NxpEse.h"
#include "phNxpEse_Api.h"

namespace vendor {
namespace nxp {
namespace nxpese {
namespace V1_0 {
namespace implementation {
using ::android::hardware::hidl_vec;
// Methods from ::vendor::nxp::nxpese::V1_0::INxpEse follow.
Return<void> NxpEse::ioctl(uint64_t ioctlType,
                           const hidl_vec<uint8_t>& inOutData,
                           ioctl_cb _hidl_cb) {
  ese_nxp_IoctlInOutData_t inpOutData;
  ese_nxp_IoctlInOutData_t* pInOutData =
      (ese_nxp_IoctlInOutData_t*)&inOutData[0];

  /*data from proxy->stub is copied to local data which can be updated by
   * underlying HAL implementation since its an inout argument*/
  memcpy(&inpOutData, pInOutData, sizeof(ese_nxp_IoctlInOutData_t));
  ESESTATUS status = phNxpEse_spiIoctl(ioctlType, &inpOutData);

  /*copy data and additional fields indicating status of ioctl operation
   * and context of the caller. Then invoke the corresponding proxy callback*/
  inpOutData.out.ioctlType = ioctlType;
  inpOutData.out.context = pInOutData->inp.context;
  inpOutData.out.result = status;
  hidl_vec<uint8_t> outputData;
  outputData.setToExternal((uint8_t*)&inpOutData.out,
                           sizeof(ese_nxp_ExtnOutputData_t));
  _hidl_cb(outputData);
  return Void();
}

// Methods from ::android::hidl::base::V1_0::IBase follow.

}  // namespace implementation
}  // namespace V1_0
}  // namespace nxpese
}  // namespace nxp
}  // namespace vendor
