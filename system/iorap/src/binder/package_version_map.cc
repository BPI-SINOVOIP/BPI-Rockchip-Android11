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

#include "package_version_map.h"

#include <android-base/logging.h>
#include <android-base/properties.h>

namespace iorap::binder {

std::shared_ptr<PackageVersionMap> PackageVersionMap::Create() {
  std::shared_ptr<PackageManagerRemote> package_manager =
      PackageManagerRemote::Create();
  if (!package_manager) {
    return std::make_shared<PackageVersionMap>();
  }

  std::optional<VersionMap> map = package_manager->GetPackageVersionMap();
  return std::make_shared<PackageVersionMap>(package_manager, map);
}

void PackageVersionMap::UpdateAll() {std::lock_guard<std::mutex> lock(mutex_);
  size_t old_size = version_map_->size();
  std::optional<VersionMap> new_version_map =
      package_manager_->GetPackageVersionMap();
  if (!new_version_map) {
    LOG(DEBUG) << "Failed to get the latest version map";
    return;
  }
  version_map_ = std::move(new_version_map);
  LOG(DEBUG) << "Update for version is done. The size is from " << old_size
             << " to " << version_map_->size();
}

bool PackageVersionMap::Update(std::string package_name, int64_t version) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!version_map_) {
    LOG(DEBUG) << "The version map doesn't exist. "
               << "The package manager may be down.";
    return false;
  }

  VersionMap::iterator it = version_map_->find(package_name);
  if (it == version_map_->end()) {
    LOG(DEBUG) << "New installed package "
               << package_name
               << " with version "
               << version;
    (*version_map_)[package_name] = version;
    return true;
  }

  if (it->second != version) {
    LOG(DEBUG) << "New version package "
               << package_name
               << " with version "
               << version;
    (*version_map_)[package_name] = version;
    return true;
  }

  LOG(DEBUG) << "Same version package "
             << package_name
             << " with version "
             << version;
  return false;
}

size_t PackageVersionMap::Size() {
  if (!version_map_) {
    LOG(DEBUG) << "The version map doesn't exist. "
               << "The package manager may be down.";
    return -1;
  }
  return version_map_->size();
}

std::optional<int64_t> PackageVersionMap::GetOrQueryPackageVersion(
    const std::string& package_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!version_map_) {
    LOG(DEBUG) << "The version map doesn't exist. "
               << "The package manager may be down.";
    return std::nullopt;
  }

  VersionMap::iterator it = version_map_->find(package_name);
  if (it == version_map_->end()) {
    LOG(DEBUG) << "Cannot find version for: " << package_name
                 << " in the hash table";
    std::optional<int64_t> version =
        package_manager_->GetPackageVersion(package_name);
    if (version) {
      LOG(VERBOSE) << "Find version for: " << package_name << " on the fly.";
      (*version_map_)[package_name] = *version;
      return *version;
    } else {
      LOG(ERROR) << "Cannot find version for: " << package_name
                 << " on the fly.";
      return -1;
    }
  }

  return it->second;
}
}  // namespace iorap::binder
