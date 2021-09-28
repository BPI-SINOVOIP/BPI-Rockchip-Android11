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

#ifndef IORAP_SRC_DB_FILE_MODELS_H_
#define IORAP_SRC_DB_FILE_MODELS_H_

#include <optional>
#include <ostream>
#include <string>

namespace iorap::db {

struct VersionedComponentName {
  VersionedComponentName(std::string package,
                         std::string activity,
                         int64_t version)
    : package_{std::move(package)},
      activity_{std::move(activity)},
      version_{version} {
  }

  const std::string& GetPackage() const {
    return package_;
  }

  const std::string& GetActivity() const {
    return activity_;
  }

  int GetVersion() const {
    return version_;
  }

 private:
  std::string package_;
  std::string activity_;
  int64_t version_;
};

inline std::ostream& operator<<(std::ostream& os, const VersionedComponentName& vcn) {
  os << vcn.GetPackage() << "/" << vcn.GetActivity() << "@" << vcn.GetVersion();
  return os;
}

class FileModelBase {
 protected:
  FileModelBase(VersionedComponentName vcn);

  virtual std::string SubDir() const = 0;
  // Include the last file component only (/a/b/c.txt -> c.txt)
  virtual std::string BaseFile() const = 0;

  virtual ~FileModelBase() {}

 public:
  virtual std::string ModelName() const = 0;
  // Return the absolute file path to this FileModel.
  std::string FilePath() const;

  // Include the full path minus the basefile (/a/b/c.txt -> /a/b)
  std::string BaseDir() const;

  // Create the base dir recursively (e.g. mkdir -p $basedir).
  bool MkdirWithParents();
 private:
  VersionedComponentName vcn_;
  std::string subdir_;
  std::string filename_;
  std::string root_path_;
};

inline std::ostream& operator<<(std::ostream& os, const FileModelBase& fmb) {
  os << fmb.ModelName() << "{'" << fmb.FilePath() << "'}";
  return os;
}

class DbHandle;

class PerfettoTraceFileModel : public FileModelBase {
 protected:
  virtual std::string ModelName() const override { return "PerfettoTrace"; }
  virtual std::string SubDir() const override { return "raw_traces"; }
  virtual std::string BaseFile() const override;

  PerfettoTraceFileModel(VersionedComponentName vcn,
                         uint64_t timestamp);

 public:
  static PerfettoTraceFileModel CalculateNewestFilePath(VersionedComponentName vcn);
  static void DeleteOlderFiles(DbHandle& db, VersionedComponentName vcn);

  virtual ~PerfettoTraceFileModel() {}
 private:
  uint64_t timestamp_;
};

class CompiledTraceFileModel : public FileModelBase {
 protected:
  virtual std::string ModelName() const override { return "CompiledTrace"; }
  virtual std::string SubDir() const override { return "compiled_traces"; }
  virtual std::string BaseFile() const override;

  CompiledTraceFileModel(VersionedComponentName vcn);

 public:
  static CompiledTraceFileModel CalculateNewestFilePath(VersionedComponentName vcn);

  virtual ~CompiledTraceFileModel() {}
};

}   // namespace iorap::db

#endif  // IORAP_SRC_DB_FILE_MODELS_H_
