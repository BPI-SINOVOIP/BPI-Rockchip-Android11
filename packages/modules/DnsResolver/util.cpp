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
 *
 */

#include "util.h"

#include <android-base/parseint.h>
#include <server_configurable_flags/get_flags.h>

using android::base::ParseInt;
using server_configurable_flags::GetServerConfigurableFlag;

socklen_t sockaddrSize(const sockaddr* sa) {
    if (sa == nullptr) return 0;

    switch (sa->sa_family) {
        case AF_INET:
            return sizeof(sockaddr_in);
        case AF_INET6:
            return sizeof(sockaddr_in6);
        default:
            return 0;
    }
}

socklen_t sockaddrSize(const sockaddr_storage& ss) {
    return sockaddrSize(reinterpret_cast<const sockaddr*>(&ss));
}

int getExperimentFlagInt(const std::string& flagName, int defaultValue) {
    int val = defaultValue;
    ParseInt(GetServerConfigurableFlag("netd_native", flagName, ""), &val);
    return val;
}
