// Copyright (C) 2018 The Android Open Source Project
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

#ifndef IORAP_SRC_INODE2FILENAME_SEARCH_DIRECTORIES_H_
#define IORAP_SRC_INODE2FILENAME_SEARCH_DIRECTORIES_H_

#include "inode2filename/inode.h"
#include "inode2filename/system_call.h"
#include "inode2filename/inode_resolver.h"

#include <fruit/fruit.h>

#include <rxcpp/rx.hpp>
namespace iorap::inode2filename {

// TODO: rename.
using SearchMode = ProcessMode;

struct SearchDirectories {
  // Type-erased subset of rxcpp::connectable_observable<?>
  struct RxAnyConnectable {
    // Connects to the underlying observable.
    //
    // This kicks off the graph, streams begin emitting items.
    // This method will block until all items have been fully emitted
    // and processed by any subscribers.
    virtual void connect() = 0;

    virtual ~RxAnyConnectable(){}
  };


  // Create a cold observable of inode results (a lazy stream) corresponding
  // to the inode search list.
  //
  // A depth-first search is done on each of the root directories (in order),
  // until all inodes have been found (or until all directories have been exhausted).
  //
  // Some internal errors may occur during emission that aren't part of an InodeResult;
  // these will be sent to the error logcat and dropped.
  //
  // Calling this function does not begin the search.
  // The returned observable will begin the search after subscribing to it.
  //
  // The emitted InodeResult stream has these guarantees:
  // - All inodes in inode_list will eventually be emitted exactly once in an InodeResult
  // - When all inodes are found, directory traversal is halted.
  // - The order of emission can be considered arbitrary.
  //
  // Lifetime rules:
  // - The observable must be fully consumed before deleting any of the SearchDirectory's
  //   borrowed constructor parameters (e.g. the SystemCall).
  // - SearchDirectory itself can be deleted at any time after creating an observable.
  rxcpp::observable<InodeResult>
      FindFilenamesFromInodes(std::vector<std::string> root_directories,
                              std::vector<Inode> inode_list,
                              SearchMode mode) const;

  // Create a cold observable of inode results (a lazy stream) corresponding
  // to the inode search list.
  //
  // A depth-first search is done on each of the root directories (in order),
  // until all inodes have been found (or until all directories have been exhausted).
  //
  // Some internal errors may occur during emission that aren't part of an InodeResult;
  // these will be sent to the error logcat and dropped.
  //
  // Calling this function does not begin the search.
  // The returned observable will begin the search after subscribing to it.
  //
  // The emitted InodeResult stream has these guarantees:
  // - All inodes in inode_list will eventually be emitted exactly once in an InodeResult
  // - When all inodes are found, directory traversal is halted.
  // - The order of emission can be considered arbitrary.
  //
  // Lifetime rules:
  // - The observable must be fully consumed before deleting any of the SearchDirectory's
  //   borrowed constructor parameters (e.g. the SystemCall).
  // - SearchDirectory itself can be deleted at any time after creating an observable.
  std::pair<rxcpp::observable<InodeResult>, std::unique_ptr<RxAnyConnectable>>
      FindFilenamesFromInodesPair(std::vector<std::string> root_directories,
                                  std::vector<Inode> inode_list,
                                  SearchMode mode) const;

  // No items on the output stream will be emitted until 'inodes' completes.
  //
  // The current algorithm is a naive DFS, so if it began too early it would either
  // miss the search items or require traversal restarts.
  //
  // See above for more details.
  rxcpp::observable<InodeResult>
      FindFilenamesFromInodes(std::vector<std::string> root_directories,
                              rxcpp::observable<Inode> inodes,
                              SearchMode mode) const;

  rxcpp::observable<InodeResult>
      ListAllFilenames(std::vector<std::string> root_directories) const;

  rxcpp::observable<InodeResult> FilterFilenamesForSpecificInodes(
      // haystack that will be subscribed to until all in inode_list are found.
      rxcpp::observable<InodeResult> all_inodes,
      // key list: traverse all_inodes until we emit all results from inode_list.
      std::vector<Inode> inode_list,
      // all_inodes have a missing device number: use stat(2) to fill it in.
      bool missing_device_number,
      bool needs_verification) const;

  rxcpp::observable<InodeResult> EmitAllFilenames(
      // haystack that will be subscribed to until all in inode_list are found.
      rxcpp::observable<InodeResult> all_inodes,
      // all_inodes have a missing device number: use stat(2) to fill it in.
      bool missing_device_number,
      bool needs_verification) const;

  // Any borrowed parameters here can also be borrowed by the observables returned by the above
  // member functions.
  //
  // The observables must be fully consumed within the lifetime of the borrowed parameters.
  INJECT(SearchDirectories(borrowed<SystemCall*> system_call))
      : system_call_(system_call) {}

  // TODO: is there a way to get rid of this second RxAnyConnectable parameter?
 private:
  // This gets passed around to lazy lambdas, so we must finish consuming any observables
  // before the injected system call is deleted.
  borrowed<SystemCall*> system_call_;
};

}  // namespace iorap::inode2filename

#endif  // IORAP_SRC_INODE2FILENAME_SEARCH_DIRECTORIES_H_
