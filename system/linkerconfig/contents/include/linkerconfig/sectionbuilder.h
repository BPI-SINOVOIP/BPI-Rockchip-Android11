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

#include <string>
#include <vector>

#include "linkerconfig/apex.h"
#include "linkerconfig/context.h"
#include "linkerconfig/section.h"

typedef android::linkerconfig::modules::Section SectionBuilder(
    android::linkerconfig::contents::Context& ctx);

typedef android::linkerconfig::modules::Section ApexSectionBuilder(
    android::linkerconfig::contents::Context& ctx,
    const android::linkerconfig::modules::ApexInfo& target_apex);

namespace android {
namespace linkerconfig {
namespace contents {
modules::Section BuildSection(const Context& ctx, const std::string& name,
                              std::vector<modules::Namespace>&& namespaces,
                              const std::vector<std::string>& visible_apexes);
SectionBuilder BuildSystemSection;
SectionBuilder BuildVendorSection;
SectionBuilder BuildProductSection;
SectionBuilder BuildUnrestrictedSection;
SectionBuilder BuildLegacySection;
SectionBuilder BuildPostInstallSection;
SectionBuilder BuildRecoverySection;
ApexSectionBuilder BuildApexDefaultSection;
ApexSectionBuilder BuildApexArtSection;
}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
