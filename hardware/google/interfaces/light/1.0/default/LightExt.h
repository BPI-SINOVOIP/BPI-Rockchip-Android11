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
#ifndef HARDWARE_GOOGLE_LIGHT_V1_0_LIGHT_H
#define HARDWARE_GOOGLE_LIGHT_V1_0_LIGHT_H

#include <hidl/Status.h>
#include <string>

#include <hidl/MQDescriptor.h>
#include <hardware/google/light/1.0/ILight.h>
#include "Light.h"

namespace hardware {
namespace google {
namespace light {
namespace V1_0 {
namespace implementation {

using ::android::hardware::Return;
using ::android::hardware::light::V2_0::LightState;
using ::android::hardware::light::V2_0::Status;
using ::android::hardware::light::V2_0::Type;
using HwILight = ::android::hardware::light::V2_0::ILight;

constexpr const char kHighBrightnessModeNode[] =
    "/sys/class/backlight/panel0-backlight/hbm_mode";

struct LightExt : public ::hardware::google::light::V1_0::ILight {
  LightExt(HwILight*&& light) : mLight(light) {
    mHasHbmNode = !access(kHighBrightnessModeNode, F_OK);
  };
  // Methods from ::android::hardware::light::V2_0::ILight follow.
  Return<Status> setLight(Type type, const LightState& state) override;
  Return<void> getSupportedTypes(getSupportedTypes_cb _hidl_cb) override {
    return mLight->getSupportedTypes(_hidl_cb);
  }

  // Methods from ::hardware::google::light::V1_0::ILight follow.
  Return<Status> setHbm(bool on) override;

 private:
  std::unique_ptr<HwILight> mLight;
  Return<Status> applyHBM(bool on);
  bool mVRMode = 0;
  bool mRegHBM = 0;
  bool mCurHBM = 0;
  bool mHasHbmNode = false;
};
}  // namespace implementation
}  // namespace V1_0
}  // namespace light
}  // namespace google
}  // namespace hardware

#endif  // HARDWARD_GOOGLE_LIGHT_V1_0_LIGHT_H
