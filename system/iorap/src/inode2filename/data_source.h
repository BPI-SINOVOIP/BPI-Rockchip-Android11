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

#ifndef IORAP_SRC_INODE2FILENAME_DATA_SOURCE_H_
#define IORAP_SRC_INODE2FILENAME_DATA_SOURCE_H_

#include "inode2filename/inode_result.h"
#include "inode2filename/system_call.h"

#include <rxcpp/rx.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace iorap::inode2filename {

enum class DataSourceKind {
  kDiskScan,
  kTextCache,
  kBpf,
};

std::vector<std::string> ToArgs(DataSourceKind data_source_kind);

struct DataSourceDependencies {
  DataSourceKind data_source = DataSourceKind::kDiskScan;
  borrowed<SystemCall*> system_call = nullptr;

  // kDiskScan-specific options. Other data sources ignore these fields.
  std::vector<std::string> root_directories;
  // kTextCache-specific options. Other data sources ignore these fields.
  std::optional<std::string> text_cache_filename;
};

std::vector<std::string> ToArgs(const DataSourceDependencies& deps);

class DataSource : std::enable_shared_from_this<DataSource> {
 public:
  static std::shared_ptr<DataSource> Create(DataSourceDependencies dependencies);

  virtual void StartRecording();  // XX: feels like this should be BPF-specific.
  virtual void StopRecording();

  // Emit all inode->filename mappings (i.e. an infinite lazy list).
  // The specific order is determined by the extra dependency options.
  //
  // The work must terminate if all subscriptions are removed.
  virtual rxcpp::observable<InodeResult> EmitInodes() const = 0;

  // Does the InodeResult include a valid device number?
  // If returning false, the InodeResolver fills in the missing device number with stat(2).
  virtual bool ResultIncludesDeviceNumber() const { return true; };

 protected:
  virtual ~DataSource() {}
  DataSource(DataSourceDependencies dependencies);
  DataSourceDependencies dependencies_;
};

}  // namespace iorap::inode2filename

#endif  // IORAP_SRC_INODE2FILENAME_DATA_SOURCE_H_
