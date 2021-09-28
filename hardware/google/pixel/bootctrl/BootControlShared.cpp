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

#define LOG_TAG "bootcontrolhal"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <bootloader_message/bootloader_message.h>
#include <libboot_control/libboot_control.h>

#include "BootControlShared.h"

namespace android {
namespace hardware {
namespace boot {
namespace V1_1 {
namespace implementation {

using android::bootable::GetMiscVirtualAbMergeStatus;
using android::bootable::InitMiscVirtualAbMessageIfNeeded;
using android::bootable::SetMiscVirtualAbMergeStatus;
using android::hardware::boot::V1_1::MergeStatus;

BootControlShared::BootControlShared() {}

bool BootControlShared::Init() {
    return InitMiscVirtualAbMessageIfNeeded();
}

Return<bool> BootControlShared::setSnapshotMergeStatus(MergeStatus status) {
    return SetMiscVirtualAbMergeStatus(getCurrentSlot(), status);
}

Return<MergeStatus> BootControlShared::getSnapshotMergeStatus() {
    MergeStatus status;
    if (!GetMiscVirtualAbMergeStatus(getCurrentSlot(), &status)) {
        return MergeStatus::UNKNOWN;
    }
    return status;
}

}  // namespace implementation
}  // namespace V1_1
}  // namespace boot
}  // namespace hardware
}  // namespace android
