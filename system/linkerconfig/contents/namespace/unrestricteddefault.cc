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

#include "linkerconfig/namespacebuilder.h"

#include "linkerconfig/common.h"
#include "linkerconfig/environment.h"
#include "linkerconfig/namespace.h"

using android::linkerconfig::modules::AsanPath;
using android::linkerconfig::modules::Namespace;

namespace android {
namespace linkerconfig {
namespace contents {
Namespace BuildUnrestrictedDefaultNamespace([[maybe_unused]] const Context& ctx) {
  Namespace ns("default", /*is_isolated=*/false, /*is_visible=*/true);

  ns.AddSearchPath("/system/${LIB}", AsanPath::WITH_DATA_ASAN);
  ns.AddSearchPath(Var("SYSTEM_EXT") + "/${LIB}", AsanPath::WITH_DATA_ASAN);
  ns.AddSearchPath("/odm/${LIB}", AsanPath::WITH_DATA_ASAN);
  ns.AddSearchPath("/vendor/${LIB}", AsanPath::WITH_DATA_ASAN);
  ns.AddSearchPath(Var("PRODUCT") + "/${LIB}", AsanPath::WITH_DATA_ASAN);

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
  });

  ns.AddProvides(GetSystemStubLibraries());
  return ns;
}
}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
