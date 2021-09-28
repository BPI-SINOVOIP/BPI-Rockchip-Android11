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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_UTILS_HAL_VENDOR_TAG_INTERFACE_H
#define HARDWARE_GOOGLE_CAMERA_HAL_UTILS_HAL_VENDOR_TAG_INTERFACE_H

#include "hal_types.h"

#include <utils/Errors.h>
#include <vector>

namespace android {
namespace google_camera_hal {

struct VendorTagInfo {
  uint32_t tag_id = 0;
  int tag_type = TYPE_BYTE;
  std::string section_name;
  std::string tag_name;
};

// This is an interface for accessing vendor tags in HAL
class VendorTagInterface {
 public:
  // Get Tag info for the give tag id
  virtual status_t GetTagInfo(uint32_t tag_id, VendorTagInfo* tag_info) = 0;

  // Get the tag id for the given section/name
  virtual status_t GetTag(const std::string section_name,
                          const std::string tag_name, uint32_t* tag_id);

 protected:
  virtual ~VendorTagInterface(){};
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_UTILS_HAL_VENDOR_TAG_INTERFACE_H
