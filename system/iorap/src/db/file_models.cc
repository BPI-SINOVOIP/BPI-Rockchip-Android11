// Copyright (C) 2019 The Android Open Source Project
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

#include "common/cmd_utils.h"
#include "db/file_models.h"
#include "db/models.h"

#include <android-base/file.h>
#include <android-base/properties.h>

#include <algorithm>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

namespace iorap::db {

static constexpr const char* kRootPathProp = "iorapd.root.dir";
static const unsigned int kPerfettoMaxTraces =
    ::android::base::GetUintProperty("iorapd.perfetto.max_traces", /*default*/10u);

static uint64_t GetTimeNanoseconds() {
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);

  uint64_t now_ns = (now.tv_sec * 1000000000LL + now.tv_nsec);
  return now_ns;
}

static bool IsDir(const std::string& dirpath) {
  struct stat st;
  if (stat(dirpath.c_str(), &st) == 0) {
    if (S_ISDIR(st.st_mode)) {
      return true;
    }
  }
  return false;
}

// Given some path /a/b/c create all of /a, /a/b, /a/b/c/ recursively.
static bool MkdirWithParents(const std::string& path) {
  size_t prev_end = 0;
  while (prev_end < path.size()) {
    size_t next_end = path.find('/', prev_end + 1);

    std::string dir_path = path.substr(0, next_end);
    if (!IsDir(dir_path)) {
#if defined(_WIN32)
      int ret = mkdir(dir_path.c_str());
#else
      mode_t old_mask = umask(0);
      // The user permission is 5 to allow system server to
      // read the files. No other users could do that because
      // the upper directory only allows system server and iorapd
      // to access. Also selinux rules prevent other users to
      // read files here.
      int ret = mkdir(dir_path.c_str(), 0755);
      umask(old_mask);
#endif
      if (ret != 0) {
        PLOG(ERROR) << "failed to create dir " << dir_path;
        return false;
      }
    }
    prev_end = next_end;

    if (next_end == std::string::npos) {
      break;
    }
  }
  return true;
}

FileModelBase::FileModelBase(VersionedComponentName vcn)
  : vcn_{std::move(vcn)} {
    root_path_ = common::GetEnvOrProperty(kRootPathProp,
                                          /*default*/"/data/misc/iorapd");
}

std::string FileModelBase::BaseDir() const {
  std::stringstream ss;

  ss << root_path_ << "/" << vcn_.GetPackage() << "/";
  ss << vcn_.GetVersion();
  ss << "/";
  ss << vcn_.GetActivity() << "/";
  ss << SubDir();

  return ss.str();
}

std::string FileModelBase::FilePath() const {
  std::stringstream ss;
  ss << BaseDir();
  ss << "/";
  ss << BaseFile();

  return ss.str();
}

bool FileModelBase::MkdirWithParents() {
  LOG(VERBOSE) << "MkdirWithParents: " << BaseDir();
  return ::iorap::db::MkdirWithParents(BaseDir());
}

PerfettoTraceFileModel::PerfettoTraceFileModel(VersionedComponentName vcn,
                                               uint64_t timestamp)
  : FileModelBase{std::move(vcn)}, timestamp_{timestamp} {
}

PerfettoTraceFileModel PerfettoTraceFileModel::CalculateNewestFilePath(VersionedComponentName vcn) {
  uint64_t timestamp = GetTimeNanoseconds();
  return PerfettoTraceFileModel{vcn, timestamp};
}

std::string PerfettoTraceFileModel::BaseFile() const {
  std::stringstream ss;
  ss << timestamp_ << ".perfetto_trace.pb";
  return ss.str();
}

void PerfettoTraceFileModel::DeleteOlderFiles(DbHandle& db, VersionedComponentName vcn) {
  std::vector<RawTraceModel> raw_traces =
      RawTraceModel::SelectByVersionedComponentName(db, vcn);

  if (WOULD_LOG(VERBOSE)) {
    size_t raw_traces_size = raw_traces.size();
    for (size_t i = 0; i < raw_traces_size; ++i) {
      LOG(VERBOSE) << "DeleteOlderFiles - selected " << raw_traces[i];
    }
    LOG(VERBOSE) << "DeleteOlderFiles - queried total " << raw_traces_size << " records";
  }

  size_t items_to_delete = 0;
  if (raw_traces.size() > kPerfettoMaxTraces) {
    items_to_delete = raw_traces.size() - kPerfettoMaxTraces;
  } else {
    LOG(VERBOSE) << "DeleteOlderFiles - don't delete older raw traces, too few files: "
                 << " wanted at least " << kPerfettoMaxTraces << ", but got " << raw_traces.size();
  }

  for (size_t i = 0; i < items_to_delete; ++i) {
    RawTraceModel& raw_trace = raw_traces[i];  // sorted ascending -> items to delete are first.
    std::string err_msg;

    if (!::android::base::RemoveFileIfExists(raw_trace.file_path, /*out*/&err_msg)) {
      LOG(ERROR) << "Failed to remove raw trace file: " << raw_trace.file_path
                 << ", reason: " << err_msg;
    } else {
      raw_trace.Delete();
      LOG(DEBUG) << "Deleted raw trace for " << vcn << " at " << raw_trace.file_path;
    }
  }
}

CompiledTraceFileModel::CompiledTraceFileModel(VersionedComponentName vcn)
  : FileModelBase{std::move(vcn)} {
}

CompiledTraceFileModel CompiledTraceFileModel::CalculateNewestFilePath(VersionedComponentName vcn) {
  return CompiledTraceFileModel{vcn};
}

std::string CompiledTraceFileModel::BaseFile() const {
  return "compiled_trace.pb";
}

}  // namespace iorap::db
