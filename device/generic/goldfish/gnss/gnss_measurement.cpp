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

#include <chrono>
#include "gnss_measurement.h"
#include "util.h"

namespace goldfish {
using ::android::hardware::hidl_vec;

GnssMeasurement20::~GnssMeasurement20() {
    if (m_isRunning) {
        stopLocked();
    }
}

Return<GnssMeasurementStatus10>
GnssMeasurement20::setCallback_2_0(const sp<ahg20::IGnssMeasurementCallback>& callback,
                                   bool enableFullTracking) {
    (void)enableFullTracking;
    if (callback == nullptr) {
        return GnssMeasurementStatus10::ERROR_GENERIC;
    }

    std::unique_lock<std::mutex> lock(m_mtx);
    if (m_isRunning) {
        stopLocked();
    }

    m_callback = callback;
    startLocked();

    return GnssMeasurementStatus10::SUCCESS;
}

Return<void> GnssMeasurement20::close() {
    std::unique_lock<std::mutex> lock(m_mtx);
    if (m_isRunning) {
        stopLocked();
    }

    m_callback = nullptr;
    return {};
}

void GnssMeasurement20::startLocked() {
    m_thread = std::thread([this](){
        while (m_isRunning) {
            update();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    });
    m_isRunning = true;
}

void GnssMeasurement20::stopLocked() {
    m_isRunning = false;
    m_thread.join();
}

void GnssMeasurement20::update() {
    using GnssMeasurement20 = ahg20::IGnssMeasurementCallback::GnssMeasurement;
    using GnssAccumulatedDeltaRangeState10 = ahg10::IGnssMeasurementCallback::GnssAccumulatedDeltaRangeState;
    using GnssMeasurementFlags10 = ahg10::IGnssMeasurementCallback::GnssMeasurementFlags;
    using GnssMultipathIndicator10 = ahg10::IGnssMeasurementCallback::GnssMultipathIndicator;
    using GnssMeasurementState20 = ahg20::IGnssMeasurementCallback::GnssMeasurementState;
    using GnssData = ahg20::IGnssMeasurementCallback::GnssData;

    ahg10::IGnssMeasurementCallback::GnssMeasurement measurement10 = {
        .flags = GnssMeasurementFlags10::HAS_CARRIER_FREQUENCY | 0,
        .svid = 6,
        .constellation = ahg10::GnssConstellationType::GPS,
        .timeOffsetNs = 0.0,
        .receivedSvTimeInNs = 8195997131077,
        .receivedSvTimeUncertaintyInNs = 15,
        .cN0DbHz = 30.0,
        .pseudorangeRateMps = -484.13739013671875,
        .pseudorangeRateUncertaintyMps = 0.12,
        .accumulatedDeltaRangeState = GnssAccumulatedDeltaRangeState10::ADR_STATE_UNKNOWN | 0,
        .accumulatedDeltaRangeM = 0.0,
        .accumulatedDeltaRangeUncertaintyM = 0.0,
        .carrierFrequencyHz = 1.59975e+09,
        .multipathIndicator = GnssMultipathIndicator10::INDICATOR_UNKNOWN
    };
    ahg11::IGnssMeasurementCallback::GnssMeasurement measurement11 = {
        .v1_0 = measurement10,
        .accumulatedDeltaRangeState = 0
    };

    ahg20::IGnssMeasurementCallback::GnssMeasurement measurement20 = {
        .v1_1 = measurement11,
        .codeType = "C",
        .state = GnssMeasurementState20::STATE_CODE_LOCK |
                 GnssMeasurementState20::STATE_BIT_SYNC |
                 GnssMeasurementState20::STATE_SUBFRAME_SYNC |
                 GnssMeasurementState20::STATE_TOW_DECODED |
                 GnssMeasurementState20::STATE_GLO_STRING_SYNC |
                 GnssMeasurementState20::STATE_GLO_TOD_DECODED,
        .constellation = ahg20::GnssConstellationType::GPS,
    };

    hidl_vec<GnssMeasurement20> measurements(1);
    measurements[0] = measurement20;

    const int64_t nowNs = util::nowNanos();
    const int64_t fullBiasNs = (nowNs % 15331) * ((nowNs & 1) ? -1 : 1);
    const int64_t hwTimeNs = nowNs + fullBiasNs; // local hardware clock

    ahg10::IGnssMeasurementCallback::GnssClock clock10 = {
        .gnssClockFlags = 0,
        .leapSecond = 0,
        .timeNs = hwTimeNs,
        .timeUncertaintyNs = 4.5,
        .fullBiasNs = fullBiasNs,
        .biasNs = 1.5,
        .biasUncertaintyNs = .7,
        .driftNsps = -51.757811607455452,
        .driftUncertaintyNsps = 310.64968328491528,
        .hwClockDiscontinuityCount = 1
    };

    GnssData gnssData = {
        .measurements = measurements,
        .clock = clock10,
        .elapsedRealtime = util::makeElapsedRealtime(util::nowNanos())
    };

    std::unique_lock<std::mutex> lock(m_mtx);
    m_callback->gnssMeasurementCb_2_0(gnssData);
}

/// old and deprecated /////////////////////////////////////////////////////////
Return<GnssMeasurementStatus10> GnssMeasurement20::setCallback_1_1(const sp<ahg11::IGnssMeasurementCallback>&, bool) {
    return GnssMeasurementStatus10::ERROR_GENERIC;
}

Return<GnssMeasurementStatus10> GnssMeasurement20::setCallback(const sp<ahg10::IGnssMeasurementCallback>&) {
    return GnssMeasurementStatus10::ERROR_GENERIC;
}


}  // namespace goldfish
