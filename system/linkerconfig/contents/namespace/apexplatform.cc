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

// This namespace is for libraries within the NNAPI APEX.

#include "linkerconfig/namespacebuilder.h"

#include "linkerconfig/apex.h"
#include "linkerconfig/common.h"
#include "linkerconfig/environment.h"
#include "linkerconfig/namespace.h"

using android::linkerconfig::modules::ApexInfo;
using android::linkerconfig::modules::AsanPath;
using android::linkerconfig::modules::Namespace;

namespace {
std::vector<std::string> required_libs = {
    "libandroidicu.so",
    "libdexfile_external.so",
    "libdexfiled_external.so",
    // TODO(b/120786417 or b/134659294): libicuuc.so and libicui18n.so are kept
    // for app compat. Uncomment those once they are marked as provided from ART
    // APEX.

    // "libicui18n.so",
    // "libicuuc.so",
    "libnativebridge.so",
    "libnativehelper.so",
    "libnativeloader.so",
    // TODO(b/122876336): Remove libpac.so once it's migrated to Webview
    "libpac.so",
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
Namespace BuildApexPlatformNamespace([[maybe_unused]] const Context& ctx) {
  Namespace ns("system", /*is_isolated=*/true, /*is_visible=*/true);

  ns.AddSearchPath("/system/${LIB}", AsanPath::WITH_DATA_ASAN);
  ns.AddPermittedPath("/apex/com.android.runtime/${LIB}/bionic",
                      AsanPath::SAME_PATH);

  ns.AddProvides(GetSystemStubLibraries());
  ns.AddRequires(required_libs);

  return ns;
}
}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
