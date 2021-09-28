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
#define LOG_TAG "libapexutil"

#include "apexutil.h"

#include <dirent.h>

#include <memory>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/result.h>
#include <apex_manifest.pb.h>

using ::android::base::Error;
using ::android::base::ReadFileToString;
using ::android::base::Result;
using ::apex::proto::ApexManifest;

namespace {

Result<ApexManifest> ParseApexManifest(const std::string &manifest_path) {
  std::string content;
  if (!ReadFileToString(manifest_path, &content)) {
    return Error() << "Failed to read manifest file: " << manifest_path;
  }
  ApexManifest manifest;
  if (!manifest.ParseFromString(content)) {
    return Error() << "Can't parse APEX manifest: " << manifest_path;
  }
  return manifest;
}

} // namespace

namespace android {
namespace apex {

std::map<std::string, ApexManifest>
GetActivePackages(const std::string &apex_root) {
  std::unique_ptr<DIR, decltype(&closedir)> dir(opendir(apex_root.c_str()),
                                                closedir);
  if (!dir) {
    return {};
  }

  std::map<std::string, ApexManifest> apexes;
  dirent *entry;
  while ((entry = readdir(dir.get())) != nullptr) {
    if (entry->d_name[0] == '.')
      continue;
    if (entry->d_type != DT_DIR)
      continue;
    if (strchr(entry->d_name, '@') != nullptr)
      continue;
    std::string apex_path = apex_root + "/" + entry->d_name;
    auto manifest = ParseApexManifest(apex_path + "/apex_manifest.pb");
    if (manifest.ok()) {
      apexes.emplace(std::move(apex_path), std::move(*manifest));
    } else {
      LOG(WARNING) << manifest.error();
    }
  }
  return apexes;
}

} // namespace apex
} // namespace android
