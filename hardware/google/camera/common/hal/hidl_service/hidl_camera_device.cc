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

#define LOG_TAG "GCH_HidlCameraDevice"
//#define LOG_NDEBUG 0
#include <log/log.h>

#include "hidl_camera_device.h"
#include "hidl_camera_device_session.h"
#include "hidl_profiler.h"
#include "hidl_utils.h"

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace V3_5 {
namespace implementation {

namespace hidl_utils = ::android::hardware::camera::implementation::hidl_utils;

using ::android::google_camera_hal::HalCameraMetadata;

const std::string HidlCameraDevice::kDeviceVersion = "3.5";

std::unique_ptr<HidlCameraDevice> HidlCameraDevice::Create(
    std::unique_ptr<CameraDevice> google_camera_device) {
  auto device = std::unique_ptr<HidlCameraDevice>(new HidlCameraDevice());
  if (device == nullptr) {
    ALOGE("%s: Cannot create a HidlCameraDevice.", __FUNCTION__);
    return nullptr;
  }

  status_t res = device->Initialize(std::move(google_camera_device));
  if (res != OK) {
    ALOGE("%s: Initializing HidlCameraDevice failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return nullptr;
  }

  return device;
}

status_t HidlCameraDevice::Initialize(
    std::unique_ptr<CameraDevice> google_camera_device) {
  if (google_camera_device == nullptr) {
    ALOGE("%s: google_camera_device is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  camera_id_ = google_camera_device->GetPublicCameraId();
  google_camera_device_ = std::move(google_camera_device);

  return OK;
}

Return<void> HidlCameraDevice::getResourceCost(
    ICameraDevice::getResourceCost_cb _hidl_cb) {
  google_camera_hal::CameraResourceCost hal_cost;
  CameraResourceCost hidl_cost;

  status_t res = google_camera_device_->GetResourceCost(&hal_cost);
  if (res != OK) {
    ALOGE("%s: Getting resource cost failed for camera %u: %s(%d)",
          __FUNCTION__, camera_id_, strerror(-res), res);
    _hidl_cb(Status::INTERNAL_ERROR, hidl_cost);
    return Void();
  }

  res = hidl_utils::ConvertToHidlResourceCost(hal_cost, &hidl_cost);
  if (res != OK) {
    _hidl_cb(Status::INTERNAL_ERROR, hidl_cost);
    return Void();
  }

  _hidl_cb(Status::OK, hidl_cost);
  return Void();
}

Return<void> HidlCameraDevice::getCameraCharacteristics(
    ICameraDevice::getCameraCharacteristics_cb _hidl_cb) {
  V3_2::CameraMetadata hidl_characteristics;
  std::unique_ptr<HalCameraMetadata> characteristics;
  status_t res =
      google_camera_device_->GetCameraCharacteristics(&characteristics);
  if (res != OK) {
    ALOGE("%s: Getting camera characteristics for camera %u failed: %s(%d)",
          __FUNCTION__, camera_id_, strerror(-res), res);
    _hidl_cb(Status::INTERNAL_ERROR, hidl_characteristics);
    return Void();
  }

  if (characteristics == nullptr) {
    ALOGE("%s: Camera characteristics for camera %u is nullptr.", __FUNCTION__,
          camera_id_);
    _hidl_cb(Status::INTERNAL_ERROR, hidl_characteristics);
    return Void();
  }

  uint32_t metadata_size = characteristics->GetCameraMetadataSize();
  hidl_characteristics.setToExternal(
      (uint8_t*)characteristics->ReleaseCameraMetadata(), metadata_size,
      /*shouldOwn*/ true);

  _hidl_cb(Status::OK, hidl_characteristics);
  return Void();
}

Return<Status> HidlCameraDevice::setTorchMode(TorchMode mode) {
  google_camera_hal::TorchMode hal_torch_mode;
  status_t res = hidl_utils::ConvertToHalTorchMode(mode, &hal_torch_mode);
  if (res != OK) {
    ALOGE("%s: failed to convert torch mode", __FUNCTION__);
    return Status::INTERNAL_ERROR;
  }

  res = google_camera_device_->SetTorchMode(hal_torch_mode);
  return hidl_utils::ConvertToHidlStatus(res);
}

Return<void> HidlCameraDevice::open(
    const sp<V3_2::ICameraDeviceCallback>& callback,
    ICameraDevice::open_cb _hidl_cb) {
  auto profiler_item =
      ::android::hardware::camera::implementation::hidl_profiler::OnCameraOpen();

  std::unique_ptr<google_camera_hal::CameraDeviceSession> session;
  status_t res = google_camera_device_->CreateCameraDeviceSession(&session);
  if (res != OK || session == nullptr) {
    ALOGE("%s: Creating CameraDeviceSession failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    _hidl_cb(hidl_utils::ConvertToHidlStatus(res), nullptr);
    return Void();
  }

  auto hidl_session =
      HidlCameraDeviceSession::Create(callback, std::move(session));
  if (hidl_session == nullptr) {
    ALOGE("%s: Creating HidlCameraDeviceSession failed.", __FUNCTION__);
    _hidl_cb(hidl_utils::ConvertToHidlStatus(res), nullptr);
    return Void();
  }

  _hidl_cb(Status::OK, hidl_session.release());
  return Void();
}

Return<void> HidlCameraDevice::dumpState(
    const ::android::hardware::hidl_handle& handle) {
  if (handle.getNativeHandle() == nullptr) {
    ALOGE("%s: handle is nullptr", __FUNCTION__);
    return Void();
  }

  if (handle->numFds != 1 || handle->numInts != 0) {
    ALOGE("%s: handle must contain 1 fd(%d) and 0 ints(%d)", __FUNCTION__,
          handle->numFds, handle->numInts);
    return Void();
  }

  int fd = handle->data[0];
  google_camera_device_->DumpState(fd);
  return Void();
}

Return<void> HidlCameraDevice::getPhysicalCameraCharacteristics(
    const hidl_string& physicalCameraId,
    ICameraDevice::getPhysicalCameraCharacteristics_cb _hidl_cb) {
  V3_2::CameraMetadata hidl_characteristics;
  std::unique_ptr<HalCameraMetadata> physical_characteristics;

  uint32_t physical_camera_id = atoi(physicalCameraId.c_str());
  status_t res = google_camera_device_->GetPhysicalCameraCharacteristics(
      physical_camera_id, &physical_characteristics);
  if (res != OK) {
    ALOGE("%s: Getting physical characteristics for camera %u failed: %s(%d)",
          __FUNCTION__, camera_id_, strerror(-res), res);
    _hidl_cb(hidl_utils::ConvertToHidlStatus(res), hidl_characteristics);
    return Void();
  }

  if (physical_characteristics == nullptr) {
    ALOGE("%s: Physical characteristics for camera %u is nullptr.",
          __FUNCTION__, physical_camera_id);
    _hidl_cb(Status::INTERNAL_ERROR, hidl_characteristics);
    return Void();
  }

  uint32_t metadata_size = physical_characteristics->GetCameraMetadataSize();
  hidl_characteristics.setToExternal(
      (uint8_t*)physical_characteristics->ReleaseCameraMetadata(),
      metadata_size, /*shouldOwn=*/true);

  _hidl_cb(Status::OK, hidl_characteristics);
  return Void();
}

Return<void> HidlCameraDevice::isStreamCombinationSupported(
    const V3_4::StreamConfiguration& streams,
    ICameraDevice::isStreamCombinationSupported_cb _hidl_cb) {
  bool is_supported = false;
  google_camera_hal::StreamConfiguration stream_config;
  status_t res = hidl_utils::ConverToHalStreamConfig(streams, &stream_config);
  if (res != OK) {
    ALOGE("%s: ConverToHalStreamConfig fail", __FUNCTION__);
    _hidl_cb(Status::INTERNAL_ERROR, is_supported);
    return Void();
  }
  is_supported =
      google_camera_device_->IsStreamCombinationSupported(stream_config);

  _hidl_cb(Status::OK, is_supported);
  return Void();
}

}  // namespace implementation
}  // namespace V3_5
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android