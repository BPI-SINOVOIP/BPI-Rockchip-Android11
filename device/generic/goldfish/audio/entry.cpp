/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <binder/ProcessState.h>
#include <hwbinder/ProcessState.h>
#include <hidl/LegacySupport.h>
#include "device_factory.h"

int main(int, char**) {
    using ::android::sp;
    using ::android::OK;
    using ::android::hardware::audio::V6_0::IDevicesFactory;
    using ::android::hardware::audio::V6_0::implementation::DevicesFactory;
    using ::android::hardware::registerPassthroughServiceImplementation;

    ::android::ProcessState::initWithDriver("/dev/vndbinder");
    ::android::ProcessState::self()->startThreadPool();
    ::android::hardware::configureRpcThreadpool(16, true /* callerWillJoin */);

    sp<IDevicesFactory> factory(new DevicesFactory());
    if (factory->registerAsService() != ::android::NO_ERROR) {
        return -EINVAL;
    }

    if (registerPassthroughServiceImplementation(
        "android.hardware.audio.effect@6.0::IEffectsFactory") != OK) {
        return -EINVAL;
    }

    if (registerPassthroughServiceImplementation(
        "android.hardware.soundtrigger@2.2::ISoundTriggerHw") != OK) {
        return -EINVAL;
    }

    ::android::hardware::joinRpcThreadpool();
    return 0;
}
