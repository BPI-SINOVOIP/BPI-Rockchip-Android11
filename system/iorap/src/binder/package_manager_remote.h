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

#ifndef IORAP_SRC_PACKAGE_MANAGER_REMOTE_H_
#define IORAP_SRC_PACKAGE_MANAGER_REMOTE_H_

#include "binder/package_change_observer.h"

#include <android/content/pm/IPackageManagerNative.h>
#include <binder/IServiceManager.h>

#include <optional>
#include <unordered_map>

namespace iorap::binder {

using IPackageManager = android::content::pm::IPackageManagerNative;
// A map between package name and its version.
using VersionMap = std::unordered_map<std::string, int64_t>;

class PackageManagerRemote;

class PackageManagerDeathRecipient : public android::IBinder::DeathRecipient {
public:
  PackageManagerDeathRecipient(std::shared_ptr<PackageManagerRemote> package_manager,
                               android::sp<PackageChangeObserver> observer) :
    package_manager_(package_manager), observer_(observer) {}
  // android::IBinder::DeathRecipient override:
  void binderDied(const android::wp<android::IBinder>& /* who */) override;

private:
  std::shared_ptr<PackageManagerRemote> package_manager_;

  android::sp<PackageChangeObserver> observer_;
};

class PackageManagerRemote {
 public:
  static std::shared_ptr<PackageManagerRemote> Create();

  // Gets the package version based on the package name.
  std::optional<int64_t> GetPackageVersion(const std::string& package_name);

  // Gets a map of package name and its version.
  std::optional<VersionMap> GetPackageVersionMap();

  void RegisterPackageChangeObserver(android::sp<PackageChangeObserver> observer);

  void UnregisterPackageChangeObserver(android::sp<PackageChangeObserver> observer);

  void RegisterPackageManagerDeathRecipient(
      android::sp<PackageManagerDeathRecipient> death_recipient);

 private:
  template <typename T>
  android::binder::Status InvokeRemote(T&& lambda);

  // Reconnects to the package manager service.
  // Retry until timeout.
  bool ReconnectWithTimeout(int64_t timeout_ms);

  // Gets the package manager service.
  static android::sp<IPackageManager> GetPackageService();

  // Gets all package names.
  std::optional<std::vector<std::string>> GetAllPackages();

  android::sp<IPackageManager> package_service_;
};
}  // namespace iorap::package_manager

#endif  // IORAP_SRC_PACKAGE_MANAGER_REMOTE_H_
