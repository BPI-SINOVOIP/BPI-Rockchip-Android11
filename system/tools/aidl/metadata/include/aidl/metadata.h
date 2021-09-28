/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <string>
#include <vector>

namespace android {

struct AidlInterfaceMetadata {
  // name of module defining package
  std::string name;

  // stability of interface (e.g. "vintf")
  std::string stability;

  // list of types e.g. android.hardware.foo::IFoo
  std::vector<std::string> types;

  // list of all hashes
  std::vector<std::string> hashes;

  static std::vector<AidlInterfaceMetadata> all();
};

}  // namespace android
