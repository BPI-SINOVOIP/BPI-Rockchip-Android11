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

#include <vector>

#include "linkerconfig/namespacebuilder.h"
#include "linkerconfig/section.h"

using android::linkerconfig::contents::SectionType;
using android::linkerconfig::modules::ApexInfo;
using android::linkerconfig::modules::Namespace;
using android::linkerconfig::modules::Section;

namespace android {
namespace linkerconfig {
namespace contents {
Section BuildApexArtSection(Context& ctx, const ApexInfo& apex_info) {
  std::vector<Namespace> namespaces;

  ctx.SetCurrentSection(SectionType::Other);

  namespaces.emplace_back(BuildApexArtDefaultNamespace(ctx));
  namespaces.emplace_back(BuildApexPlatformNamespace(ctx));

  return BuildSection(ctx,
                      apex_info.name,
                      std::move(namespaces),
                      {
                          "com.android.art",
                          "com.android.conscrypt",
                          "com.android.neuralnetworks",
                          "com.android.os.statsd",
                      });
}
}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
