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

inline void MockVndkVariables(std::string partition, std::string vndk_ver) {
  using android::linkerconfig::modules::Variables;

  Variables::AddValue(partition + "_VNDK_VERSION", vndk_ver);
  Variables::AddValue("LLNDK_LIBRARIES_" + partition, "llndk_libraries");
  Variables::AddValue("PRIVATE_LLNDK_LIBRARIES_" + partition,
                      "private_llndk_libraries");
  Variables::AddValue("VNDK_SAMEPROCESS_LIBRARIES_" + partition,
                      "vndk_sameprocess_libraries");
  Variables::AddValue("VNDK_CORE_LIBRARIES_" + partition, "vndk_core_libraries");
  Variables::AddValue("SANITIZER_DEFAULT_" + partition,
                      "sanitizer_default_libraries");
}

inline void MockVariables(std::string vndk_ver = "Q") {
  using android::linkerconfig::modules::Variables;

  MockVndkVariables("VENDOR", vndk_ver);
  Variables::AddValue("ro.vndk.version", vndk_ver);

  MockVndkVariables("PRODUCT", vndk_ver);
  Variables::AddValue("ro.product.vndk.version", vndk_ver);

  Variables::AddValue("SYSTEM_EXT", "/system_ext");
  Variables::AddValue("PRODUCT", "/procut");

  Variables::AddValue("VNDK_USING_CORE_VARIANT_LIBRARIES",
                      "vndk_using_core_variant_libraries");
  Variables::AddValue("STUB_LIBRARIES", "stub_libraries");
  Variables::AddValue("SANITIZER_RUNTIME_LIBRARIES",
                      "sanitizer_runtime_libraries");
}

inline void MockVnkdLite() {
  android::linkerconfig::modules::Variables::AddValue("ro.vndk.lite", "true");
}