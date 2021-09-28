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

// SP-HAL(Sameprocess-HAL)s are the only vendor libraries that are allowed to be
// loaded inside system processes. libEGL_<chipset>.so, libGLESv2_<chipset>.so,
// android.hardware.graphics.mapper@2.0-impl.so, etc are SP-HALs.
//
// This namespace is exclusivly for SP-HALs. When the framework tries to
// dynamically load SP-HALs, android_dlopen_ext() is used to explicitly specify
// that they should be searched and loaded from this namespace.
//
// Note that there is no link from the default namespace to this namespace.

#include "linkerconfig/namespacebuilder.h"

using android::linkerconfig::modules::AsanPath;
using android::linkerconfig::modules::Namespace;

namespace android {
namespace linkerconfig {
namespace contents {
Namespace BuildSphalNamespace([[maybe_unused]] const Context& ctx) {
  // Visible to allow use with android_dlopen_ext, and with
  // android_link_namespaces in libnativeloader.
  Namespace ns("sphal",
               /*is_isolated=*/!ctx.IsUnrestrictedSection(),
               /*is_visible=*/true);
  ns.AddSearchPath("/odm/${LIB}", AsanPath::WITH_DATA_ASAN);
  ns.AddSearchPath("/vendor/${LIB}", AsanPath::WITH_DATA_ASAN);
  ns.AddSearchPath("/vendor/${LIB}/egl", AsanPath::WITH_DATA_ASAN);
  ns.AddSearchPath("/vendor/${LIB}/hw", AsanPath::NONE);

  ns.AddPermittedPath("/odm/${LIB}", AsanPath::WITH_DATA_ASAN);
  ns.AddPermittedPath("/vendor/${LIB}", AsanPath::WITH_DATA_ASAN);
  ns.AddPermittedPath("/system/vendor/${LIB}", AsanPath::NONE);

  if (ctx.IsApexBinaryConfig() && !ctx.IsVndkAvailable()) {
    // If device is legacy, let Sphal libraries access to system lib path for
    // VNDK-SP libraries
    ns.AddSearchPath("/system/${LIB}", AsanPath::WITH_DATA_ASAN);
    ns.AddPermittedPath("/system/${LIB}", AsanPath::WITH_DATA_ASAN);
  }

  if (ctx.IsApexBinaryConfig()) {
    if (ctx.IsVndkAvailable()) {
      ns.GetLink("vndk").AddSharedLib(
          Var("VNDK_SAMEPROCESS_LIBRARIES_VENDOR", ""));
      ns.GetLink(ctx.GetSystemNamespaceName())
          .AddSharedLib(Var("LLNDK_LIBRARIES_VENDOR", ""));
    }
  } else {
    // Once in this namespace, access to libraries in /system/lib is restricted.
    // Only libs listed here can be used. Order is important here as the
    // namespaces are tried in this order. rs should be before vndk because both
    // are capable of loading libRS_internal.so
    if (ctx.IsSystemSection() || ctx.IsUnrestrictedSection()) {
      ns.GetLink("rs").AddSharedLib("libRS_internal.so");
    }
    ns.GetLink(ctx.GetSystemNamespaceName())
        .AddSharedLib(Var("LLNDK_LIBRARIES_VENDOR", ""));
    ns.GetLink("vndk").AddSharedLib(
        Var("VNDK_SAMEPROCESS_LIBRARIES_VENDOR", ""));
    ns.AddRequires(std::vector{"libneuralnetworks.so"});
  }

  return ns;
}
}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
