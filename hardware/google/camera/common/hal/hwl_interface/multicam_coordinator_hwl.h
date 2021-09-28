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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_HWL_MULTICAM_COORDINATOR_HWL_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_HWL_MULTICAM_COORDINATOR_HWL_H_

#include "hal_types.h"

namespace android {
namespace google_camera_hal {

// This structure is used to update info on a physical pipelines.
struct MultiCamPhysicalUpdate {
  uint32_t current_camera_id;  // The physical camera id of the pipeline
  uint32_t logical_camera_id;  // The logical camera id for the usecase
  uint32_t lead_camera_id;     // The physical camera id of the lead pipeline
  bool sync_active;            // Should synchronize the two sensors
  bool lpm_enabled;            // Should shut off the inactive pipeline
  bool active;                 // Is this pipeline active.
};

// This is used to send current crop info and possibly update the physical crop
// if needed.
struct MultiCamPhysicalCropInfo {
  const Rect* logical_crop;  // logical user crop
  Rect* crop;                // physical crop
};

class IMulticamCoordinatorHwl {
 public:
  // Get lead camera id from metadata
  virtual status_t GetLeadCameraId(HalCameraMetadata* result_metadata,
                                   uint32_t* lead_camera_id) const = 0;
  // Is follower's metadata or not.
  virtual bool IsFollowerMetadata(HalCameraMetadata* result_metadata) const = 0;

  // Do any required adjustment on the physical crop for realtime pipeline
  virtual status_t DoPhysicalCropAdjustment(
      MultiCamPhysicalCropInfo* crop_info) = 0;
  // Undo any adjustment done on the physical crop in the realtime pipeline
  virtual status_t UndoPhysicalCropAdjustment(
      MultiCamPhysicalCropInfo* crop_info) = 0;
  // Set all the required multicam info based on the update
  virtual status_t UpdatePhysicalInfo(
      HalCameraMetadata* request_metadata,
      const MultiCamPhysicalUpdate* physical_update) = 0;

  virtual ~IMulticamCoordinatorHwl() = default;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_HWL_MULTICAM_COORDINATOR_HWL_H_
