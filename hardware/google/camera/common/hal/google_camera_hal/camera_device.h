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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_CAMERA_DEVICE_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_CAMERA_DEVICE_H_

#include "camera_buffer_allocator_hwl.h"
#include "camera_device_hwl.h"
#include "camera_device_session.h"
#include "hal_camera_metadata.h"

namespace android {
namespace google_camera_hal {

// Camera Device implements ICameraDevice. It provides methods to query static
// information about a camera device and create a camera device session for
// active use. It does not hold any states of the camera device.
class CameraDevice {
 public:
  // Create a camera device given a camera device HWL.
  // camera_device_hwl must be valid.
  // camera_allocator_hwl is owned by the caller and must be valid during the
  // lifetime of CameraDevice
  static std::unique_ptr<CameraDevice> Create(
      std::unique_ptr<CameraDeviceHwl> camera_device_hwl,
      CameraBufferAllocatorHwl* camera_allocator_hwl = nullptr);

  virtual ~CameraDevice();

  // Get the resource cost of this camera device.
  status_t GetResourceCost(CameraResourceCost* cost);

  // Get the characteristics of this camera device.
  // characteristics will be filled with this camera device's characteristics.
  status_t GetCameraCharacteristics(
      std::unique_ptr<HalCameraMetadata>* characteristics);

  // Get the characteristics of this camera device's physical camera if the
  // physical_camera_id belongs to this camera device.
  // characteristics will be filled with the physical camera ID's
  // characteristics.
  status_t GetPhysicalCameraCharacteristics(
      uint32_t physical_camera_id,
      std::unique_ptr<HalCameraMetadata>* characteristics);

  // Set the torch mode of the camera device. The torch mode status remains
  // unchanged after this CameraDevice instance is destroyed.
  status_t SetTorchMode(TorchMode mode);

  // Create a CameraDeviceSession to handle capture requests. This method will
  // return ALREADY_EXISTS if previous session has not been destroyed.
  // Created CameraDeviceSession remain valid even after this CameraDevice
  // instance is destroyed.
  status_t CreateCameraDeviceSession(
      std::unique_ptr<CameraDeviceSession>* session);

  // Dump the camera device states in fd, using dprintf() or write().
  status_t DumpState(int fd);

  // Get the public camera ID for this camera device.
  uint32_t GetPublicCameraId() const {
    return public_camera_id_;
  };

  // Query whether a particular logical and physical streams combination are
  // supported. stream_config contains the stream configurations.
  bool IsStreamCombinationSupported(const StreamConfiguration& stream_config);

  status_t LoadExternalCaptureSession();

 protected:
  CameraDevice() = default;

 private:
  status_t Initialize(std::unique_ptr<CameraDeviceHwl> camera_device_hwl,
                      CameraBufferAllocatorHwl* camera_allocator_hwl);

  uint32_t public_camera_id_ = 0;

  std::unique_ptr<CameraDeviceHwl> camera_device_hwl_;

  // hwl allocator
  CameraBufferAllocatorHwl* camera_allocator_hwl_ = nullptr;

  std::vector<GetCaptureSessionFactoryFunc> external_session_factory_entries_;
  // Opened library handles that should be closed on destruction
  std::vector<void*> external_capture_session_lib_handles_;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_CAMERA_DEVICE_H_
