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

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace android {
namespace linkerconfig {
namespace modules {
struct ApexInfo {
  std::string name;
  std::string namespace_name;
  std::string path;
  std::vector<std::string> provide_libs;
  std::vector<std::string> require_libs;
  bool has_bin;
  bool has_lib;

  ApexInfo() = default;  // for std::map::operator[]
  ApexInfo(std::string name, std::string path,
           std::vector<std::string> provide_libs,
           std::vector<std::string> require_libs, bool has_bin, bool has_lib)
      : name(std::move(name)),
        path(std::move(path)),
        provide_libs(std::move(provide_libs)),
        require_libs(std::move(require_libs)),
        has_bin(has_bin),
        has_lib(has_lib) {
    this->namespace_name = this->name;
    std::replace(
        this->namespace_name.begin(), this->namespace_name.end(), '.', '_');
  }
};

std::map<std::string, ApexInfo> ScanActiveApexes(const std::string& root);
}  // namespace modules
}  // namespace linkerconfig
}  // namespace android