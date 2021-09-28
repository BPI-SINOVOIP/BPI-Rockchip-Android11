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

#ifndef UPDATE_ENGINE_DYNAMIC_PARTITION_UTILS_H_
#define UPDATE_ENGINE_DYNAMIC_PARTITION_UTILS_H_

#include <string>

#include <liblp/builder.h>

namespace chromeos_update_engine {

// Delete all groups (and their partitions) in |builder| that have names
// ending with |suffix|.
void DeleteGroupsWithSuffix(android::fs_mgr::MetadataBuilder* builder,
                            const std::string& suffix);

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_DYNAMIC_PARTITION_UTILS_H_
