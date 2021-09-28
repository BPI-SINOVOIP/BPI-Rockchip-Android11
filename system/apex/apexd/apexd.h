/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef ANDROID_APEXD_APEXD_H_
#define ANDROID_APEXD_APEXD_H_

#include <string>
#include <vector>

#include <android-base/macros.h>
#include <android-base/result.h>

#include "apex_constants.h"
#include "apex_file.h"

namespace android {
namespace apex {

class CheckpointInterface;

android::base::Result<void> resumeRevertIfNeeded();

// Keep it for now to make otapreopt_chroot keep happy.
// TODO(b/137086602): remove this function.
android::base::Result<void> scanPackagesDirAndActivate(
    const char* apex_package_dir);
void scanStagedSessionsDirAndStage();
android::base::Result<void> preinstallPackages(
    const std::vector<std::string>& paths) WARN_UNUSED;
android::base::Result<void> postinstallPackages(
    const std::vector<std::string>& paths) WARN_UNUSED;

android::base::Result<void> stagePackages(
    const std::vector<std::string>& tmpPaths) WARN_UNUSED;
android::base::Result<void> unstagePackages(
    const std::vector<std::string>& paths) WARN_UNUSED;

android::base::Result<std::vector<ApexFile>> submitStagedSession(
    const int session_id, const std::vector<int>& child_session_ids,
    const bool has_rollback_enabled, const bool is_rollback,
    const int rollback_id) WARN_UNUSED;
android::base::Result<void> markStagedSessionReady(const int session_id)
    WARN_UNUSED;
android::base::Result<void> markStagedSessionSuccessful(const int session_id)
    WARN_UNUSED;
android::base::Result<void> revertActiveSessions(
    const std::string& crashing_native_process);
android::base::Result<void> revertActiveSessionsAndReboot(
    const std::string& crashing_native_process);

android::base::Result<void> activatePackage(const std::string& full_path)
    WARN_UNUSED;
android::base::Result<void> deactivatePackage(const std::string& full_path)
    WARN_UNUSED;

std::vector<ApexFile> getActivePackages();
android::base::Result<ApexFile> getActivePackage(
    const std::string& package_name);

std::vector<ApexFile> getFactoryPackages();

android::base::Result<void> abortStagedSession(const int session_id);
android::base::Result<void> abortActiveSession();

android::base::Result<ino_t> snapshotCeData(const int user_id,
                                            const int rollback_id,
                                            const std::string& apex_name);
android::base::Result<void> restoreCeData(const int user_id,
                                          const int rollback_id,
                                          const std::string& apex_name);
android::base::Result<void> destroyDeSnapshots(const int rollback_id);
android::base::Result<void> destroyCeSnapshotsNotSpecified(
    int user_id, const std::vector<int>& retain_rollback_ids);

int onBootstrap();
// Small helper function to tell if device is currently booting.
bool isBooting();
// Sets the values of gVoldService and gInFsCheckpointMode.
void initializeVold(CheckpointInterface* checkpoint_service);
// Initializes in-memory state (e.g. pre-installed data, activated apexes).
// Must be called first before calling any other boot sequence related function.
void initialize(CheckpointInterface* checkpoint_service);
// Migrates sessions from /data/apex/session to /metadata/session.i
// Must only be called during boot (i.e apexd.status is not "ready" or
// "activated").
android::base::Result<void> migrateSessionsDirIfNeeded();
// Apex activation logic. Scans staged apex sessions and activates apexes.
// Must only be called during boot (i.e apexd.status is not "ready" or
// "activated").
void onStart();
// Notifies system that apexes are activated by setting apexd.status property to
// "activated".
// Must only be called during boot (i.e. apexd.status is not "ready" or
// "activated").
void onAllPackagesActivated();
// Notifies system that apexes are ready by setting apexd.status property to
// "ready".
// Must only be called during boot (i.e. apexd.status is not "ready" or
// "activated").
void onAllPackagesReady();
void bootCompletedCleanup();
int snapshotOrRestoreDeUserData();

int unmountAll();

// Optimistically tries to remount as many APEX packages as possible.
// For more documentation see corresponding binder call in IApexService.aidl.
android::base::Result<void> remountPackages();

}  // namespace apex
}  // namespace android

#endif  // ANDROID_APEXD_APEXD_H_
