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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_UTILS_CAMERA_ID_MANAGER_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_UTILS_CAMERA_ID_MANAGER_H_

#include <utils/Errors.h>
#include <memory>
#include <string>
#include <vector>

#include "hal_types.h"

namespace android {
namespace google_camera_hal {

using ::android::status_t;

// Invalid camera ID
constexpr uint32_t kInvalidCameraId = std::numeric_limits<uint32_t>::max();

// Holds information about a camera's IDs
struct CameraIdMap {
  // An unique camera ID. This should be the internal camera ID and does not
  // correspond to the public camera ID published to the camera framework
  uint32_t id = kInvalidCameraId;

  // Whether this camera ID is visible to camera framework.
  bool visible_to_framework = false;

  // physical_camera_ids contains the physical cameras underneath this logical
  // camera. If this logical camera does not contain multiple physical cameras,
  // physical_camera_ids should be empty.
  std::vector<uint32_t> physical_camera_ids;
};

// CameraIdManager manages public and internal camera IDs.
// Internal camera IDs are the camera IDs assigned by CameraProviderHwl.
// Public camera IDs are the camera IDs that camera framework sees.
class CameraIdManager {
 public:
  // Create a CameraIdManager given a list of the internal camera info.
  // Public camera IDs are assigned to the visible cameras in the id_maps list
  // first, and then to the non-visible ones, in the same order that the id_maps
  // list is ordered
  static std::unique_ptr<CameraIdManager> Create(
      const std::vector<CameraIdMap>& id_maps);

  virtual ~CameraIdManager() = default;

  // Return the camera IDs that are visible to camera framework.
  std::vector<std::uint32_t> GetVisibleCameraIds() const;

  // Return camera IDs, including those that are not visible to the framework.
  std::vector<std::uint32_t> GetCameraIds() const;

  // Get the list of physical camera IDs for the given logical camera. Returns
  // an empty vector if the specified ID is a physical camera. The IDs are
  // public IDs as understood by the camera framework
  std::vector<std::uint32_t> GetPhysicalCameraIds(
      uint32_t public_camera_id) const;

  // Get the public camera ID of an internal camera ID.
  status_t GetPublicCameraId(uint32_t internal_camera_id,
                             uint32_t* public_camera_id) const;

  // Get the internal camera ID of a public camera ID.
  status_t GetInternalCameraId(uint32_t public_camera_id,
                               uint32_t* internal_camera_id) const;

 protected:
  CameraIdManager() = default;

 private:
  status_t Initialize(const std::vector<CameraIdMap>& id_maps);

  status_t ValidateInput(const std::vector<CameraIdMap>& id_maps);

  status_t ValidateMappedIds();

  void PrintCameraIdMapping();

  // Index is the public camera ID.
  // Value is the internal camera ID.
  std::vector<uint32_t> public_camera_internal_ids_;

  size_t visible_camera_count_ = 0;

  // Index is public camera ID.
  // Value is the list of physical camera IDs belonging to the device at this
  // index. Physical IDs in the list are in the public domain also.
  //
  // For a public camera ID:
  //    physical_camera_ids_[public_logical_id] = { public_physical_id_list }
  //    physical_camera_ids_[public_physical_id] = {}
  std::vector<std::vector<uint32_t>> physical_camera_ids_;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_UTILS_CAMERA_ID_MANAGER_H_
