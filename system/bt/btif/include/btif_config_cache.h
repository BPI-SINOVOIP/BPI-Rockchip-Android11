/*
 *  Copyright 2020 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#pragma once

#include <map>
#include <unordered_set>

#include "common/lru.h"
#include "osi/include/config.h"
#include "osi/include/log.h"
#include "raw_address.h"

class BtifConfigCache {
 public:
  explicit BtifConfigCache(size_t capacity);
  ~BtifConfigCache();

  void Clear();
  void Init(std::unique_ptr<config_t> source);
  const std::list<section_t>& GetPersistentSections();
  config_t PersistentSectionCopy();
  bool HasSection(const std::string& section_name);
  bool HasUnpairedSection(const std::string& section_name);
  bool HasPersistentSection(const std::string& section_name);
  bool HasKey(const std::string& section_name, const std::string& key);
  bool RemoveKey(const std::string& section_name, const std::string& key);
  void RemovePersistentSectionsWithKey(const std::string& key);

  // Setters and getters
  void SetString(std::string section_name, std::string key, std::string value);
  std::optional<std::string> GetString(const std::string& section_name,
                                       const std::string& key);
  void SetInt(std::string section_name, std::string key, int value);
  std::optional<int> GetInt(const std::string& section_name,
                            const std::string& key);
  void SetUint64(std::string section_name, std::string key, uint64_t value);
  std::optional<uint64_t> GetUint64(const std::string& section_name,
                                    const std::string& key);
  void SetBool(std::string section_name, std::string key, bool value);
  std::optional<bool> GetBool(const std::string& section_name,
                              const std::string& key);

 private:
  bluetooth::common::LruCache<std::string, section_t> unpaired_devices_cache_;
  config_t paired_devices_list_;
};
