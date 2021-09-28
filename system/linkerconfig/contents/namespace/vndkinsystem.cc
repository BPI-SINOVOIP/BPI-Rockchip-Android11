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

// This namespace is where no-vendor-variant VNDK libraries are loaded for a
// vendor process.  Note that we do not simply export these libraries from the
// "system" namespace, because in some cases both the core variant and the
// vendor variant of a VNDK library may be loaded.  In such cases, we do not
// want to eliminate double-loading because doing so means the global states
// of the library would be shared.
//
// Only the no-vendor-variant VNDK libraries are whitelisted in this namespace.
// This is to ensure that we do not load libraries needed by no-vendor-variant
// VNDK libraries into vndk_in_system namespace.

#include "linkerconfig/namespacebuilder.h"

#include "linkerconfig/environment.h"

using android::linkerconfig::modules::AsanPath;
using android::linkerconfig::modules::IsProductVndkVersionDefined;
using android::linkerconfig::modules::Namespace;

namespace android {
namespace linkerconfig {
namespace contents {
Namespace BuildVndkInSystemNamespace([[maybe_unused]] const Context& ctx) {
  Namespace ns("vndk_in_system",
               /*is_isolated=*/true,
               /*is_visible=*/false);

  // The search paths here should be kept the same as that of the 'system' namespace.
  ns.AddSearchPath("/system/${LIB}", AsanPath::WITH_DATA_ASAN);
  ns.AddSearchPath(Var("SYSTEM_EXT") + "/${LIB}", AsanPath::WITH_DATA_ASAN);
  if (!IsProductVndkVersionDefined()) {
    ns.AddSearchPath(Var("PRODUCT") + "/${LIB}", AsanPath::WITH_DATA_ASAN);
  }

  if (android::linkerconfig::modules::IsVndkInSystemNamespace()) {
    ns.AddWhitelisted(Var("VNDK_USING_CORE_VARIANT_LIBRARIES"));
  }

  // The links here should be identical to that of the 'vndk' namespace for the
  // [vendor] section, with the following exceptions:
  //   1. 'vndk_in_system' needs to be freely linked back to 'vndk'.
  //   2. 'vndk_in_system' does not need to link to 'default', as any library that
  //      requires anything vendor would not be a vndk_in_system library.
  if (ctx.IsProductSection()) {
    ns.GetLink(ctx.GetSystemNamespaceName())
        .AddSharedLib(Var("LLNDK_LIBRARIES_PRODUCT"));
  } else {
    ns.GetLink(ctx.GetSystemNamespaceName())
        .AddSharedLib(Var("LLNDK_LIBRARIES_VENDOR"));
  }
  ns.GetLink("vndk").AllowAllSharedLibs();
  ns.AddRequires(std::vector{"libneuralnetworks.so"});

  return ns;
}
}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
