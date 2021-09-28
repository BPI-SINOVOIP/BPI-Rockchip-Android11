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
#include <memory>
#include <android/hardware/audio/common/6.0/types.h>
#include <android/hardware/audio/6.0/types.h>

namespace android {
namespace hardware {
namespace audio {
namespace V6_0 {
namespace implementation {

using namespace ::android::hardware::audio::common::V6_0;
using namespace ::android::hardware::audio::V6_0;

struct DevicePortSink {
    virtual ~DevicePortSink() {}
    virtual Result getPresentationPosition(uint64_t &frames, TimeSpec &ts) = 0;
    virtual int write(const void *data, size_t nBytes) = 0;

    static std::unique_ptr<DevicePortSink> create(const DeviceAddress &,
                                                  const AudioConfig &,
                                                  const hidl_bitfield<AudioOutputFlag> &,
                                                  uint64_t &frames);
};

}  // namespace implementation
}  // namespace V6_0
}  // namespace audio
}  // namespace hardware
}  // namespace android
