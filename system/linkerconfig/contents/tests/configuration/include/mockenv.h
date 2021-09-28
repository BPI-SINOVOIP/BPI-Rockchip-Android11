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
#pragma once

#include "linkerconfig/variables.h"

inline void MockGenericVariables() {
  android::linkerconfig::modules::Variables::AddValue("VENDOR_VNDK_VERSION",
                                                      "99");
  android::linkerconfig::modules::Variables::AddValue("PRODUCT_VNDK_VERSION",
                                                      "99");
  android::linkerconfig::modules::Variables::AddValue("PRODUCT", "product");
  android::linkerconfig::modules::Variables::AddValue("SYSTEM_EXT",
                                                      "system_ext");
  android::linkerconfig::modules::Variables::AddValue("LLNDK_LIBRARIES_VENDOR",
                                                      "llndk_libraries");
  android::linkerconfig::modules::Variables::AddValue("LLNDK_LIBRARIES_PRODUCT",
                                                      "llndk_libraries");
  android::linkerconfig::modules::Variables::AddValue(
      "SANITIZER_RUNTIME_LIBRARIES", "sanitizer_runtime_libraries");
  android::linkerconfig::modules::Variables::AddValue(
      "PRIVATE_LLNDK_LIBRARIES_VENDOR", "private_llndk_libraries");
  android::linkerconfig::modules::Variables::AddValue(
      "PRIVATE_LLNDK_LIBRARIES_PRODUCT", "private_llndk_libraries");
  android::linkerconfig::modules::Variables::AddValue(
      "VNDK_SAMEPROCESS_LIBRARIES_VENDOR", "vndk_sameprocess_libraries");
  android::linkerconfig::modules::Variables::AddValue(
      "VNDK_SAMEPROCESS_LIBRARIES_PRODUCT", "vndk_sameprocess_libraries");
  android::linkerconfig::modules::Variables::AddValue(
      "VNDK_CORE_LIBRARIES_VENDOR", "vndk_core_libraries");
  android::linkerconfig::modules::Variables::AddValue(
      "VNDK_CORE_LIBRARIES_PRODUCT", "vndk_core_libraries");
  android::linkerconfig::modules::Variables::AddValue(
      "VNDK_USING_CORE_VARIANT_LIBRARIES", "");
  android::linkerconfig::modules::Variables::AddValue("STUB_LIBRARIES",
                                                      "stub_libraries");
  android::linkerconfig::modules::Variables::AddValue(
      "SANITIZER_DEFAULT_VENDOR", "sanitizer_default_libraries");
  android::linkerconfig::modules::Variables::AddValue(
      "SANITIZER_DEFAULT_PRODUCT", "sanitizer_default_libraries");
}

inline void MockVndkVersion(std::string vndk_version) {
  android::linkerconfig::modules::Variables::AddValue("VENDOR_VNDK_VERSION",
                                                      vndk_version);
  android::linkerconfig::modules::Variables::AddValue("PRODUCT_VNDK_VERSION",
                                                      vndk_version);
}

inline void MockVndkUsingCoreVariant() {
  android::linkerconfig::modules::Variables::AddValue(
      "VNDK_USING_CORE_VARIANT_LIBRARIES", "vndk_using_core_variant_libraries");
}

inline void MockVnkdLite() {
  android::linkerconfig::modules::Variables::AddValue("ro.vndk.lite", "true");
}

inline android::linkerconfig::contents::Context GenerateContextWithVndk() {
  android::linkerconfig::modules::ApexInfo vndk_apex;
  vndk_apex.name = "com.android.vndk.v99";

  android::linkerconfig::contents::Context ctx;
  ctx.AddApexModule(vndk_apex);

  return ctx;
}
