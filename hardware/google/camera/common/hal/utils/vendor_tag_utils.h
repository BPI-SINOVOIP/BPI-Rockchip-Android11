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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_CAMERA_VENDOR_TAG_UTILS_H
#define HARDWARE_GOOGLE_CAMERA_HAL_CAMERA_VENDOR_TAG_UTILS_H

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "hal_types.h"
#include "vendor_tag_interface.h"

namespace android {
namespace google_camera_hal {
namespace vendor_tag_utils {

// Returns the list of combined vendor tags. Returns an error if any tag IDs
// overlap, or if any vendor tag (section name + tag name) overlaps
status_t CombineVendorTags(const std::vector<VendorTagSection>& source1,
                           const std::vector<VendorTagSection>& source2,
                           std::vector<VendorTagSection>* destination);

}  // namespace vendor_tag_utils

// Utility class to create vendor tag descriptors from a list of vendor tag
// sections, in order to provide to camera metadata framework. This class helps
// build a wrapper around vendor tag operations expected by the camera
// framework. These are eseentially the 'vendor_tag_ops_t' to be set by calling
// set_camera_metadata_vendor_ops(). This wrapper is a singleton because there
// could be only one set of callbacks set per camera provider. The HWL or HAL
// layers should use this wrapper instead of directly invoking
// set_camera_metadata_vendor_ops()
class VendorTagManager : public VendorTagInterface {
 public:
  static VendorTagManager& GetInstance();

  virtual ~VendorTagManager() = default;

  // Add a set of vendor tags, combine them with any tags added earlier, and set
  // callbacks for the camera metadata framework if they haven't been set
  // already.
  status_t AddTags(const std::vector<VendorTagSection>& tag_sections);

  // Get the combined list of all tags that have been added so far.
  const std::vector<VendorTagSection>& GetTags() const;

  // Clears all the vendor tag data that was set via AddTags(), and resets
  // the vendor tag operations previously set to the camera metadata framework
  void Reset();

  // Vendor tag operations needed by camera metadata framework, as defined in
  // vendor_tag_ops_t struct.
  int GetCount() const;
  void GetAllTags(uint32_t* tag_array) const;
  const char* GetSectionName(uint32_t tag_id) const;
  const char* GetTagName(uint32_t tag_id) const;
  int GetTagType(uint32_t tag_id) const;

  // Disallow copy and assignment operators
  VendorTagManager(VendorTagManager const&) = delete;
  VendorTagManager& operator=(VendorTagManager const&) = delete;

  // Get Tag info for the give tag id
  status_t GetTagInfo(uint32_t tag_id, VendorTagInfo* tag_info) override;

  // Get the tag id for the given section/name
  status_t GetTag(const std::string section_name, const std::string tag_name,
                  uint32_t* tag_id) override;

 private:
  VendorTagManager() = default;

  // Map from vendor tag ID to VendorTagInfo. Used for camera framework
  // vendor tag callbacks, protected by api_mutex_.
  std::unordered_map<uint32_t, VendorTagInfo> vendor_tag_map_;

  using TagString = std::pair<std::string, std::string>;

  struct TagStringHash {
    size_t operator()(const TagString& pair) const {
      std::hash<TagString::first_type> h1;
      std::hash<TagString::second_type> h2;
      return h1(pair.first) ^ h2(pair.second);
    }
  };

  std::unordered_map<const TagString, uint32_t, TagStringHash>
      vendor_tag_inverse_map_;

  // Protects the public entry points into this class.
  mutable std::mutex api_mutex_;

  // Combined list of all tags added with AddTags().
  std::vector<VendorTagSection> tag_sections_;
};
}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_CAMERA_VENDOR_TAG_UTILS_H
