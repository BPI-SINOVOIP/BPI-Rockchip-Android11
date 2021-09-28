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

#include <android-base/logging.h>

#include "supplicant_hidl_test_utils.h"
#include "supplicant_hidl_test_utils_1_3.h"

using ::android::sp;
using ::android::hardware::wifi::supplicant::V1_0::SupplicantStatus;
using ::android::hardware::wifi::supplicant::V1_0::SupplicantStatusCode;
using ::android::hardware::wifi::supplicant::V1_3::ISupplicant;
using ::android::hardware::wifi::supplicant::V1_3::ISupplicantStaIface;
using ::android::hardware::wifi::supplicant::V1_3::ISupplicantStaNetwork;

sp<ISupplicantStaIface> getSupplicantStaIface_1_3(
    const android::sp<android::hardware::wifi::supplicant::V1_3::ISupplicant>&
        supplicant) {
    return ISupplicantStaIface::castFrom(getSupplicantStaIface(supplicant));
}

sp<ISupplicantStaNetwork> createSupplicantStaNetwork_1_3(
    const android::sp<android::hardware::wifi::supplicant::V1_3::ISupplicant>&
        supplicant) {
    return ISupplicantStaNetwork::castFrom(
        createSupplicantStaNetwork(supplicant));
}

sp<ISupplicant> getSupplicant_1_3(const std::string& supplicant_instance_name,
                                  bool isP2pOn) {
    return ISupplicant::castFrom(
        getSupplicant(supplicant_instance_name, isP2pOn));
}

bool isFilsSupported(sp<ISupplicantStaIface> sta_iface) {
    uint32_t keyMgmtMask = 0;
    sta_iface->getKeyMgmtCapabilities_1_3(
        [&](const SupplicantStatus& status, uint32_t keyMgmtMaskInternal) {
            EXPECT_EQ(SupplicantStatusCode::SUCCESS, status.code);
            keyMgmtMask = keyMgmtMaskInternal;
        });

    return (keyMgmtMask & (ISupplicantStaNetwork::KeyMgmtMask::FILS_SHA256 |
                           ISupplicantStaNetwork::KeyMgmtMask::FILS_SHA384));
}
