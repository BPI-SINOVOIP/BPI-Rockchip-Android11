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

#include "agnss.h"

namespace goldfish {

Return<void> AGnss20::setCallback(const sp<ahg20::IAGnssCallback>& callback) {
    (void)callback;
    return {};
}

Return<bool> AGnss20::dataConnClosed() {
    return false;
}

Return<bool> AGnss20::dataConnFailed() {
    return false;
}

Return<bool> AGnss20::setServer(ahg20::IAGnssCallback::AGnssType type,
                                const hidl_string& hostname,
                                int32_t port) {
    (void)type;
    (void)hostname;
    (void)port;
    return true;
}

Return<bool> AGnss20::dataConnOpen(uint64_t networkHandle,
                                   const hidl_string& apn,
                                   ahg20::IAGnss::ApnIpType apnIpType) {
    (void)networkHandle;
    (void)apn;
    (void)apnIpType;
    return false;
}

}  // namespace goldfish
