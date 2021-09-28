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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_HWL_INTERFACE_CAMERA_PROVIDER_HWL_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_HWL_INTERFACE_CAMERA_PROVIDER_HWL_H_

#include <utils/Errors.h>

#include "camera_buffer_allocator_hwl.h"
#include "camera_device_hwl.h"
#include "hal_types.h"

namespace android {
namespace google_camera_hal {
// Camera provider HWL, which enumerates the available individual camera devices
// in the system, and provides updates about changes to device status.
class CameraProviderHwl {
 public:
  virtual ~CameraProviderHwl() = default;

  // Set camera provider callback functions to camera HWL.
  virtual status_t SetCallback(const HwlCameraProviderCallback& callback) = 0;

  // Trigger a deferred callback (such as physical camera avail/unavail) right
  // after setCallback() is called.
  virtual status_t TriggerDeferredCallbacks() = 0;

  // Get all vendor tags supported by devices. The tags are grouped into
  // sections.
  virtual status_t GetVendorTags(
      std::vector<VendorTagSection>* vendor_tag_sections) = 0;

  // Return the camera IDs that are visible to camera framework.
  virtual status_t GetVisibleCameraIds(
      std::vector<std::uint32_t>* camera_ids) = 0;

  // Check if the combinations of camera ids and corresponding stream
  // configurations are supported.
  virtual status_t IsConcurrentStreamCombinationSupported(
      const std::vector<CameraIdAndStreamConfiguration>&, bool*) = 0;

  // Return the combinations of camera ids that can stream concurrently with
  // guaranteed stream combinations
  virtual status_t GetConcurrentStreamingCameraIds(
      std::vector<std::unordered_set<uint32_t>>* combinations) = 0;

  // Return if setting torch mode API is supported. Not all camera devices
  // support torch mode so enabling torch mode for a devices is okay to
  // fail if the camera device doesn't support torch mode.
  virtual bool IsSetTorchModeSupported() = 0;

  // Create a camera device HWL for camera_id.
  virtual status_t CreateCameraDeviceHwl(
      uint32_t camera_id,
      std::unique_ptr<CameraDeviceHwl>* camera_device_hwl) = 0;

  // Create a camera buffer allocator, if HWL didn't support vendor buffer
  // allocator, need return INVALID_OPERATION.
  virtual status_t CreateBufferAllocatorHwl(
      std::unique_ptr<CameraBufferAllocatorHwl>* camera_buffer_allocator_hwl) = 0;
};
typedef CameraProviderHwl* (*CreateCameraProviderHwl_t)();
}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_HWL_INTERFACE_CAMERA_PROVIDER_HWL_H_
