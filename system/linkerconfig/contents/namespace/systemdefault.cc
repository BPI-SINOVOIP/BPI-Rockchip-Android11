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

// Framework-side code runs in this namespace. Libs from /vendor partition can't
// be loaded in this namespace.

#include "linkerconfig/common.h"
#include "linkerconfig/environment.h"
#include "linkerconfig/namespace.h"
#include "linkerconfig/namespacebuilder.h"

using android::linkerconfig::modules::AsanPath;
using android::linkerconfig::modules::IsProductVndkVersionDefined;
using android::linkerconfig::modules::Namespace;

namespace android {
namespace linkerconfig {
namespace contents {
Namespace BuildSystemDefaultNamespace([[maybe_unused]] const Context& ctx) {
  bool is_fully_treblelized = ctx.IsDefaultConfig();
  std::string product = Var("PRODUCT");
  std::string system_ext = Var("SYSTEM_EXT");

  // Visible to allow links to be created at runtime, e.g. through
  // android_link_namespaces in libnativeloader.
  Namespace ns("default",
               /*is_isolated=*/is_fully_treblelized,
               /*is_visible=*/true);

  ns.AddSearchPath("/system/${LIB}", AsanPath::WITH_DATA_ASAN);
  ns.AddSearchPath(system_ext + "/${LIB}", AsanPath::WITH_DATA_ASAN);
  if (!IsProductVndkVersionDefined() || !is_fully_treblelized) {
    // System processes can search product libs only if product VNDK is not
    // enforced.
    ns.AddSearchPath(product + "/${LIB}", AsanPath::WITH_DATA_ASAN);
  }
  if (!is_fully_treblelized) {
    ns.AddSearchPath("/vendor/${LIB}", AsanPath::WITH_DATA_ASAN);
    ns.AddSearchPath("/odm/${LIB}", AsanPath::WITH_DATA_ASAN);
  }

  if (is_fully_treblelized) {
    // We can't have entire /system/${LIB} as permitted paths because doing so
    // makes it possible to load libs in /system/${LIB}/vndk* directories by
    // their absolute paths, e.g. dlopen("/system/lib/vndk/libbase.so"). VNDK
    // libs are built with previous versions of Android and thus must not be
    // loaded into this namespace where libs built with the current version of
    // Android are loaded. Mixing the two types of libs in the same namespace
    // can cause unexpected problems.
    const std::vector<std::string> permitted_paths = {
        "/system/${LIB}/drm",
        "/system/${LIB}/extractors",
        "/system/${LIB}/hw",
        system_ext + "/${LIB}",

        // These are where odex files are located. libart has to be able to
        // dlopen the files
        "/system/framework",

        "/system/app",
        "/system/priv-app",
        system_ext + "/framework",
        system_ext + "/app",
        system_ext + "/priv-app",
        "/vendor/framework",
        "/vendor/app",
        "/vendor/priv-app",
        "/system/vendor/framework",
        "/system/vendor/app",
        "/system/vendor/priv-app",
        "/odm/framework",
        "/odm/app",
        "/odm/priv-app",
        "/oem/app",
        product + "/framework",
        product + "/app",
        product + "/priv-app",
        "/data",
        "/mnt/expand",
        "/apex/com.android.runtime/${LIB}/bionic",
        "/system/${LIB}/bootstrap"};

    for (const auto& path : permitted_paths) {
      ns.AddPermittedPath(path, AsanPath::SAME_PATH);
    }
    if (!IsProductVndkVersionDefined()) {
      // System processes can use product libs only if product VNDK is not enforced.
      ns.AddPermittedPath(product + "/${LIB}", AsanPath::SAME_PATH);
    }
  }

  ns.AddRequires(std::vector{
      // Keep in sync with the "platform" namespace in art/build/apex/ld.config.txt.
      "libdexfile_external.so",
      "libdexfiled_external.so",
      "libnativebridge.so",
      "libnativehelper.so",
      "libnativeloader.so",
      "libandroidicu.so",
      // TODO(b/122876336): Remove libpac.so once it's migrated to Webview
      "libpac.so",
      // TODO(b/120786417 or b/134659294): libicuuc.so
      // and libicui18n.so are kept for app compat.
      "libicui18n.so",
      "libicuuc.so",
      // resolv
      "libnetd_resolv.so",
      // nn
      "libneuralnetworks.so",
      // statsd
      "libstatspull.so",
      "libstatssocket.so",
      // adbd
      "libadb_pairing_auth.so",
      "libadb_pairing_connection.so",
      "libadb_pairing_server.so",
  });

  ns.AddProvides(GetSystemStubLibraries());
  return ns;
}
}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
