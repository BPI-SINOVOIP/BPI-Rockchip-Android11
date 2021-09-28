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
#define LOG_TAG "hardware.google.light@1.1-service"

#include <hardware/lights.h>
#include <hidl/LegacySupport.h>
#include <hardware/google/light/1.1/ILight.h>
#include "LightExt.h"

namespace android {
namespace hardware {
namespace light {
namespace V2_0 {
namespace implementation {

extern ILight* HIDL_FETCH_ILight(const char* /* name */);

}  // namespace implementation
}  // namespace V2_0
}  // namespace light
}  // namespace hardware
}  // namespace android

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using hardware::google::light::V1_1::implementation::LightExt;
using hwLight = hardware::google::light::V1_1::ILight;

int main() {
  configureRpcThreadpool(1, true /*willJoinThreadpool*/);

  android::sp<hwLight> light = new LightExt{
      android::hardware::light::V2_0::implementation::HIDL_FETCH_ILight(
          nullptr)};
  auto ret = light->registerAsService();
  if (ret != android::OK) {
    ALOGE("open light servie failed, ret=%d", ret);
    return 1;
  }
  joinRpcThreadpool();
  return 1;
}
