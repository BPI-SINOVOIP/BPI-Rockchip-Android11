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

#ifndef IORAP_SRC_INODE2FILENAME_INODE_RESOLVER_H_
#define IORAP_SRC_INODE2FILENAME_INODE_RESOLVER_H_

#include "common/expected.h"
#include "inode2filename/data_source.h"
#include "inode2filename/inode.h"
#include "inode2filename/system_call.h"

#include <rxcpp/rx.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace iorap::inode2filename {

enum class ProcessMode {
  // Test modes:
  kInProcessDirect,  // Execute the code directly.
  kInProcessIpc,     // Execute code via IPC layer using multiple threads.
  // Shipping mode:
  kOutOfProcessIpc,  // Execute code via fork+exec with IPC.

  // Note: in-process system-wide stat(2)/readdir/etc is blocked by selinux.
  // Attempting to call the test modes will fail with -EPERM.
  //
  // Use fork+exec mode in shipping configurations, which spawns inode2filename
  // as a separate command.
};

enum class VerifyKind {
  kNone,
  kStat,
};

std::vector<std::string> ToArgs(VerifyKind verify_kind);

struct InodeResolverDependencies : public DataSourceDependencies {
  ProcessMode process_mode = ProcessMode::kInProcessDirect;
  VerifyKind verify{VerifyKind::kStat};  // Filter out results that aren't up-to-date with stat(2) ?
};

std::vector<std::string> ToArgs(const InodeResolverDependencies& deps);

// Create an rx-observable chain that allows searching for inode->filename mappings given
// a set of inode keys.
class InodeResolver : public std::enable_shared_from_this<InodeResolver> {
 public:
  static std::shared_ptr<InodeResolver> Create(InodeResolverDependencies dependencies,
                                               std::shared_ptr<DataSource> data_source);  // nonnull

  // Convenience function for above: Uses DataSource::Create for the data-source.
  static std::shared_ptr<InodeResolver> Create(InodeResolverDependencies dependencies);

  // Search the associated data source to map each inode in 'inodes' to a file path.
  //
  // Observes DataSource::EmitInodes(), which is unsubscribed from early once all inodes are found.
  //
  // Notes:
  // * Searching does not begin until all 'inodes' are observed to avoid rescanning.
  // * If the observable is unsubscribed to prior to completion, searching will halt.
  //
  // Post-condition: All emitted results are in inodes, and all inodes are in emitted results.
  rxcpp::observable<InodeResult>
      FindFilenamesFromInodes(rxcpp::observable<Inode> inodes) const;
      // TODO: feels like we could turn this into a general helper?
  // Convenience function for above.
  virtual rxcpp::observable<InodeResult>
      FindFilenamesFromInodes(std::vector<Inode> inodes) const;

  // Enumerate *all* inodes available from the data source, associating it with a filepath.
  //
  // Depending on the data source (e.g. diskscan), it can take a very long time for this observable
  // to complete. The intended use-case is for development/debugging, not for production.
  //
  // Observes DataSource::EmitInodes() until it reaches #on_completed.
  //
  // Notes:
  // * If the observable is unsubscribed to prior to completion, searching will halt.
  virtual rxcpp::observable<InodeResult>
      EmitAll() const;

  // Notifies the DataSource to begin recording.
  // Some DataSources may be continuously refreshing, but only if recording is enabled.
  // To get the most up-to-date data, toggle recording before reading the inodes out.
  void StartRecording();  // XX: feels like this should be BPF-specific.

  // Notifies the DataSource to stop recording.
  // Some DataSources may be continuously refreshing, but only if recording is enabled.
  // The snapshot of data returned by e.g. #EmitAll would then not change outside of recording.
  void StopRecording();

  virtual ~InodeResolver();
 private:
  struct Impl;
  Impl* impl_;

 protected:
  InodeResolver(InodeResolverDependencies dependencies);
  InodeResolver(InodeResolverDependencies dependencies, std::shared_ptr<DataSource> data_source);
  InodeResolverDependencies& GetDependencies();
  const InodeResolverDependencies& GetDependencies() const;
};

}

#endif  // IORAP_SRC_INODE2FILENAME_INODE_RESOLVER_H_
