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

#include "apex_manifest.h"
#include <android-base/file.h>

#include <memory>
#include <string>

using android::base::Error;
using android::base::Result;

namespace android {
namespace apex {

Result<ApexManifest> ParseManifest(const std::string& content) {
  ApexManifest apex_manifest;

  if (!apex_manifest.ParseFromString(content)) {
    return Error() << "Can't parse APEX manifest.";
  }

  // Verifying required fields.
  // name
  if (apex_manifest.name().empty()) {
    return Error() << "Missing required field \"name\" from APEX manifest.";
  }

  // version
  if (apex_manifest.version() == 0) {
    return Error() << "Missing required field \"version\" from APEX manifest.";
  }
  return apex_manifest;
}

std::string GetPackageId(const ApexManifest& apexManifest) {
  return apexManifest.name() + "@" + std::to_string(apexManifest.version());
}

Result<ApexManifest> ReadManifest(const std::string& path) {
  std::string content;
  if (!android::base::ReadFileToString(path, &content)) {
    return Error() << "Failed to read manifest file: " << path;
  }
  return ParseManifest(content);
}

}  // namespace apex
}  // namespace android
