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

#include "linkerconfig/context.h"

#include <android-base/strings.h>

#include "linkerconfig/environment.h"
#include "linkerconfig/log.h"
#include "linkerconfig/namespacebuilder.h"
#include "linkerconfig/variables.h"

using android::base::StartsWith;
using android::linkerconfig::modules::ApexInfo;
using android::linkerconfig::modules::Namespace;

namespace android {
namespace linkerconfig {
namespace contents {

bool Context::IsSystemSection() const {
  return current_section_ == SectionType::System;
}

bool Context::IsVendorSection() const {
  return current_section_ == SectionType::Vendor;
}

bool Context::IsProductSection() const {
  return current_section_ == SectionType::Product;
}

bool Context::IsUnrestrictedSection() const {
  return current_section_ == SectionType::Unrestricted;
}

bool Context::IsDefaultConfig() const {
  return current_linkerconfig_type_ == LinkerConfigType::Default;
}

bool Context::IsLegacyConfig() const {
  return current_linkerconfig_type_ == LinkerConfigType::Legacy;
}

// TODO(b/153944540) : Remove VNDK Lite supports
bool Context::IsVndkliteConfig() const {
  return current_linkerconfig_type_ == LinkerConfigType::Vndklite;
}

bool Context::IsRecoveryConfig() const {
  return current_linkerconfig_type_ == LinkerConfigType::Recovery;
}

bool Context::IsApexBinaryConfig() const {
  return current_linkerconfig_type_ == LinkerConfigType::ApexBinary;
}

void Context::SetCurrentSection(SectionType section_type) {
  current_section_ = section_type;
}

std::string Context::GetSystemNamespaceName() const {
  return (IsVendorSection() || IsProductSection() || IsApexBinaryConfig()) &&
                 !IsVndkliteConfig()
             ? "system"
             : "default";
}

void Context::SetCurrentLinkerConfigType(LinkerConfigType config_type) {
  current_linkerconfig_type_ = config_type;
}

bool Context::IsVndkAvailable() const {
  for (auto& apex : GetApexModules()) {
    if (StartsWith(apex.name, "com.android.vndk.")) {
      return true;
    }
  }
  return false;
}

void Context::RegisterApexNamespaceBuilder(const std::string& name,
                                           ApexNamespaceBuilder builder) {
  builders_[name] = builder;
}

Namespace Context::BuildApexNamespace(const ApexInfo& apex_info,
                                      bool visible) const {
  auto builder = builders_.find(apex_info.name);
  if (builder != builders_.end()) {
    return builder->second(*this, apex_info);
  }

  return BaseContext::BuildApexNamespace(apex_info, visible);
}

std::string Var(const std::string& name) {
  auto val = modules::Variables::GetValue(name);
  if (val.has_value()) {
    return *val;
  }
  CHECK(!"undefined var") << name << " is not defined";
  return "";
}

std::string Var(const std::string& name, const std::string& default_value) {
  auto val = modules::Variables::GetValue(name);
  if (val.has_value()) {
    return *val;
  }
  return default_value;
}

bool Context::IsSectionVndkEnabled() const {
  if (!IsVndkAvailable() || android::linkerconfig::modules::IsVndkLiteDevice()) {
    return false;
  }
  if (IsVendorSection()) {
    return true;
  }
  if (IsProductSection() &&
      android::linkerconfig::modules::IsProductVndkVersionDefined()) {
    return true;
  }

  return false;
}

}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
