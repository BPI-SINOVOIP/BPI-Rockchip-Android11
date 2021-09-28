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

#include "linkerconfig/apexconfig.h"
#include "linkerconfig/environment.h"
#include "linkerconfig/sectionbuilder.h"

using android::linkerconfig::modules::ApexInfo;
using android::linkerconfig::modules::DirToSection;
using android::linkerconfig::modules::Section;

namespace android {
namespace linkerconfig {
namespace contents {
android::linkerconfig::modules::Configuration CreateApexConfiguration(
    Context& ctx, const ApexInfo& apex_info) {
  std::vector<Section> sections;

  ctx.SetCurrentLinkerConfigType(
      android::linkerconfig::contents::LinkerConfigType::ApexBinary);

  std::vector<DirToSection> dirToSection = {
      {apex_info.path + "/bin", apex_info.name}};

  if (apex_info.name == "com.android.art") {
    sections.push_back(BuildApexArtSection(ctx, apex_info));
  } else {
    sections.push_back(BuildApexDefaultSection(ctx, apex_info));
  }
  return android::linkerconfig::modules::Configuration(std::move(sections),
                                                       dirToSection);
}
}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
