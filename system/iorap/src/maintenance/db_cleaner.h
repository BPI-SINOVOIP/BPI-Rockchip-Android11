// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef IORAP_SRC_MAINTENANCE_VERSION_UPDATE_H_
#define IORAP_SRC_MAINTENANCE_VERSION_UPDATE_H_

#include <android/content/pm/IPackageManagerNative.h>

#include <string>
#include <vector>

#include "binder/package_version_map.h"
#include "db/file_models.h"

namespace iorap::maintenance {

// Clean up the database.
// Remove all relevant data for old-version packages.
void CleanUpDatabase(const db::DbHandle& db,
                     std::shared_ptr<binder::PackageVersionMap> version_map);
}  // namespace iorap::maintenance

#endif  // IORAP_SRC_MAINTENANCE_VERSION_UPDATE_H_
