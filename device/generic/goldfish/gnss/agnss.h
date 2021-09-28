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
#include <android/hardware/gnss/1.0/IAGnss.h>
#include <android/hardware/gnss/2.0/IAGnss.h>

namespace goldfish {
namespace ahg = ::android::hardware::gnss;
namespace ahg10 = ahg::V1_0;
namespace ahg20 = ahg::V2_0;

using ::android::sp;
using ::android::hardware::hidl_string;
using ::android::hardware::Return;

struct AGnss20 : public ahg20::IAGnss {
    Return<void> setCallback(const sp<ahg20::IAGnssCallback>& callback) override;
    Return<bool> dataConnClosed() override;
    Return<bool> dataConnFailed() override;
    Return<bool> setServer(ahg20::IAGnssCallback::AGnssType type,
                           const hidl_string& hostname,
                           int32_t port) override;
    Return<bool> dataConnOpen(uint64_t networkHandle, const hidl_string& apn,
                              ahg20::IAGnss::ApnIpType apnIpType) override;
};

}  // namespace goldfish
