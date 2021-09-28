/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.1 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.1
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "rebootescrow-impl/RebootEscrow.h"

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

using aidl::android::hardware::rebootescrow::RebootEscrow;

constexpr auto kRebootEscrowDeviceProperty = "ro.rebootescrow.device";
constexpr auto kRebootEscrowDeviceDefault = "/dev/access-kregistry";

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);

    auto rebootEscrowDevicePath =
            android::base::GetProperty(kRebootEscrowDeviceProperty, kRebootEscrowDeviceDefault);
    auto re = ndk::SharedRefBase::make<RebootEscrow>(rebootEscrowDevicePath);
    const std::string instance = std::string() + RebootEscrow::descriptor + "/default";
    binder_status_t status = AServiceManager_addService(re->asBinder().get(), instance.c_str());
    CHECK(status == STATUS_OK);

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;
}
