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

#define LOG_TAG "GCH_HidlCameraProvider"
//#define LOG_NDEBUG 0
#include <log/log.h>
#include <regex>

#include "camera_device.h"
#include "hidl_camera_device.h"
#include "hidl_camera_provider.h"
#include "hidl_utils.h"

namespace android {
namespace hardware {
namespace camera {
namespace provider {
namespace V2_6 {
namespace implementation {

namespace hidl_utils = ::android::hardware::camera::implementation::hidl_utils;

using ::android::google_camera_hal::CameraDevice;

const std::string HidlCameraProvider::kProviderName = "internal";
// "device@<version>/internal/<id>"
const std::regex HidlCameraProvider::kDeviceNameRegex(
    "device@([0-9]+\\.[0-9]+)/internal/(.+)");

std::unique_ptr<HidlCameraProvider> HidlCameraProvider::Create() {
  auto provider = std::unique_ptr<HidlCameraProvider>(new HidlCameraProvider());
  if (provider == nullptr) {
    ALOGE("%s: Cannot create a HidlCameraProvider.", __FUNCTION__);
    return nullptr;
  }

  status_t res = provider->Initialize();
  if (res != OK) {
    ALOGE("%s: Initializing HidlCameraProvider failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return nullptr;
  }

  return provider;
}

status_t HidlCameraProvider::Initialize() {
  google_camera_provider_ = CameraProvider::Create();
  if (google_camera_provider_ == nullptr) {
    ALOGE("%s: Creating CameraProvider failed.", __FUNCTION__);
    return NO_INIT;
  }

  camera_provider_callback_ = {
      .camera_device_status_change = google_camera_hal::CameraDeviceStatusChangeFunc(
          [this](std::string camera_id,
                 google_camera_hal::CameraDeviceStatus new_status) {
            if (callbacks_ == nullptr) {
              ALOGE("%s: callbacks_ is null", __FUNCTION__);
              return;
            }
            CameraDeviceStatus hidl_camera_device_status;
            status_t res = hidl_utils::ConvertToHidlCameraDeviceStatus(
                new_status, &hidl_camera_device_status);
            if (res != OK) {
              ALOGE(
                  "%s: Converting to hidl camera device status failed: %s(%d)",
                  __FUNCTION__, strerror(-res), res);
              return;
            }

            std::unique_lock<std::mutex> lock(callbacks_lock_);
            callbacks_->cameraDeviceStatusChange(
                "device@" +
                    device::V3_5::implementation::HidlCameraDevice::kDeviceVersion +
                    "/" + kProviderName + "/" + camera_id,
                hidl_camera_device_status);
          }),
      .physical_camera_device_status_change = google_camera_hal::
          PhysicalCameraDeviceStatusChangeFunc([this](
                                                   std::string camera_id,
                                                   std::string physical_camera_id,
                                                   google_camera_hal::CameraDeviceStatus
                                                       new_status) {
            if (callbacks_ == nullptr) {
              ALOGE("%s: callbacks_ is null", __FUNCTION__);
              return;
            }
            auto castResult =
                provider::V2_6::ICameraProviderCallback::castFrom(callbacks_);
            if (!castResult.isOk()) {
              ALOGE("%s: callbacks_ cannot be casted to version 2.6",
                    __FUNCTION__);
              return;
            }
            sp<provider::V2_6::ICameraProviderCallback> callbacks_2_6_ =
                castResult;
            if (callbacks_2_6_ == nullptr) {
              ALOGE("%s: callbacks_2_6_ is null", __FUNCTION__);
              return;
            }

            CameraDeviceStatus hidl_camera_device_status;
            status_t res = hidl_utils::ConvertToHidlCameraDeviceStatus(
                new_status, &hidl_camera_device_status);
            if (res != OK) {
              ALOGE(
                  "%s: Converting to hidl camera device status failed: %s(%d)",
                  __FUNCTION__, strerror(-res), res);
              return;
            }

            std::unique_lock<std::mutex> lock(callbacks_lock_);
            callbacks_2_6_->physicalCameraDeviceStatusChange(
                "device@" +
                    device::V3_5::implementation::HidlCameraDevice::kDeviceVersion +
                    "/" + kProviderName + "/" + camera_id,
                physical_camera_id, hidl_camera_device_status);
          }),
      .torch_mode_status_change = google_camera_hal::TorchModeStatusChangeFunc(
          [this](std::string camera_id,
                 google_camera_hal::TorchModeStatus new_status) {
            if (callbacks_ == nullptr) {
              ALOGE("%s: callbacks_ is null", __FUNCTION__);
              return;
            }

            TorchModeStatus hidl_torch_status;
            status_t res = hidl_utils::ConvertToHidlTorchModeStatus(
                new_status, &hidl_torch_status);
            if (res != OK) {
              ALOGE("%s: Converting to hidl torch status failed: %s(%d)",
                    __FUNCTION__, strerror(-res), res);
              return;
            }

            std::unique_lock<std::mutex> lock(callbacks_lock_);
            callbacks_->torchModeStatusChange(
                "device@" +
                    device::V3_5::implementation::HidlCameraDevice::kDeviceVersion +
                    "/" + kProviderName + "/" + camera_id,
                hidl_torch_status);
          }),
  };

  google_camera_provider_->SetCallback(&camera_provider_callback_);
  // purge pending malloc pages after initialization
  mallopt(M_PURGE, 0);
  return OK;
}

Return<Status> HidlCameraProvider::setCallback(
    const sp<ICameraProviderCallback>& callback) {
  {
    std::unique_lock<std::mutex> lock(callbacks_lock_);
    callbacks_ = callback;
  }

  google_camera_provider_->TriggerDeferredCallbacks();

  return Status::OK;
}

Return<void> HidlCameraProvider::getVendorTags(getVendorTags_cb _hidl_cb) {
  hidl_vec<VendorTagSection> hidl_vendor_tag_sections;
  std::vector<google_camera_hal::VendorTagSection> hal_vendor_tag_sections;

  status_t res =
      google_camera_provider_->GetVendorTags(&hal_vendor_tag_sections);
  if (res != OK) {
    ALOGE("%s: Getting vendor tags failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    _hidl_cb(Status::INTERNAL_ERROR, hidl_vendor_tag_sections);
    return Void();
  }

  res = hidl_utils::ConvertToHidlVendorTagSections(hal_vendor_tag_sections,
                                                   &hidl_vendor_tag_sections);
  if (res != OK) {
    ALOGE("%s: Converting to hidl vendor tags failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    _hidl_cb(Status::INTERNAL_ERROR, hidl_vendor_tag_sections);
    return Void();
  }

  _hidl_cb(Status::OK, hidl_vendor_tag_sections);
  return Void();
}

Return<void> HidlCameraProvider::getCameraIdList(getCameraIdList_cb _hidl_cb) {
  std::vector<uint32_t> camera_ids;
  hidl_vec<hidl_string> hidl_camera_ids;

  status_t res = google_camera_provider_->GetCameraIdList(&camera_ids);
  if (res != OK) {
    ALOGE("%s: Getting camera ID list failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    _hidl_cb(Status::INTERNAL_ERROR, hidl_camera_ids);
    return Void();
  }

  hidl_camera_ids.resize(camera_ids.size());
  for (uint32_t i = 0; i < hidl_camera_ids.size(); i++) {
    // camera ID is in the form of "device@<major>.<minor>/<type>/<id>"
    hidl_camera_ids[i] =
        "device@" +
        device::V3_5::implementation::HidlCameraDevice::kDeviceVersion + "/" +
        kProviderName + "/" + std::to_string(camera_ids[i]);
  }

  _hidl_cb(Status::OK, hidl_camera_ids);
  return Void();
}

Return<void> HidlCameraProvider::getConcurrentStreamingCameraIds(
    getConcurrentStreamingCameraIds_cb _hidl_cb) {
  hidl_vec<hidl_vec<hidl_string>> hidl_camera_id_combinations;
  std::vector<std::unordered_set<uint32_t>> camera_id_combinations;
  status_t res = google_camera_provider_->GetConcurrentStreamingCameraIds(
      &camera_id_combinations);
  if (res != OK) {
    ALOGE(
        "%s: Getting the combinations of concurrent streaming camera ids "
        "failed: %s(%d)",
        __FUNCTION__, strerror(-res), res);
    _hidl_cb(Status::INTERNAL_ERROR, hidl_camera_id_combinations);
    return Void();
  }
  hidl_camera_id_combinations.resize(camera_id_combinations.size());
  int i = 0;
  for (auto& combination : camera_id_combinations) {
    hidl_vec<hidl_string> hidl_combination(combination.size());
    int c = 0;
    for (auto& camera_id : combination) {
      hidl_combination[c] = std::to_string(camera_id);
      c++;
    }
    hidl_camera_id_combinations[i] = hidl_combination;
    i++;
  }
  _hidl_cb(Status::OK, hidl_camera_id_combinations);
  return Void();
}

Return<void> HidlCameraProvider::isConcurrentStreamCombinationSupported(
    const hidl_vec<CameraIdAndStreamCombination>& configs,
    isConcurrentStreamCombinationSupported_cb _hidl_cb) {
  std::vector<google_camera_hal::CameraIdAndStreamConfiguration>
      devices_stream_configs(configs.size());
  status_t res = OK;
  size_t c = 0;
  for (auto& config : configs) {
    res = hidl_utils::ConverToHalStreamConfig(
        config.streamConfiguration,
        &devices_stream_configs[c].stream_configuration);
    if (res != OK) {
      ALOGE("%s: ConverToHalStreamConfig failed", __FUNCTION__);
      _hidl_cb(Status::INTERNAL_ERROR, false);
      return Void();
    }
    uint32_t camera_id = atoi(config.cameraId.c_str());
    devices_stream_configs[c].camera_id = camera_id;
    c++;
  }
  bool is_supported = false;
  res = google_camera_provider_->IsConcurrentStreamCombinationSupported(
      devices_stream_configs, &is_supported);
  if (res != OK) {
    ALOGE("%s: ConverToHalStreamConfig failed", __FUNCTION__);
    _hidl_cb(Status::INTERNAL_ERROR, false);
    return Void();
  }
  _hidl_cb(Status::OK, is_supported);
  return Void();
}

Return<void> HidlCameraProvider::isSetTorchModeSupported(
    isSetTorchModeSupported_cb _hidl_cb) {
  bool is_supported = google_camera_provider_->IsSetTorchModeSupported();
  _hidl_cb(Status::OK, is_supported);
  return Void();
}

Return<void> HidlCameraProvider::getCameraDeviceInterface_V1_x(
    const hidl_string& /*cameraDeviceName*/,
    getCameraDeviceInterface_V1_x_cb _hidl_cb) {
  _hidl_cb(Status::OPERATION_NOT_SUPPORTED, nullptr);
  return Void();
}

bool HidlCameraProvider::ParseDeviceName(const hidl_string& device_name,
                                         std::string* device_version,
                                         std::string* camera_id) {
  std::string device_name_std(device_name.c_str());
  std::smatch sm;

  if (std::regex_match(device_name_std, sm,
                       HidlCameraProvider::kDeviceNameRegex)) {
    if (device_version != nullptr) {
      *device_version = sm[1];
    }
    if (camera_id != nullptr) {
      *camera_id = sm[2];
    }
    return true;
  }
  return false;
}

Return<void> HidlCameraProvider::getCameraDeviceInterface_V3_x(
    const hidl_string& camera_device_name,
    getCameraDeviceInterface_V3_x_cb _hidl_cb) {
  std::unique_ptr<CameraDevice> google_camera_device;

  // Parse camera_device_name.
  std::string camera_id, device_version;

  bool match = ParseDeviceName(camera_device_name, &device_version, &camera_id);
  if (!match) {
    ALOGE("%s: Device name parse fail. ", __FUNCTION__);
    _hidl_cb(Status::ILLEGAL_ARGUMENT, nullptr);
    return Void();
  }

  status_t res = google_camera_provider_->CreateCameraDevice(
      atoi(camera_id.c_str()), &google_camera_device);
  if (res != OK) {
    ALOGE("%s: Creating CameraDevice failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    _hidl_cb(hidl_utils::ConvertToHidlStatus(res), nullptr);
    return Void();
  }

  auto hidl_camera_device =
      device::V3_5::implementation::HidlCameraDevice::Create(
          std::move(google_camera_device));
  if (hidl_camera_device == nullptr) {
    ALOGE("%s: Creating HidlCameraDevice failed", __FUNCTION__);
    _hidl_cb(Status::INTERNAL_ERROR, nullptr);
    return Void();
  }

  _hidl_cb(Status::OK, hidl_camera_device.release());
  return Void();
}

Return<void> HidlCameraProvider::notifyDeviceStateChange(
    hardware::hidl_bitfield<DeviceState> /*newState*/) {
  return Void();
}

ICameraProvider* HIDL_FETCH_ICameraProvider(const char* name) {
  std::string provider_name = HidlCameraProvider::kProviderName + "/0";
  if (provider_name.compare(name) != 0) {
    ALOGE("%s: Unknown provider name: %s", __FUNCTION__, name);
    return nullptr;
  }

  auto provider = HidlCameraProvider::Create();
  if (provider == nullptr) {
    ALOGE("%s: Cannot create a HidlCameraProvider.", __FUNCTION__);
    return nullptr;
  }

  return provider.release();
}

}  // namespace implementation
}  // namespace V2_6
}  // namespace provider
}  // namespace camera
}  // namespace hardware
}  // namespace android
