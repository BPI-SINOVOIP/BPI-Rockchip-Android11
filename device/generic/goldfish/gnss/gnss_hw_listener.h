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
#include <vector>
#include "data_sink.h"

namespace goldfish {
using ::android::hardware::hidl_bitfield;

class GnssHwListener {
public:
    explicit GnssHwListener(const DataSink* sink);
    void reset();
    void consume(char);

private:
    bool parse(const char* begin, const char* end, const ahg20::ElapsedRealtime&);
    bool parseGPRMC(const char* begin, const char* end, const ahg20::ElapsedRealtime&);
    bool parseGPGGA(const char* begin, const char* end, const ahg20::ElapsedRealtime&);

    const DataSink*     m_sink;
    std::vector<char>   m_buffer;

    double              m_altitude = 0;

    hidl_bitfield<ahg10::GnssLocationFlags> m_flags;
};

}  // namespace goldfish
