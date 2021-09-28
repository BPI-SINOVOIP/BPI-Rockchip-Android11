//
// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "libgsi/libgsi.h"

#include <string.h>
#include <unistd.h>

#include <string>

#include <android-base/file.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>

#include "file_paths.h"
#include "libgsi_private.h"

namespace android {
namespace gsi {

using namespace std::literals;
using android::base::ReadFileToString;
using android::base::Split;
using android::base::unique_fd;

bool GetActiveDsu(std::string* active_dsu) {
    return android::base::ReadFileToString(kDsuActiveFile, active_dsu);
}

bool IsGsiRunning() {
    return !access(kGsiBootedIndicatorFile, F_OK);
}

bool IsGsiInstalled() {
    return !access(kDsuInstallStatusFile, F_OK);
}

static bool WriteAndSyncFile(const std::string& data, const std::string& file) {
    unique_fd fd(open(file.c_str(), O_WRONLY | O_NOFOLLOW | O_CLOEXEC));
    if (fd < 0) {
        return false;
    }
    if (!android::base::WriteFully(fd, data.c_str(), data.size())) {
        return false;
    }
    return fsync(fd) == 0;
}

std::string GetDsuSlot(const std::string& install_dir) {
    return android::base::Basename(install_dir);
}

bool CanBootIntoGsi(std::string* error) {
    // Always delete this as a safety precaution, so we can return to the
    // original system image. If we're confident GSI will boot, this will
    // get re-created by MarkSystemAsGsi.
    android::base::RemoveFileIfExists(kGsiBootedIndicatorFile);

    if (!IsGsiInstalled()) {
        *error = "not detected";
        return false;
    }

    std::string boot_key;
    if (!GetInstallStatus(&boot_key)) {
        *error = "error ("s + strerror(errno) + ")";
        return false;
    }

    // Give up if we've failed to boot kMaxBootAttempts times.
    int attempts;
    if (GetBootAttempts(boot_key, &attempts)) {
        if (attempts + 1 > kMaxBootAttempts) {
            *error = "exceeded max boot attempts";
            return false;
        }

        std::string new_key;
        if (!access(kDsuOneShotBootFile, F_OK)) {
            // Mark the GSI as disabled. This only affects the next boot, not
            // the current boot. Note that we leave the one_shot status behind.
            // This is so IGsiService can still return GSI_STATE_SINGLE_BOOT
            // while the GSI is running.
            new_key = kInstallStatusDisabled;
        } else {
            new_key = std::to_string(attempts + 1);
        }
        if (!WriteAndSyncFile(new_key, kDsuInstallStatusFile)) {
            *error = "error ("s + strerror(errno) + ")";
            return false;
        }
        return true;
    }

    if (boot_key != kInstallStatusOk) {
        *error = "not enabled";
        return false;
    }
    return true;
}

bool UninstallGsi() {
    return android::base::WriteStringToFile(kInstallStatusWipe, kDsuInstallStatusFile);
}

bool DisableGsi() {
    return android::base::WriteStringToFile(kInstallStatusDisabled, kDsuInstallStatusFile);
}

bool MarkSystemAsGsi() {
    return android::base::WriteStringToFile("1", kGsiBootedIndicatorFile);
}

bool GetInstallStatus(std::string* status) {
    return android::base::ReadFileToString(kDsuInstallStatusFile, status);
}

bool GetBootAttempts(const std::string& boot_key, int* attempts) {
    return android::base::ParseInt(boot_key, attempts);
}

}  // namespace gsi
}  // namespace android
