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
#define LOG_TAG "EmulatedTorchState"
#include "EmulatedTorchState.h"

#include <log/log.h>

namespace android {

using android::google_camera_hal::TorchModeStatus;

status_t EmulatedTorchState::SetTorchMode(TorchMode mode) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (camera_open_) {
    ALOGE("%s: Camera device open, torch cannot be controlled using this API!",
          __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  torch_cb_(camera_id_, (mode == TorchMode::kOn)
                            ? TorchModeStatus::kAvailableOn
                            : TorchModeStatus::kAvailableOff);

  return OK;
}

void EmulatedTorchState::AcquireFlashHw() {
  std::lock_guard<std::mutex> lock(mutex_);
  camera_open_ = true;
  torch_cb_(camera_id_, TorchModeStatus::kNotAvailable);
}

void EmulatedTorchState::ReleaseFlashHw() {
  std::lock_guard<std::mutex> lock(mutex_);
  camera_open_ = false;
  torch_cb_(camera_id_, TorchModeStatus::kAvailableOff);
}

}  // namespace android
