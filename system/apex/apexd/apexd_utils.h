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

#ifndef ANDROID_APEXD_APEXD_UTILS_H_
#define ANDROID_APEXD_APEXD_UTILS_H_

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <android-base/chrono_utils.h>
#include <android-base/logging.h>
#include <android-base/result.h>
#include <android-base/scopeguard.h>
#include <android-base/strings.h>
#include <cutils/android_reboot.h>

#include "apex_constants.h"
#include "string_log.h"

using android::base::ErrnoError;
using android::base::Error;
using android::base::Result;

namespace android {
namespace apex {

inline int WaitChild(pid_t pid) {
  int status;
  pid_t got_pid = TEMP_FAILURE_RETRY(waitpid(pid, &status, 0));

  if (got_pid != pid) {
    PLOG(WARNING) << "waitpid failed: wanted " << pid << ", got " << got_pid;
    return 1;
  }

  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    return 0;
  } else {
    return status;
  }
}

// TODO(ioffe): change to Result<void>?
inline int ForkAndRun(const std::vector<std::string>& args,
                      std::string* error_msg) {
  LOG(DEBUG) << "Forking : " << android::base::Join(args, " ");
  std::vector<const char*> argv;
  argv.resize(args.size() + 1, nullptr);
  std::transform(args.begin(), args.end(), argv.begin(),
                 [](const std::string& in) { return in.c_str(); });

  // 3) Fork.
  pid_t pid = fork();
  if (pid == -1) {
    // Fork failed.
    *error_msg = PStringLog() << "Unable to fork";
    return -1;
  }

  if (pid == 0) {
    execv(argv[0], const_cast<char**>(argv.data()));
    PLOG(ERROR) << "execv failed";
    _exit(1);
  }

  int rc = WaitChild(pid);
  if (rc != 0) {
    *error_msg = StringLog() << "Failed run: status=" << rc;
  }
  return rc;
}

template <typename Fn>
Result<void> WalkDir(const std::string& path, Fn fn) {
  namespace fs = std::filesystem;
  std::error_code ec;
  auto it = fs::directory_iterator(path, ec);
  auto end = fs::directory_iterator();
  while (!ec && it != end) {
    fn(*it);
    it.increment(ec);
  }
  if (ec) {
    return Error() << "Can't open " << path
                   << " for reading : " << ec.message();
  }
  return {};
}

template <typename FilterFn>
Result<std::vector<std::string>> ReadDir(const std::string& path, FilterFn fn) {
  namespace fs = std::filesystem;

  std::vector<std::string> ret;
  auto status = WalkDir(path, [&](const fs::directory_entry& entry) {
    if (fn(entry)) {
      ret.push_back(entry.path());
    }
  });
  if (!status.ok()) {
    return status.error();
  }
  return ret;
}

inline bool IsEmptyDirectory(const std::string& path) {
  auto res = ReadDir(path, [](auto _) { return true; });
  return res.ok() && res->empty();
}

inline Result<void> createDirIfNeeded(const std::string& path, mode_t mode) {
  struct stat stat_data;

  if (stat(path.c_str(), &stat_data) != 0) {
    if (errno == ENOENT) {
      if (mkdir(path.c_str(), mode) != 0) {
        return ErrnoError() << "Could not mkdir " << path;
      }
    } else {
      return ErrnoError() << "Could not stat " << path;
    }
  } else {
    if (!S_ISDIR(stat_data.st_mode)) {
      return Error() << path << " exists and is not a directory.";
    }
  }

  // Need to manually call chmod because mkdir will create a folder with
  // permissions mode & ~umask.
  if (chmod(path.c_str(), mode) != 0) {
    return ErrnoError() << "Could not chmod " << path;
  }

  return {};
}

inline Result<void> DeleteDirContent(const std::string& path) {
  auto files = ReadDir(path, [](auto _) { return true; });
  if (!files.ok()) {
    return Error() << "Failed to delete " << path << " : " << files.error();
  }
  for (const std::string& file : *files) {
    if (unlink(file.c_str()) != 0) {
      return ErrnoError() << "Failed to delete " << file;
    }
  }
  return {};
}

inline Result<void> DeleteDir(const std::string& path) {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::remove_all(path, ec);
  if (ec) {
    return Error() << "Failed to delete path " << path << " : " << ec.message();
  }
  return {};
}

inline Result<ino_t> get_path_inode(const std::string& path) {
  struct stat buf;
  memset(&buf, 0, sizeof(buf));
  if (stat(path.c_str(), &buf) != 0) {
    return ErrnoError() << "Failed to stat " << path;
  } else {
    return buf.st_ino;
  }
}

inline Result<bool> PathExists(const std::string& path) {
  namespace fs = std::filesystem;

  std::error_code ec;
  if (!fs::exists(fs::path(path), ec)) {
    if (ec) {
      return Error() << "Failed to access " << path << " : " << ec.message();
    } else {
      return false;
    }
  }
  return true;
}

inline void Reboot() {
  LOG(INFO) << "Rebooting device";
  if (android_reboot(ANDROID_RB_RESTART2, 0, nullptr) != 0) {
    LOG(ERROR) << "Failed to reboot device";
  }
}

inline Result<void> WaitForFile(const std::string& path,
                                std::chrono::nanoseconds timeout) {
  android::base::Timer t;
  bool has_slept = false;
  while (t.duration() < timeout) {
    struct stat sb;
    if (stat(path.c_str(), &sb) != -1) {
      if (has_slept) {
        LOG(INFO) << "wait for '" << path << "' took " << t;
      }
      return {};
    }
    std::this_thread::sleep_for(5ms);
    has_slept = true;
  }
  return ErrnoError() << "wait for '" << path << "' timed out and took " << t;
}

inline Result<std::vector<std::string>> GetSubdirs(const std::string& path) {
  namespace fs = std::filesystem;
  auto filter_fn = [](const std::filesystem::directory_entry& entry) {
    std::error_code ec;
    bool result = entry.is_directory(ec);
    if (ec) {
      LOG(ERROR) << "Failed to check is_directory : " << ec.message();
      return false;
    }
    return result;
  };
  return ReadDir(path, filter_fn);
}

inline Result<std::vector<std::string>> GetDeUserDirs() {
  return GetSubdirs(kDeNDataDir);
}

// Returns first path between |first_dir| and |second_dir| that correspond to a
// existing directory. Returns error if neither |first_dir| nor |second_dir|
// correspond to an existing directory.
inline Result<std::string> FindFirstExistingDirectory(
    const std::string& first_dir, const std::string& second_dir) {
  struct stat stat_buf;
  if (stat(first_dir.c_str(), &stat_buf) != 0) {
    PLOG(WARNING) << "Failed to stat " << first_dir;
    if (stat(second_dir.c_str(), &stat_buf) != 0) {
      return ErrnoError() << "Failed to stat " << second_dir;
    }
    if (!S_ISDIR(stat_buf.st_mode)) {
      return Error() << second_dir << " is not a directory";
    }
    return second_dir;
  }

  if (S_ISDIR(stat_buf.st_mode)) {
    return first_dir;
  }
  LOG(WARNING) << first_dir << " is not a directory";

  if (stat(second_dir.c_str(), &stat_buf) != 0) {
    return ErrnoError() << "Failed to stat " << second_dir;
  }
  if (!S_ISDIR(stat_buf.st_mode)) {
    return Error() << second_dir << " is not a directory";
  }
  return second_dir;
}

// Copies all entries under |from| directory to |to| directory, and then them.
// Leaving |from| empty.
inline Result<void> MoveDir(const std::string& from, const std::string& to) {
  struct stat stat_buf;
  if (stat(to.c_str(), &stat_buf) != 0) {
    return ErrnoError() << "Failed to stat " << to;
  }
  if (!S_ISDIR(stat_buf.st_mode)) {
    return Error() << to << " is not a directory";
  }

  namespace fs = std::filesystem;
  std::error_code ec;
  auto it = fs::directory_iterator(from, ec);
  if (ec) {
    return Error() << "Can't read " << from << " : " << ec.message();
  }

  for (const auto& end = fs::directory_iterator(); it != end;) {
    auto from_path = it->path();
    it.increment(ec);
    if (ec) {
      return Error() << "Can't read " << from << " : " << ec.message();
    }
    auto to_path = to / from_path.filename();
    fs::copy(from_path, to_path, fs::copy_options::recursive, ec);
    if (ec) {
      return Error() << "Failed to copy " << from_path << " to " << to_path
                     << " : " << ec.message();
    }
    fs::remove_all(from_path, ec);
    if (ec) {
      return Error() << "Failed to delete " << from_path << " : "
                     << ec.message();
    }
  }
  return {};
}

}  // namespace apex
}  // namespace android

#endif  // ANDROID_APEXD_APEXD_UTILS_H_
