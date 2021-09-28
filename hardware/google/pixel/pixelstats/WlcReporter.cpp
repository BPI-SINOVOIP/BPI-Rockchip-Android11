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

#define LOG_TAG "pixelstats-wlc"

#include <android-base/file.h>
#include <android-base/strings.h>
#include <hardware/google/pixel/pixelstats/pixelatoms.pb.h>
#include <log/log.h>
#include <pixelstats/OrientationCollector.h>
#include <pixelstats/WlcReporter.h>

#define GOOGLE_PTMC_ID 0x72
#define ID_UNKNOWN 0

using android::base::ReadFileToString;
using android::frameworks::stats::V1_0::IStats;
using android::frameworks::stats::V1_0::VendorAtom;

namespace android {
namespace hardware {
namespace google {
namespace pixel {

WlcReporter::WlcStatus::WlcStatus()
    : is_charging(false), check_charger_vendor_id(false), check_vendor_id_attempts(0) {}

void WlcReporter::checkAndReport(bool online, const char *ptmc_uevent) {
    bool wireless_charging = online;
    bool started_wireless_charging = wireless_charging && !wlc_status_.is_charging;
    wlc_status_.is_charging = wireless_charging;

    if (started_wireless_charging) {
        reportOrientation();
        wlc_status_.check_vendor_id_attempts = 0;
        wlc_status_.check_charger_vendor_id = true;
    }
    if (!wireless_charging) {
        wlc_status_.check_charger_vendor_id = false;
    }
    if (wireless_charging) {
        checkVendorId(ptmc_uevent);
    }
}

void WlcReporter::checkVendorId(const char *ptmc_uevent) {
    if (!ptmc_uevent || !wlc_status_.check_charger_vendor_id) {
        return;
    }
    if (reportVendor(ptmc_uevent)) {
        wlc_status_.check_charger_vendor_id = false;
    }
}

bool WlcReporter::reportVendor(const char *ptmc_uevent) {
    int ptmcId = readPtmcId(ptmc_uevent);
    if (ptmcId == ID_UNKNOWN) {
        if (++(wlc_status_.check_vendor_id_attempts) < kMaxVendorIdAttempts) {
            return false;
        } /* else if ptmc not ready after retry assume ptmc not supported by charger */
    }
    sp<IStats> stats_client = IStats::tryGetService();
    std::vector<VendorAtom::Value> values(1);

    if (stats_client == nullptr) {
        ALOGE("logWlc get IStats fail.");
        return true;
    }

    int vendorCharger = (ptmcId == GOOGLE_PTMC_ID)
                                ? PixelAtoms::WirelessChargingStats::VENDOR_GOOGLE
                                : PixelAtoms::WirelessChargingStats::VENDOR_UNKNOWN;
    VendorAtom::Value tmp;
    tmp.intValue(vendorCharger);
    values[PixelAtoms::WirelessChargingStats::kChargerVendorFieldNumber - kVendorAtomOffset] = tmp;

    // Send vendor atom to IStats HAL
    VendorAtom event = {.reverseDomainName = PixelAtoms::ReverseDomainNames().pixel(),
                        .atomId = PixelAtoms::Ids::WIRELESS_CHARGING_STATS,
                        .values = values};
    Return<void> retStat = stats_client->reportVendorAtom(event);
    if (!retStat.isOk()) {
        ALOGE("Unable to report WLC_STATS to Stats service");
    }
    return true;
}

int WlcReporter::readPtmcId(const char *ptmc_uevent) {
    int id;
    if (sscanf(ptmc_uevent, "POWER_SUPPLY_PTMC_ID=%x", &id) != 1) {
        return ID_UNKNOWN;
    }
    return id;
}

/*  Reference to frameworks/native/libs/ui/include/ui/DisplayInfo.h
 *  translate orientation value from sensor to enum define in
 *  pixelatoms.proto
 */
int WlcReporter::translateDeviceOrientationToAtomValue(int orientation) {
    switch (orientation) {
        case 0:
            return PixelAtoms::DeviceOrientation::ORIENTATION_0;
        case 1:
            return PixelAtoms::DeviceOrientation::ORIENTATION_90;
        case 2:
            return PixelAtoms::DeviceOrientation::ORIENTATION_180;
        case 3:
            return PixelAtoms::DeviceOrientation::ORIENTATION_270;
        default:
            return PixelAtoms::DeviceOrientation::ORIENTATION_UNKNOWN;
    }
}

void WlcReporter::reportOrientation() {
    sp<IStats> stats_client = IStats::tryGetService();

    if (stats_client == nullptr) {
        ALOGE("logWlc get IStats fail.");
        return;
    }
    std::vector<VendorAtom::Value> values(1);


    int orientationFromSensor;
    sp<OrientationCollector> orientationCollector;
    orientationCollector = OrientationCollector::createOrientationCollector();
    if (orientationCollector != nullptr) {
        orientationCollector->pollOrientation(&orientationFromSensor);
        VendorAtom::Value tmp;
        tmp.intValue(translateDeviceOrientationToAtomValue(orientationFromSensor));
        values[PixelAtoms::DeviceOrientation::kOrientationFieldNumber - kVendorAtomOffset] = tmp;

        VendorAtom event = {.reverseDomainName = PixelAtoms::ReverseDomainNames().pixel(),
                            .atomId = PixelAtoms::Ids::DEVICE_ORIENTATION,
                            .values = values};
        Return<void> retOrientation = stats_client->reportVendorAtom(event);
        if (!retOrientation.isOk())
            ALOGE("Unable to report Orientation to Stats service");
        orientationCollector->disableOrientationSensor();
    }
}

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
