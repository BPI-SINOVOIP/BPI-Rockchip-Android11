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
#pragma once

#include "linkerconfig/apex.h"
#include "linkerconfig/context.h"
#include "linkerconfig/namespace.h"

namespace android {
namespace linkerconfig {
namespace contents {

enum class VndkUserPartition {
  Vendor,
  Product,
};

typedef modules::Namespace NamespaceBuilder(const Context& ctx);

NamespaceBuilder BuildSystemDefaultNamespace;
NamespaceBuilder BuildSphalNamespace;
NamespaceBuilder BuildRsNamespace;
NamespaceBuilder BuildVendorDefaultNamespace;
NamespaceBuilder BuildSystemNamespace;
NamespaceBuilder BuildVndkInSystemNamespace;
NamespaceBuilder BuildProductDefaultNamespace;
NamespaceBuilder BuildUnrestrictedDefaultNamespace;
NamespaceBuilder BuildPostInstallNamespace;
NamespaceBuilder BuildRecoveryDefaultNamespace;

modules::Namespace BuildVndkNamespace(const Context& ctx,
                                      VndkUserPartition vndk_user);

modules::Namespace BuildArtNamespace(const Context& ctx,
                                     const modules::ApexInfo& apex_info);

// Namespaces for APEX binaries
modules::Namespace BuildApexDefaultNamespace(const Context& ctx,
                                             const modules::ApexInfo& apex_info);
NamespaceBuilder BuildApexPlatformNamespace;
NamespaceBuilder BuildApexArtDefaultNamespace;

void RegisterApexNamespaceBuilders(Context& ctx);
}  // namespace contents
}  // namespace linkerconfig
}  // namespace android
