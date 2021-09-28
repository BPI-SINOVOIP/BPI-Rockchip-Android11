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

#ifndef HARDWARE_GOOGLE_PIXEL_PIXELSTATS_WLCREPORTER_H
#define HARDWARE_GOOGLE_PIXEL_PIXELSTATS_WLCREPORTER_H

#include <android/frameworks/stats/1.0/IStats.h>

using android::frameworks::stats::V1_0::IStats;

namespace android {
namespace hardware {
namespace google {
namespace pixel {

/**
 * A class to upload wireless metrics
 */
class WlcReporter : public RefBase {
  public:
    void checkAndReport(const bool online, const char *ptmc_uevent);

  private:
    struct WlcStatus {
        bool is_charging;
        bool check_charger_vendor_id;
        int check_vendor_id_attempts;
        WlcStatus();
    };
    WlcStatus wlc_status_;

    void checkVendorId(const char *ptmc_uevent);

    void reportOrientation();
    bool reportVendor(const char *ptmc_uevent);
    // Translate device orientation value from sensor Hal to atom enum value
    int translateDeviceOrientationToAtomValue(int orientation);

    // Proto messages are 1-indexed and VendorAtom field numbers start at 2, so
    // store everything in the values array at the index of the field number
    // -2.
    const int kVendorAtomOffset = 2;
    const int kMaxVendorIdAttempts = 5;

    int readPtmcId(const char *ptmc_uevent);
};

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_PIXELSTATS_WLCREPORTER_H
