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
#include <atomic>
#include <mutex>
#include <thread>
#include <android/hardware/gnss/2.0/IGnssMeasurement.h>

namespace goldfish {
namespace ahg = ::android::hardware::gnss;
namespace ahg20 = ahg::V2_0;
namespace ahg11 = ahg::V1_1;
namespace ahg10 = ahg::V1_0;
using GnssMeasurementStatus10 = ahg10::IGnssMeasurement::GnssMeasurementStatus;

using ::android::sp;
using ::android::hardware::Return;

struct GnssMeasurement20 : public ahg20::IGnssMeasurement {
    ~GnssMeasurement20();

    // Methods from V2_0::IGnssMeasurement follow.
    Return<GnssMeasurementStatus10> setCallback_2_0(const sp<ahg20::IGnssMeasurementCallback>& callback, bool enableFullTracking) override;

    // Methods from V1_1::IGnssMeasurement follow.
    Return<GnssMeasurementStatus10> setCallback_1_1(const sp<ahg11::IGnssMeasurementCallback>& callback, bool enableFullTracking) override;

    // Methods from V1_0::IGnssMeasurement follow.
    Return<GnssMeasurementStatus10> setCallback(const sp<ahg10::IGnssMeasurementCallback>& callback) override;
    Return<void> close() override;

private:
    void startLocked();
    void stopLocked();
    void update();

    sp<ahg20::IGnssMeasurementCallback> m_callback;
    std::thread                         m_thread;
    std::atomic<bool>                   m_isRunning;
    mutable std::mutex                  m_mtx;
};

}  // namespace goldfish
