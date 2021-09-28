//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "update_engine/boot_control_android.h"

#include <memory>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <bootloader_message/bootloader_message.h>
#include <brillo/message_loops/message_loop.h>

#include "update_engine/common/utils.h"
#include "update_engine/dynamic_partition_control_android.h"

using std::string;

using android::dm::DmDeviceState;
using android::hardware::hidl_string;
using android::hardware::Return;
using android::hardware::boot::V1_0::BoolResult;
using android::hardware::boot::V1_0::CommandResult;
using android::hardware::boot::V1_0::IBootControl;
using Slot = chromeos_update_engine::BootControlInterface::Slot;

namespace {

auto StoreResultCallback(CommandResult* dest) {
  return [dest](const CommandResult& result) { *dest = result; };
}
}  // namespace

namespace chromeos_update_engine {

namespace boot_control {

// Factory defined in boot_control.h.
std::unique_ptr<BootControlInterface> CreateBootControl() {
  auto boot_control = std::make_unique<BootControlAndroid>();
  if (!boot_control->Init()) {
    return nullptr;
  }
  return std::move(boot_control);
}

}  // namespace boot_control

bool BootControlAndroid::Init() {
  module_ = IBootControl::getService();
  if (module_ == nullptr) {
    LOG(ERROR) << "Error getting bootctrl HIDL module.";
    return false;
  }

  LOG(INFO) << "Loaded boot control hidl hal.";

  dynamic_control_ = std::make_unique<DynamicPartitionControlAndroid>();

  return true;
}

unsigned int BootControlAndroid::GetNumSlots() const {
  return module_->getNumberSlots();
}

BootControlInterface::Slot BootControlAndroid::GetCurrentSlot() const {
  return module_->getCurrentSlot();
}


bool BootControlAndroid::GetPartitionDevice(const string& partition_name,
                                            Slot slot,
                                            string* device) const {
  return dynamic_control_->GetPartitionDevice(
      partition_name, slot, GetCurrentSlot(), device);
}

bool BootControlAndroid::IsSlotBootable(Slot slot) const {
  Return<BoolResult> ret = module_->isSlotBootable(slot);
  if (!ret.isOk()) {
    LOG(ERROR) << "Unable to determine if slot " << SlotName(slot)
               << " is bootable: " << ret.description();
    return false;
  }
  if (ret == BoolResult::INVALID_SLOT) {
    LOG(ERROR) << "Invalid slot: " << SlotName(slot);
    return false;
  }
  return ret == BoolResult::TRUE;
}

bool BootControlAndroid::MarkSlotUnbootable(Slot slot) {
  CommandResult result;
  auto ret = module_->setSlotAsUnbootable(slot, StoreResultCallback(&result));
  if (!ret.isOk()) {
    LOG(ERROR) << "Unable to call MarkSlotUnbootable for slot "
               << SlotName(slot) << ": " << ret.description();
    return false;
  }
  if (!result.success) {
    LOG(ERROR) << "Unable to mark slot " << SlotName(slot)
               << " as unbootable: " << result.errMsg.c_str();
  }
  return result.success;
}

bool BootControlAndroid::SetActiveBootSlot(Slot slot) {
  CommandResult result;
  auto ret = module_->setActiveBootSlot(slot, StoreResultCallback(&result));
  if (!ret.isOk()) {
    LOG(ERROR) << "Unable to call SetActiveBootSlot for slot " << SlotName(slot)
               << ": " << ret.description();
    return false;
  }
  if (!result.success) {
    LOG(ERROR) << "Unable to set the active slot to slot " << SlotName(slot)
               << ": " << result.errMsg.c_str();
  }
  return result.success;
}

bool BootControlAndroid::MarkBootSuccessfulAsync(
    base::Callback<void(bool)> callback) {
  CommandResult result;
  auto ret = module_->markBootSuccessful(StoreResultCallback(&result));
  if (!ret.isOk()) {
    LOG(ERROR) << "Unable to call MarkBootSuccessful: " << ret.description();
    return false;
  }
  if (!result.success) {
    LOG(ERROR) << "Unable to mark boot successful: " << result.errMsg.c_str();
  }
  return brillo::MessageLoop::current()->PostTask(
             FROM_HERE, base::Bind(callback, result.success)) !=
         brillo::MessageLoop::kTaskIdNull;
}

bool BootControlAndroid::IsSlotMarkedSuccessful(
    BootControlInterface::Slot slot) const {
  Return<BoolResult> ret = module_->isSlotMarkedSuccessful(slot);
  CommandResult result;
  if (!ret.isOk()) {
    LOG(ERROR) << "Unable to determine if slot " << SlotName(slot)
               << " is marked successful: " << ret.description();
    return false;
  }
  if (ret == BoolResult::INVALID_SLOT) {
    LOG(ERROR) << "Invalid slot: " << SlotName(slot);
    return false;
  }
  return ret == BoolResult::TRUE;
}

DynamicPartitionControlInterface*
BootControlAndroid::GetDynamicPartitionControl() {
  return dynamic_control_.get();
}

}  // namespace chromeos_update_engine
