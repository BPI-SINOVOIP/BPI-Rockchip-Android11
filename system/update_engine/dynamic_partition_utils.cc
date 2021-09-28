//
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
//

#include "update_engine/dynamic_partition_utils.h"

#include <vector>

#include <base/logging.h>
#include <base/strings/string_util.h>

using android::fs_mgr::MetadataBuilder;

namespace chromeos_update_engine {

void DeleteGroupsWithSuffix(MetadataBuilder* builder,
                            const std::string& suffix) {
  std::vector<std::string> groups = builder->ListGroups();
  for (const auto& group_name : groups) {
    if (base::EndsWith(group_name, suffix, base::CompareCase::SENSITIVE)) {
      LOG(INFO) << "Removing group " << group_name;
      builder->RemoveGroupAndPartitions(group_name);
    }
  }
}

}  // namespace chromeos_update_engine
