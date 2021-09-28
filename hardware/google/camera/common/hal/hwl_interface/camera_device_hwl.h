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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_HWL_INTERFACE_CAMERA_DEVICE_HWL_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_HWL_INTERFACE_CAMERA_DEVICE_HWL_H_

#include <utils/Errors.h>

#include "camera_buffer_allocator_hwl.h"
#include "camera_device_session_hwl.h"
#include "hal_camera_metadata.h"
#include "hal_types.h"

namespace android {
namespace google_camera_hal {

// Camera device HWL, which is associated with a certain camera ID. The camera
// device can be a logical camera that contains multiple physical camera, or
// a single physical camera. It provides methods to query static information
// about the associated camera devices. It does not hold any states of the
// camera device.
class CameraDeviceHwl {
 public:
  virtual ~CameraDeviceHwl() = default;

  // Get the camera ID of this camera device HWL.
  virtual uint32_t GetCameraId() const = 0;

  // Get the resource cost of this camera device HWL.
  virtual status_t GetResourceCost(CameraResourceCost* cost) const = 0;

  // Get the characteristics of this camera device HWL.
  // characteristics will be filled by CameraDeviceHwl.
  virtual status_t GetCameraCharacteristics(
      std::unique_ptr<HalCameraMetadata>* characteristics) const = 0;

  // Get the characteristics of the physical camera of this camera device.
  // characteristics will be filled by CameraDeviceHwl.
  virtual status_t GetPhysicalCameraCharacteristics(
      uint32_t physical_camera_id,
      std::unique_ptr<HalCameraMetadata>* characteristics) const = 0;

  // Set the torch mode of the camera device. The torch mode status remains
  // unchanged after this CameraDevice instance is destroyed.
  virtual status_t SetTorchMode(TorchMode mode) = 0;

  // Dump the camera device states in fd, using dprintf() or write().
  virtual status_t DumpState(int fd) = 0;

  // Create a camera device session for this device. This method will not be
  // called before previous session has been destroyed.
  // Created CameraDeviceSession remain valid even after this CameraDevice
  // instance is destroyed.
  // camera_allocator_hwl will be used by the HWL session when create HW
  // pipeline, it should be valid during the lifetime of the HWL session.
  virtual status_t CreateCameraDeviceSessionHwl(
      CameraBufferAllocatorHwl* camera_allocator_hwl,
      std::unique_ptr<CameraDeviceSessionHwl>* session) = 0;

  // Query whether a particular logical and physical streams combination are
  // supported. stream_config contains the stream configurations.
  virtual bool IsStreamCombinationSupported(
      const StreamConfiguration& stream_config) = 0;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_HWL_INTERFACE_CAMERA_DEVICE_HWL_H_