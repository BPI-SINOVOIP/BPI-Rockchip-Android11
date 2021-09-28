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
#include <fmq/EventFlag.h>

namespace android {
namespace hardware {
namespace audio {
namespace V6_0 {
namespace implementation {

struct IOThread {
    static constexpr uint32_t STAND_BY_REQUEST = 1 << 20;
    static constexpr uint32_t EXIT_REQUEST = 1 << 21;

    virtual ~IOThread() {}
    virtual EventFlag *getEventFlag() = 0;
    virtual bool notify(uint32_t mask);
    virtual bool standby();
    virtual void requestExit();
};

}  // namespace implementation
}  // namespace V6_0
}  // namespace audio
}  // namespace hardware
}  // namespace android
