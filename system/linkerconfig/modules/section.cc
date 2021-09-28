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
#include "linkerconfig/section.h"

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <utility>

#include <android-base/result.h>
#include <android-base/strings.h>

#include "linkerconfig/log.h"

using android::base::Join;
using android::base::Result;

namespace android {
namespace linkerconfig {
namespace modules {
void Section::WriteConfig(ConfigWriter& writer) {
  writer.WriteLine("[" + name_ + "]");

  std::sort(namespaces_.begin(),
            namespaces_.end(),
            [](const auto& lhs, const auto& rhs) -> bool {
              // make "default" a smallest one
              if (lhs.GetName() == "default") return true;
              if (rhs.GetName() == "default") return false;
              return lhs.GetName() < rhs.GetName();
            });

  if (namespaces_.size() > 1) {
    std::vector<std::string> additional_namespaces;
    for (const auto& ns : namespaces_) {
      if (ns.GetName() != "default") {
        additional_namespaces.push_back(ns.GetName());
      }
    }
    writer.WriteLine("additional.namespaces = " +
                     Join(additional_namespaces, ','));
  }

  for (auto& ns : namespaces_) {
    ns.WriteConfig(writer);
  }
}

Result<void> Section::Resolve(const BaseContext& ctx) {
  std::unordered_map<std::string, std::string> providers;
  for (auto& ns : namespaces_) {
    for (const auto& lib : ns.GetProvides()) {
      if (auto iter = providers.find(lib); iter != providers.end()) {
        return Errorf("duplicate: {} is provided by {} and {} in [{}]",
                      lib,
                      iter->second,
                      ns.GetName(),
                      name_);
      }
      providers[lib] = ns.GetName();
    }
  }

  std::unordered_map<std::string, ApexInfo> candidates_providers;
  for (const auto& apex : ctx.GetApexModules()) {
    for (const auto& lib : apex.provide_libs) {
      candidates_providers[lib] = apex;
    }
  }

  // Reserve enough space for namespace vector which can be increased maximum as
  // much as available APEX modules. Appending new namespaces without reserving
  // enough space from iteration can crash the process.
  namespaces_.reserve(namespaces_.size() + ctx.GetApexModules().size());

  auto iter = namespaces_.begin();
  do {
    auto& ns = *iter;
    for (const auto& lib : ns.GetRequires()) {
      if (auto it = providers.find(lib); it != providers.end()) {
        // If required library can be provided by existing namespaces, link to
        // the namespace.
        ns.GetLink(it->second).AddSharedLib(lib);
      } else if (auto it = candidates_providers.find(lib);
                 it != candidates_providers.end()) {
        // If required library can be provided by a APEX module, create a new
        // namespace with the APEX and add it to this section.
        auto new_ns = ctx.BuildApexNamespace(it->second, false);

        // Update providing library map from the new namespace
        for (const auto& new_lib : new_ns.GetProvides()) {
          if (providers.find(new_lib) == providers.end()) {
            providers[new_lib] = new_ns.GetName();
          }
        }
        ns.GetLink(new_ns.GetName()).AddSharedLib(lib);
        namespaces_.push_back(std::move(new_ns));
      } else if (ctx.IsStrictMode()) {
        return Errorf(
            "not found: {} is required by {} in [{}]", lib, ns.GetName(), name_);
      }
    }
    iter++;
  } while (iter != namespaces_.end());

  return {};
}

Namespace* Section::GetNamespace(const std::string& namespace_name) {
  for (auto& ns : namespaces_) {
    if (ns.GetName() == namespace_name) {
      return &ns;
    }
  }

  return nullptr;
}

std::string Section::GetName() {
  return name_;
}
}  // namespace modules
}  // namespace linkerconfig
}  // namespace android
