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

#ifndef IORAP_SRC_PACKAGE_VERSION_MAP_H_
#define IORAP_SRC_PACKAGE_VERSION_MAP_H_

#include <android/content/pm/IPackageManagerNative.h>
#include <binder/IServiceManager.h>

#include <optional>
#include <unordered_map>

#include "package_manager_remote.h"

namespace iorap::binder {

class PackageVersionMap {
 public:
  static std::shared_ptr<PackageVersionMap> Create();

  PackageVersionMap(std::shared_ptr<PackageManagerRemote> package_manager,
                    std::optional<VersionMap> version_map)
      : package_manager_(package_manager),
        version_map_(version_map) {}

  PackageVersionMap()
      : package_manager_(nullptr), version_map_(std::nullopt) {}

  // Updates the version specified by 'package_name' to 'version'.
  //
  // Post-condition: Find(package_name) == version.
  // * if the package is newly installed, insert and return true.
  // * if the package version is changed, update the version to the
  //   given one and return true.
  // * otherwise, return false.
  bool Update(std::string package_name, int64_t version);

  void UpdateAll();

  // Finds the version of the package in the hash table.
  // -1 means the app is installed by unversioned.
  // Empty means the app is not inside the RAM version map, maybe due to
  // the app is newly installed.
  std::optional<int64_t> Find(const std::string& package_name) {
    VersionMap::iterator it = version_map_->find(package_name);
    if (it == version_map_->end()) {
      return std::nullopt;
    }
    return it->second;
  }

  size_t Size();

  // Gets or queries the version for the package.
  //
  // The method firstly access the hash map in the RAM, which is built when
  // iorapd starts. If the version is not in the map, it tries the query
  // the package manager via IPC, with a cost of ~0.6ms.
  //
  // If no version can be found for some reason, return -1,
  // because when an app has no version the package manager returns -1.
  std::optional<int64_t> GetOrQueryPackageVersion(
      const std::string& package_name);

 private:
  std::shared_ptr<PackageManagerRemote> package_manager_;
  std::optional<VersionMap> version_map_;
  std::mutex mutex_;
};
}  // namespace iorap::binder

#endif  // IORAP_SRC_PACKAGE_MANAGER_REMOTE_H_
