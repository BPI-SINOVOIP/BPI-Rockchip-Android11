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

#include <string>
#include <vector>

namespace android {
namespace apex {

static constexpr const char* kApexDataDir = "/data/apex";
static constexpr const char* kActiveApexPackagesDataDir = "/data/apex/active";
static constexpr const char* kApexBackupDir = "/data/apex/backup";
static constexpr const char* kApexHashTreeDir = "/data/apex/hashtree";
static constexpr const char* kApexPackageSystemDir = "/system/apex";
static constexpr const char* kApexPackageSystemExtDir = "/system_ext/apex";
static constexpr const char* kApexPackageVendorDir = "/vendor/apex";
static const std::vector<std::string> kApexPackageBuiltinDirs = {
    kApexPackageSystemDir,
    kApexPackageSystemExtDir,
    "/product/apex",
    kApexPackageVendorDir,
};
static constexpr const char* kApexRoot = "/apex";
static constexpr const char* kStagedSessionsDir = "/data/app-staging";

static constexpr const char* kApexDataSubDir = "apexdata";
static constexpr const char* kApexSnapshotSubDir = "apexrollback";
static constexpr const char* kPreRestoreSuffix = "-prerestore";

static constexpr const char* kDeSysDataDir = "/data/misc";
static constexpr const char* kDeNDataDir = "/data/misc_de";
static constexpr const char* kCeDataDir = "/data/misc_ce";

static constexpr const char* kApexPackageSuffix = ".apex";

static constexpr const char* kManifestFilenameJson = "apex_manifest.json";
static constexpr const char* kManifestFilenamePb = "apex_manifest.pb";
}  // namespace apex
}  // namespace android
