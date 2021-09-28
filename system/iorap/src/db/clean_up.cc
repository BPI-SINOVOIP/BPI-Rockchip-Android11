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

#include "db/clean_up.h"

#include <android-base/file.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "db/file_models.h"
#include "db/models.h"

namespace iorap::db {

void CleanUpFilesForActivity(const db::DbHandle& db,
                             const db::VersionedComponentName& vcn) {
  LOG(DEBUG) << "Clean up files for activity " << vcn.GetActivity();
  // Remove perfetto traces.
  std::vector<db::RawTraceModel> raw_traces =
      db::RawTraceModel::SelectByVersionedComponentName(db, vcn);
  for (db::RawTraceModel raw_trace : raw_traces) {
    std::string file_path = raw_trace.file_path;
    LOG(DEBUG) << "Remove file: " << file_path;
    std::filesystem::remove(file_path.c_str());
    raw_trace.Delete();
  }

  // Remove compiled traces.
  std::optional<db::PrefetchFileModel> prefetch_file =
      db::PrefetchFileModel::SelectByVersionedComponentName(db, vcn);

  if (prefetch_file) {
    std::string file_path = prefetch_file->file_path;
    LOG(DEBUG) << "Remove file: " << file_path;
    std::filesystem::remove(file_path.c_str());
    prefetch_file->Delete();
  }
}

void CleanUpFilesForPackage(const db::DbHandle& db,
                            int package_id,
                            const std::string& package_name,
                            int64_t version) {
  LOG(DEBUG) << "Clean up files for package " << package_name << " with version "
             << version;
  std::vector<db::ActivityModel> activities =
      db::ActivityModel::SelectByPackageId(db, package_id);

  for (db::ActivityModel activity : activities) {
    db::VersionedComponentName vcn{package_name, activity.name, version};
    CleanUpFilesForActivity(db, vcn);
  }
}

void CleanUpFilesForPackage(const db::DbHandle& db,
                            const std::string& package_name,
                            int64_t version) {
  std::optional<PackageModel> package =
        PackageModel::SelectByNameAndVersion(db, package_name.c_str(), version);

  if (!package) {
    LOG(DEBUG) << "No package to clean up " << package_name << " with version " << version;
    return;
  }

  CleanUpFilesForPackage(db, package->id, package_name, version);
}

void CleanUpFilesForDb(const db::DbHandle& db) {
  std::vector<db::PackageModel> packages = db::PackageModel::SelectAll(db);
  for (db::PackageModel package : packages) {
      CleanUpFilesForPackage(db, package.id, package.name, package.version);
  }
}

void CleanUpFilesForPackage(const std::string& db_path,
                            const std::string& package_name) {
  iorap::db::SchemaModel db_schema = db::SchemaModel::GetOrCreate(db_path);
  db::DbHandle db{db_schema.db()};
  CleanUpFilesForPackage(db, package_name);
}


void CleanUpFilesForPackage(const db::DbHandle& db,
                            const std::string& package_name) {
  std::vector<PackageModel> packages = PackageModel::SelectByName(db, package_name.c_str());;

  for (PackageModel& package : packages) {
    CleanUpFilesForPackage(db, package.id, package.name, package.version);
  }

  if (packages.empty()) {
    LOG(DEBUG) << "No package rows to clean up " << package_name;
  }
}

}  // namespace iorap::db
