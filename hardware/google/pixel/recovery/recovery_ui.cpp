/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <stdint.h>
#include <string.h>

#include <string>
#include <string_view>
#include <vector>

#include <android-base/endian.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <app_nugget.h>
#include <misc_writer/misc_writer.h>
#include <nos/NuggetClient.h>
#include <nos/debug.h>
#include <recovery_ui/device.h>
#include <recovery_ui/screen_ui.h>

namespace android {
namespace hardware {
namespace google {
namespace pixel {

namespace {

/** Wipe user data from Titan M. */
bool WipeTitanM() {
    // Connect to Titan M
    ::nos::NuggetClient client;
    client.Open();
    if (!client.IsOpen()) {
        LOG(ERROR) << "Failed to connect to Titan M";
        return false;
    }

    // Tell it to wipe user data
    const uint32_t magicValue = htole32(ERASE_CONFIRMATION);
    std::vector<uint8_t> magic(sizeof(magicValue));
    memcpy(magic.data(), &magicValue, sizeof(magicValue));
    const uint32_t status
            = client.CallApp(APP_ID_NUGGET, NUGGET_PARAM_NUKE_FROM_ORBIT, magic, nullptr);
    if (status != APP_SUCCESS) {
        LOG(ERROR) << "Titan M user data wipe failed: " << ::nos::StatusCodeString(status)
                   << " (" << status << ")";
        return false;
    }

    LOG(INFO) << "Titan M wipe successful";
    return true;
}

// Wipes the provisioned flag as part of data wipe.
bool WipeProvisionedFlag() {
    // Must be consistent with the one in init.hardware.rc (10-byte `theme-dark`).
    MiscWriter misc_writer(MiscWriterActions::kClearDarkThemeFlag);
    if (!misc_writer.PerformAction()) {
        LOG(ERROR) << "Failed to clear the dark theme flag";
        return false;
    }
    LOG(INFO) << "Provisioned flag wiped successful";
    return true;
}

// Provision Silent OTA(SOTA) flag while reason is "enable-sota"
bool ProvisionSilentOtaFlag(const std::string& reason) {
    if (android::base::StartsWith(reason, MiscWriter::kSotaFlag)) {
        MiscWriter misc_writer(MiscWriterActions::kSetSotaFlag);
        if (!misc_writer.PerformAction()) {
            LOG(ERROR) << "Failed to set the silent ota flag";
            return false;
        }
        LOG(INFO) << "Silent ota flag set successful";
    }
    return true;
}

}  // namespace

class PixelDevice : public ::Device {
  public:
    explicit PixelDevice(::ScreenRecoveryUI* const ui) : ::Device(ui) {}

    /** Hook to wipe user data not stored in /data */
    bool PostWipeData() override {
        // Try to do everything but report a failure if anything wasn't successful
        bool totalSuccess = false;
        ::RecoveryUI* const ui = GetUI();

        ui->Print("Wiping Titan M...\n");

        uint32_t retries = 5;
        while (retries--) {
            if (WipeTitanM()) {
                totalSuccess = true;
                break;
            }
        }

        if (!WipeProvisionedFlag()) {
            totalSuccess = false;
        }

        // Extendable to wipe other components

        // Additional behavior along with wiping data
        auto reason = GetReason();
        CHECK(reason.has_value());
        if (!ProvisionSilentOtaFlag(reason.value())) {
            totalSuccess = false;
        }

        return totalSuccess;
    }
};

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

Device *make_device() {
    return new ::android::hardware::google::pixel::PixelDevice(new ::ScreenRecoveryUI);
}
