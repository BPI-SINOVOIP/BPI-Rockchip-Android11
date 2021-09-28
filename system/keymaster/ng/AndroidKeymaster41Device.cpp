/*
 **
 ** Copyright 2019, The Android Open Source Project
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#define LOG_TAG "android.hardware.keymaster@4.1 ref impl"
#include <log/log.h>

#include "include/AndroidKeymaster41Device.h"

#include <keymaster/android_keymaster.h>

namespace keymaster::V4_1 {

using V4_0::ng::hidlKeyParams2Km;

namespace {

inline V41ErrorCode legacy_enum_conversion(const keymaster_error_t value) {
    return static_cast<V41ErrorCode>(value);
}

}  // namespace

IKeymasterDevice* CreateKeymasterDevice(SecurityLevel securityLevel) {
    return new AndroidKeymaster41Device(securityLevel);
}

Return<V41ErrorCode>
AndroidKeymaster41Device::deviceLocked(bool passwordOnly,
                                       const VerificationToken& verificationToken) {
    keymaster::VerificationToken serializableToken;
    serializableToken.challenge = verificationToken.challenge;
    serializableToken.timestamp = verificationToken.timestamp;
    serializableToken.parameters_verified.Reinitialize(
        hidlKeyParams2Km(verificationToken.parametersVerified));
    serializableToken.security_level =
        static_cast<keymaster_security_level_t>(verificationToken.securityLevel);
    serializableToken.mac =
        KeymasterBlob(verificationToken.mac.data(), verificationToken.mac.size());
    return legacy_enum_conversion(
        impl_->DeviceLocked(DeviceLockedRequest(passwordOnly, std::move(serializableToken))).error);
}

Return<V41ErrorCode> AndroidKeymaster41Device::earlyBootEnded() {
    return legacy_enum_conversion(impl_->EarlyBootEnded().error);
}

}  // namespace keymaster::V4_1
