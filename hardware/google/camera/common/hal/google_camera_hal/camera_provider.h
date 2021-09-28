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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_CAMERA_PROVIDER_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_CAMERA_PROVIDER_H_

#include <utils/Errors.h>
#include <memory>

#include "camera_buffer_allocator_hwl.h"
#include "camera_device.h"
#include "camera_provider_callback.h"
#include "camera_provider_hwl.h"
#include "vendor_tags.h"

namespace android {
namespace google_camera_hal {
class CameraProvider {
 public:
  // Create a Camera Provider.
  // If camera_provider_hwl is nullptr, CameraProvider will try to open
  // a library containing the camera_provider_hwl implementation for the device.
  static std::unique_ptr<CameraProvider> Create(
      std::unique_ptr<CameraProviderHwl> camera_provider_hwl = nullptr);
  virtual ~CameraProvider();

  // Set callback functions.
  status_t SetCallback(const CameraProviderCallback* callback);

  // Trigger deferred callbacks (such as physical camera avail/unavail) right
  // after setCallback() is called.
  status_t TriggerDeferredCallbacks();

  // Get vendor tags.
  status_t GetVendorTags(
      std::vector<VendorTagSection>* vendor_tag_sections) const;

  // Get a list of camera IDs.
  status_t GetCameraIdList(std::vector<uint32_t>* camera_ids) const;

  // Return if torch mode is supported.
  bool IsSetTorchModeSupported() const;

  // Create a CameraDevice for camera_id.
  status_t CreateCameraDevice(uint32_t camera_id,
                              std::unique_ptr<CameraDevice>* device);

  // Get the combinations of camera ids which support concurrent streaming
  status_t GetConcurrentStreamingCameraIds(
      std::vector<std::unordered_set<uint32_t>>* camera_id_combinations);

  // Check if a set of concurrent stream  configurations are supported
  status_t IsConcurrentStreamCombinationSupported(
      const std::vector<CameraIdAndStreamConfiguration>& configs,
      bool* is_supported);

 protected:
  CameraProvider() = default;

 private:
  status_t Initialize(std::unique_ptr<CameraProviderHwl> camera_provider_hwl);

  // Initialize the vendor tag manager
  status_t InitializeVendorTags();

  status_t CreateCameraProviderHwl(
      std::unique_ptr<CameraProviderHwl>* camera_provider_hwl);

  // Provider library handle.
  void* hwl_lib_handle_ = nullptr;

  std::unique_ptr<CameraProviderHwl> camera_provider_hwl_;

  const CameraProviderCallback* provider_callback_ = nullptr;
  HwlCameraProviderCallback hwl_provider_callback_;

  std::unique_ptr<CameraBufferAllocatorHwl> camera_allocator_hwl_;
  // Combined list of vendor tags from HAL and HWL
  std::vector<VendorTagSection> vendor_tag_sections_;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_CAMERA_PROVIDER_H_
