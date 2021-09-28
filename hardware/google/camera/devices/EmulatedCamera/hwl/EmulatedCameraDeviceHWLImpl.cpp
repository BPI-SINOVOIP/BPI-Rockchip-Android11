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

//#define LOG_NDEBUG 0
#define LOG_TAG "EmulatedCameraDeviceHwlImpl"
#include "EmulatedCameraDeviceHWLImpl.h"

#include <hardware/camera_common.h>
#include <log/log.h>

#include "EmulatedCameraDeviceSessionHWLImpl.h"
#include "utils/HWLUtils.h"

namespace android {

std::unique_ptr<CameraDeviceHwl> EmulatedCameraDeviceHwlImpl::Create(
    uint32_t camera_id, std::unique_ptr<HalCameraMetadata> static_meta,
    PhysicalDeviceMapPtr physical_devices,
    std::shared_ptr<EmulatedTorchState> torch_state) {
  auto device = std::unique_ptr<EmulatedCameraDeviceHwlImpl>(
      new EmulatedCameraDeviceHwlImpl(camera_id, std::move(static_meta),
                                      std::move(physical_devices),
                                      torch_state));

  if (device == nullptr) {
    ALOGE("%s: Creating EmulatedCameraDeviceHwlImpl failed.", __FUNCTION__);
    return nullptr;
  }

  status_t res = device->Initialize();
  if (res != OK) {
    ALOGE("%s: Initializing EmulatedCameraDeviceHwlImpl failed: %s (%d).",
          __FUNCTION__, strerror(-res), res);
    return nullptr;
  }

  ALOGI("%s: Created EmulatedCameraDeviceHwlImpl for camera %u", __FUNCTION__,
        device->camera_id_);

  return device;
}

EmulatedCameraDeviceHwlImpl::EmulatedCameraDeviceHwlImpl(
    uint32_t camera_id, std::unique_ptr<HalCameraMetadata> static_meta,
    PhysicalDeviceMapPtr physical_devices,
    std::shared_ptr<EmulatedTorchState> torch_state)
    : camera_id_(camera_id),
      static_metadata_(std::move(static_meta)),
      physical_device_map_(std::move(physical_devices)),
      torch_state_(torch_state) {}

uint32_t EmulatedCameraDeviceHwlImpl::GetCameraId() const {
  return camera_id_;
}

status_t EmulatedCameraDeviceHwlImpl::Initialize() {
  auto ret = GetSensorCharacteristics(static_metadata_.get(), &sensor_chars_);
  if (ret != OK) {
    ALOGE("%s: Unable to extract sensor characteristics %s (%d)", __FUNCTION__,
          strerror(-ret), ret);
    return ret;
  }

  stream_coniguration_map_ =
      std::make_unique<StreamConfigurationMap>(*static_metadata_);

  return OK;
}

status_t EmulatedCameraDeviceHwlImpl::GetResourceCost(
    CameraResourceCost* cost) const {
  // TODO: remove hardcode
  cost->resource_cost = 100;

  return OK;
}

status_t EmulatedCameraDeviceHwlImpl::GetCameraCharacteristics(
    std::unique_ptr<HalCameraMetadata>* characteristics) const {
  if (characteristics == nullptr) {
    return BAD_VALUE;
  }

  *characteristics = HalCameraMetadata::Clone(static_metadata_.get());

  return OK;
}

status_t EmulatedCameraDeviceHwlImpl::GetPhysicalCameraCharacteristics(
    uint32_t physical_camera_id,
    std::unique_ptr<HalCameraMetadata>* characteristics) const {
  if (characteristics == nullptr) {
    return BAD_VALUE;
  }

  if (physical_device_map_.get() == nullptr) {
    ALOGE("%s: Camera %d is not a logical device!", __func__, camera_id_);
    return NO_INIT;
  }

  if (physical_device_map_->find(physical_camera_id) ==
      physical_device_map_->end()) {
    ALOGE("%s: Physical camera id %d is not part of logical camera %d!",
          __func__, physical_camera_id, camera_id_);
    return BAD_VALUE;
  }

  *characteristics = HalCameraMetadata::Clone(
      physical_device_map_->at(physical_camera_id).second.get());

  return OK;
}

status_t EmulatedCameraDeviceHwlImpl::SetTorchMode(TorchMode mode) {
  if (torch_state_.get() == nullptr) {
    return INVALID_OPERATION;
  }

  return torch_state_->SetTorchMode(mode);
}

status_t EmulatedCameraDeviceHwlImpl::DumpState(int /*fd*/) {
  return OK;
}

status_t EmulatedCameraDeviceHwlImpl::CreateCameraDeviceSessionHwl(
    CameraBufferAllocatorHwl* /*camera_allocator_hwl*/,
    std::unique_ptr<CameraDeviceSessionHwl>* session) {
  if (session == nullptr) {
    ALOGE("%s: session is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  std::unique_ptr<HalCameraMetadata> meta =
      HalCameraMetadata::Clone(static_metadata_.get());
  *session = EmulatedCameraDeviceSessionHwlImpl::Create(
      camera_id_, std::move(meta), ClonePhysicalDeviceMap(physical_device_map_),
      torch_state_);
  if (*session == nullptr) {
    ALOGE("%s: Cannot create EmulatedCameraDeviceSessionHWlImpl.", __FUNCTION__);
    return BAD_VALUE;
  }

  if (torch_state_.get() != nullptr) {
    torch_state_->AcquireFlashHw();
  }

  return OK;
}

bool EmulatedCameraDeviceHwlImpl::IsStreamCombinationSupported(
    const StreamConfiguration& stream_config) {
  return EmulatedSensor::IsStreamCombinationSupported(
      stream_config, *stream_coniguration_map_, sensor_chars_);
}

}  // namespace android
