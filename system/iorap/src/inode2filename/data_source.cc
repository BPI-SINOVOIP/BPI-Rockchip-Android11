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

#include "inode2filename/data_source.h"

#include "common/cmd_utils.h"
#include "inode2filename/inode_resolver.h"
#include "inode2filename/search_directories.h"

#include <android-base/logging.h>

#include <fstream>
#include <stdio.h>

namespace rx = rxcpp;

namespace iorap::inode2filename {

std::vector<std::string> ToArgs(DataSourceKind data_source_kind) {
  const char* value = nullptr;

  switch (data_source_kind) {
    case DataSourceKind::kDiskScan:
      value = "diskscan";
      break;
    case DataSourceKind::kTextCache:
      value = "textcache";
      break;
    case DataSourceKind::kBpf:
      value = "bpf";
      break;
  }

  std::vector<std::string> args;
  iorap::common::AppendNamedArg(args, "--data-source", value);
  return args;
}

std::vector<std::string> ToArgs(const DataSourceDependencies& deps) {
  std::vector<std::string> args;

  iorap::common::AppendArgsRepeatedly(args, ToArgs(deps.data_source));
  // intentionally skip system_call; it does not have a command line equivalent
  iorap::common::AppendNamedArgRepeatedly(args, "--root", deps.root_directories);

  if (deps.text_cache_filename) {
    iorap::common::AppendNamedArg(args, "--textcache", *(deps.text_cache_filename));
  }

  return args;
}

class DiskScanDataSource : public DataSource {
 public:
  DiskScanDataSource(DataSourceDependencies dependencies)
    : DataSource(std::move(dependencies)) {
    DCHECK_NE(dependencies_.root_directories.size(), 0u) << "Root directories can't be empty";
  }

  virtual rxcpp::observable<InodeResult> EmitInodes() const override {
    SearchDirectories searcher{/*borrow*/dependencies_.system_call};
    return searcher.ListAllFilenames(dependencies_.root_directories);
  }

  // Since not all Inodes emitted are the ones searched for, doing additional stat(2) calls here
  // would be redundant.
  //
  // We set the device number to a dummy value here (-1) so that InodeResolver
  // can fill it in later with stat(2). This is effectively the same thing as always doing
  // verification.
  virtual bool ResultIncludesDeviceNumber() const { return false; };

  virtual ~DiskScanDataSource() {}
};

static inline void LeftTrim(/*inout*/std::string& s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
    return !std::isspace(ch);
  }));
}

class TextCacheDataSource : public DataSource {
 public:
  TextCacheDataSource(DataSourceDependencies dependencies)
    : DataSource(std::move(dependencies)) {
    DCHECK(dependencies_.text_cache_filename.has_value()) << "Must have text cache filename";
  }

  virtual rxcpp::observable<InodeResult> EmitInodes() const override {
    const std::string& file_name = *dependencies_.text_cache_filename;

    return rx::observable<>::create<InodeResult>(
      [file_name](rx::subscriber<InodeResult> dest) {
        LOG(VERBOSE) << "TextCacheDataSource (lambda)";

        std::ifstream ifs{file_name};

        if (!ifs.is_open()) {
          dest.on_error(rxcpp::util::make_error_ptr(
              std::ios_base::failure("Could not open specified text cache filename")));
          return;
        }

        while (dest.is_subscribed() && ifs) {
          LOG(VERBOSE) << "TextCacheDataSource (read line)";
          // TODO.

          uint64_t device_number = 0;
          uint64_t inode_number = 0;
          uint64_t file_size = 0;

          // Parse lines of form:
          //   "$device_number $inode $filesize $filename..."
          // This format conforms to system/extras/pagecache/pagecache.py

          ifs >> device_number;
          ifs >> inode_number;
          ifs >> file_size;
          (void)file_size;  // Not used in iorapd.

          std::string value_filename;
          std::getline(/*inout*/ifs, /*out*/value_filename);

          if (value_filename.empty()) {
            // Ignore empty lines.
            continue;
          }

          // getline, unlike ifstream, does not ignore spaces.
          // There's always at least 1 space in a textcache output file.
          // However, drop *all* spaces on the left since filenames starting with a space are
          // ambiguous to us.
          LeftTrim(/*inout*/value_filename);

          Inode inode = Inode::FromDeviceAndInode(static_cast<dev_t>(device_number),
                                                  static_cast<ino_t>(inode_number));

          LOG(VERBOSE) << "TextCacheDataSource (on_next) " << inode << "->" << value_filename;
          dest.on_next(InodeResult::makeSuccess(inode, value_filename));
        }

        dest.on_completed();
      }
    );

    // TODO: plug into the filtering and verification graph.
  }

  virtual ~TextCacheDataSource() {}
};

DataSource::DataSource(DataSourceDependencies dependencies)
  : dependencies_{std::move(dependencies)} {
  DCHECK(dependencies_.system_call != nullptr);
}

std::shared_ptr<DataSource> DataSource::Create(DataSourceDependencies dependencies) {
  switch (dependencies.data_source) {
    case DataSourceKind::kDiskScan:
      return std::shared_ptr<DataSource>{new DiskScanDataSource{std::move(dependencies)}};
    case DataSourceKind::kTextCache:
      return std::shared_ptr<DataSource>{new TextCacheDataSource{std::move(dependencies)}};
    case DataSourceKind::kBpf:
      // TODO: BPF-based data source.
      LOG(FATAL) << "Not implemented yet";
      return nullptr;
    default:
      LOG(FATAL) << "Invalid data source value";
      return nullptr;
  }
}

void DataSource::StartRecording() {}
void DataSource::StopRecording() {}

}  // namespace iorap::inode2filename
