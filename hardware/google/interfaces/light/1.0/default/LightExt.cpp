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
#include <android-base/file.h>
#include <hardware/lights.h>
#include <log/log.h>

#include "LightExt.h"

namespace hardware {
namespace google {
namespace light {
namespace V1_0 {
namespace implementation {
using ::android::hardware::light::V2_0::Brightness;

Return<Status> LightExt::applyHBM(bool on) {
  if (!mHasHbmNode) return Status::UNKNOWN;

  /* skip if no change */
  if (mRegHBM == mCurHBM) return Status::SUCCESS;

  if (!android::base::WriteStringToFile((on ? "1" : "0"),
                                        kHighBrightnessModeNode)) {
    ALOGE("write HBM failed!");
    return Status::UNKNOWN;
  }

  mCurHBM = on;
  ALOGI("Set HBM to %d", on);
  return Status::SUCCESS;
}

// Methods from ::android::hardware::light::V2_0::ILight follow.
Return<Status> LightExt::setLight(Type type, const LightState& state) {
  if (type == Type::BACKLIGHT) {
    if (state.brightnessMode == Brightness::LOW_PERSISTENCE) {
      applyHBM(false);
      mVRMode = true;
    } else {
      applyHBM(mRegHBM);
      mVRMode = false;
    }
  }
  return mLight->setLight(type, state);
}

// Methods from ::hardware::google::light::V1_0::ILight follow.
Return<Status> LightExt::setHbm(bool on) {
  /* save the request state */
  mRegHBM = on;

  if (mVRMode) return Status::UNKNOWN;

  Status status = applyHBM(mRegHBM);

  return status;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace light
}  // namespace google
}  // namespace hardware
