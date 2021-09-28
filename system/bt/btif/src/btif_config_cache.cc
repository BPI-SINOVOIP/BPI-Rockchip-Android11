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

#include <limits>

#include "btif_config_cache.h"

namespace {

const std::unordered_set<std::string> kLinkKeyTypes = {
    "LinkKey",      "LE_KEY_PENC", "LE_KEY_PID",
    "LE_KEY_PCSRK", "LE_KEY_LENC", "LE_KEY_LCSRK"};

const std::unordered_set<std::string> kLocalSectionNames = {"Info", "Metrics",
                                                            "Adapter"};

bool is_link_key(const std::string& key) {
  return kLinkKeyTypes.find(key) != kLinkKeyTypes.end();
}

bool has_link_key_in_section(const section_t& section) {
  for (const auto& entry : section.entries) {
    if (is_link_key(entry.key)) {
      return true;
    }
  }
  return false;
}

bool is_local_section_info(const std::string& section) {
  return kLocalSectionNames.find(section) != kLocalSectionNames.end();
}

// trim new line in place, return true if newline was found
bool trim_new_line(std::string& value) {
  size_t newline_position = value.find_first_of('\n');
  if (newline_position != std::string::npos) {
    value.erase(newline_position);
    return true;
  }
  return false;
}

}  // namespace

BtifConfigCache::BtifConfigCache(size_t capacity)
    : unpaired_devices_cache_(capacity, "bt_config_cache") {
  LOG(INFO) << __func__ << ", capacity: " << capacity;
}

BtifConfigCache::~BtifConfigCache() { Clear(); }

void BtifConfigCache::Clear() {
  unpaired_devices_cache_.Clear();
  paired_devices_list_.sections.clear();
}

void BtifConfigCache::Init(std::unique_ptr<config_t> source) {
  // get the config persistent data from btif_config file
  paired_devices_list_ = std::move(*source);
  source.reset();
}

bool BtifConfigCache::HasPersistentSection(const std::string& section_name) {
  return paired_devices_list_.Find(section_name) !=
         paired_devices_list_.sections.end();
}

bool BtifConfigCache::HasUnpairedSection(const std::string& section_name) {
  return unpaired_devices_cache_.HasKey(section_name);
}

bool BtifConfigCache::HasSection(const std::string& section_name) {
  return HasUnpairedSection(section_name) || HasPersistentSection(section_name);
}

bool BtifConfigCache::HasKey(const std::string& section_name,
                             const std::string& key) {
  auto section_iter = paired_devices_list_.Find(section_name);
  if (section_iter != paired_devices_list_.sections.end()) {
    return section_iter->Has(key);
  }
  section_t* section = unpaired_devices_cache_.Find(section_name);
  if (section == nullptr) {
    return false;
  }
  return section->Has(key);
}

// remove sections with the restricted key
void BtifConfigCache::RemovePersistentSectionsWithKey(const std::string& key) {
  for (auto it = paired_devices_list_.sections.begin();
       it != paired_devices_list_.sections.end();) {
    if (it->Has(key)) {
      it = paired_devices_list_.sections.erase(it);
      continue;
    }
    it++;
  }
}

/* remove a key from section, section itself is removed when empty */
bool BtifConfigCache::RemoveKey(const std::string& section_name,
                                const std::string& key) {
  section_t* section = unpaired_devices_cache_.Find(section_name);
  if (section != nullptr) {
    auto entry_iter = section->Find(key);
    if (entry_iter == section->entries.end()) {
      return false;
    }
    section->entries.erase(entry_iter);
    if (section->entries.empty()) {
      unpaired_devices_cache_.Remove(section_name);
    }
    return true;
  } else {
    auto section_iter = paired_devices_list_.Find(section_name);
    if (section_iter == paired_devices_list_.sections.end()) {
      return false;
    }
    auto entry_iter = section_iter->Find(key);
    if (entry_iter == section_iter->entries.end()) {
      return false;
    }
    section_iter->entries.erase(entry_iter);
    if (section_iter->entries.empty()) {
      paired_devices_list_.sections.erase(section_iter);
    } else if (!has_link_key_in_section(*section_iter)) {
      // if no link key in section after removal, move it to unpaired section
      auto moved_section = std::move(*section_iter);
      paired_devices_list_.sections.erase(section_iter);
      unpaired_devices_cache_.Put(section_name, std::move(moved_section));
    }
    return true;
  }
}

/* clone persistent sections (Local Adapter sections, remote paired devices
 * section,..) */
config_t BtifConfigCache::PersistentSectionCopy() {
  return paired_devices_list_;
}

const std::list<section_t>& BtifConfigCache::GetPersistentSections() {
  return paired_devices_list_.sections;
}

void BtifConfigCache::SetString(std::string section_name, std::string key,
                                std::string value) {
  if (trim_new_line(section_name) || trim_new_line(key) ||
      trim_new_line(value)) {
    android_errorWriteLog(0x534e4554, "70808273");
  }
  if (section_name.empty()) {
    LOG(FATAL) << "Empty section not allowed";
    return;
  }
  if (key.empty()) {
    LOG(FATAL) << "Empty key not allowed";
    return;
  }
  if (!paired_devices_list_.Has(section_name)) {
    // section is not in paired_device_list, handle it in unpaired devices cache
    section_t section = {};
    bool in_unpaired_cache = true;
    if (!unpaired_devices_cache_.Get(section_name, &section)) {
      // it's a new unpaired section, add it to unpaired devices cache
      section.name = section_name;
      in_unpaired_cache = false;
    }
    // set key to value and replace existing key if already exist
    section.Set(key, value);

    if (is_local_section_info(section_name) ||
        (is_link_key(key) && RawAddress::IsValidAddress(section_name))) {
      // remove this section that has the LinkKey from unpaired devices cache.
      if (in_unpaired_cache) {
        unpaired_devices_cache_.Remove(section_name);
      }
      // when a unpaired section got the LinkKey, move this section to the
      // paired devices list
      paired_devices_list_.sections.emplace_back(std::move(section));
    } else {
      // update to the unpaired devices cache
      unpaired_devices_cache_.Put(section_name, section);
    }
  } else {
    // already have section in paired device list, add key-value entry.
    auto section_found = paired_devices_list_.Find(section_name);
    if (section_found == paired_devices_list_.sections.end()) {
      LOG(WARNING) << __func__ << " , section_found not found!";
      return;
    }
    section_found->Set(key, value);
  }
}

std::optional<std::string> BtifConfigCache::GetString(
    const std::string& section_name, const std::string& key) {
  // Check paired sections first
  auto section_iter = paired_devices_list_.Find(section_name);
  if (section_iter != paired_devices_list_.sections.end()) {
    auto entry_iter = section_iter->Find(key);
    if (entry_iter == section_iter->entries.end()) {
      return std::nullopt;
    }
    return entry_iter->value;
  }
  // Check unpaired sections later
  section_t section = {};
  if (!unpaired_devices_cache_.Get(section_name, &section)) {
    return std::nullopt;
  }
  auto entry_iter = section.Find(key);
  if (entry_iter == section.entries.end()) {
    return std::nullopt;
  }
  return entry_iter->value;
}

void BtifConfigCache::SetInt(std::string section_name, std::string key,
                             int value) {
  SetString(std::move(section_name), std::move(key), std::to_string(value));
}

std::optional<int> BtifConfigCache::GetInt(const std::string& section_name,
                                           const std::string& key) {
  auto value = GetString(section_name, key);
  if (!value) {
    return std::nullopt;
  }
  char* endptr;
  long ret_long = strtol(value->c_str(), &endptr, 0);
  if (*endptr != '\0') {
    LOG(WARNING) << "Failed to parse value to long for section " << section_name
                 << ", key " << key;
    return std::nullopt;
  }
  if (ret_long >= std::numeric_limits<int>::max()) {
    LOG(WARNING) << "Integer overflow when parsing value to int for section "
                 << section_name << ", key " << key;
    return std::nullopt;
  }
  return static_cast<int>(ret_long);
}

void BtifConfigCache::SetUint64(std::string section_name, std::string key,
                                uint64_t value) {
  SetString(std::move(section_name), std::move(key), std::to_string(value));
}

std::optional<uint64_t> BtifConfigCache::GetUint64(
    const std::string& section_name, const std::string& key) {
  auto value = GetString(section_name, key);
  if (!value) {
    return std::nullopt;
  }
  char* endptr;
  uint64_t ret = strtoull(value->c_str(), &endptr, 0);
  if (*endptr != '\0') {
    LOG(WARNING) << "Failed to parse value to uint64 for section "
                 << section_name << ", key " << key;
    return std::nullopt;
  }
  return ret;
}

void BtifConfigCache::SetBool(std::string section_name, std::string key,
                              bool value) {
  SetString(std::move(section_name), std::move(key), value ? "true" : "false");
}

std::optional<bool> BtifConfigCache::GetBool(const std::string& section_name,
                                             const std::string& key) {
  auto value = GetString(section_name, key);
  if (!value) {
    return std::nullopt;
  }
  if (*value == "true") {
    return true;
  }
  if (*value == "false") {
    return false;
  }
  LOG(WARNING) << "Failed to parse value to boolean for section "
               << section_name << ", key " << key;
  return std::nullopt;
}
