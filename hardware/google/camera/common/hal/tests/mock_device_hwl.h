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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_TESTS_MOCK_DEVICE_HWL_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_TESTS_MOCK_DEVICE_HWL_H_

#include <camera_device_hwl.h>
#include <unordered_map>

#include "mock_device_session_hwl.h"

namespace android {
namespace google_camera_hal {

class MockDeviceHwl : public CameraDeviceHwl {
 public:
  static std::unique_ptr<MockDeviceHwl> Create() {
    return std::unique_ptr<MockDeviceHwl>(new MockDeviceHwl());
  }

  virtual ~MockDeviceHwl() = default;

  // Override functions in CameraDeviceHwl start.
  uint32_t GetCameraId() const {
    return camera_id_;
  };

  status_t GetResourceCost(CameraResourceCost* cost) const {
    if (cost == nullptr) {
      return BAD_VALUE;
    }

    *cost = resource_cost_;
    return OK;
  }

  status_t GetCameraCharacteristics(
      std::unique_ptr<HalCameraMetadata>* characteristics) const {
    if (characteristics == nullptr) {
      return BAD_VALUE;
    }

    *characteristics = HalCameraMetadata::Clone(characteristics_.get());
    if (*characteristics == nullptr) {
      return NO_MEMORY;
    }
    return OK;
  }

  status_t GetPhysicalCameraCharacteristics(
      uint32_t physical_camera_id,
      std::unique_ptr<HalCameraMetadata>* characteristics) const {
    if (characteristics == nullptr) {
      return BAD_VALUE;
    }

    auto physical_characteristics =
        physical_camera_characteristics_.find(physical_camera_id);
    if (physical_characteristics == physical_camera_characteristics_.end()) {
      return BAD_VALUE;
    }

    *characteristics =
        HalCameraMetadata::Clone(physical_characteristics->second.get());

    return OK;
  }

  status_t SetTorchMode(TorchMode /*mode*/) {
    return OK;
  }

  // Dump the camera device states in fd, using dprintf() or write().
  status_t DumpState(int fd) {
    if (fd < 0) {
      return BAD_VALUE;
    }

    dprintf(fd, "%s", dump_string_.c_str());

    return OK;
  }

  status_t CreateCameraDeviceSessionHwl(
      CameraBufferAllocatorHwl* /*camera_allocator_hwl*/,
      std::unique_ptr<CameraDeviceSessionHwl>* session) {
    if (session == nullptr) {
      return BAD_VALUE;
    }

    auto session_hwl = std::make_unique<MockDeviceSessionHwl>();
    if (session_hwl == nullptr) {
      return NO_MEMORY;
    }
    session_hwl->DelegateCallsToFakeSession();
    *session = std::move(session_hwl);

    return OK;
  }

  bool IsStreamCombinationSupported(const StreamConfiguration& /*stream_config*/) {
    return true;
  }
  // Override functions in CameraDeviceHwl end.

  // The following members are public so the test can change the values easily.
  uint32_t camera_id_ = 0;
  CameraResourceCost resource_cost_;
  std::unique_ptr<HalCameraMetadata> characteristics_;

  // Map from physical camera ID to physical camera characteristics.
  std::unordered_map<uint32_t, std::unique_ptr<HalCameraMetadata>>
      physical_camera_characteristics_;

  std::string dump_string_;

 protected:
  MockDeviceHwl() {
    characteristics_ = HalCameraMetadata::Create(
        /*num_entries=*/0, /*data_bytes=*/0);
  };
};
}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_TESTS_MOCK_DEVICE_HWL_H_
