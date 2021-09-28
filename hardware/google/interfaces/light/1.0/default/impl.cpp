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

#include <hardware/lights.h>
#include <hidl/LegacySupport.h>
#include <hardware/google/light/1.0/ILight.h>
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

using hardware::google::light::V1_0::ILight;
using hardware::google::light::V1_0::implementation::LightExt;

extern "C" ILight *HIDL_FETCH_ILight(const char * /*instance*/) {
  return new LightExt{
      android::hardware::light::V2_0::implementation::HIDL_FETCH_ILight(
          nullptr)};
}
