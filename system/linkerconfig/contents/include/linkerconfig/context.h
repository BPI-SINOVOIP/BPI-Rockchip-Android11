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

#include <functional>
#include <map>
#include <optional>
#include <string>

#include "linkerconfig/basecontext.h"

namespace android {
namespace linkerconfig {
namespace contents {

class Context;
using ApexNamespaceBuilder =
    std::function<modules::Namespace(const Context&, const modules::ApexInfo&)>;

enum class SectionType {
  System,
  Vendor,
  Product,
  Unrestricted,
  Other,
};

enum class LinkerConfigType {
  Default,
  Legacy,
  Vndklite,
  Recovery,
  ApexBinary,
};

class Context : public modules::BaseContext {
 public:
  Context()
      : current_section_(SectionType::System),
        current_linkerconfig_type_(LinkerConfigType::Default) {
  }
  bool IsSystemSection() const;
  bool IsVendorSection() const;
  bool IsProductSection() const;
  bool IsUnrestrictedSection() const;

  bool IsDefaultConfig() const;
  bool IsLegacyConfig() const;
  bool IsVndkliteConfig() const;
  bool IsRecoveryConfig() const;
  bool IsApexBinaryConfig() const;

  void SetCurrentSection(SectionType value);
  void SetCurrentLinkerConfigType(LinkerConfigType value);

  // Returns true if vndk apex is available
  bool IsVndkAvailable() const;

  // Returns the namespace that covers /system/${LIB}.
  std::string GetSystemNamespaceName() const;

  modules::Namespace BuildApexNamespace(const modules::ApexInfo& apex_info,
                                        bool visible) const override;
  void RegisterApexNamespaceBuilder(const std::string& name,
                                    ApexNamespaceBuilder builder);

  bool IsSectionVndkEnabled() const;

 private:
  std::map<std::string, ApexNamespaceBuilder> builders_;

  SectionType current_section_;
  LinkerConfigType current_linkerconfig_type_;
};

std::string Var(const std::string& name);

std::string Var(const std::string& name, const std::string& default_value);

}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
