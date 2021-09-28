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

#include "ApiChecker.h"

#include <string>
#include <unordered_map>

using android::base::Result;

namespace {

Result<void> CompareProps(const sysprop::Properties& latest,
                          const sysprop::Properties& current) {
  std::unordered_map<std::string, sysprop::Property> props;

  for (int i = 0; i < current.prop_size(); ++i) {
    const auto& prop = current.prop(i);
    props[prop.api_name()] = prop;
  }

  std::string err;

  bool latest_empty = true;
  for (int i = 0; i < latest.prop_size(); ++i) {
    const auto& latest_prop = latest.prop(i);
    if (latest_prop.deprecated() || latest_prop.scope() == sysprop::Internal) {
      continue;
    }

    latest_empty = false;

    auto itr = props.find(latest_prop.api_name());
    if (itr == props.end()) {
      err += "Prop " + latest_prop.api_name() + " has been removed\n";
      continue;
    }

    const auto& current_prop = itr->second;

    if (latest_prop.type() != current_prop.type()) {
      err += "Type of prop " + latest_prop.api_name() + " has been changed\n";
    }
    // Readonly > Writeonce > ReadWrite
    if (latest_prop.access() > current_prop.access()) {
      err += "Accessibility of prop " + latest_prop.api_name() +
             " has become more restrictive\n";
    }
    // Public < Internal
    if (latest_prop.scope() < current_prop.scope()) {
      err += "Scope of prop " + latest_prop.api_name() +
             " has become more restrictive\n";
    }
    if (latest_prop.prop_name() != current_prop.prop_name()) {
      err += "Underlying property of prop " + latest_prop.api_name() +
             " has been changed\n";
    }
    if (latest_prop.enum_values() != current_prop.enum_values()) {
      err += "Enum values of prop " + latest_prop.api_name() +
             " has been changed\n";
    }
    if (latest_prop.integer_as_bool() != current_prop.integer_as_bool()) {
      err += "Integer-as-bool of prop " + latest_prop.api_name() +
             " has been changed\n";
    }
  }

  if (!latest_empty) {
    if (latest.owner() != current.owner()) {
      err += "owner of module " + latest.module() + " has been changed\n";
    }
  }

  if (err.empty())
    return {};
  else
    return Errorf("{}", err);
}

}  // namespace

Result<void> CompareApis(const sysprop::SyspropLibraryApis& latest,
                         const sysprop::SyspropLibraryApis& current) {
  std::unordered_map<std::string, sysprop::Properties> propsMap;

  for (int i = 0; i < current.props_size(); ++i) {
    propsMap[current.props(i).module()] = current.props(i);
  }

  for (int i = 0; i < latest.props_size(); ++i) {
    // Checking whether propsMap contains latest.props(i)->module() or not
    // is intentionally skipped to handle the case that latest.props(i) has
    // only deprecated properties.
    if (auto res =
            CompareProps(latest.props(i), propsMap[latest.props(i).module()]);
        !res.ok()) {
      return res;
    }
  }

  return {};
}
