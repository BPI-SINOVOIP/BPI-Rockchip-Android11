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

#include "gnss_configuration.h"

namespace goldfish {

Return<bool> GnssConfiguration20::setEsExtensionSec(uint32_t emergencyExtensionSeconds) {
    (void)emergencyExtensionSeconds;
    return false;
}

Return<bool> GnssConfiguration20::setBlacklist(const hidl_vec<ahg11::IGnssConfiguration::BlacklistedSource>& blacklist) {
    (void)blacklist;
    return false;
}

Return<bool> GnssConfiguration20::setSuplVersion(uint32_t version) {
    (void)version;
    return true;
}

Return<bool> GnssConfiguration20::setSuplMode(hidl_bitfield<SuplMode> mode) {
    (void)mode;
    return true;
}

Return<bool> GnssConfiguration20::setGpsLock(hidl_bitfield<GpsLock> lock) {
    (void)lock;
    return false;
}

Return<bool> GnssConfiguration20::setLppProfile(hidl_bitfield<LppProfile> lppProfile) {
    (void)lppProfile;
    return true;
}

Return<bool> GnssConfiguration20::setGlonassPositioningProtocol(hidl_bitfield<GlonassPosProtocol> protocol) {
    (void)protocol;
    return true;
}

Return<bool> GnssConfiguration20::setEmergencySuplPdn(bool enable) {
    (void)enable;
    return true;
}

/// old and deprecated /////////////////////////////////////////////////////////
Return<bool> GnssConfiguration20::setSuplEs(bool enable) {
    (void)enable;
    return false;
}

}  // namespace goldfish
