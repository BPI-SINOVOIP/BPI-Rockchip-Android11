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

#include <system/camera_metadata.h>
#include <camera_metadata_hidden.h>

#define LOG_TAG "GCH_VendorTagUtils"
#include <log/log.h>

#include <map>
#include <string>
#include <unordered_set>

#include "vendor_tag_utils.h"

namespace android {
namespace google_camera_hal {
namespace vendor_tag_utils {

status_t CombineVendorTags(const std::vector<VendorTagSection>& source1,
                           const std::vector<VendorTagSection>& source2,
                           std::vector<VendorTagSection>* destination) {
  if (destination == nullptr) {
    ALOGE("%s destination is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  // Temporary sets to guarantee the uniqueness of IDs and tag names
  std::unordered_set<std::string> tag_names;
  std::unordered_set<uint32_t> tag_ids;
  // Maps unique vendor tag section names to a list of tags
  std::map<std::string, std::vector<VendorTag>> section_tags;

  // Loop through the source1 and source2 section lists
  for (auto& section_list : {source1, source2}) {
    // Loop through every section
    for (const VendorTagSection& section : section_list) {
      // Loop through every tag in this section's tag list
      for (const VendorTag& tag : section.tags) {
        section_tags[section.section_name].push_back(tag);

        // Ensure that the vendor tag name is unique
        std::string full_tag_name = section.section_name + "." + tag.tag_name;
        auto [name_it, name_inserted] = tag_names.insert(full_tag_name);
        if (!name_inserted) {
          ALOGE(
              "%s Error! Vendor tag name collision: Tag %s is used more than "
              "once",
              __FUNCTION__, full_tag_name.c_str());
          return BAD_VALUE;
        }

        // Ensure that the vendor tag ID is unique
        auto [id_it, id_inserted] = tag_ids.insert(tag.tag_id);
        if (!id_inserted) {
          ALOGE(
              "%s Error! Vendor tag ID collision: Tag 0x%x (%u) is used more "
              "than once",
              __FUNCTION__, tag.tag_id, tag.tag_id);
          return BAD_VALUE;
        }
      }
    }
  }

  // Convert the section_tags map to the resulting type
  destination->resize(section_tags.size());
  size_t index = 0;
  for (auto& [section_name, section_tags] : section_tags) {
    destination->at(index).section_name = section_name;
    destination->at(index).tags = section_tags;
    index++;
  }

  return OK;
}
}  // namespace vendor_tag_utils

// Vendor tag operations called by the camera metadata framework
static int GetCount(const vendor_tag_ops_t* /*tag_ops*/) {
  return VendorTagManager::GetInstance().GetCount();
}

static void GetAllTags(const vendor_tag_ops_t* /*tag_ops*/,
                       uint32_t* tag_array) {
  return VendorTagManager::GetInstance().GetAllTags(tag_array);
}

static const char* GetSectionName(const vendor_tag_ops_t* /*tag_ops*/,
                                  uint32_t tag_id) {
  return VendorTagManager::GetInstance().GetSectionName(tag_id);
}

static const char* GetTagName(const vendor_tag_ops_t* /*tag_ops*/,
                              uint32_t tag_id) {
  return VendorTagManager::GetInstance().GetTagName(tag_id);
}

static int GetTagType(const vendor_tag_ops_t* /*tag_ops*/, uint32_t tag_id) {
  return VendorTagManager::GetInstance().GetTagType(tag_id);
}

VendorTagManager& VendorTagManager::GetInstance() {
  static VendorTagManager instance;
  return instance;
}

status_t VendorTagManager::AddTags(
    const std::vector<VendorTagSection>& tag_sections) {
  std::lock_guard<std::mutex> lock(api_mutex_);

  std::vector<VendorTagSection> combined_tags;
  status_t res = vendor_tag_utils::CombineVendorTags(
      tag_sections_, tag_sections, &combined_tags);
  if (res != OK) {
    ALOGE("%s: CombineVendorTags() failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }
  tag_sections_ = combined_tags;

  // Add new tags to internal maps to help speed up the metadata framework
  // lookup calls
  for (auto& section : tag_sections) {
    for (auto& tag : section.tags) {
      vendor_tag_map_[tag.tag_id] =
          VendorTagInfo{.tag_id = tag.tag_id,
                        .tag_type = static_cast<int>(tag.tag_type),
                        .section_name = section.section_name,
                        .tag_name = tag.tag_name};

      vendor_tag_inverse_map_[TagString(section.section_name, tag.tag_name)] =
          tag.tag_id;
    }
  }

  // Vendor tag callbacks used by the camera metadata framework
  static vendor_tag_ops_t vendor_tag_ops = {
      .get_tag_count = google_camera_hal::GetCount,
      .get_all_tags = google_camera_hal::GetAllTags,
      .get_section_name = google_camera_hal::GetSectionName,
      .get_tag_name = google_camera_hal::GetTagName,
      .get_tag_type = google_camera_hal::GetTagType,
  };
  res = set_camera_metadata_vendor_ops(&vendor_tag_ops);
  if (res != OK) {
    ALOGE("%s: set_camera_metadata_vendor_ops() failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  return OK;
}

const std::vector<VendorTagSection>& VendorTagManager::GetTags() const {
  return tag_sections_;
}

void VendorTagManager::Reset() {
  std::lock_guard<std::mutex> lock(api_mutex_);
  vendor_tag_map_.clear();
  tag_sections_.clear();
  set_camera_metadata_vendor_ops(nullptr);
}

int VendorTagManager::GetCount() const {
  std::lock_guard<std::mutex> lock(api_mutex_);
  return static_cast<int>(vendor_tag_map_.size());
}

void VendorTagManager::GetAllTags(uint32_t* tag_array) const {
  std::lock_guard<std::mutex> lock(api_mutex_);
  if (tag_array == nullptr) {
    ALOGE("%s tag_array is nullptr", __FUNCTION__);
    return;
  }

  uint32_t index = 0;
  for (auto& [tag_id, tag_descriptor] : vendor_tag_map_) {
    tag_array[index++] = tag_id;
  }
}

const char* VendorTagManager::GetSectionName(uint32_t tag_id) const {
  std::lock_guard<std::mutex> lock(api_mutex_);
  auto it = vendor_tag_map_.find(tag_id);
  if (it == vendor_tag_map_.end()) {
    ALOGE("%s Unknown vendor tag ID: %u", __FUNCTION__, tag_id);
    return "unknown";
  }

  return it->second.section_name.c_str();
}

const char* VendorTagManager::GetTagName(uint32_t tag_id) const {
  std::lock_guard<std::mutex> lock(api_mutex_);
  auto it = vendor_tag_map_.find(tag_id);
  if (it == vendor_tag_map_.end()) {
    ALOGE("%s Unknown vendor tag ID: %u", __FUNCTION__, tag_id);
    return "unknown";
  }

  return it->second.tag_name.c_str();
}

int VendorTagManager::GetTagType(uint32_t tag_id) const {
  std::lock_guard<std::mutex> lock(api_mutex_);
  auto it = vendor_tag_map_.find(tag_id);
  if (it == vendor_tag_map_.end()) {
    ALOGE("%s Unknown vendor tag ID: 0x%x (%u)", __FUNCTION__, tag_id, tag_id);
    return -1;
  }

  return it->second.tag_type;
}

status_t VendorTagManager::GetTagInfo(uint32_t tag_id, VendorTagInfo* tag_info) {
  if (tag_info == nullptr) {
    ALOGE("%s tag_info is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }
  std::lock_guard<std::mutex> lock(api_mutex_);
  auto itr = vendor_tag_map_.find(tag_id);
  if (itr == vendor_tag_map_.end()) {
    ALOGE("%s Given tag_id not found", __FUNCTION__);
    return BAD_VALUE;
  }

  *tag_info = itr->second;
  return OK;
}

status_t VendorTagManager::GetTag(const std::string section_name,
                                  const std::string tag_name, uint32_t* tag_id) {
  if (tag_id == nullptr) {
    ALOGE("%s tag_id is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }
  std::lock_guard<std::mutex> lock(api_mutex_);

  const TagString section_tag{section_name, tag_name};

  auto itr = vendor_tag_inverse_map_.find(section_tag);
  if (itr == vendor_tag_inverse_map_.end()) {
    ALOGE("%s Given section/tag names not found", __FUNCTION__);
    return BAD_VALUE;
  }

  *tag_id = itr->second;
  return OK;
}

}  // namespace google_camera_hal
}  // namespace android
