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
#include "linkerconfig/sectionbuilder.h"

#include "linkerconfig/common.h"
#include "linkerconfig/log.h"
#include "linkerconfig/namespace.h"
#include "linkerconfig/namespacebuilder.h"
#include "linkerconfig/section.h"

namespace android {
namespace linkerconfig {
namespace contents {

using modules::Namespace;
using modules::Section;

Section BuildSection(const Context& ctx, const std::string& name,
                     std::vector<Namespace>&& namespaces,
                     const std::vector<std::string>& visible_apexes) {
  // add additional visible APEX namespaces
  for (const auto& apex : ctx.GetApexModules()) {
    if (std::find(visible_apexes.begin(), visible_apexes.end(), apex.name) !=
        visible_apexes.end()) {
      auto ns = ctx.BuildApexNamespace(apex, true);
      namespaces.push_back(std::move(ns));
    }
  }

  // resolve provide/require constraints
  Section section(std::move(name), std::move(namespaces));
  if (auto res = section.Resolve(ctx); !res.ok()) {
    LOG(ERROR) << res.error();
  }

  AddStandardSystemLinks(ctx, &section);
  return section;
}
}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
