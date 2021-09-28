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

#ifndef EMULATOR_CAMERA_HAL_HWL_CAMERA_PROVIDER_HWL_H
#define EMULATOR_CAMERA_HAL_HWL_CAMERA_PROVIDER_HWL_H

#include <camera_provider_hwl.h>
#include <hal_types.h>
#include <json/json.h>
#include <json/reader.h>
#include <future>

namespace android {

using google_camera_hal::CameraBufferAllocatorHwl;
using google_camera_hal::CameraDeviceHwl;
using google_camera_hal::CameraDeviceStatus;
using google_camera_hal::CameraIdAndStreamConfiguration;
using google_camera_hal::CameraProviderHwl;
using google_camera_hal::HalCameraMetadata;
using google_camera_hal::HwlCameraProviderCallback;
using google_camera_hal::HwlPhysicalCameraDeviceStatusChangeFunc;
using google_camera_hal::HwlTorchModeStatusChangeFunc;
using google_camera_hal::VendorTagSection;

class EmulatedCameraProviderHwlImpl : public CameraProviderHwl {
 public:
  // Return a unique pointer to EmulatedCameraProviderHwlImpl. Calling Create()
  // again before the previous one is destroyed will fail.
  static std::unique_ptr<EmulatedCameraProviderHwlImpl> Create();

  virtual ~EmulatedCameraProviderHwlImpl() {
    WaitForStatusCallbackFuture();
  }

  // Override functions in CameraProviderHwl.
  status_t SetCallback(const HwlCameraProviderCallback& callback) override;
  status_t TriggerDeferredCallbacks() override;

  status_t GetVendorTags(
      std::vector<VendorTagSection>* vendor_tag_sections) override;

  status_t GetVisibleCameraIds(std::vector<std::uint32_t>* camera_ids) override;

  bool IsSetTorchModeSupported() override {
    return true;
  }

  status_t GetConcurrentStreamingCameraIds(
      std::vector<std::unordered_set<uint32_t>>*) override;

  status_t IsConcurrentStreamCombinationSupported(
      const std::vector<CameraIdAndStreamConfiguration>&, bool*) override;

  status_t CreateCameraDeviceHwl(
      uint32_t camera_id,
      std::unique_ptr<CameraDeviceHwl>* camera_device_hwl) override;

  status_t CreateBufferAllocatorHwl(std::unique_ptr<CameraBufferAllocatorHwl>*
                                        camera_buffer_allocator_hwl) override;
  // End of override functions in CameraProviderHwl.

 private:
  status_t Initialize();
  uint32_t ParseCharacteristics(const Json::Value& root, ssize_t id);
  status_t GetTagFromName(const char* name, uint32_t* tag);
  status_t WaitForQemuSfFakeCameraPropertyAvailable();
  bool SupportsMandatoryConcurrentStreams(uint32_t camera_id);

  static const char* kConfigurationFileLocation[];

  std::vector<std::unique_ptr<HalCameraMetadata>> static_metadata_;
  // Logical to physical camera Id mapping. Empty value vector in case
  // of regular non-logical device.
  std::unordered_map<uint32_t, std::vector<std::pair<CameraDeviceStatus, uint32_t>>> camera_id_map_;
  HwlTorchModeStatusChangeFunc torch_cb_;
  HwlPhysicalCameraDeviceStatusChangeFunc physical_camera_status_cb_;

  std::mutex status_callback_future_lock_;
  std::future<void> status_callback_future_;
  void WaitForStatusCallbackFuture();
  void NotifyPhysicalCameraUnavailable();
};

extern "C" CameraProviderHwl* CreateCameraProviderHwl() {
  auto provider = EmulatedCameraProviderHwlImpl::Create();
  return provider.release();
}

}  // namespace android

#endif  // EMULATOR_CAMERA_HAL_HWL_CAMERA_PROVIDER_HWL_H
