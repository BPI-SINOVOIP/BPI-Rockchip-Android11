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

// This namespace is exclusively for vndk-sp libs.

#include "linkerconfig/environment.h"
#include "linkerconfig/namespacebuilder.h"

using android::linkerconfig::modules::AsanPath;
using android::linkerconfig::modules::Namespace;

namespace android {
namespace linkerconfig {
namespace contents {
Namespace BuildVndkNamespace([[maybe_unused]] const Context& ctx,
                             VndkUserPartition vndk_user) {
  bool is_system_or_unrestricted_section = ctx.IsSystemSection() ||
                                           ctx.IsApexBinaryConfig() ||
                                           ctx.IsUnrestrictedSection();
  bool is_vndklite = ctx.IsVndkliteConfig();
  // In the system section, we need to have an additional vndk namespace for
  // product apps. We must have a different name "vndk_product" for this
  // namespace. "vndk_product" namespace is used only from the native_loader for
  // product apps.
  const char* name;
  if (is_system_or_unrestricted_section &&
      vndk_user == VndkUserPartition::Product) {
    name = "vndk_product";
  } else {
    name = "vndk";
  }

  // Isolated but visible when used in the [system] or [unrestricted] section to
  // allow links to be created at runtime, e.g. through android_link_namespaces
  // in libnativeloader. Otherwise it isn't isolated, so visibility doesn't
  // matter.
  Namespace ns(name,
               /*is_isolated=*/ctx.IsSystemSection() || ctx.IsApexBinaryConfig(),
               /*is_visible=*/is_system_or_unrestricted_section);

  std::vector<std::string> lib_paths;
  std::vector<std::string> vndk_paths;
  std::string vndk_version;
  if (vndk_user == VndkUserPartition::Product) {
    lib_paths = {"/product/${LIB}/"};
    vndk_version = Var("PRODUCT_VNDK_VERSION");
  } else {
    // default for vendor
    lib_paths = {"/odm/${LIB}/", "/vendor/${LIB}/"};
    vndk_version = Var("VENDOR_VNDK_VERSION");
  }

  for (const auto& lib_path : lib_paths) {
    ns.AddSearchPath(lib_path + "vndk-sp", AsanPath::WITH_DATA_ASAN);
    if (!is_system_or_unrestricted_section) {
      ns.AddSearchPath(lib_path + "vndk", AsanPath::WITH_DATA_ASAN);
    }
  }
  ns.AddSearchPath("/apex/com.android.vndk.v" + vndk_version + "/${LIB}",
                   AsanPath::SAME_PATH);

  if (is_system_or_unrestricted_section &&
      vndk_user == VndkUserPartition::Vendor) {
    // It is for vendor sp-hal
    ns.AddPermittedPath("/odm/${LIB}/hw", AsanPath::WITH_DATA_ASAN);
    ns.AddPermittedPath("/odm/${LIB}/egl", AsanPath::WITH_DATA_ASAN);
    ns.AddPermittedPath("/vendor/${LIB}/hw", AsanPath::WITH_DATA_ASAN);
    ns.AddPermittedPath("/vendor/${LIB}/egl", AsanPath::WITH_DATA_ASAN);
    if (!is_vndklite) {
      ns.AddPermittedPath("/system/vendor/${LIB}/hw", AsanPath::NONE);
    }
    ns.AddPermittedPath("/system/vendor/${LIB}/egl", AsanPath::NONE);

    // This is exceptionally required since android.hidl.memory@1.0-impl.so is here
    ns.AddPermittedPath(
        "/apex/com.android.vndk.v" + Var("VENDOR_VNDK_VERSION") + "/${LIB}/hw",
        AsanPath::SAME_PATH);
  }

  // For the non-system section, the links should be identical to that of the
  // 'vndk_in_system' namespace, except the links to 'default' and 'vndk_in_system'.
  if (vndk_user == VndkUserPartition::Product) {
    ns.GetLink(ctx.GetSystemNamespaceName())
        .AddSharedLib({Var("LLNDK_LIBRARIES_PRODUCT")});
  } else {
    ns.GetLink(ctx.GetSystemNamespaceName())
        .AddSharedLib({Var("LLNDK_LIBRARIES_VENDOR")});
  }

  if (!is_vndklite) {
    if (is_system_or_unrestricted_section) {
      if (vndk_user == VndkUserPartition::Vendor) {
        // The "vndk" namespace links to the system namespace for LLNDK libs above
        // and links to "sphal" namespace for vendor libs. The ordering matters;
        // the system namespace has higher priority than the "sphal" namespace.
        ns.GetLink("sphal").AllowAllSharedLibs();
      }
    } else {
      // [vendor] or [product] section
      ns.GetLink("default").AllowAllSharedLibs();

      if (android::linkerconfig::modules::IsVndkInSystemNamespace()) {
        ns.GetLink("vndk_in_system")
            .AddSharedLib(Var("VNDK_USING_CORE_VARIANT_LIBRARIES"));
      }
    }
  }

  ns.AddRequires(std::vector{"libneuralnetworks.so"});

  return ns;
}
}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
