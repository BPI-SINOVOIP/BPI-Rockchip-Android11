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

//#define LOG_NDEBUG 0
#define LOG_TAG "GCH_CameraIdManager"
#include <log/log.h>

#include "camera_id_manager.h"

namespace android {
namespace google_camera_hal {

std::unique_ptr<CameraIdManager> CameraIdManager::Create(
    const std::vector<CameraIdMap>& id_maps) {
  auto camera_id_manager =
      std::unique_ptr<CameraIdManager>(new CameraIdManager());

  if (camera_id_manager == nullptr) {
    ALOGE("%s: Creating CameraIdManager failed.", __FUNCTION__);
    return nullptr;
  }

  status_t res = camera_id_manager->Initialize(id_maps);
  if (res != OK) {
    ALOGE("%s: Initializing CameraIdManager failed: %s (%d).", __FUNCTION__,
          strerror(-res), res);
    return nullptr;
  }

  return camera_id_manager;
}

status_t CameraIdManager::Initialize(const std::vector<CameraIdMap>& id_maps) {
  if (id_maps.size() == 0) {
    ALOGW("%s: camera_ids is empty.", __FUNCTION__);
    return OK;
  }

  status_t res = ValidateInput(id_maps);
  if (res != OK) {
    return res;
  }

  // First, add the internal camera IDs for cameras visible to the framework
  for (auto camera : id_maps) {
    if (camera.visible_to_framework) {
      visible_camera_count_++;
      public_camera_internal_ids_.push_back(camera.id);
      physical_camera_ids_.push_back(camera.physical_camera_ids);
    }
  }

  // Next, add the non-visible public IDs to the end of the list
  for (auto camera : id_maps) {
    if (!camera.visible_to_framework) {
      public_camera_internal_ids_.push_back(camera.id);
      physical_camera_ids_.push_back(camera.physical_camera_ids);
    }
  }

  // Change the internal cameras' IDs to public framework IDs
  for (auto& physical_ids : physical_camera_ids_) {
    for (size_t i = 0; i < physical_ids.size(); i++) {
      // Remap the original physical camera ID to the new public ID domain
      if (GetPublicCameraId(physical_ids[i], &physical_ids[i]) != OK) {
        return BAD_VALUE;
      }
    }
  }

  res = ValidateMappedIds();
  if (res != OK) {
    return res;
  }

  PrintCameraIdMapping();
  return OK;
}

status_t CameraIdManager::ValidateInput(const std::vector<CameraIdMap>& id_maps) {
  bool has_visible_camera = false;

  std::vector<uint32_t> physical_ids;
  // Check for logical cameras that may not be visible to the framework
  for (auto camera : id_maps) {
    if (camera.physical_camera_ids.size() > 0 && !camera.visible_to_framework) {
      ALOGE("%s: Logical cameras should be visible to the framework.",
            __FUNCTION__);
      return BAD_VALUE;
    } else if (camera.physical_camera_ids.size() == 0) {
      physical_ids.push_back(camera.id);
    }

    if (camera.visible_to_framework) {
      has_visible_camera = true;
    }
  }

  // There should be at least one visible camera unless we are initialized with
  // an empty camera list
  if (id_maps.size() > 0 && !has_visible_camera) {
    ALOGE("%s: There is no public camera ID visiable to the framework.",
          __FUNCTION__);
    return BAD_VALUE;
  }

  // Check to see if physical camera IDs are not re-used as logical cameras
  for (auto logical_camera : id_maps) {
    if (logical_camera.physical_camera_ids.size() > 0) {
      for (uint32_t physical_id : logical_camera.physical_camera_ids) {
        auto it =
            std::find(physical_ids.begin(), physical_ids.end(), physical_id);
        if (it == physical_ids.end()) {
          ALOGE(
              "%s: Logical camera with ID %u lists physical camera id %u,"
              "which is not a physical camera",
              __FUNCTION__, logical_camera.id, physical_id);
          return BAD_VALUE;
        }
      }
    }
  }

  return OK;
}

status_t CameraIdManager::ValidateMappedIds() {
  // Make a copy of the internal IDs
  auto v = public_camera_internal_ids_;
  // Camera IDs must be unique, except for kInvalidCameraId values
  v.erase(std::remove(v.begin(), v.end(), kInvalidCameraId), v.end());
  if (std::unique(v.begin(), v.end()) != v.end()) {
    ALOGE("%s: Camera IDs should be unique.", __FUNCTION__);
    return BAD_VALUE;
  }

  return OK;
}

void CameraIdManager::PrintCameraIdMapping() {
  ALOGI("%s: Found %zu public camera IDs with %zu visible to the framework.",
        __FUNCTION__, public_camera_internal_ids_.size(), visible_camera_count_);

  for (uint32_t i = 0; i < public_camera_internal_ids_.size(); i++) {
    const char* visibility =
        (i + 1 <= visible_camera_count_ ? "visible" : "NOT visible");
    ALOGI("%s: Public camera ID %u is %s, and maps to internal camera ID %u",
          __FUNCTION__, i, visibility, public_camera_internal_ids_[i]);
  }

  uint32_t public_id = 0;
  for (auto physical_id_list : physical_camera_ids_) {
    for (uint32_t phys_id : physical_id_list) {
      ALOGI("%s: Public camera ID %u uses physical camera ID %u", __FUNCTION__,
            public_id, phys_id);
    }
    public_id++;
  }
}

std::vector<std::uint32_t> CameraIdManager::GetVisibleCameraIds() const {
  std::vector<std::uint32_t> camera_ids;

  for (uint32_t i = 0; i < visible_camera_count_; i++) {
    camera_ids.push_back(i);
  }

  return camera_ids;
}

std::vector<std::uint32_t> CameraIdManager::GetCameraIds() const {
  std::vector<std::uint32_t> camera_ids;

  for (uint32_t i = 0; i < public_camera_internal_ids_.size(); i++) {
    camera_ids.push_back(i);
  }

  return camera_ids;
}

std::vector<std::uint32_t> CameraIdManager::GetPhysicalCameraIds(
    uint32_t public_camera_id) const {
  if (public_camera_id >= public_camera_internal_ids_.size()) {
    return {};
  }

  return physical_camera_ids_[public_camera_id];
}

status_t CameraIdManager::GetPublicCameraId(uint32_t internal_camera_id,
                                            uint32_t* public_camera_id) const {
  if (public_camera_id == nullptr) {
    return BAD_VALUE;
  }

  for (uint32_t i = 0; i < public_camera_internal_ids_.size(); i++) {
    if (public_camera_internal_ids_[i] == internal_camera_id) {
      *public_camera_id = i;
      return OK;
    }
  }

  return NAME_NOT_FOUND;
}

status_t CameraIdManager::GetInternalCameraId(
    uint32_t public_camera_id, uint32_t* internal_camera_id) const {
  if (internal_camera_id == nullptr) {
    return BAD_VALUE;
  }

  if (public_camera_id >= public_camera_internal_ids_.size()) {
    return BAD_VALUE;
  }

  *internal_camera_id = public_camera_internal_ids_[public_camera_id];

  return OK;
}

}  // namespace google_camera_hal
}  // namespace android
