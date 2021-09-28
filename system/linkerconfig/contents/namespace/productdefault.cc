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

#include "linkerconfig/environment.h"
#include "linkerconfig/namespacebuilder.h"

using android::linkerconfig::modules::AsanPath;
using android::linkerconfig::modules::Namespace;

namespace android {
namespace linkerconfig {
namespace contents {
Namespace BuildProductDefaultNamespace([[maybe_unused]] const Context& ctx) {
  Namespace ns("default", /*is_isolated=*/true, /*is_visible=*/true);

  ns.AddSearchPath(Var("PRODUCT", "product") + "/${LIB}",
                   AsanPath::WITH_DATA_ASAN);
  ns.AddPermittedPath(Var("PRODUCT", "product"), AsanPath::WITH_DATA_ASAN);

  ns.GetLink(ctx.GetSystemNamespaceName())
      .AddSharedLib(
          {Var("LLNDK_LIBRARIES_PRODUCT"), Var("SANITIZER_DEFAULT_PRODUCT")});
  ns.GetLink("vndk").AddSharedLib({Var("VNDK_SAMEPROCESS_LIBRARIES_PRODUCT"),
                                   Var("VNDK_CORE_LIBRARIES_PRODUCT")});
  if (android::linkerconfig::modules::IsVndkInSystemNamespace()) {
    ns.GetLink("vndk_in_system")
        .AddSharedLib(Var("VNDK_USING_CORE_VARIANT_LIBRARIES"));
  }
  ns.AddRequires(std::vector{
      "libneuralnetworks.so",
  });
  return ns;
}
}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
