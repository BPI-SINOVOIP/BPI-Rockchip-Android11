/*
 * Copyright (C) 2019 The Android Open Source Project
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

#pragma once

#include <string>

#include "apex_file.h"

namespace android {
namespace apex {

enum PrepareHashTreeResult {
  kReuse = 0,
  KRegenerate = 1,
};

// Generates a dm-verity hashtree of a given |apex| if |hashtree_file| doesn't
// exist or it's root_digest doesn't match |verity_data.root_digest|. Otherwise
// does nothing.
android::base::Result<PrepareHashTreeResult> PrepareHashTree(
    const ApexFile& apex, const ApexVerityData& verity_data,
    const std::string& hashtree_file);

void RemoveObsoleteHashTrees();

}  // namespace apex
}  // namespace android
