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

#include "linkerconfig/environment.h"

#include <unistd.h>

#include "linkerconfig/variables.h"

namespace android {
namespace linkerconfig {
namespace modules {
bool IsLegacyDevice() {
  return (!Variables::GetValue("ro.vndk.version").has_value() &&
          !Variables::GetValue("ro.vndk.lite").has_value()) ||
         Variables::GetValue("ro.treble.enabled") == "false";
}

bool IsVndkLiteDevice() {
  return Variables::GetValue("ro.vndk.lite").value_or("") == "true";
}

bool IsVndkInSystemNamespace() {
  return Variables::GetValue("VNDK_USING_CORE_VARIANT_LIBRARIES").has_value();
}

std::string GetVendorVndkVersion() {
  return Variables::GetValue("ro.vndk.version").value_or("");
}

std::string GetProductVndkVersion() {
  return Variables::GetValue("ro.product.vndk.version").value_or("");
}

bool IsProductVndkVersionDefined() {
  return Variables::GetValue("ro.product.vndk.version").has_value();
}

bool IsRecoveryMode() {
  return access("/system/bin/recovery", F_OK) == 0;
}
}  // namespace modules
}  // namespace linkerconfig
}  // namespace android
