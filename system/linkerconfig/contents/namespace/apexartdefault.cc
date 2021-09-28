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
#include "linkerconfig/namespacebuilder.h"

#include "linkerconfig/namespace.h"

using android::linkerconfig::modules::Namespace;

namespace android {
namespace linkerconfig {
namespace contents {
Namespace BuildApexArtDefaultNamespace([[maybe_unused]] const Context& ctx) {
  Namespace ns("default", /*is_isolated=*/true, /*is_visible=*/false);

  // The default namespace here only links to other namespaces, in particular
  // "art" where the real library loading takes place. Any outgoing links from
  // "art" also need to be present here.
  ns.GetLink("com_android_art").AllowAllSharedLibs();
  ns.GetLink("system").AllowAllSharedLibs();
  ns.AddRequires(std::vector{"libadbconnection_client.so"});

  return ns;
}
}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
