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
#include <memory>
#include "data_sink.h"
#include "gnss_hw_conn.h"

namespace goldfish {
namespace ahg = ::android::hardware::gnss;
namespace ahg20 = ahg::V2_0;
namespace ahg11 = ahg::V1_1;
namespace ahg10 = ahg::V1_0;
namespace ahgmc10 = ahg::measurement_corrections::V1_0;
namespace ahgvc10 = ahg::visibility_control::V1_0;

using ::android::sp;
using ::android::hardware::Return;

struct Gnss20 : public ahg20::IGnss {
    // Methods from V2_0::IGnss follow.
    Return<sp<ahg20::IGnssConfiguration>> getExtensionGnssConfiguration_2_0() override;
    Return<sp<ahg20::IGnssDebug>> getExtensionGnssDebug_2_0() override;
    Return<sp<ahg20::IAGnss>> getExtensionAGnss_2_0() override;
    Return<sp<ahg20::IAGnssRil>> getExtensionAGnssRil_2_0() override;
    Return<sp<ahg20::IGnssMeasurement>> getExtensionGnssMeasurement_2_0() override;
    Return<bool> setCallback_2_0(const sp<ahg20::IGnssCallback>& callback) override;
    Return<sp<ahgmc10::IMeasurementCorrections>>
    getExtensionMeasurementCorrections() override;
    Return<sp<ahgvc10::IGnssVisibilityControl>> getExtensionVisibilityControl() override;
    Return<sp<ahg20::IGnssBatching>> getExtensionGnssBatching_2_0() override;
    Return<bool> injectBestLocation_2_0(const ahg20::GnssLocation& location) override;

    // Methods from V1_1::IGnss follow.
    Return<bool> setCallback_1_1(const sp<ahg11::IGnssCallback>& callback) override;
    Return<bool> setPositionMode_1_1(ahg10::IGnss::GnssPositionMode mode,
                                     ahg10::IGnss::GnssPositionRecurrence recurrence,
                                     uint32_t minIntervalMs, uint32_t preferredAccuracyMeters,
                                     uint32_t preferredTimeMs, bool lowPowerMode) override;
    Return<sp<ahg11::IGnssConfiguration>> getExtensionGnssConfiguration_1_1() override;
    Return<sp<ahg11::IGnssMeasurement>> getExtensionGnssMeasurement_1_1() override;
    Return<bool> injectBestLocation(const ahg10::GnssLocation& location) override;

    // Methods from V1_0::IGnss follow.
    Return<bool> setCallback(const sp<ahg10::IGnssCallback>& callback) override;
    Return<bool> start() override;
    Return<bool> stop() override;
    Return<void> cleanup() override;
    Return<bool> injectTime(int64_t timeMs, int64_t timeReferenceMs,
                            int32_t uncertaintyMs) override;
    Return<bool> injectLocation(double latitudeDegrees, double longitudeDegrees,
                                float accuracyMeters) override;
    Return<void> deleteAidingData(ahg10::IGnss::GnssAidingData aidingDataFlags) override;
    Return<bool> setPositionMode(ahg10::IGnss::GnssPositionMode mode,
                                 ahg10::IGnss::GnssPositionRecurrence recurrence,
                                 uint32_t minIntervalMs, uint32_t preferredAccuracyMeters,
                                 uint32_t preferredTimeMs) override;
    Return<sp<ahg10::IAGnssRil>> getExtensionAGnssRil() override;
    Return<sp<ahg10::IGnssGeofencing>> getExtensionGnssGeofencing() override;
    Return<sp<ahg10::IAGnss>> getExtensionAGnss() override;
    Return<sp<ahg10::IGnssNi>> getExtensionGnssNi() override;
    Return<sp<ahg10::IGnssMeasurement>> getExtensionGnssMeasurement() override;
    Return<sp<ahg10::IGnssNavigationMessage>> getExtensionGnssNavigationMessage() override;
    Return<sp<ahg10::IGnssXtra>> getExtensionXtra() override;
    Return<sp<ahg10::IGnssConfiguration>> getExtensionGnssConfiguration() override;
    Return<sp<ahg10::IGnssDebug>> getExtensionGnssDebug() override;
    Return<sp<ahg10::IGnssBatching>> getExtensionGnssBatching() override;

private:
    bool open();
    void cleanupImpl();
    bool injectBestLocationImpl(const ahg10::GnssLocation&,
                                const ahg20::ElapsedRealtime);


    DataSink m_dataSink;    // all updates go here

    std::unique_ptr<GnssHwConn> m_gnssHwConn;
    mutable std::mutex          m_gnssHwConnMtx;
};

}  // namespace goldfish
