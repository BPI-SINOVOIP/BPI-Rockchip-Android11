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

#ifndef IORAP_SRC_DB_CLEANER_H_
#define IORAP_SRC_DB_CLEANER_H_

#include <android/content/pm/IPackageManagerNative.h>

#include <string>
#include <vector>

#include "binder/package_version_map.h"
#include "db/file_models.h"

namespace iorap::db {

// Clean up perfetto traces and compiled traces in disk and rows
// in raw_traces and prefetch_files in the db.
void CleanUpFilesForDb(const db::DbHandle& db);

// Clean up perfetto traces and compiled traces in disk and rows
// in raw_traces and prefetch_files in the db for a package id.
void CleanUpFilesForPackage(const db::DbHandle& db,
                            int package_id,
                            const std::string& package_name,
                            int64_t version);

// Clean up all package rows (and files) associated with a package by name.
void CleanUpFilesForPackage(const std::string& db_path,
                            const std::string& package_name);

// Clean up all package rows (and files) associated with a package by name.
void CleanUpFilesForPackage(const db::DbHandle& db,
                            const std::string& package_name);
// Clean up perfetto traces and compiled traces in disk and rows
// in raw_traces and prefetch_files in the db for a package name
// and version.
void CleanUpFilesForPackage(const db::DbHandle& db,
                            const std::string& package_name,
                            int64_t version);
}  // namespace iorap::db

#endif  // IORAP_SRC_DB_CLEANER_H_
