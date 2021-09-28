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
#define LOG_TAG "android.hardware.boot@1.1-impl"

#include <memory>

#include <log/log.h>

#include <android-base/logging.h>
#include <hardware/boot_control.h>
#include <hardware/hardware.h>

#include "BootControl.h"

namespace android {
namespace hardware {
namespace boot {
namespace V1_1 {
namespace implementation {

using ::android::hardware::boot::V1_0::CommandResult;

BootControl::BootControl(boot_control_module_t *module) : mModule(module) {
    impl_.Init();
}

BootControl::BootControl() {
    impl_.Init();
}


bool BootControl::Init() {
    ALOGI("Info rk BootControl::Init ");

    return impl_.Init();
}

// Methods from ::android::hardware::boot::V1_0::IBootControl follow.
Return<uint32_t> BootControl::getNumberSlots() {
    ALOGI("Info rk BootControl::getNumberSlots ");

    return impl_.GetNumberSlots();
}

Return<uint32_t> BootControl::getCurrentSlot() {
    ALOGI("Info rk BootControl::getCurrentSlot ");

    mModule->getNumberSlots(mModule);
    return impl_.GetCurrentSlot();
}

Return<void> BootControl::markBootSuccessful(markBootSuccessful_cb _hidl_cb) {
    struct CommandResult cr;
    ALOGI("Info rk BootControl::markBootSuccessful ");

    if (0 == impl_.MarkBootSuccessful()) {
        cr.success = true;
        cr.errMsg = "Success";
        ALOGI("Info rk BootControl::markBootSuccessful ok ");
    } else {
        cr.success = false;
        cr.errMsg = "Operation failed";
        ALOGE("Info rk BootControl::markBootSuccessful failed ");
    }
    _hidl_cb(cr);
    return Void();
}

Return<void> BootControl::setActiveBootSlot(uint32_t slot, setActiveBootSlot_cb _hidl_cb) {
    struct CommandResult cr;
    ALOGI("Info rk BootControl::setActiveBootSlot ");

    if (0 == impl_.SetActiveBootSlot(slot)) {
        cr.success = true;
        cr.errMsg = "Success";
        ALOGI("Info rk BootControl::setActiveBootSlot ok ");
    } else {
        cr.success = false;
        cr.errMsg = "Operation failed";
        ALOGE("Info rk BootControl::setActiveBootSlot failed ");
    }
    _hidl_cb(cr);
    return Void();
}

Return<void> BootControl::setSlotAsUnbootable(uint32_t slot, setSlotAsUnbootable_cb _hidl_cb) {
    struct CommandResult cr;
    ALOGI("Info rk BootControl::setSlotAsUnbootable ");

    if (0 == impl_.SetSlotAsUnbootable(slot)) {
        cr.success = true;
        cr.errMsg = "Success";
        ALOGI("Info rk BootControl::setSlotAsUnbootable ok ");
    } else {
        cr.success = false;
        cr.errMsg = "Operation failed";
        ALOGE("Info rk BootControl::setSlotAsUnbootable failed ");
    }
    _hidl_cb(cr);
    return Void();
}

Return<BoolResult> BootControl::isSlotBootable(uint32_t slot) {
    ALOGI("Info rk BootControl::isSlotBootable ");

    if (!impl_.IsValidSlot(slot)) {
        ALOGE("Info rk BootControl::isSlotBootable Invalid slot ");
        return BoolResult::INVALID_SLOT;
    }
    return impl_.IsSlotBootable(slot) ? BoolResult::TRUE : BoolResult::FALSE;
}

Return<BoolResult> BootControl::isSlotMarkedSuccessful(uint32_t slot) {
    ALOGI("Info rk BootControl::isSlotMarkedSuccessful ");

    if (!impl_.IsValidSlot(slot)) {
        return BoolResult::INVALID_SLOT;
        ALOGE("Info rk BootControl::isSlotMarkedSuccessful invlaid slot ");
    }
    return impl_.IsSlotMarkedSuccessful(slot) ? BoolResult::TRUE : BoolResult::FALSE;
}

Return<void> BootControl::getSuffix(uint32_t slot, getSuffix_cb _hidl_cb) {
    hidl_string ans;
    ALOGI("Info rk BootControl::getSuffix ");

    const char* suffix = impl_.GetSuffix(slot);
    if (suffix) {
        ans = suffix;
    }
    _hidl_cb(ans);
    return Void();
}

Return<bool> BootControl::setSnapshotMergeStatus(MergeStatus status) {
    ALOGI("Info rk BootControl::setSnapshotMergeStatus ");

    return impl_.SetSnapshotMergeStatus(status);
}

Return<MergeStatus> BootControl::getSnapshotMergeStatus() {
    ALOGI("Info rk BootControl::getSnapshotMergeStatus ");

    return impl_.GetSnapshotMergeStatus();
}

IBootControl* HIDL_FETCH_IBootControl(const char* /* hal */) {
    setenv("ANDROID_LOG_TAGS", "*:v", 1);
    LOG(INFO) << "LOG INFO rk BootControl HIDL_FETCH_IBootControl ";
    ALOGI("Info rk BootControl HIDL_FETCH_IBootControl ");

#if 0
    auto module = std::make_unique<BootControl>();
	if (!module) {
		 ALOGE("module is NULL");
	}

    if (!module->Init()) {
        ALOGE("Could not initialize BootControl module");
        return nullptr;
    }
    return module.release();
#else
    int ret = 0;
    boot_control_module_t *module = NULL;
    hw_module_t **hwm = reinterpret_cast<hw_module_t **>(&module);
    ret = hw_get_module(BOOT_CONTROL_HARDWARE_MODULE_ID, const_cast<const hw_module_t **>(hwm));
    if (ret) {
        ALOGE("hw_get_module %s failed: %d", BOOT_CONTROL_HARDWARE_MODULE_ID, ret);
        return nullptr;
    }
    module->init(module);

    auto hal = new BootControl(module);
    if (!hal->Init()) {
        ALOGE("Failed to initialize boot control HAL");
    }
    return hal;

#endif
}

}  // namespace implementation
}  // namespace V1_1
}  // namespace boot
}  // namespace hardware
}  // namespace android
