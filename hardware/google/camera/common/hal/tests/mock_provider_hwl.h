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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_TESTS_MOCK_PROVIDER_HWL_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_TESTS_MOCK_PROVIDER_HWL_H_

#include <camera_buffer_allocator_hwl.h>
#include <camera_id_manager.h>
#include <camera_provider_hwl.h>
#include <hal_types.h>

namespace android {
namespace google_camera_hal {

class MockProviderHwl : public CameraProviderHwl {
 public:
  static std::unique_ptr<MockProviderHwl> Create() {
    return std::unique_ptr<MockProviderHwl>(new MockProviderHwl());
  }

  virtual ~MockProviderHwl() = default;

  // Override functions in CameraProviderHwl.
  status_t SetCallback(const HwlCameraProviderCallback& callback) override {
    const HwlCameraProviderCallback* hwl_camera_provider_callback = &callback;
    if (hwl_camera_provider_callback == nullptr) {
      return BAD_VALUE;
    }

    hwl_camera_provider_callback->camera_device_status_change(
        /*camera_id=*/0, camera_device_status_);
    hwl_camera_provider_callback->torch_mode_status_change(
        /*camera_id=*/0, torch_status_);
    return OK;
  };

  status_t TriggerDeferredCallbacks() override {
    return OK;
  };

  status_t GetVendorTags(
      std::vector<VendorTagSection>* vendor_tag_sections) override {
    if (vendor_tag_sections == nullptr) {
      return BAD_VALUE;
    }

    *vendor_tag_sections = vendor_tag_sections_;
    return OK;
  }

  status_t GetVisibleCameraIds(std::vector<uint32_t>* cameras_ids) override {
    if (cameras_ids == nullptr) {
      return BAD_VALUE;
    }

    cameras_ids->clear();
    for (auto camera : cameras_) {
      if (camera.visible_to_framework) {
        cameras_ids->push_back(camera.id);
      }
    }
    return OK;
  }

  bool IsSetTorchModeSupported() override {
    return is_torch_supported_;
  }

  status_t IsConcurrentStreamCombinationSupported(
      const std::vector<CameraIdAndStreamConfiguration>&,
      bool* is_supported) override {
    if (is_supported == nullptr) {
      return BAD_VALUE;
    }
    *is_supported = false;
    return OK;
  }

  status_t GetConcurrentStreamingCameraIds(
      std::vector<std::unordered_set<uint32_t>>*) override {
    return OK;
  }

  status_t CreateCameraDeviceHwl(
      uint32_t /*cameraId*/,
      std::unique_ptr<CameraDeviceHwl>* /*camera_device_hwl*/) override {
    // TODO(b/138960498): support mock device HWL.
    return INVALID_OPERATION;
  }

  status_t CreateBufferAllocatorHwl(std::unique_ptr<CameraBufferAllocatorHwl>*
                                        camera_buffer_allocator_hwl) override {
    if (camera_buffer_allocator_hwl == nullptr) {
      ALOGE("%s: camera_buffer_allocator_hwl is nullptr.", __FUNCTION__);
      return BAD_VALUE;
    }

    return OK;
  }
  // End of override functions in CameraProviderHwl.

  // The following members are public so the test can change the values easily.
  std::vector<VendorTagSection> vendor_tag_sections_;
  std::vector<CameraIdMap> cameras_;
  bool is_torch_supported_ = false;
  CameraDeviceStatus camera_device_status_ = CameraDeviceStatus::kNotPresent;
  TorchModeStatus torch_status_ = TorchModeStatus::kAvailableOff;

 private:
  MockProviderHwl() = default;
};
}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_TESTS_MOCK_PROVIDER_HWL_H_
