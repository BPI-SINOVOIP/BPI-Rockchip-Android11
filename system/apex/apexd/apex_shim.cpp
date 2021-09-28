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

#include "apex_shim.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <openssl/sha.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

#include "apex_constants.h"
#include "apex_file.h"
#include "string_log.h"

using android::base::ErrnoError;
using android::base::Error;
using android::base::Result;

namespace android {
namespace apex {
namespace shim {

namespace fs = std::filesystem;

namespace {

static constexpr const char* kApexCtsShimPackage = "com.android.apex.cts.shim";
static constexpr const char* kHashFilePath = "etc/hash.txt";
static constexpr const int kBufSize = 1024;
static constexpr const fs::perms kForbiddenFilePermissions =
    fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec;
static constexpr const char* kExpectedCtsShimFiles[] = {
    "apex_manifest.json",
    "apex_manifest.pb",
    "etc/hash.txt",
    "app/CtsShim/CtsShim.apk",
    "app/CtsShimTargetPSdk/CtsShimTargetPSdk.apk",
    "priv-app/CtsShimPriv/CtsShimPriv.apk",
};

Result<std::string> CalculateSha512(const std::string& path) {
  LOG(DEBUG) << "Calculating SHA512 of " << path;
  SHA512_CTX ctx;
  SHA512_Init(&ctx);
  std::ifstream apex(path, std::ios::binary);
  if (apex.bad()) {
    return Error() << "Failed to open " << path;
  }
  char buf[kBufSize];
  while (!apex.eof()) {
    apex.read(buf, kBufSize);
    if (apex.bad()) {
      return Error() << "Failed to read " << path;
    }
    int bytes_read = apex.gcount();
    SHA512_Update(&ctx, buf, bytes_read);
  }
  uint8_t hash[SHA512_DIGEST_LENGTH];
  SHA512_Final(hash, &ctx);
  std::stringstream ss;
  ss << std::hex;
  for (int i = 0; i < SHA512_DIGEST_LENGTH; i++) {
    ss << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
  }
  return ss.str();
}

Result<std::vector<std::string>> GetAllowedHashes(const std::string& path) {
  using android::base::ReadFileToString;
  using android::base::StringPrintf;
  const std::string& file_path =
      StringPrintf("%s/%s", path.c_str(), kHashFilePath);
  LOG(DEBUG) << "Reading SHA512 from " << file_path;
  std::string hash;
  if (!ReadFileToString(file_path, &hash, false /* follows symlinks */)) {
    return ErrnoError() << "Failed to read " << file_path;
  }
  std::vector<std::string> allowed_hashes = android::base::Split(hash, "\n");
  auto system_shim_hash = CalculateSha512(
      StringPrintf("%s/%s", kApexPackageSystemDir, shim::kSystemShimApexName));
  if (!system_shim_hash.ok()) {
    return system_shim_hash.error();
  }
  allowed_hashes.push_back(std::move(*system_shim_hash));
  return allowed_hashes;
}
}  // namespace

bool IsShimApex(const ApexFile& apex_file) {
  return apex_file.GetManifest().name() == kApexCtsShimPackage;
}

Result<void> ValidateShimApex(const std::string& mount_point,
                              const ApexFile& apex_file) {
  LOG(DEBUG) << "Validating shim apex " << mount_point;
  const ApexManifest& manifest = apex_file.GetManifest();
  if (!manifest.preinstallhook().empty() ||
      !manifest.postinstallhook().empty()) {
    return Errorf("Shim apex is not allowed to have pre or post install hooks");
  }
  std::error_code ec;
  std::unordered_set<std::string> expected_files;
  for (auto file : kExpectedCtsShimFiles) {
    expected_files.insert(file);
  }

  auto iter = fs::recursive_directory_iterator(mount_point, ec);
  // Unfortunately fs::recursive_directory_iterator::operator++ can throw an
  // exception, which means that it's impossible to use range-based for loop
  // here.
  // TODO: wrap into a non-throwing iterator to support range-based for loop.
  while (iter != fs::end(iter)) {
    auto path = iter->path();
    // Resolve the mount point to ensure any trailing slash is removed.
    auto resolved_mount_point = fs::path(mount_point).string();
    auto local_path = path.string().substr(resolved_mount_point.length() + 1);
    fs::file_status status = iter->status(ec);

    if (fs::is_symlink(status)) {
      return Error()
             << "Shim apex is not allowed to contain symbolic links, found "
             << path;
    } else if (fs::is_regular_file(status)) {
      if ((status.permissions() & kForbiddenFilePermissions) !=
          fs::perms::none) {
        return Error() << path << " has illegal permissions";
      }
      auto ex = expected_files.find(local_path);
      if (ex != expected_files.end()) {
        expected_files.erase(local_path);
      } else {
        return Error() << path << " is an unexpected file inside the shim apex";
      }
    } else if (!fs::is_directory(status)) {
      // If this is not a symlink, a file or a directory, fail.
      return Error() << "Unexpected file entry in shim apex: " << iter->path();
    }
    iter = iter.increment(ec);
    if (ec) {
      return Error() << "Failed to scan " << mount_point << " : "
                     << ec.message();
    }
  }

  return {};
}

Result<void> ValidateUpdate(const std::string& system_apex_path,
                            const std::string& new_apex_path) {
  LOG(DEBUG) << "Validating update of shim apex to " << new_apex_path
             << " using system shim apex " << system_apex_path;
  auto allowed = GetAllowedHashes(system_apex_path);
  if (!allowed.ok()) {
    return allowed.error();
  }
  auto actual = CalculateSha512(new_apex_path);
  if (!actual.ok()) {
    return actual.error();
  }
  auto it = std::find(allowed->begin(), allowed->end(), *actual);
  if (it == allowed->end()) {
    return Error() << new_apex_path << " has unexpected SHA512 hash "
                   << *actual;
  }
  return {};
}

}  // namespace shim
}  // namespace apex
}  // namespace android
