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

#define LOG_TAG "hwc-platform"

#include "platform.h"
#include "drmdevice.h"

#include <log/log.h>

namespace android {
std::tuple<int, std::vector<DrmCompositionPlane>> Planner::TryHwcPolicy(
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc, bool gles_policy) {
  std::vector<DrmCompositionPlane> composition;
  int ret = -1;
  // Go through the provisioning stages and provision planes
  for (auto &i : stages_) {
    if(i->SupportPlatform(crtc->get_soc_id())){
      switch(crtc->get_soc_id()){
        // Before E.C.O.
        case 0x3566:
        case 0x3568:
          ret = i->TryHwcPolicy(&composition, layers, crtc, true);
          if (ret) {
            ALOGE("Failed provision stage with ret %d", ret);
            return std::make_tuple(ret, std::vector<DrmCompositionPlane>());
          }
          break;
        // After E.C.O.
        case 0x3566a:
        case 0x3568a:
          ret = i->TryHwcPolicy(&composition, layers, crtc, gles_policy);
          if (ret) {
            ALOGE("Failed provision stage with ret %d", ret);
            return std::make_tuple(ret, std::vector<DrmCompositionPlane>());
          }
          break;
        default:
            ALOGE("Failed provision stage with ret %d", ret);
            return std::make_tuple(ret, std::vector<DrmCompositionPlane>());
          break;
      }
    }
  }

  return std::make_tuple(ret, std::move(composition));
}

}  // namespace android
