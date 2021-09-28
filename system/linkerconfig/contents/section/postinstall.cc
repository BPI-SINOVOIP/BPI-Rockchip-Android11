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

// Namespace config for binaries under /postinstall.
// Only default namespace is defined and default has no directories
// other than /system/lib in the search paths. This is because linker calls
// realpath on the search paths and this causes selinux denial if the paths
// (/vendor, /odm) are not allowed to the postinstall binaries. There is no
// reason to allow the binaries to access the paths.

#include "linkerconfig/sectionbuilder.h"

#include "linkerconfig/namespacebuilder.h"

using android::linkerconfig::contents::SectionType;
using android::linkerconfig::modules::Namespace;
using android::linkerconfig::modules::Section;

namespace android {
namespace linkerconfig {
namespace contents {
Section BuildPostInstallSection(Context& ctx) {
  ctx.SetCurrentSection(SectionType::Other);
  std::vector<Namespace> namespaces;

  namespaces.emplace_back(BuildPostInstallNamespace(ctx));

  return Section("postinstall", std::move(namespaces));
}
}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
