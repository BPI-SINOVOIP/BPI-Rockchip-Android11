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

#include <system/audio.h>
#include <log/log.h>
#include "device_factory.h"
#include "primary_device.h"
#include "debug.h"

namespace android {
namespace hardware {
namespace audio {
namespace V6_0 {
namespace implementation {

using ::android::hardware::Void;

#ifdef __LP64__
#define LIB_PATH_PREFIX "vendor/lib64/hw/"
#else
#define LIB_PATH_PREFIX "vendor/lib/hw/"
#endif

DevicesFactory::DevicesFactory() {
    mLegacyLib.reset(dlopen(
        LIB_PATH_PREFIX "android.hardware.audio@6.0-impl.so", RTLD_NOW));
    LOG_ALWAYS_FATAL_IF(!mLegacyLib);

    typedef IDevicesFactory *(*Func)(const char *);
    const auto func = reinterpret_cast<Func>(
        dlsym(mLegacyLib.get(), "HIDL_FETCH_IDevicesFactory"));
    LOG_ALWAYS_FATAL_IF(!func);

    mLegacyFactory.reset((*func)("default"));
    LOG_ALWAYS_FATAL_IF(!mLegacyFactory);
}

Return<void> DevicesFactory::openDevice(const hidl_string& device,
                                        openDevice_cb _hidl_cb) {
    if (device == AUDIO_HARDWARE_MODULE_ID_PRIMARY)
        _hidl_cb(Result::OK, new PrimaryDevice);
    else {
        mLegacyFactory->openDevice(device, _hidl_cb);
    }

    return Void();
}

Return<void> DevicesFactory::openPrimaryDevice(openPrimaryDevice_cb _hidl_cb) {
    _hidl_cb(Result::OK, new PrimaryDevice);
    return Void();
}

}  // namespace implementation
}  // namespace V6_0
}  // namespace audio
}  // namespace hardware
}  // namespace android
