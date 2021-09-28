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

#define LOG_TAG "apexd"

#include "apex_preinstalled_data.h"

#include <unordered_map>

#include <android-base/file.h>
#include <android-base/result.h>
#include <android-base/strings.h>

#include "apex_constants.h"
#include "apex_file.h"
#include "apexd_utils.h"
#include "string_log.h"

using android::base::Error;
using android::base::Result;

namespace android {
namespace apex {

namespace {

struct ApexPreinstalledData {
  std::string name;
  std::string key;
  std::string path;
};

std::unordered_map<std::string, ApexPreinstalledData> gScannedPreinstalledData;

Result<std::vector<ApexPreinstalledData>> collectPreinstalleDataFromDir(
    const std::string& dir) {
  LOG(INFO) << "Scanning " << dir << " for preinstalled data";
  std::vector<ApexPreinstalledData> ret;
  if (access(dir.c_str(), F_OK) != 0 && errno == ENOENT) {
    LOG(INFO) << "... does not exist. Skipping";
    return ret;
  }
  const bool scanBuiltinApexes = isPathForBuiltinApexes(dir);
  if (!scanBuiltinApexes) {
    return Error() << "Can't scan preinstalled APEX data from " << dir;
  }
  Result<std::vector<std::string>> apex_files = FindApexFilesByName(dir);
  if (!apex_files.ok()) {
    return apex_files.error();
  }

  for (const auto& file : *apex_files) {
    Result<ApexFile> apex_file = ApexFile::Open(file);
    if (!apex_file.ok()) {
      return Error() << "Failed to open " << file << " : " << apex_file.error();
    }
    ApexPreinstalledData apexPreInstalledData;
    apexPreInstalledData.name = apex_file->GetManifest().name();
    apexPreInstalledData.key = apex_file->GetBundledPublicKey();
    apexPreInstalledData.path = apex_file->GetPath();
    ret.push_back(apexPreInstalledData);
  }
  return ret;
}

Result<void> updatePreinstalledData(
    const std::vector<ApexPreinstalledData>& apexes) {
  for (const ApexPreinstalledData& apex : apexes) {
    if (gScannedPreinstalledData.find(apex.name) ==
        gScannedPreinstalledData.end()) {
      gScannedPreinstalledData.insert({apex.name, apex});
    } else {
      const std::string& existing_key =
          gScannedPreinstalledData.at(apex.name).key;
      if (existing_key != apex.key) {
        return Error() << "Key for package " << apex.name
                       << " does not match with previously scanned key";
      }
    }
  }
  return {};
}

}  // namespace

Result<void> collectPreinstalledData(const std::vector<std::string>& dirs) {
  for (const auto& dir : dirs) {
    Result<std::vector<ApexPreinstalledData>> preinstalledData =
        collectPreinstalleDataFromDir(dir);
    if (!preinstalledData.ok()) {
      return Error() << "Failed to collect keys from " << dir << " : "
                     << preinstalledData.error();
    }
    Result<void> st = updatePreinstalledData(*preinstalledData);
    if (!st.ok()) {
      return st;
    }
  }
  return {};
}

Result<const std::string> getApexKey(const std::string& name) {
  if (gScannedPreinstalledData.find(name) == gScannedPreinstalledData.end()) {
    return Error() << "No preinstalled data found for package " << name;
  }
  return gScannedPreinstalledData[name].key;
}

Result<const std::string> getApexPreinstalledPath(const std::string& name) {
  if (gScannedPreinstalledData.find(name) == gScannedPreinstalledData.end()) {
    return Error() << "No preinstalled data found for package " << name;
  }
  return gScannedPreinstalledData[name].path;
}

bool HasPreInstalledVersion(const std::string& name) {
  return gScannedPreinstalledData.find(name) != gScannedPreinstalledData.end();
}

}  // namespace apex
}  // namespace android
