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
#include "linkerconfig/common.h"

#include <string>
#include <vector>

#include <android-base/properties.h>
#include <android-base/strings.h>

#include "linkerconfig/context.h"
#include "linkerconfig/section.h"
#include "linkerconfig/variables.h"

namespace {
const std::vector<std::string> kBionicLibs = {
    "libc.so",
    "libdl.so",
    "libdl_android.so",
    "libm.so",
};
}  // namespace

namespace android {
namespace linkerconfig {
namespace contents {

using android::linkerconfig::modules::Namespace;
using android::linkerconfig::modules::Section;

void AddStandardSystemLinks(const Context& ctx, Section* section) {
  const bool debuggable = android::base::GetBoolProperty("ro.debuggable", false);
  const std::string system_ns_name = ctx.GetSystemNamespaceName();
  const bool is_section_vndk_enabled = ctx.IsSectionVndkEnabled();
  section->ForEachNamespaces([&](Namespace& ns) {
    if (ns.GetName() != system_ns_name) {
      ns.GetLink(system_ns_name).AddSharedLib(kBionicLibs);
      if (!is_section_vndk_enabled || ns.GetName() != "default") {
        ns.GetLink(system_ns_name)
            .AddSharedLib(Var("SANITIZER_RUNTIME_LIBRARIES"));
      }
      if (debuggable) {
        // Library on the system image that can be dlopened for debugging purposes.
        ns.GetLink(system_ns_name).AddSharedLib("libfdtrack.so");
      }
    }
  });
}

std::vector<std::string> GetSystemStubLibraries() {
  return android::base::Split(Var("STUB_LIBRARIES", ""), ":");
}

}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
