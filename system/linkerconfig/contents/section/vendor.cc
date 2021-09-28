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

// Namespace config for vendor processes.

#include "linkerconfig/sectionbuilder.h"

#include "linkerconfig/common.h"
#include "linkerconfig/environment.h"
#include "linkerconfig/namespacebuilder.h"
#include "linkerconfig/section.h"

using android::linkerconfig::contents::SectionType;
using android::linkerconfig::modules::Namespace;
using android::linkerconfig::modules::Section;

namespace android {
namespace linkerconfig {
namespace contents {
Section BuildVendorSection(Context& ctx) {
  ctx.SetCurrentSection(SectionType::Vendor);
  std::vector<Namespace> namespaces;

  bool is_vndklite = ctx.IsVndkliteConfig();

  namespaces.emplace_back(BuildVendorDefaultNamespace(ctx));
  // VNDK-Lite devices does not contain VNDK and System namespace in vendor
  // section. Instead they (except libraries from APEX) will be loaded from
  // default namespace, so VNDK libraries can access private platform libraries.
  if (!is_vndklite) {
    namespaces.emplace_back(BuildVndkNamespace(ctx, VndkUserPartition::Vendor));
    namespaces.emplace_back(BuildSystemNamespace(ctx));
  }

  if (android::linkerconfig::modules::IsVndkInSystemNamespace()) {
    namespaces.emplace_back(BuildVndkInSystemNamespace(ctx));
  }

  return BuildSection(ctx,
                      "vendor",
                      std::move(namespaces),
                      {
                          "com.android.art",
                          "com.android.neuralnetworks",
                          "com.android.runtime",
                      });
}
}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
