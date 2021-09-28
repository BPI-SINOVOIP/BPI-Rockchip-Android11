/*
 * Copyright (C) 2019 The Android Open Source Project
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

#define LOG_TAG "light"

#include <android-base/file.h>
#include <hardware/lights.h>
#include <log/log.h>

#include "LightExt.h"

namespace hardware {
namespace google {
namespace light {
namespace V1_1 {
namespace implementation {
using ::android::hardware::light::V2_0::Brightness;

#define HBM_OFF "0"
#define HBM_ON  "1"
#define HBM_SV  "2"

Return<Status> LightExt::applyHBM() {
  if (!mHasHbmNode) return Status::UNKNOWN;

  /* skip if no change */
  if (mRegHBM == mCurHBM && mRegHBMSV == mCurHBMSV) return Status::SUCCESS;

  //          off
  //        <--------
  //    0,0 --------> 0,1
  //    | ^   sv      ^ |
  // hbm| | off    nop| |nop
  //    V |   sv      | V
  //   1,0 -------->  1,1
  //       <--------
  //          hbm

  // Transition from (mCurHBM, mCurHBMSV) --> (mRegHBM, mRegHBMSV)
  const char *action = NULL;

  if (!mRegHBM && !mRegHBMSV) {
    // to state (0,0)
    action = HBM_OFF;
  } else if (mRegHBMSV) {
    // to state (0,1) or (1,1).
    if (!mCurHBMSV)
      action = HBM_SV;
  } else {
      // to state (1,0)
      action = HBM_ON;
  }

  if (action) {
    if (!android::base::WriteStringToFile(action, kHighBrightnessModeNode)) {
      ALOGE("write %s to HBM sysfs file failed!", action);
      return Status::UNKNOWN;
    }

    ALOGD("write %s to HBM sysfs file succeeded", action);
  }

  mCurHBM = mRegHBM;
  mCurHBMSV = mRegHBMSV;

  return Status::SUCCESS;
}

// Methods from ::android::hardware::light::V2_0::ILight follow.
Return<Status> LightExt::setLight(Type type, const LightState& state) {
  if (type == Type::BACKLIGHT) {
    if (state.brightnessMode == Brightness::LOW_PERSISTENCE) {
      mRegHBM = false;
      mRegHBMSV = false;
      applyHBM();
      mVRMode = true;
    } else {
      // VR has higher priority than HBM. We cannot update HBM status
      // when VR is enabled. Disable VR before apply HBM mode
      Status status = mLight->setLight(type, state);
      mVRMode = false;
      applyHBM();
      return status;
    }
  }
  return mLight->setLight(type, state);
}

// Methods from ::hardware::google::light::V1_0::ILight follow.
Return<Status> LightExt::setHbm(bool on) {
  /* save the request state */
  mRegHBM = on;

  if (mVRMode) return Status::UNKNOWN;

  Status status = applyHBM();

  if (status != Status::SUCCESS)
    mRegHBM = mCurHBM;

  return status;
}

// Methods from ::hardware::google::light::V1_1::ILight follow.
Return<Status> LightExt::setHbmSv(bool on) {
  /* save the request state */
  mRegHBMSV = on;

  if (mVRMode) return Status::UNKNOWN;

  Status status = applyHBM();

  if (status != Status::SUCCESS)
    mRegHBMSV = mCurHBMSV;

  return status;
}

Return<bool> LightExt::getHbmSv() {
  return mCurHBMSV;
}

}  // namespace implementation
}  // namespace V1_1
}  // namespace light
}  // namespace google
}  // namespace hardware
