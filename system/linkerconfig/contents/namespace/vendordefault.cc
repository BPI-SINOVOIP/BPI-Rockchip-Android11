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

// This is the default linker namespace for a vendor process (a process started
// from /vendor/bin/*).

#include "linkerconfig/namespacebuilder.h"

#include "linkerconfig/common.h"
#include "linkerconfig/environment.h"

using android::linkerconfig::modules::AsanPath;
using android::linkerconfig::modules::GetVendorVndkVersion;
using android::linkerconfig::modules::Namespace;

namespace {

// Keep in sync with the "platform" namespace in art/build/apex/ld.config.txt.
const std::vector<std::string> kVndkLiteVendorRequires = {
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
};

}  // namespace

namespace android {
namespace linkerconfig {
namespace contents {
Namespace BuildVendorDefaultNamespace([[maybe_unused]] const Context& ctx) {
  bool is_vndklite = ctx.IsVndkliteConfig();

  Namespace ns(
      "default", /*is_isolated=*/!is_vndklite, /*is_visible=*/!is_vndklite);

  ns.AddSearchPath("/odm/${LIB}", AsanPath::WITH_DATA_ASAN);
  // Allow loosen restriction between vndk and private platform libraries
  if (is_vndklite) {
    ns.AddSearchPath("/odm/${LIB}/vndk", AsanPath::WITH_DATA_ASAN);
    ns.AddSearchPath("/odm/${LIB}/vndk-sp", AsanPath::WITH_DATA_ASAN);
  }

  ns.AddSearchPath("/vendor/${LIB}", AsanPath::WITH_DATA_ASAN);
  // Allow loosen restriction between vndk and private platform libraries
  if (is_vndklite) {
    ns.AddSearchPath("/vendor/${LIB}/vndk", AsanPath::WITH_DATA_ASAN);
    ns.AddSearchPath("/vendor/${LIB}/vndk-sp", AsanPath::WITH_DATA_ASAN);
  }

  // VNDK-Lite devices require broader access from vendor to system/product partition
  if (is_vndklite) {
    ns.AddSearchPath("/system/${LIB}", AsanPath::WITH_DATA_ASAN);
    ns.AddSearchPath(Var("SYSTEM_EXT") + "/${LIB}", AsanPath::WITH_DATA_ASAN);
    ns.AddSearchPath(Var("PRODUCT") + "/${LIB}", AsanPath::WITH_DATA_ASAN);
    // Put system vndk at the last search order in vndk_lite for GSI
    ns.AddSearchPath(
        "/apex/com.android.vndk.v" + Var("VENDOR_VNDK_VERSION") + "/${LIB}",
        AsanPath::SAME_PATH);
  }

  ns.AddSearchPath("/vendor/${LIB}/hw", AsanPath::WITH_DATA_ASAN);
  ns.AddSearchPath("/vendor/${LIB}/egl", AsanPath::WITH_DATA_ASAN);

  ns.AddPermittedPath("/odm", AsanPath::WITH_DATA_ASAN);
  ns.AddPermittedPath("/vendor", AsanPath::WITH_DATA_ASAN);
  ns.AddPermittedPath("/system/vendor", AsanPath::NONE);

  if (is_vndklite) {
    // Because vendor-default NS works like system-default NS for VNDK-lite
    // devices the requires/provides are added just like system-default.
    ns.AddRequires(kVndkLiteVendorRequires);
    ns.AddProvides(GetSystemStubLibraries());
  } else {
    ns.GetLink(ctx.GetSystemNamespaceName())
        .AddSharedLib(
            {Var("LLNDK_LIBRARIES_VENDOR"), Var("SANITIZER_DEFAULT_VENDOR")});
    ns.GetLink("vndk").AddSharedLib({Var("VNDK_SAMEPROCESS_LIBRARIES_VENDOR"),
                                     Var("VNDK_CORE_LIBRARIES_VENDOR")});
    if (android::linkerconfig::modules::IsVndkInSystemNamespace()) {
      ns.GetLink("vndk_in_system")
          .AddSharedLib(Var("VNDK_USING_CORE_VARIANT_LIBRARIES"));
    }
  }
  ns.AddRequires(std::vector{"libneuralnetworks.so"});
  return ns;
}
}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
