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

#include "OfflineUnwinder_impl.h"

#include <gtest/gtest.h>

using namespace simpleperf;

bool CheckUnwindMaps(UnwindMaps& maps, const MapSet& map_set) {
  if (maps.Total() != map_set.maps.size()) {
    return false;
  }
  unwindstack::MapInfo* prev_real_map = nullptr;
  for (size_t i = 0; i < maps.Total(); i++) {
    unwindstack::MapInfo* info = maps.Get(i);
    if (info == nullptr || map_set.maps.find(info->start) == map_set.maps.end()) {
      return false;
    }
    if (info->prev_real_map != prev_real_map) {
      return false;
    }
    if (!info->IsBlank()) {
      prev_real_map = info;
    }
  }
  return true;
}

TEST(OfflineUnwinder, UnwindMaps) {
  // 1. Create fake map entries.
  std::unique_ptr<Dso> fake_dso = Dso::CreateDso(DSO_UNKNOWN_FILE, "unknown");
  std::vector<MapEntry> map_entries;
  for (size_t i = 0; i < 10; i++) {
    map_entries.emplace_back(i, 1, i, fake_dso.get(), false);
  }

  // 2. Init with empty maps.
  MapSet map_set;
  UnwindMaps maps;
  maps.UpdateMaps(map_set);
  ASSERT_TRUE(CheckUnwindMaps(maps, map_set));

  // 3. Add maps starting from even addr.
  map_set.version = 1;
  for (size_t i = 0; i < map_entries.size(); i += 2) {
    map_set.maps.insert(std::make_pair(map_entries[i].start_addr, &map_entries[i]));
  }

  maps.UpdateMaps(map_set);
  ASSERT_TRUE(CheckUnwindMaps(maps, map_set));

  // 4. Add maps starting from odd addr.
  map_set.version = 2;
  for (size_t i = 1; i < 10; i += 2) {
    map_set.maps.insert(std::make_pair(map_entries[i].start_addr, &map_entries[i]));
  }
  maps.UpdateMaps(map_set);
  ASSERT_TRUE(CheckUnwindMaps(maps, map_set));

  // 5. Remove maps starting from even addr.
  map_set.version = 3;
  for (size_t i = 0; i < 10; i += 2) {
    map_set.maps.erase(map_entries[i].start_addr);
  }
  maps.UpdateMaps(map_set);
  ASSERT_TRUE(CheckUnwindMaps(maps, map_set));

  // 6. Remove all maps.
  map_set.version = 4;
  map_set.maps.clear();
  maps.UpdateMaps(map_set);
  ASSERT_TRUE(CheckUnwindMaps(maps, map_set));
}
