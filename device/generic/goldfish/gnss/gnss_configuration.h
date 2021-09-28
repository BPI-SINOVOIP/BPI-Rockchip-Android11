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

#pragma once
#include <android/hardware/gnss/2.0/IGnssConfiguration.h>

namespace goldfish {
namespace ahg = ::android::hardware::gnss;
namespace ahg20 = ahg::V2_0;
namespace ahg11 = ahg::V1_1;
namespace ahg10 = ahg::V1_0;

using ::android::sp;
using ::android::hardware::hidl_bitfield;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;

struct Gnss20;

struct GnssConfiguration20 : public ahg20::IGnssConfiguration {
    // Methods from ::android::hardware::gnss::V2_0::IGnssConfiguration follow.
    Return<bool> setEsExtensionSec(uint32_t emergencyExtensionSeconds) override;

    // Methods from ::android::hardware::gnss::V1_1::IGnssConfiguration follow.
    Return<bool> setBlacklist(const hidl_vec<ahg11::IGnssConfiguration::BlacklistedSource>& blacklist) override;

    // Methods from ::android::hardware::gnss::V1_0::IGnssConfiguration follow.
    Return<bool> setSuplEs(bool enabled) override;
    Return<bool> setSuplVersion(uint32_t version) override;
    Return<bool> setSuplMode(hidl_bitfield<SuplMode> mode) override;
    Return<bool> setGpsLock(hidl_bitfield<GpsLock> lock) override;
    Return<bool> setLppProfile(hidl_bitfield<LppProfile> lppProfile) override;
    Return<bool> setGlonassPositioningProtocol(hidl_bitfield<GlonassPosProtocol> protocol) override;
    Return<bool> setEmergencySuplPdn(bool enable) override;
};

}  // namespace goldfish
