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

#ifndef ANDROID_APEXD_APEXD_ROLLBACK_UTILS_H_
#define ANDROID_APEXD_APEXD_ROLLBACK_UTILS_H_

#include <filesystem>
#include <string>

#include <android-base/logging.h>
#include <android-base/macros.h>
#include <android-base/result.h>
#include <android-base/scopeguard.h>
#include <logwrap/logwrap.h>
#include <selinux/android.h>

using android::base::Error;
using android::base::Result;

namespace android {
namespace apex {

static constexpr const char* kCpPath = "/system/bin/cp";

/**
 * Copies everything including directories from the "from" path to the "to"
 * path. Note that this will fail if run before APEXes are mounted, due to a
 * dependency on runtime.
 */
int32_t copy_directory_recursive(const char* from, const char* to) {
  const char* const argv[] = {
      kCpPath,
      "-F", /* delete any existing destination file first
                      (--remove-destination) */
      "-p", /* preserve timestamps, ownership, and permissions */
      "-R", /* recurse into subdirectories (DEST must be a directory) */
      "-P", /* Do not follow symlinks [default] */
      "-d", /* don't dereference symlinks */
      from,
      to};

  LOG(DEBUG) << "Copying " << from << " to " << to;
  return logwrap_fork_execvp(arraysize(argv), argv, nullptr, false, LOG_ALOG,
                             false, nullptr);
}

/**
 * Deletes any files at to_path, and then copies all files and directories
 * from from_path into to_path. Note that this must be run after APEXes are
 * mounted.
 */
inline Result<void> ReplaceFiles(const std::string& from_path,
                                 const std::string& to_path) {
  namespace fs = std::filesystem;

  std::error_code error_code;
  fs::remove_all(to_path, error_code);
  if (error_code) {
    return Error() << "Failed to delete existing files at " << to_path << " : "
                   << error_code.message();
  }

  auto deleter = [&] {
    std::error_code error_code;
    fs::remove_all(to_path, error_code);
    if (error_code) {
      LOG(ERROR) << "Failed to clean up files at " << to_path << " : "
                 << error_code.message();
    }
  };
  auto scope_guard = android::base::make_scope_guard(deleter);

  int rc = copy_directory_recursive(from_path.c_str(), to_path.c_str());
  if (rc != 0) {
    return Error() << "Failed to copy from [" << from_path << "] to ["
                   << to_path << "]";
  }
  scope_guard.Disable();
  return {};
}

inline Result<void> RestoreconPath(const std::string& path) {
  unsigned int seflags = SELINUX_ANDROID_RESTORECON_RECURSE;
  if (selinux_android_restorecon(path.c_str(), seflags) < 0) {
    return Error() << "Failed to restorecon " << path;
  }
  return {};
}

}  // namespace apex
}  // namespace android

#endif  // ANDROID_APEXD_APEXD_ROLLBACK_UTILS_H_
