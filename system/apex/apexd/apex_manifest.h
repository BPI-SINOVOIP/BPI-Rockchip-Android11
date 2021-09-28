/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef ANDROID_APEXD_APEX_MANIFEST_H_
#define ANDROID_APEXD_APEX_MANIFEST_H_

#include <android-base/result.h>

#include "apex_manifest.pb.h"

#include <string>

using ::apex::proto::ApexManifest;

namespace android {
namespace apex {
// Parses and validates APEX manifest.
android::base::Result<ApexManifest> ParseManifest(const std::string& content);
// Returns package id of an ApexManifest
std::string GetPackageId(const ApexManifest& apex_manifest);
// Reads and parses APEX manifest from the file on disk.
android::base::Result<ApexManifest> ReadManifest(const std::string& path);
}  // namespace apex
}  // namespace android

#endif  // ANDROID_APEXD_APEX_MANIFEST_H_
