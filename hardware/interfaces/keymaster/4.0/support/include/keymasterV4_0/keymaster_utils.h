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

#ifndef HARDWARE_INTERFACES_KEYMASTER_40_SUPPORT_KEYMASTER_UTILS_H_
#define HARDWARE_INTERFACES_KEYMASTER_40_SUPPORT_KEYMASTER_UTILS_H_

#include <android/hardware/keymaster/4.0/types.h>
#include <optional>
#include <vector>

namespace android {
namespace hardware {
namespace keymaster {
namespace V4_0 {

/**
 * Define a lexicographical ordering on HmacSharingParameters.  The parameters to
 * IKeymasterDevice::computeSharedHmac are required to be delivered in the order specified by this
 * comparison operator.
 */
bool operator<(const HmacSharingParameters& a, const HmacSharingParameters& b);

namespace support {

inline static hidl_vec<uint8_t> blob2hidlVec(const uint8_t* data, const size_t length) {
    hidl_vec<uint8_t> result(data, data + length);
    return result;
}

inline static hidl_vec<uint8_t> blob2hidlVec(const std::string& value) {
    hidl_vec<uint8_t> result(reinterpret_cast<const uint8_t*>(value.data()),
                             reinterpret_cast<const uint8_t*>(value.data()) + value.size());
    return result;
}

inline static hidl_vec<uint8_t> blob2hidlVec(const std::vector<uint8_t>& blob) {
    hidl_vec<uint8_t> result(blob.data(), blob.data() + static_cast<size_t>(blob.size()));
    return result;
}

HardwareAuthToken hidlVec2AuthToken(const hidl_vec<uint8_t>& buffer);
hidl_vec<uint8_t> authToken2HidlVec(const HardwareAuthToken& token);

// Serializes and deserializes a verification token. This format is private and
// not stable between releases and should not be persisted to disk.
//
// Currently doesn't support the |parametersVerified| field, will fail if set.
//
std::optional<VerificationToken> deserializeVerificationToken(
        const std::vector<uint8_t>& serializedToken);
std::optional<std::vector<uint8_t>> serializeVerificationToken(const VerificationToken& token);

uint32_t getOsVersion();
uint32_t getOsPatchlevel();

}  // namespace support
}  // namespace V4_0
}  // namespace keymaster
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_INTERFACES_KEYMASTER_40_SUPPORT_KEYMASTER_UTILS_H_
