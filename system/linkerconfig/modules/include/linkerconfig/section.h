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

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <android-base/result.h>

#include "linkerconfig/basecontext.h"
#include "linkerconfig/configwriter.h"
#include "linkerconfig/namespace.h"

namespace android {
namespace linkerconfig {
namespace modules {

class Section {
 public:
  Section(std::string name, std::vector<Namespace> namespaces)
      : name_(std::move(name)), namespaces_(std::move(namespaces)) {
  }

  Section(const Section&) = delete;
  Section(Section&&) = default;

  void WriteConfig(ConfigWriter& writer);
  std::vector<std::string> GetBinaryPaths();
  std::string GetName();

  android::base::Result<void> Resolve(const BaseContext& ctx);
  Namespace* GetNamespace(const std::string& namespace_name);

  template <class _Function>
  void ForEachNamespaces(_Function f) {
    for (auto& ns : namespaces_) {
      f(ns);
    }
  }

 private:
  const std::string name_;
  std::vector<Namespace> namespaces_;
};
}  // namespace modules
}  // namespace linkerconfig
}  // namespace android
