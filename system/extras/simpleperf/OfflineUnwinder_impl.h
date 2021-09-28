/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <unwindstack/Maps.h>

#include "thread_tree.h"

namespace simpleperf {

class UnwindMaps : public unwindstack::Maps {
 public:
  void UpdateMaps(const MapSet& map_set);

 private:
  uint64_t version_ = 0u;
  std::vector<const MapEntry*> entries_;
};

}  // namespace simpleperf
