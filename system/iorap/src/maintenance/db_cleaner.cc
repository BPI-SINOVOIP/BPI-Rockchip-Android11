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

#include "maintenance/db_cleaner.h"

#include <android-base/file.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "db/clean_up.h"
#include "db/file_models.h"
#include "db/models.h"

namespace iorap::maintenance {

// Enable foreign key restriction.
const constexpr char* kForeignKeyOnSql = "PRAGMA foreign_keys = ON;";

void CleanUpDatabase(const db::DbHandle& db,
                     std::shared_ptr<binder::PackageVersionMap> version_map) {
  std::vector<db::PackageModel> packages = db::PackageModel::SelectAll(db);
  // Enable cascade deletion.
  if (!db::DbQueryBuilder::ExecuteOnce(db, kForeignKeyOnSql)) {
    LOG(ERROR) << "Fail to turn on foreign key restraint!";
  }

  for (db::PackageModel package : packages) {
    std::optional<int64_t> version = version_map->Find(package.name);
    if (!version) {
      LOG(DEBUG) << "Fail to find version for package " << package.name
                 << " with version " << package.version
                 << ". The package manager may be down.";
      continue;
    }
    // Package is cleanup if it
    // * is not in the version map, it may be uninstalled
    // * has an different version with the latest one
    if (*version != package.version) {
      db::CleanUpFilesForPackage(db, package.id, package.name, package.version);
      if (!package.Delete()) {
        LOG(ERROR) << "Fail to delete package " << package.name
                   << " with version " << package.version;
      }
    }
  }
}

}  // namespace iorap::maintenance
