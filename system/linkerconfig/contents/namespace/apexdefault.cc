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

#include "linkerconfig/apex.h"
#include "linkerconfig/environment.h"
#include "linkerconfig/namespace.h"

using android::linkerconfig::modules::ApexInfo;
using android::linkerconfig::modules::AsanPath;
using android::linkerconfig::modules::Namespace;

namespace android {
namespace linkerconfig {
namespace contents {
Namespace BuildApexDefaultNamespace([[maybe_unused]] const Context& ctx,
                                    const ApexInfo& apex_info) {
  Namespace ns("default", /*is_isolated=*/true, /*is_visible=*/false);

  ns.AddSearchPath(apex_info.path + "/${LIB}", AsanPath::SAME_PATH);
  ns.AddPermittedPath(apex_info.path + "/${LIB}", AsanPath::SAME_PATH);
  ns.AddPermittedPath("/system/${LIB}");

  ns.AddRequires(apex_info.require_libs);
  ns.AddProvides(apex_info.provide_libs);

  return ns;
}
}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
