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

#include <log/log.h>
#include "data_sink.h"

namespace goldfish {

void DataSink::gnssLocation(const ahg20::GnssLocation& loc) const {
    std::unique_lock<std::mutex> lock(mtx);
    if (cb20) {
        cb20->gnssLocationCb_2_0(loc);
    }
}

void DataSink::gnssSvStatus(const hidl_vec<ahg20::IGnssCallback::GnssSvInfo>& svInfoList20) const {
    std::unique_lock<std::mutex> lock(mtx);
    if (cb20) {
        cb20->gnssSvStatusCb_2_0(svInfoList20);
    }
}

void DataSink::gnssStatus(const ahg10::IGnssCallback::GnssStatusValue status) const {
    std::unique_lock<std::mutex> lock(mtx);
    if (cb20) {
        cb20->gnssStatusCb(status);
    }
}

void DataSink::gnssNmea(const ahg10::GnssUtcTime t,
                        const hidl_string& nmea) const {
    std::unique_lock<std::mutex> lock(mtx);
    if (cb20) {
        cb20->gnssNmeaCb(t, nmea);
    }
}

void DataSink::setCallback20(sp<ahg20::IGnssCallback> cb) {
    std::unique_lock<std::mutex> lock(mtx);
    cb20 = std::move(cb);
}

void DataSink::cleanup() {
    std::unique_lock<std::mutex> lock(mtx);
    cb20 = nullptr;
}

}  // namespace goldfish
