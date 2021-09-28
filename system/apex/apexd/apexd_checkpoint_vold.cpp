/*
 * Copyright (C) 2018 The Android Open Source Project
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

#define LOG_TAG "apexd"

#include "apexd_checkpoint_vold.h"

#include <android-base/logging.h>
#include <android/os/IVold.h>
#include <binder/IServiceManager.h>

using android::sp;
using android::base::Error;
using android::base::Result;
using android::os::IVold;

namespace android {
namespace apex {

Result<VoldCheckpointInterface> VoldCheckpointInterface::Create() {
  auto voldService =
      defaultServiceManager()->getService(android::String16("vold"));
  if (voldService != nullptr) {
    return VoldCheckpointInterface(
        android::interface_cast<android::os::IVold>(voldService));
  }
  return Errorf("Failed to retrieve vold service.");
}

VoldCheckpointInterface::VoldCheckpointInterface(sp<IVold>&& vold_service) {
  vold_service_ = vold_service;
  supports_fs_checkpoints_ = false;
  android::binder::Status status =
      vold_service_->supportsCheckpoint(&supports_fs_checkpoints_);
  if (!status.isOk()) {
    LOG(ERROR) << "Failed to check if filesystem checkpoints are supported: "
               << status.toString8().c_str();
  }
}

VoldCheckpointInterface::VoldCheckpointInterface(
    VoldCheckpointInterface&& other) noexcept {
  vold_service_ = std::move(other.vold_service_);
  supports_fs_checkpoints_ = other.supports_fs_checkpoints_;
}

VoldCheckpointInterface::~VoldCheckpointInterface() {
  // Just here to be able to forward-declare IVold.
}

Result<bool> VoldCheckpointInterface::SupportsFsCheckpoints() {
  return supports_fs_checkpoints_;
}

Result<bool> VoldCheckpointInterface::NeedsCheckpoint() {
  if (supports_fs_checkpoints_) {
    bool needs_checkpoint = false;
    android::binder::Status status =
        vold_service_->needsCheckpoint(&needs_checkpoint);
    if (!status.isOk()) {
      return Error() << status.toString8().c_str();
    }
    return needs_checkpoint;
  }
  return false;
}

Result<bool> VoldCheckpointInterface::NeedsRollback() {
  if (supports_fs_checkpoints_) {
    bool needs_rollback = false;
    android::binder::Status status =
        vold_service_->needsRollback(&needs_rollback);
    if (!status.isOk()) {
      return Error() << status.toString8().c_str();
    }
    return needs_rollback;
  }
  return false;
}

Result<void> VoldCheckpointInterface::StartCheckpoint(int32_t numRetries) {
  if (supports_fs_checkpoints_) {
    android::binder::Status status = vold_service_->startCheckpoint(numRetries);
    if (!status.isOk()) {
      return Error() << status.toString8().c_str();
    }
    return {};
  }
  return Errorf("Device does not support filesystem checkpointing");
}

Result<void> VoldCheckpointInterface::AbortChanges(const std::string& msg,
                                                   bool retry) {
  vold_service_->abortChanges(msg, retry);
  return {};
}

}  // namespace apex
}  // namespace android
