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

#ifndef ANDROID_APEXD_APEXD_PRIVATE_H_
#define ANDROID_APEXD_APEXD_PRIVATE_H_

#include <string>

#include <android-base/result.h>
#include "apex_database.h"
#include "apex_manifest.h"

namespace android {
namespace apex {

class ApexFile;

static constexpr int kMkdirMode = 0755;

namespace apexd_private {

std::string GetPackageMountPoint(const ApexManifest& manifest);
std::string GetPackageTempMountPoint(const ApexManifest& manifest);
std::string GetActiveMountPoint(const ApexManifest& manifest);

android::base::Result<void> BindMount(const std::string& target,
                                      const std::string& source);
android::base::Result<MountedApexDatabase::MountedApexData> TempMountPackage(
    const ApexFile& apex, const std::string& mount_point);
android::base::Result<void> Unmount(
    const MountedApexDatabase::MountedApexData& data);

}  // namespace apexd_private
}  // namespace apex
}  // namespace android

#endif  // ANDROID_APEXD_APEXD_PRIVATE_H_
