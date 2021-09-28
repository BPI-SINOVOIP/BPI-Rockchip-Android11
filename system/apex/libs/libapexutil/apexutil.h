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
#pragma once

#include <map>
#include <string>

#include <apex_manifest.pb.h>

namespace android {
namespace apex {

// Returns active APEX packages as a map of path(e.g. /apex/com.android.foo) to
// ApexManifest. This is very similar to ApexService::getActivePackages, but it
// doesn't rely on whether APEXes are flattened or not.
// For testing purpose, it accepts the apex root path which is defined by
// kApexRoot constant.
std::map<std::string, ::apex::proto::ApexManifest>
GetActivePackages(const std::string &apex_root);

constexpr const char *const kApexRoot = "/apex";

} // namespace apex
} // namespace android