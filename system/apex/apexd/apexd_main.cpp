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

#define LOG_TAG "apexd"

#include <strings.h>

#include <ApexProperties.sysprop.h>
#include <android-base/logging.h>

#include "apexd.h"
#include "apexd_checkpoint_vold.h"
#include "apexd_prepostinstall.h"
#include "apexd_prop.h"
#include "apexservice.h"

#include <android-base/properties.h>

namespace {

int HandleSubcommand(char** argv) {
  if (strcmp("--pre-install", argv[1]) == 0) {
    LOG(INFO) << "Preinstall subcommand detected";
    return android::apex::RunPreInstall(argv);
  }

  if (strcmp("--post-install", argv[1]) == 0) {
    LOG(INFO) << "Postinstall subcommand detected";
    return android::apex::RunPostInstall(argv);
  }

  if (strcmp("--bootstrap", argv[1]) == 0) {
    LOG(INFO) << "Bootstrap subcommand detected";
    return android::apex::onBootstrap();
  }

  if (strcmp("--unmount-all", argv[1]) == 0) {
    LOG(INFO) << "Unmount all subcommand detected";
    return android::apex::unmountAll();
  }

  if (strcmp("--snapshotde", argv[1]) == 0) {
    LOG(INFO) << "Snapshot DE subcommand detected";
    // Need to know if checkpointing is enabled so that a prerestore snapshot
    // can be taken if it's not.
    android::base::Result<android::apex::VoldCheckpointInterface>
        vold_service_st = android::apex::VoldCheckpointInterface::Create();
    if (!vold_service_st.ok()) {
      LOG(ERROR) << "Could not retrieve vold service: "
                 << vold_service_st.error();
    } else {
      android::apex::initializeVold(&*vold_service_st);
    }

    int result = android::apex::snapshotOrRestoreDeUserData();

    if (result == 0) {
      // Notify other components (e.g. init) that all APEXs are ready to be used
      // Note that it's important that the binder service is registered at this
      // point, since other system services might depend on it.
      android::apex::onAllPackagesReady();
    }
    return result;
  }

  LOG(ERROR) << "Unknown subcommand: " << argv[1];
  return 1;
}

void InstallSigtermSignalHandler() {
  struct sigaction action = {};
  action.sa_handler = [](int /*signal*/) {
    // Handle SIGTERM gracefully.
    // By default, when SIGTERM is received a process will exit with non-zero
    // exit code, which will trigger reboot_on_failure handler if one is
    // defined. This doesn't play well with userspace reboot which might
    // terminate apexd with SIGTERM if apexd was running at the moment of
    // userspace reboot, hence this custom handler to exit gracefully.
    _exit(0);
  };
  sigaction(SIGTERM, &action, nullptr);
}

}  // namespace

int main(int /*argc*/, char** argv) {
  android::base::InitLogging(argv, &android::base::KernelLogger);
  // TODO: add a -v flag or an external setting to change LogSeverity.
  android::base::SetMinimumLogSeverity(android::base::VERBOSE);

  InstallSigtermSignalHandler();

  const bool has_subcommand = argv[1] != nullptr;
  if (!android::sysprop::ApexProperties::updatable().value_or(false)) {
    LOG(INFO) << "This device does not support updatable APEX. Exiting";
    if (!has_subcommand) {
      // mark apexd as activated so that init can proceed
      android::apex::onAllPackagesActivated();
    } else if (strcmp("--snapshotde", argv[1]) == 0) {
      // mark apexd as ready
      android::apex::onAllPackagesReady();
    }
    return 0;
  }

  if (has_subcommand) {
    return HandleSubcommand(argv);
  }

  android::base::Result<android::apex::VoldCheckpointInterface>
      vold_service_st = android::apex::VoldCheckpointInterface::Create();
  android::apex::VoldCheckpointInterface* vold_service = nullptr;
  if (!vold_service_st.ok()) {
    LOG(ERROR) << "Could not retrieve vold service: "
               << vold_service_st.error();
  } else {
    vold_service = &*vold_service_st;
  }
  android::apex::initialize(vold_service);

  bool booting = android::apex::isBooting();
  if (booting) {
    if (auto res = android::apex::migrateSessionsDirIfNeeded(); !res.ok()) {
      LOG(ERROR) << "Failed to migrate sessions to /metadata partition : "
                 << res.error();
    }
    android::apex::onStart();
  }
  android::apex::binder::CreateAndRegisterService();
  android::apex::binder::StartThreadPool();

  if (booting) {
    // Notify other components (e.g. init) that all APEXs are correctly mounted
    // and activated (but are not yet ready to be used). Configuration based on
    // activated APEXs may be performed at this point, but use of APEXs
    // themselves should wait for the ready status instead, which is set when
    // the "--snapshotde" subcommand is received and snapshot/restore is
    // complete.
    android::apex::onAllPackagesActivated();
    android::apex::waitForBootStatus(
        android::apex::revertActiveSessionsAndReboot,
        android::apex::bootCompletedCleanup);
  }

  android::apex::binder::AllowServiceShutdown();

  android::apex::binder::JoinThreadPool();
  return 1;
}
