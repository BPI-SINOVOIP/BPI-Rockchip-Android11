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
#include <android/hardware/gnss/2.0/IGnss.h>
#include <mutex>

namespace goldfish {
namespace ahg = ::android::hardware::gnss;
namespace ahg20 = ahg::V2_0;
namespace ahg10 = ahg::V1_0;

using ::android::sp;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;

class DataSink {
public:
    void gnssLocation(const ahg20::GnssLocation&) const;
    void gnssSvStatus(const hidl_vec<ahg20::IGnssCallback::GnssSvInfo>&) const;
    void gnssStatus(const ahg10::IGnssCallback::GnssStatusValue) const;
    void gnssNmea(const ahg10::GnssUtcTime, const hidl_string&) const;

    void setCallback20(sp<ahg20::IGnssCallback>);
    void cleanup();

private:
    sp<ahg20::IGnssCallback> cb20;
    mutable std::mutex       mtx;
};

}  // namespace goldfish
