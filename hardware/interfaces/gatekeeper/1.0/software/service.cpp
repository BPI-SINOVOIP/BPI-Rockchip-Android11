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
#define LOG_TAG "android.hardware.gatekeeper@1.0-service"

#include <android-base/logging.h>
#include <android/hardware/gatekeeper/1.0/IGatekeeper.h>

#include <hidl/LegacySupport.h>

#include "SoftGateKeeperDevice.h"

// Generated HIDL files
using android::SoftGateKeeperDevice;
using android::hardware::gatekeeper::V1_0::IGatekeeper;

int main() {
    ::android::hardware::configureRpcThreadpool(1, true /* willJoinThreadpool */);
    android::sp<SoftGateKeeperDevice> gatekeeper(new SoftGateKeeperDevice());
    auto status = gatekeeper->registerAsService();
    if (status != android::OK) {
        LOG(FATAL) << "Could not register service for Gatekeeper 1.0 (software) (" << status << ")";
    }

    android::hardware::joinRpcThreadpool();
    return -1;  // Should never get here.
}
